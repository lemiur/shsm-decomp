/* sibling: the sibling's asm tree (written by dtk from the ELF), both sides' function models, and the database
 * build/index/sibling/<name>/sibling.sqlite (tables meta, functions, unused, shsm). */
#include "sibling.h"

#include "avp/fs.h"
#include "avp/pool.h"
#include "avp/proc.h"
#include "shsm_asm.h"
#include "shsm_libraries.h"
#include "twins.h"

#include <stdlib.h>
#include <string.h>

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static void exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) sql_fail(db);
}

static sqlite3_stmt *prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static const char *col(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return NULL;
    return avp_strdup(a, (const char *)sqlite3_column_text(st, c));
}

static void bind_str(sqlite3_stmt *st, int i, const char *s) {
    if (s) sqlite3_bind_text(st, i, s, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, i);
}

Sibling *sibling_open(const ShsmRepo *r, const char *name) {
    AvpArena *a = avp_arena();
    Sibling *sb = avp_alloc(a, sizeof *sb);
    sb->repo = r;
    sb->name = name ? name : SIBLING_DEFAULT_NAME;
    for (const char *p = sb->name; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '.')
            shsm_raise(SHSM_EXC_USAGE, "--name must be a plain word: %s", sb->name);
    sb->dir = avp_path_join(a, avp_path_join(a, r->build_dir, "sibling"), sb->name);
    const char *idx = avp_path_join(a, avp_path_join(a, r->index_dir, "sibling"), sb->name);
    if (!avp_makedirs(idx, true)) shsm_raise_io();
    sb->db_path = avp_path_join(a, idx, "sibling.sqlite");
    if (sqlite3_open_v2(sb->db_path, &sb->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        shsm_raise(SHSM_EXC_SQLITE, "cannot open %s", sb->db_path);
    sqlite3_busy_timeout(sb->db, 120000);
    exec(sb->db, "create table if not exists meta(key text primary key, value text)");
    exec(sb->db, "create table if not exists functions(address integer primary key, name text, size integer,"
                 " nrows integer, lib text, object text, ord integer, exact text, ref text, callees text)");
    exec(sb->db, "create table if not exists unused(lib text, object text, name text, size integer, ord integer,"
                 " nbefore integer)");
    /* a database indexed before the column existed: add it (NULL rows read as -1 until the next index) */
    sqlite3_exec(sb->db, "alter table unused add column nbefore integer", NULL, NULL, NULL);
    exec(sb->db, "create table if not exists shsm(address integer primary key, name text, size integer,"
                 " nrows integer, lib text, unit text, exact text, ref text, callees text, tier integer,"
                 " peer integer)");
    return sb;
}

void sibling_close(Sibling *sb) {
    if (sb && sb->db) sqlite3_close(sb->db);
}

static i64 count_rows(Sibling *sb, const char *table) {
    AvpArena *a = avp_scratch();
    sqlite3_stmt *st = prep(sb->db, avp_sprintf(a, "select count(*) from %s", table));
    i64 n = sqlite3_step(st) == SQLITE_ROW ? sqlite3_column_int64(st, 0) : 0;
    sqlite3_finalize(st);
    return n;
}

bool sibling_has_index(Sibling *sb) { return count_rows(sb, "functions") > 0; }
bool sibling_has_pairs(Sibling *sb) { return count_rows(sb, "shsm") > 0; }

/* ---- reading asm trees (both sides, in parallel) ---- */

typedef struct {
    const char *path, *unit;
} AsmWork;
typedef struct {
    SibFnVec fns;
    const char *error;
    int code;
} AsmResult;

static int read_listing(void *ctx, size_t index, const void *item, void *result) {
    (void)ctx;
    (void)index;
    const AsmWork *w = item;
    AsmResult *res = result;
    AvpArena *a = avp_arena_new(0); /* kept: the functions point into it */
    AvpTry t;
    ShsmAsmFile *af = NULL;
    if (AVP_TRY(&t)) {
        af = shsm_asm_parse_file(w->path, w->unit, 0);
        AVP_VEC(AvpStr) ctors = {0}; /* symbols of this unit's .ctors section: its static initialisers */
        for (size_t i = 0; i < af->blocks.n; i++) {
            const ShsmAsmBlock *b = af->blocks.v[i];
            if (!avp_str_eqz(b->section, ".ctors")) continue;
            for (size_t k = 0; k < b->items.n; k++)
                if (b->items.v[k].sym.p && b->items.v[k].sym.n) avp_vec_push(&ctors, b->items.v[k].sym);
        }
        for (size_t i = 0; i < af->blocks.n; i++) {
            const ShsmAsmBlock *b = af->blocks.v[i];
            if (strcmp(b->kind, "function") != 0 || !b->rows.n) continue;
            if (!(avp_str_eqz(b->section, ".text") || avp_str_eqz(b->section, ".init"))) continue;
            /* dtk's alignment padding between objects (pad_ in the retail tree, gap_ in the sibling's) is no function */
            if (avp_str_startswith(b->name, AVP_S("pad_")) || avp_str_startswith(b->name, AVP_S("gap_"))) continue;
            SibFn f = {0};
            f.address = b->address;
            f.name = avp_cz(a, b->name);
            f.object = w->unit ? avp_strdup(a, w->unit) : NULL;
            f.size = (b->size != SHSM_NONE && b->size) ? b->size : 4 * (i64)b->rows.n;
            f.nrows = (i64)b->rows.n;
            f.peer = -1;
            f.initialiser = sibling_is_initialiser(f.name);
            for (size_t k = 0; k < ctors.n; k++)
                if (avp_str_eq(ctors.v[k], b->name)) f.initialiser = true;
            char fp[3][21];
            twins_fingerprint(b, fp);
            memcpy(f.exact, fp[0], 21);
            memcpy(f.ref, fp[1], 21);
            AVP_VEC(const char *) cal = {0};
            for (size_t r = 0; r < b->rows.n; r++) {
                const ShsmAsmRow *row = &b->rows.v[r];
                if (row->label != SHSM_NONE || !row->reloc || strcmp(row->reloc, "rel24") != 0) continue;
                if (!row->sym.p || !row->sym.n) continue;
                avp_vec_push(&cal, avp_cz(a, row->sym));
            }
            if (cal.n) {
                f.callees = avp_memdup(a, cal.v, cal.n * sizeof *cal.v);
                f.ncallees = cal.n;
            }
            avp_vec_free(&cal);
            avp_vec_push(&res->fns, f);
        }
        avp_vec_free(&ctors);
        avp_try_end(&t);
    } else {
        res->code = t.code;
        res->error = _strdup(t.msg ? t.msg : "");
    }
    if (af) shsm_asm_free(af);
    return 0;
}

static int by_address(const void *x, const void *y) {
    const SibFn *a = x, *b = y;
    return a->address < b->address ? -1 : a->address > b->address;
}

static SibFnVec read_tree(const AsmWork *work, size_t n, int jobs) {
    AsmResult *res = avp_xcalloc(n ? n : 1, sizeof *res);
    int nj = jobs > 0 ? jobs : shsm_default_jobs();
    avp_pool_map(NULL, n <= 1 ? 1 : nj, read_listing, NULL, work, n, sizeof *work, res, sizeof *res);
    SibFnVec out = {0};
    for (size_t i = 0; i < n; i++) {
        if (res[i].error) shsm_raise(res[i].code, "%s: %s", work[i].path, res[i].error);
        for (size_t k = 0; k < res[i].fns.n; k++) avp_vec_push(&out, res[i].fns.v[k]);
        avp_vec_free(&res[i].fns);
    }
    free(res);
    qsort(out.v, out.n, sizeof *out.v, by_address);
    return out;
}

SibFnVec sibling_load_shsm_asm(AvpArena *a, const ShsmRepo *r, const char *asm_dir, int jobs) {
    ShsmListings *ls = shsm_asm_listings(a, asm_dir ? asm_dir : r->asm_dir);
    AVP_VEC(AsmWork) work = {0};
    for (size_t i = 0; i < ls->rels.n; i++) {
        AvpStr rel = ls->rels.v[i];
        if (rel.n > 2 && avp_str_endswith(rel, AVP_S(".s"))) rel.n -= 2;
        avp_vec_push(&work, ((AsmWork){avp_cz(a, ls->paths.v[i]), avp_cz(a, rel)}));
    }
    if (!work.n) shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "no asm listings under %s (run configure and ninja)",
                            asm_dir ? asm_dir : r->asm_dir);
    SibFnVec v = read_tree(work.v, work.n, jobs);
    avp_vec_free(&work);
    const ShsmLibraries *libs = shsm_load_libraries(r);
    for (size_t i = 0; i < v.n; i++) {
        const char *lib = shsm_libraries_lookup(a, libs, v.v[i].address);
        v.v[i].lib = lib && *lib ? lib : "unknown";
    }
    return v;
}

/* ---- index ---- */

static void run_dtk(AvpArena *a, const ShsmRepo *r, const char *const *argv) {
    AvpProcResult res = avp_run_text(a, r->root, argv);
    if (res.status != AVP_PROC_OK || res.returncode != 0) {
        AvpStr err = avp_str_strip(res.err.n ? res.err : res.out);
        if (err.n > 600) err = avp_strn(err.p + err.n - 600, 600);
        shsm_raise(SHSM_EXC_OS, "dtk %s %s failed (exit %lld): %s", argv[1], argv[2], (long long)res.returncode,
                   avp_cz(a, err));
    }
}

static AvpStr read_file(AvpArena *a, const char *path) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    return avp_strn((const char *)b.v, b.n);
}

