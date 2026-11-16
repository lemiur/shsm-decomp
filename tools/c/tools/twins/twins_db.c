/* twins.py fingerprints and open_db: normalise_row, fingerprint, _stamp, the fingerprint table. */
#include "twins.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/hash.h"
#include "avp/pool.h"
#include "shsm_re.h"

#include <sqlite3.h>
#include <string.h>

const char *const TWINS_LEVELS[TWINS_NLEVELS] = {"exact", "ref", "shape"};

int twins_level_index(const char *name) {
    for (int i = 0; i < TWINS_NLEVELS; i++)
        if (name && strcmp(name, TWINS_LEVELS[i]) == 0) return i;
    return -1;
}

/* ---- normalise_row ---- */

/* _sym_token: the operand text with the referenced symbol (quoted or bare) replaced by a marker \x00. */
static AvpStr sym_token(AvpArena *a, AvpStr ops, AvpStr sym) {
    AvpStr esc = avp_str_replace(a, sym, AVP_S("\\"), AVP_S("\\\\"), -1);
    esc = avp_str_replace(a, esc, AVP_S("\""), AVP_S("\\\""), -1);
    AvpStr quoted = avp_pyfmt(a, "\"%S\"", &esc);
    AvpStr spelled[2] = {quoted, sym};
    for (int k = 0; k < 2; k++) {
        i64 i = avp_str_find(ops, spelled[k], 0, AVP_END);
        if (i >= 0) {
            AvpBuf b;
            avp_buf_init(&b, a);
            avp_buf_putn(&b, ops.p, (size_t)i);
            avp_buf_putc(&b, '\0');
            avp_buf_putn(&b, ops.p + i + spelled[k].n, ops.n - (size_t)i - spelled[k].n);
            return avp_buf_str(&b);
        }
    }
    return ops;
}

void twins_normalise_row(AvpArena *a, const ShsmAsmRow *row, i64 fn_start, const char *fn_name, AvpStr out[3]) {
    static const AvpStr NUL = {"\0", 1}, MARK = {"\x01", 1};
    AvpStr m = row->mnemonic;
    AvpStr ops = row->operands.p ? row->operands : AVP_S("");
    AvpStr lab = {NULL, 0};
    i64 label = row->label;
    AvpStr fname = avp_str(fn_name);
    bool has_sym = row->sym.p && row->sym.n;
    if (label == SHSM_NONE && fn_name && *fn_name && has_sym && avp_str_eq(row->sym, fname) && row->reloc &&
        strcmp(row->reloc, "rel24") == 0)
        label = fn_start; /* recursion or a loop back to the start: position-independent */
    if (label != SHSM_NONE) {
        lab = avp_pyfmt(a, "@%+d", label - fn_start);
        ops = shsm_re_sub(a, shsm_re("\\.L_[0-9A-Fa-f]{8}", 0), MARK, ops, 0);
        if (avp_str_find(ops, MARK, 0, AVP_END) < 0 && fn_name && *fn_name)
            ops = avp_str_replace(a, sym_token(a, ops, fname), NUL, MARK, -1);
    }
    AvpStr exact, ref;
    if (has_sym && !lab.p) {
        AvpStr marked = sym_token(a, ops, row->sym);
        ref = avp_str_replace(a, marked, NUL, row->sym, -1);
        AvpStr kind = avp_pyfmt(a, "<%s>", row->reloc ? row->reloc : "sym");
        exact = avp_str_replace(a, marked, NUL, kind, -1);
    } else {
        ref = exact = ops;
    }
    AvpStr shape = shsm_re_sub(a, shsm_re("\\b(r|f|qr)\\d+\\b", 0), AVP_S("\\1"), exact, 0);
    shape = shsm_re_sub(a, shsm_re("\\bcr\\d\\b", 0), AVP_S("cr"), shape, 0);
    shape = shsm_re_sub(a, shsm_re("(?<![\\w@])-?(?:0x[0-9A-Fa-f]+|\\d+)\\b", 0), AVP_S("#"), shape, 0);
    if (lab.p) {
        exact = avp_str_replace(a, exact, MARK, lab, -1);
        ref = avp_str_replace(a, ref, MARK, lab, -1);
        shape = avp_str_replace(a, shape, MARK, lab, -1);
    }
    out[0] = avp_pyfmt(a, "%S %S", &m, &exact);
    out[1] = avp_pyfmt(a, "%S %S", &m, &ref);
    out[2] = avp_pyfmt(a, "%S %S", &m, &shape);
}

