/* fragments.py database: _stamp, db_path, _scan_file, build_corpus, _fragment_records, open_db. */
#include "frag.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/hash.h"
#include "avp/pool.h"
#include "avp/sort.h"

#include <stdlib.h>
#include <string.h>

const char *frag_db_path(AvpArena *a, const ShsmRepo *r) { return avp_path_join(a, r->index_dir, "fragments.sqlite"); }

AVP_NORETURN void frag_sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static void exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) frag_sql_fail(db);
}

static void repr_value(AvpArena *a, AvpBuf *b, sqlite3_stmt *st, int col) {
    switch (sqlite3_column_type(st, col)) {
    case SQLITE_NULL: avp_buf_putz(b, "None"); break;
    case SQLITE_INTEGER: avp_buf_printf(b, "%lld", (long long)sqlite3_column_int64(st, col)); break;
    default: {
        AvpStr t = avp_strn((const char *)sqlite3_column_text(st, col), (size_t)sqlite3_column_bytes(st, col));
        avp_buf_put(b, avp_repr_str(a, t));
    }
    }
}

/* sha1("%s|%d|%d|%r" % (SCHEMA, window, sites, rows)) */
const char *frag_stamp(AvpArena *a, ShsmAsmIndex *ix, i64 window, i64 sites) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ix->db, "select path, size, mtime_ns from files order by path", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(ix->db);
    AvpBuf b = {0};
    avp_buf_printf(&b, FRAG_SCHEMA "|%lld|%lld|[", (long long)window, (long long)sites);
    int n = 0, rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        if (n++) avp_buf_putz(&b, ", ");
        avp_buf_putc(&b, '(');
        for (int c = 0; c < 3; c++) {
            if (c) avp_buf_putz(&b, ", ");
            repr_value(a, &b, st, c);
        }
        avp_buf_putc(&b, ')');
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) frag_sql_fail(ix->db);
    avp_buf_putc(&b, ']');
    AvpStr s = avp_buf_str(&b);
    char *hex = avp_sha1_hex(a, s.p, s.n);
    avp_buf_free(&b);
    return hex;
}

/* ---- _scan_file ---- */

typedef struct {
    i64 address, first;
    const char *name, *unit;
    FragRowIn *rows;
    size_t n;
} ScanFn;

typedef struct {
    const char *path, *unit;
} ScanWork;

typedef struct {
    AvpArena *arena;
    AVP_VEC(ScanFn) fns;
    char *error;
    int code;
} ScanResult;

static int scan_file(void *ctx, size_t index, const void *item, void *result) {
    (void)ctx;
    (void)index;
    const ScanWork *w = item;
    ScanResult *res = result;
    res->arena = avp_arena_new(0);
    AvpArena *a = res->arena;
    AvpTry t;
    ShsmAsmFile *af = NULL;
    if (AVP_TRY(&t)) {
        af = shsm_asm_parse_file(w->path, w->unit, 0);
        for (size_t i = 0; i < af->blocks.n; i++) {
            const ShsmAsmBlock *b = af->blocks.v[i];
            if (strcmp(b->kind, "function") != 0) continue;
            if (!(avp_str_eqz(b->section, ".text") || avp_str_eqz(b->section, ".init")) || !b->rows.n) continue;
            size_t n = b->rows.n;
            bool *flags = avp_alloc(a, n * sizeof *flags);
            frag_frame_flags(b->rows.v, n, flags);
            ScanFn f = {0};
            f.address = b->address;
            f.first = b->rows.v[0].address;
            f.name = avp_cz(a, b->name);
            f.unit = w->unit ? avp_strdup(a, w->unit) : NULL;
            f.n = n;
            f.rows = avp_alloc(a, n * sizeof *f.rows);
            for (size_t r = 0; r < n; r++) {
                if (flags[r]) {
                    f.rows[r].sep = true;
                    continue;
                }
                FragNorm x;
                frag_norm_row(a, &b->rows.v[r], b->address, b->name, &x);
                FragRowIn *ri = &f.rows[r];
                ri->tok[FRAG_STRICT] = x.strict;
                ri->tok[FRAG_LOOSE] = x.loose;
                ri->nregs = x.nregs;
                ri->regs = avp_memdup(a, x.regs, sizeof x.regs);
                ri->disp = x.disp;
            }
            avp_vec_push(&res->fns, f);
        }
        avp_try_end(&t);
    } else {
        res->code = t.code;
        res->error = _strdup(t.msg ? t.msg : "");
    }
    if (af) shsm_asm_free(af);
    return 0;
}