static void write_file(const char *path, AvpStr s) {
    if (!avp_write_file(path, s.p, s.n)) shsm_raise_io();
}

static void walk_listings(AvpArena *a, const char *top, void *vec) {
    AVP_VEC(AsmWork) *work = vec;
    AvpWalk *w = avp_walk_open(top, true);
    AvpWalkItem it;
    while (avp_walk_next(w, &it)) {
        for (size_t i = 0; i < it.files.n; i++) {
            AvpStr f = it.files.v[i];
            if (!avp_str_endswith(f, AVP_S(".s"))) continue;
            const char *dir = avp_cz(a, it.dirpath);
            const char *path = avp_path_join(a, dir, avp_cz(a, f));
            avp_vec_push(work, ((AsmWork){path, avp_cz(a, avp_strn(f.p, f.n - 2))}));
        }
    }
    avp_walk_close(w);
}

i64 sibling_index(Sibling *sb, const char *elf, const char *map, int jobs, ShsmOutput *out) {
    AvpArena *a = avp_arena();
    const ShsmRepo *r = sb->repo;
    if (!avp_isfile(elf)) shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "sibling ELF not found: %s (pass --elf)", elf);
    if (!avp_isfile(map)) shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "sibling map not found: %s (pass --map)", map);
    const char *dtk = shsm_repo_tool(a, r, "dtk");
    if (!avp_isfile(dtk)) shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "dtk not found: %s", dtk);
    if (avp_exists(sb->dir) && !avp_rmtree(sb->dir, 0)) shsm_raise_io();
    if (!avp_makedirs(sb->dir, true)) shsm_raise_io();
    AvpStr elf_bytes = read_file(a, elf);
    u64 text_end = sibling_elf_text_end(elf_bytes);
    if (!text_end) shsm_raise(SHSM_EXC_VALUE, "%s: not a big-endian ELF32 with a .text section", elf);
    const char *cfg = avp_path_join(a, sb->dir, "cfg");
    run_dtk(a, r, (const char *const[]){dtk, "elf", "config", elf, cfg, NULL});
    const char *splits = avp_path_join(a, sb->dir, "splits.txt"), *symbols = avp_path_join(a, sb->dir, "symbols.txt");
    write_file(splits, sibling_fix_splits(a, read_file(a, avp_path_join(a, cfg, "splits.txt")), text_end));
    write_file(symbols, sibling_fix_symbols(a, read_file(a, avp_path_join(a, cfg, "symbols.txt"))));
    const char *dol = avp_path_join(a, sb->dir, "sibling.dol");
    run_dtk(a, r, (const char *const[]){dtk, "elf2dol", elf, dol, NULL});
    const char *yml = avp_path_join(a, sb->dir, "config.yml");
    write_file(yml, avp_str(avp_sprintf(a, "object: %s\nsymbols: %s\nsplits: %s\nquick_analysis: true\n"
                                           "symbols_known: true\n",
                                        shsm_repo_rel(a, r, dol), shsm_repo_rel(a, r, symbols),
                                        shsm_repo_rel(a, r, splits))));
    const char *split = avp_path_join(a, sb->dir, "split");
    run_dtk(a, r, (const char *const[]){dtk, "dol", "split", yml, split, NULL});
    AVP_VEC(AsmWork) work = {0};
    walk_listings(a, avp_path_join(a, split, "asm"), &work);
    SibFnVec fns = read_tree(work.v, work.n, jobs);
    avp_vec_free(&work);
    SibMap *m = sibling_parse_map(a, read_file(a, map));
    if (!m->text.n) shsm_raise(SHSM_EXC_VALUE, "%s: no .text section layout symbols", map);
    AvpIMap *at = avp_imap_new();
    for (size_t i = 0; i < m->text.n; i++) {
        bool created;
        AvpVal *v = avp_imap_put(at, (u64)m->text.v[i].address, &created);
        if (created) v->p = &m->text.v[i];
    }
    exec(sb->db, "begin immediate");
    exec(sb->db, "delete from functions");
    exec(sb->db, "delete from unused");
    exec(sb->db, "delete from shsm");
    sqlite3_stmt *st = prep(sb->db, "insert or replace into functions values(?,?,?,?,?,?,?,?,?,?)");
    i64 mapped = 0;
    for (size_t i = 0; i < fns.n; i++) {
        SibFn *f = &fns.v[i];
        AvpVal *v = avp_imap_get(at, (u64)f->address);
        const SibMapSym *ms = v ? v->p : NULL;
        mapped += ms != NULL;
        AvpBuf cb;
        avp_buf_init(&cb, avp_scratch());
        for (size_t k = 0; k < f->ncallees; k++) {
            if (k) avp_buf_putc(&cb, '\n');
            avp_buf_putz(&cb, f->callees[k]);
        }
        sqlite3_bind_int64(st, 1, f->address);
        bind_str(st, 2, ms ? ms->name : f->name);
        sqlite3_bind_int64(st, 3, f->size);
        sqlite3_bind_int64(st, 4, f->nrows);
        bind_str(st, 5, ms ? ms->lib : NULL);
        bind_str(st, 6, ms ? ms->object : NULL);
        if (ms) sqlite3_bind_int64(st, 7, ms->order);
        else sqlite3_bind_null(st, 7);
        bind_str(st, 8, f->exact);
        bind_str(st, 9, f->ref);
        bind_str(st, 10, avp_buf_cz(&cb));
        if (sqlite3_step(st) != SQLITE_DONE) sql_fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    st = prep(sb->db, "insert into unused(lib, object, name, size, ord, nbefore) values(?,?,?,?,?,?)");
    for (size_t i = 0; i < m->unused.n; i++) {
        const SibMapSym *u = &m->unused.v[i];
        bind_str(st, 1, u->lib);
        bind_str(st, 2, u->object);
        bind_str(st, 3, u->name);
        sqlite3_bind_int64(st, 4, u->size);
        sqlite3_bind_int64(st, 5, u->order);
        sqlite3_bind_int64(st, 6, u->before);
        if (sqlite3_step(st) != SQLITE_DONE) sql_fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    st = prep(sb->db, "insert or replace into meta values(?,?)");
    const char *meta[][2] = {{"schema", SIBLING_SCHEMA}, {"elf", elf}, {"map", map}};
    for (int k = 0; k < 3; k++) {
        bind_str(st, 1, meta[k][0]);
        bind_str(st, 2, meta[k][1]);
        if (sqlite3_step(st) != SQLITE_DONE) sql_fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    exec(sb->db, "commit");
    avp_imap_free(at);
    if (out) {
        shsm_output_linez(out, avp_sprintf(a, "%s: %zu functions in the asm tree, %lld of them at a symbol of the"
                                              " map's .text layout (%zu map symbols, %zu UNUSED)",
                                           sb->name, fns.n, (long long)mapped, m->text.n, m->unused.n));
        shsm_output_linez(out, avp_sprintf(a, "database %s", shsm_repo_rel(a, r, sb->db_path)));
    }
    return (i64)fns.n;
}

/* ---- loading ---- */

static void split_callees(AvpArena *a, SibFn *f, const char *text) {
    if (!text || !*text) return;
    AvpStrVec parts = avp_str_split(avp_str(text), AVP_S("\n"), -1);
    f->callees = avp_alloc(a, parts.n * sizeof *f->callees);
    for (size_t k = 0; k < parts.n; k++) f->callees[k] = avp_cz(a, parts.v[k]);
    f->ncallees = parts.n;
    avp_vec_free(&parts);
}

SibFnVec sibling_load_sibling(AvpArena *a, Sibling *sb) {
    SibFnVec v = {0};
    sqlite3_stmt *st = prep(sb->db, "select address, name, size, nrows, lib, object, ord, exact, ref, callees"
                                    " from functions order by address");
    while (sqlite3_step(st) == SQLITE_ROW) {
        SibFn f = {0};
        f.address = sqlite3_column_int64(st, 0);
        f.name = col(a, st, 1);
        f.size = sqlite3_column_int64(st, 2);
        f.nrows = sqlite3_column_int64(st, 3);
        f.lib = col(a, st, 4);
        f.object = col(a, st, 5);
        f.order = sqlite3_column_type(st, 6) == SQLITE_NULL ? -1 : sqlite3_column_int64(st, 6);
        const char *e = col(a, st, 7), *r = col(a, st, 8);
        if (e) strncpy(f.exact, e, 20);
        if (r) strncpy(f.ref, r, 20);
        split_callees(a, &f, col(a, st, 9));
        f.peer = -1;
        avp_vec_push(&v, f);
    }
    sqlite3_finalize(st);
    return v;
}

SibFnVec sibling_load_shsm(AvpArena *a, Sibling *sb) {
    SibFnVec v = {0};
    sqlite3_stmt *st = prep(sb->db, "select address, name, size, nrows, lib, unit, exact, ref, callees, tier, peer"
                                    " from shsm order by address");
    while (sqlite3_step(st) == SQLITE_ROW) {
        SibFn f = {0};
        f.address = sqlite3_column_int64(st, 0);
        f.name = col(a, st, 1);
        f.size = sqlite3_column_int64(st, 2);
        f.nrows = sqlite3_column_int64(st, 3);
        f.lib = col(a, st, 4);
        f.object = col(a, st, 5);
        const char *e = col(a, st, 6), *r = col(a, st, 7);
        if (e) strncpy(f.exact, e, 20);
        if (r) strncpy(f.ref, r, 20);
        split_callees(a, &f, col(a, st, 8));
        f.tier = (int)sqlite3_column_int64(st, 9);
        f.peer = sqlite3_column_type(st, 10) == SQLITE_NULL ? -1 : sqlite3_column_int64(st, 10); /* an address */
        avp_vec_push(&v, f);
    }
    sqlite3_finalize(st);
    return v;
}

void sibling_store_pairs(Sibling *sb, const SibFnVec *s, const SibFnVec *g) {
    exec(sb->db, "begin immediate");
    exec(sb->db, "delete from shsm");
    sqlite3_stmt *st = prep(sb->db, "insert or replace into shsm values(?,?,?,?,?,?,?,?,?,?,?)");
    for (size_t i = 0; i < s->n; i++) {
        const SibFn *f = &s->v[i];
        AvpBuf cb;
        avp_buf_init(&cb, avp_scratch());
        for (size_t k = 0; k < f->ncallees; k++) {
            if (k) avp_buf_putc(&cb, '\n');
            avp_buf_putz(&cb, f->callees[k]);
        }
        sqlite3_bind_int64(st, 1, f->address);
        bind_str(st, 2, f->name);
        sqlite3_bind_int64(st, 3, f->size);
        sqlite3_bind_int64(st, 4, f->nrows);
        bind_str(st, 5, f->lib);
        bind_str(st, 6, f->object);
        bind_str(st, 7, f->exact);
        bind_str(st, 8, f->ref);
        bind_str(st, 9, avp_buf_cz(&cb));
        sqlite3_bind_int64(st, 10, f->tier);
        if (f->peer >= 0) sqlite3_bind_int64(st, 11, g->v[f->peer].address);
        else sqlite3_bind_null(st, 11);
        if (sqlite3_step(st) != SQLITE_DONE) sql_fail(sb->db);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    exec(sb->db, "commit");
}

SibMapSymVec sibling_unused_all(AvpArena *a, Sibling *sb) {
    SibMapSymVec out = {0};
    sqlite3_stmt *st = prep(sb->db, "select lib, object, name, size, ord, nbefore from unused order by rowid");
    while (sqlite3_step(st) == SQLITE_ROW) {
        SibMapSym u = {0};
        u.lib = col(a, st, 0);
        u.object = col(a, st, 1);
        u.name = col(a, st, 2);
        u.size = sqlite3_column_int64(st, 3);
        u.order = sqlite3_column_int64(st, 4);
        u.before = sqlite3_column_type(st, 5) == SQLITE_NULL ? -1 : sqlite3_column_int64(st, 5);
        if (!u.lib) u.lib = "";
        avp_vec_push(&out, u);
    }
    sqlite3_finalize(st);
    return out;
}

AvpStrVec sibling_unused_of(AvpArena *a, Sibling *sb, const char *lib, const char *object) {
    AvpStrVec out = {0};
    sqlite3_stmt *st = prep(sb->db, "select name from unused where lib = ? and object = ? order by ord");
    bind_str(st, 1, lib ? lib : "");
    bind_str(st, 2, object);
    while (sqlite3_step(st) == SQLITE_ROW) avp_vec_push(&out, avp_str(col(a, st, 0)));
    sqlite3_finalize(st);
    return out;
}