void twins_fingerprint(const ShsmAsmBlock *b, char out[3][21]) {
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    AvpHasher h[3];
    for (int i = 0; i < 3; i++) avp_hasher_init(&h[i], AVP_HASH_SHA1);
    const char *name = b->name.p ? avp_cz(s, b->name) : NULL;
    for (size_t r = 0; r < b->rows.n; r++) {
        AvpArenaMark rm = avp_arena_mark(s);
        AvpStr t[3];
        twins_normalise_row(s, &b->rows.v[r], b->address, name, t);
        for (int i = 0; i < 3; i++) {
            avp_hasher_update(&h[i], t[i].p, t[i].n);
            avp_hasher_update(&h[i], "\n", 1);
        }
        avp_arena_reset(s, rm);
    }
    for (int i = 0; i < 3; i++) {
        u8 d[20];
        avp_hasher_final(&h[i], d);
        for (int k = 0; k < 10; k++) snprintf(out[i] + 2 * k, 3, "%02x", d[k]);
        out[i][20] = '\0';
    }
    avp_arena_reset(s, mk);
}

/* ---- the table ---- */

const char *twins_db_path(AvpArena *a, const ShsmRepo *r) { return avp_path_join(a, r->index_dir, "twins.sqlite"); }

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static void exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) sql_fail(db);
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

const char *twins_stamp(AvpArena *a, ShsmAsmIndex *ix) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ix->db, "select path, size, mtime_ns from files order by path", -1, &st, NULL) != SQLITE_OK)
        sql_fail(ix->db);
    AvpBuf b = {0};
    avp_buf_putz(&b, TWINS_SCHEMA "|[");
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
    if (rc != SQLITE_DONE) sql_fail(ix->db);
    avp_buf_putc(&b, ']');
    AvpStr s = avp_buf_str(&b);
    char *hex = avp_sha1_hex(a, s.p, s.n);
    avp_buf_free(&b);
    return hex;
}

typedef struct {
    i64 address;
    const char *name, *unit;
    i64 size, nrows;
    char fp[3][21];
} FpRow;

typedef struct {
    const char *path, *unit;
} FpWork;

typedef struct {
    AvpArena *arena;
    AVP_VEC(FpRow) rows;
    const char *error; /* heap, or NULL */
    int code;
} FpResult;

static int fingerprint_file(void *ctx, size_t index, const void *item, void *result) {
    (void)ctx;
    (void)index;
    const FpWork *w = item;
    FpResult *res = result;
    res->arena = avp_arena_new(0);
    AvpTry t;
    ShsmAsmFile *af = NULL;
    if (AVP_TRY(&t)) {
        af = shsm_asm_parse_file(w->path, w->unit, 0);
        for (size_t i = 0; i < af->blocks.n; i++) {
            const ShsmAsmBlock *b = af->blocks.v[i];
            if (strcmp(b->kind, "function") != 0) continue;
            if (!(avp_str_eqz(b->section, ".text") || avp_str_eqz(b->section, ".init")) || !b->rows.n) continue;
            FpRow r = {0};
            r.address = b->address;
            r.name = avp_cz(res->arena, b->name);
            r.unit = w->unit ? avp_strdup(res->arena, w->unit) : NULL;
            r.size = (b->size != SHSM_NONE && b->size) ? b->size : 4 * (i64)b->rows.n;
            r.nrows = (i64)b->rows.n;
            twins_fingerprint(b, r.fp);
            avp_vec_push(&res->rows, r);
        }
        avp_try_end(&t);
    } else {
        res->code = t.code;
        res->error = _strdup(t.msg ? t.msg : "");
    }
    if (af) shsm_asm_free(af);
    return 0;
}

static void bind_text_or_null(sqlite3_stmt *st, int i, const char *s) {
    if (s) sqlite3_bind_text(st, i, s, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, i);
}