FragCorpus *frag_build_corpus(const ShsmRepo *r, ShsmAsmIndex *ix, int jobs) {
    AvpArena *a = avp_arena();
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ix->db, "select distinct f.path, f.unit from files f join symbols s on s.file_id = f.id"
                                   " where s.kind = 'function'", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(ix->db);
    AVP_VEC(ScanWork) work = {0};
    while (sqlite3_step(st) == SQLITE_ROW) {
        ScanWork w;
        w.path = avp_path_join(a, r->asm_dir, (const char *)sqlite3_column_text(st, 0));
        w.unit = sqlite3_column_type(st, 1) == SQLITE_NULL ? NULL
                                                             : avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        avp_vec_push(&work, w);
    }
    sqlite3_finalize(st);
    ScanResult *res = avp_xcalloc(work.n ? work.n : 1, sizeof *res);
    int nj = jobs > 0 ? jobs : shsm_default_jobs();
    avp_pool_map(NULL, work.n <= 1 ? 1 : nj, scan_file, NULL, work.v, work.n, sizeof *work.v, res, sizeof *res);
    for (size_t i = 0; i < work.n; i++) {
        if (res[i].error) {
            int code = res[i].code;
            const char *msg = avp_strdup(a, res[i].error);
            for (size_t k = 0; k < work.n; k++) {
                free(res[k].error);
                avp_vec_free(&res[k].fns);
                avp_arena_free(res[k].arena);
            }
            free(res);
            avp_vec_free(&work);
            avp_throw(code, "%s", msg);
        }
    }
    FragCorpus *c = frag_corpus_new(FRAG_NLEVELS);
    for (size_t i = 0; i < work.n; i++) {
        for (size_t k = 0; k < res[i].fns.n; k++) {
            const ScanFn *sf = &res[i].fns.v[k];
            i32 fi = frag_corpus_add(c, sf->rows, sf->n);
            FragFn *f = &c->fns.v[fi];
            f->address = sf->address;
            f->first = sf->first;
            f->name = avp_strdup(c->arena, sf->name);
            f->unit = sf->unit ? avp_strdup(c->arena, sf->unit) : NULL;
        }
        avp_vec_free(&res[i].fns);
        avp_arena_free(res[i].arena);
    }
    free(res);
    avp_vec_free(&work);
    return c;
}

/* ---- _fragment_records ---- */

typedef struct {
    i64 length, nfuncs, nunits, nocc, score, first_addr;
    AvpStrVec pattern;
    const FragRepeat *f;
} Rec;

static int cmp_len_occ(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const FragRepeat *a = x, *b = y;
    if (a->length != b->length) return a->length > b->length ? -1 : 1;
    if (a->occ.n != b->occ.n) return a->occ.n > b->occ.n ? -1 : 1;
    return 0;
}

static int cmp_rec(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Rec *a = x, *b = y;
    if (a->score != b->score) return a->score > b->score ? -1 : 1;
    if (a->nfuncs != b->nfuncs) return a->nfuncs > b->nfuncs ? -1 : 1;
    if (a->first_addr != b->first_addr) return a->first_addr < b->first_addr ? -1 : 1;
    return 0;
}