sqlite3 *twins_open_db(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild, int jobs) {
    AvpArena *a = avp_arena();
    if (!avp_makedirs(r->index_dir, true)) shsm_raise_io();
    const char *path = twins_db_path(a, r);
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        const char *msg = db ? avp_strdup(a, sqlite3_errmsg(db)) : "unable to open database file";
        sqlite3_close(db);
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_busy_timeout(db, 5000); /* sqlite3.connect's default timeout */
    exec(db, "create table if not exists meta(key text primary key, value text)");
    exec(db, "create table if not exists fingerprints(address integer primary key, name text, unit text,"
             " size integer, nrows integer, exact text, ref text, shape text)");
    const char *stamp = twins_stamp(a, ix);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "select value from meta where key='stamp'", -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    const char *have = NULL;
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL)
        have = avp_strdup(a, (const char *)sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    if (!(rebuild || !have || strcmp(have, stamp) != 0)) return db;

    if (sqlite3_prepare_v2(ix->db, "select distinct f.path, f.unit from files f join symbols s on s.file_id = f.id"
                                   " where s.kind = 'function'", -1, &st, NULL) != SQLITE_OK)
        sql_fail(ix->db);
    AVP_VEC(FpWork) work = {0};
    while (sqlite3_step(st) == SQLITE_ROW) {
        FpWork w;
        w.path = avp_path_join(a, r->asm_dir, (const char *)sqlite3_column_text(st, 0));
        w.unit = sqlite3_column_type(st, 1) == SQLITE_NULL ? NULL
                                                             : avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        avp_vec_push(&work, w);
    }
    sqlite3_finalize(st);
    FpResult *res = avp_xcalloc(work.n ? work.n : 1, sizeof *res);
    int nj = jobs > 0 ? jobs : shsm_default_jobs();
    avp_pool_map(NULL, work.n <= 1 ? 1 : nj, fingerprint_file, NULL, work.v, work.n, sizeof *work.v, res, sizeof *res);
    for (size_t i = 0; i < work.n; i++) {
        if (res[i].error) {
            int code = res[i].code;
            const char *msg = avp_strdup(a, res[i].error);
            for (size_t k = 0; k < work.n; k++) {
                free((void *)res[k].error);
                avp_vec_free(&res[k].rows);
                avp_arena_free(res[k].arena);
            }
            free(res);
            sqlite3_close(db);
            avp_throw(code, "%s", msg);
        }
    }
    exec(db, "begin");
    exec(db, "delete from fingerprints");
    if (sqlite3_prepare_v2(db, "insert or replace into fingerprints values (?,?,?,?,?,?,?,?)", -1, &st, NULL) !=
        SQLITE_OK)
        sql_fail(db);
    for (size_t i = 0; i < work.n; i++) {
        for (size_t k = 0; k < res[i].rows.n; k++) {
            const FpRow *fr = &res[i].rows.v[k];
            sqlite3_bind_int64(st, 1, fr->address);
            bind_text_or_null(st, 2, fr->name);
            bind_text_or_null(st, 3, fr->unit);
            sqlite3_bind_int64(st, 4, fr->size);
            sqlite3_bind_int64(st, 5, fr->nrows);
            for (int c = 0; c < 3; c++) sqlite3_bind_text(st, 6 + c, fr->fp[c], -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) != SQLITE_DONE) sql_fail(db);
            sqlite3_reset(st);
        }
        avp_vec_free(&res[i].rows);
        avp_arena_free(res[i].arena);
    }
    sqlite3_finalize(st);
    free(res);
    avp_vec_free(&work);
    for (int lv = 0; lv < TWINS_NLEVELS; lv++)
        exec(db, avp_sprintf(a, "create index if not exists fp_%s on fingerprints(%s)", TWINS_LEVELS[lv],
                             TWINS_LEVELS[lv]));
    if (sqlite3_prepare_v2(db, "insert or replace into meta values ('stamp', ?)", -1, &st, NULL) != SQLITE_OK)
        sql_fail(db);
    sqlite3_bind_text(st, 1, stamp, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_DONE) sql_fail(db);
    sqlite3_finalize(st);
    exec(db, "commit");
    return db;
}