static void write_level(sqlite3 *db, FragCorpus *c, int level, FragRepeats frags) {
    AvpArena *a = avp_arena_new(0);
    FragRepeats kept = {0};
    for (size_t i = 0; i < frags.n; i++) {
        if (frag_boilerplate(c, level, &frags.v[i])) avp_vec_free(&frags.v[i].occ);
        else avp_vec_push(&kept, frags.v[i]);
    }
    avp_vec_free(&frags);
    avp_sort(kept.v, kept.n, sizeof *kept.v, cmp_len_occ, NULL, false);
    kept = frag_drop_contained(kept);
    Rec *recs = avp_xcalloc(kept.n ? kept.n : 1, sizeof *recs);
    for (size_t i = 0; i < kept.n; i++) {
        const FragRepeat *f = &kept.v[i];
        Rec *x = &recs[i];
        x->f = f;
        x->length = f->length;
        AvpISet *fis = avp_iset_new();
        AvpSet *units = avp_set_new();
        bool none_unit = false;
        for (size_t o = 0; o < f->occ.n; o++) {
            i32 fi = f->occ.v[o].fi;
            if (!avp_iset_add(fis, (u64)(u32)fi)) continue;
            const char *u = c->fns.v[fi].unit;
            if (u) avp_set_add(units, avp_str(u));
            else none_unit = true;
        }
        x->nfuncs = (i64)avp_iset_len(fis);
        x->nunits = (i64)avp_set_len(units) + (none_unit ? 1 : 0);
        avp_iset_free(fis);
        avp_set_free(units);
        x->nocc = (i64)f->occ.n;
        x->score = x->nfuncs * x->length;
        x->first_addr = c->fns.v[f->occ.v[0].fi].first + 4 * (i64)f->occ.v[0].p;
        size_t ns = f->occ.n < FRAG_RENDER_SAMPLE ? f->occ.n : FRAG_RENDER_SAMPLE;
        FragRow *rows = avp_alloc(a, ns * (size_t)f->length * sizeof *rows);
        for (size_t o = 0; o < ns; o++)
            frag_corpus_rows(c, f->occ.v[o].fi, f->occ.v[o].p, f->length, rows + o * (size_t)f->length);
        FragRender rr = frag_render(a, rows, ns, (size_t)f->length);
        for (size_t k = 0; k < rr.lines.n; k++) avp_vec_push(&x->pattern, rr.lines.v[k]);
        frag_render_free(&rr, ns);
    }
    avp_sort(recs, kept.n, sizeof *recs, cmp_rec, NULL, false);
    sqlite3_stmt *fs, *os;
    if (sqlite3_prepare_v2(db, "insert into fragments values (?,?,?,?,?,?,?,?,?)", -1, &fs, NULL) != SQLITE_OK)
        frag_sql_fail(db);
    if (sqlite3_prepare_v2(db, "insert into occurrences values (?,?,?,?,?,?)", -1, &os, NULL) != SQLITE_OK)
        frag_sql_fail(db);
    for (size_t i = 0; i < kept.n; i++) {
        const Rec *x = &recs[i];
        AvpArenaMark mk = avp_arena_mark(a);
        char fid[32];
        snprintf(fid, sizeof fid, "%c%zu", FRAG_LEVEL_PREFIX[level], i + 1);
        AvpJson *pj = avp_json_arr(a);
        for (size_t k = 0; k < x->pattern.n; k++) avp_json_arr_push(a, pj, avp_json_str_ref(a, x->pattern.v[k]));
        AvpStr pat = avp_json_dumps(a, pj, NULL);
        sqlite3_bind_text(fs, 1, fid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(fs, 2, FRAG_LEVELS[level], -1, SQLITE_STATIC);
        sqlite3_bind_int64(fs, 3, (i64)i + 1);
        sqlite3_bind_int64(fs, 4, x->length);
        sqlite3_bind_int64(fs, 5, x->nfuncs);
        sqlite3_bind_int64(fs, 6, x->nunits);
        sqlite3_bind_int64(fs, 7, x->nocc);
        sqlite3_bind_int64(fs, 8, x->score);
        sqlite3_bind_text(fs, 9, pat.p, (int)pat.n, SQLITE_TRANSIENT);
        if (sqlite3_step(fs) != SQLITE_DONE) frag_sql_fail(db);
        sqlite3_reset(fs);
        for (size_t o = 0; o < x->f->occ.n; o++) {
            const FragFn *fn = &c->fns.v[x->f->occ.v[o].fi];
            i64 s = x->f->occ.v[o].p;
            sqlite3_bind_text(os, 1, fid, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(os, 2, fn->address);
            sqlite3_bind_text(os, 3, fn->name, -1, SQLITE_STATIC);
            if (fn->unit) sqlite3_bind_text(os, 4, fn->unit, -1, SQLITE_STATIC);
            else sqlite3_bind_null(os, 4);
            sqlite3_bind_int64(os, 5, s);
            sqlite3_bind_int64(os, 6, fn->first + 4 * s);
            if (sqlite3_step(os) != SQLITE_DONE) frag_sql_fail(db);
            sqlite3_reset(os);
        }
        avp_arena_reset(a, mk);
    }
    sqlite3_finalize(fs);
    sqlite3_finalize(os);
    for (size_t i = 0; i < kept.n; i++) avp_vec_free(&recs[i].pattern);
    free(recs);
    frag_repeats_free(&kept);
    avp_arena_free(a);
}

static void put_meta(sqlite3 *db, const char *k, const char *v) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "insert or replace into meta values (?, ?)", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(db);
    sqlite3_bind_text(st, 1, k, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, v, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) frag_sql_fail(db);
    sqlite3_finalize(st);
}

sqlite3 *frag_open_db(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild, int jobs, i64 window, i64 sites,
                      FragLogFn log) {
    AvpArena *a = avp_arena();
    if (!avp_makedirs(r->index_dir, true)) shsm_raise_io();
    const char *path = frag_db_path(a, r);
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        const char *msg = db ? avp_strdup(a, sqlite3_errmsg(db)) : "unable to open database file";
        sqlite3_close(db);
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_busy_timeout(db, 5000); /* sqlite3.connect's default timeout */
    exec(db, "create table if not exists meta(key text primary key, value text)");
    const char *stamp = frag_stamp(a, ix, window, sites);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "select value from meta where key='stamp'", -1, &st, NULL) != SQLITE_OK)
        frag_sql_fail(db);
    const char *have = NULL;
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL)
        have = avp_strdup(a, (const char *)sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    if (!rebuild && have && strcmp(have, stamp) == 0) return db;
    double t0 = avp_time();
    FragCorpus *c = frag_build_corpus(r, ix, jobs);
    double t1 = avp_time();
    exec(db, "begin");
    exec(db, "drop table if exists fragments");
    exec(db, "drop table if exists occurrences");
    exec(db, "create table fragments(id text primary key, level text, rank integer, length integer,"
             " nfuncs integer, nunits integer, nocc integer, score integer, pattern text)");
    exec(db, "create table occurrences(fragment text, fn_address integer, fn_name text, unit text,"
             " row integer, address integer)");
    for (int lv = 0; lv < FRAG_NLEVELS; lv++)
        write_level(db, c, lv, frag_repeats(c, lv, (i32)window, (i32)sites));
    exec(db, "create index if not exists occ_fragment on occurrences(fragment)");
    exec(db, "create index if not exists occ_fn on occurrences(fn_address)");
    double t2 = avp_time();
    i64 rows = 0;
    for (size_t i = 0; i < c->fns.n; i++) rows += (i64)c->fns.v[i].n;
    put_meta(db, "stamp", stamp);
    put_meta(db, "window", avp_sprintf(a, "%lld", (long long)window));
    put_meta(db, "sites", avp_sprintf(a, "%lld", (long long)sites));
    put_meta(db, "functions", avp_sprintf(a, "%zu", c->fns.n));
    put_meta(db, "rows", avp_sprintf(a, "%lld", (long long)rows));
    put_meta(db, "scan_seconds", avp_cz(a, avp_pyfmt(a, "%.1f", t1 - t0)));
    put_meta(db, "search_seconds", avp_cz(a, avp_pyfmt(a, "%.1f", t2 - t1)));
    exec(db, "commit");
    if (log)
        log(avp_cz(a, avp_pyfmt(a, "fragments: indexed %d functions in %.1f s (scan %.1f s, search %.1f s)",
                                (i64)c->fns.n, t2 - t0, t1 - t0, t2 - t1)));
    frag_corpus_free(c);
    return db;
}
