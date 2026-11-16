/* The history database (open_db, migrate_rows, Indexer, insert_snapshot) and recording (record_rev,
 * record_worktree, cmd_record, cmd_backfill). */
#include "sh_priv.h"
#include "shsm_git.h"

#include <math.h>
#include <string.h>

#define PARSER_VERSION "3"
#define WORKTREE_REPORT "build/R5WEA4/report.json"

static const char SCHEMA[] =
    "CREATE TABLE IF NOT EXISTS snapshots (\n"
    "    id INTEGER PRIMARY KEY,\n"
    "    key TEXT UNIQUE NOT NULL,\n"
    "    kind TEXT NOT NULL,\n"
    "    time INTEGER NOT NULL,\n"
    "    subject TEXT NOT NULL DEFAULT '',\n"
    "    report_sha TEXT NOT NULL DEFAULT '',\n"
    "    env_sig TEXT NOT NULL DEFAULT ''\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS rows (\n"
    "    snap INTEGER NOT NULL REFERENCES snapshots(id),\n"
    "    symbol TEXT NOT NULL,\n"
    "    addr INTEGER NOT NULL,\n"
    "    unit TEXT NOT NULL,\n"
    "    path TEXT NOT NULL DEFAULT '',\n"
    "    score REAL NOT NULL,\n"
    "    size INTEGER NOT NULL DEFAULT 0,\n"
    "    hash TEXT NOT NULL,\n"
    "    basis TEXT NOT NULL,\n"
    "    unit_hash TEXT NOT NULL DEFAULT '',\n"
    "    line_lo INTEGER NOT NULL DEFAULT 0,\n"
    "    line_hi INTEGER NOT NULL DEFAULT 0,\n"
    "    PRIMARY KEY (snap, unit, addr, symbol)\n"
    ") WITHOUT ROWID;\n"
    "CREATE INDEX IF NOT EXISTS rows_addr ON rows(addr);\n"
    "CREATE TABLE IF NOT EXISTS sources (hash TEXT PRIMARY KEY, text TEXT NOT NULL);\n"
    "CREATE TABLE IF NOT EXISTS blobs (key TEXT PRIMARY KEY, json TEXT NOT NULL);\n";

/* the body of the rows table's CREATE statement, as migrate_rows cut it out of SCHEMA */
static const char ROWS_CREATE[] =
    "\n"
    "    snap INTEGER NOT NULL REFERENCES snapshots(id),\n"
    "    symbol TEXT NOT NULL,\n"
    "    addr INTEGER NOT NULL,\n"
    "    unit TEXT NOT NULL,\n"
    "    path TEXT NOT NULL DEFAULT '',\n"
    "    score REAL NOT NULL,\n"
    "    size INTEGER NOT NULL DEFAULT 0,\n"
    "    hash TEXT NOT NULL,\n"
    "    basis TEXT NOT NULL,\n"
    "    unit_hash TEXT NOT NULL DEFAULT '',\n"
    "    line_lo INTEGER NOT NULL DEFAULT 0,\n"
    "    line_hi INTEGER NOT NULL DEFAULT 0,\n"
    "    PRIMARY KEY (snap, unit, addr, symbol)\n";
#define ROWS_COLS "snap, symbol, addr, unit, path, score, size, hash, basis, unit_hash, line_lo, line_hi"

AVP_NORETURN void sh_sql_fail(ShCtx *c) {
    avp_throw(SH_EXC_OTHER, "sqlite3.OperationalError: %s", c->db ? sqlite3_errmsg(c->db) : "unable to open database file");
}

void sh_exec(ShCtx *c, const char *sql) {
    if (sqlite3_exec(c->db, sql, NULL, NULL, NULL) != SQLITE_OK) sh_sql_fail(c);
}

sqlite3_stmt *sh_prepare(ShCtx *c, const char *sql) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(c->db, sql, -1, &st, NULL) != SQLITE_OK) sh_sql_fail(c);
    return st;
}

static void bind_str(sqlite3_stmt *st, int i, AvpStr s) { sqlite3_bind_text(st, i, s.p ? s.p : "", (int)s.n, SQLITE_TRANSIENT); }
static void bind_cz(sqlite3_stmt *st, int i, const char *s) { sqlite3_bind_text(st, i, s, -1, SQLITE_TRANSIENT); }

static void step_done(ShCtx *c, sqlite3_stmt *st) {
    int rc = sqlite3_step(st);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        sh_sql_fail(c);
    }
}

/* migrate_rows: rebuild an old rows table under the (snap, unit, addr, symbol) key */
static void migrate_rows(ShCtx *c) {
    sqlite3_stmt *st = sh_prepare(c, "PRAGMA table_info(rows)");
    AvpStr key[16];
    int pk[16];
    int nk = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int p = sqlite3_column_int(st, 5);
        if (p && nk < 16) {
            const char *nm = (const char *)sqlite3_column_text(st, 1);
            key[nk] = avp_str(avp_strdup(c->a, nm ? nm : ""));
            pk[nk++] = p;
        }
    }
    sqlite3_finalize(st);
    /* sorted by pk position */
    for (int x = 1; x < nk; x++)
        for (int y = x; y > 0 && pk[y - 1] > pk[y]; y--) {
            int tp = pk[y];
            pk[y] = pk[y - 1];
            pk[y - 1] = tp;
            AvpStr ts = key[y];
            key[y] = key[y - 1];
            key[y - 1] = ts;
        }
    static const char *const want[] = {"snap", "unit", "addr", "symbol"};
    if (nk == 0) return;
    bool same = nk == 4;
    for (int x = 0; same && x < 4; x++) same = avp_str_eqz(key[x], want[x]);
    if (same) return;
    sh_exec(c, "BEGIN");
    sh_exec(c, "DROP TABLE IF EXISTS rows_migrating");
    sh_exec(c, avp_sprintf(c->a, "CREATE TABLE rows_migrating (%s) WITHOUT ROWID", ROWS_CREATE));
    sh_exec(c, "INSERT INTO rows_migrating (" ROWS_COLS ") SELECT " ROWS_COLS " FROM rows");
    i64 counts[2];
    const char *q[2] = {"SELECT COUNT(*) FROM rows", "SELECT COUNT(*) FROM rows_migrating"};
    for (int x = 0; x < 2; x++) {
        st = sh_prepare(c, q[x]);
        sqlite3_step(st);
        counts[x] = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    if (counts[0] != counts[1]) {
        sqlite3_exec(c->db, "ROLLBACK", NULL, NULL, NULL);
        avp_throw(SH_EXC_RUNTIME, "rows migration copied %lld of %lld rows; database left unchanged", counts[1], counts[0]);
    }
    sh_exec(c, "DROP TABLE rows");
    sh_exec(c, "ALTER TABLE rows_migrating RENAME TO rows");
    sh_exec(c, "CREATE INDEX IF NOT EXISTS rows_addr ON rows(addr)");
    sh_exec(c, "COMMIT");
}

void sh_db_open(ShCtx *c, const char *path) {
    char *abs = avp_path_abspath(c->a, path);
    AvpStr d = avp_path_dirname(abs);
    char *dz = avp_cz(c->a, d);
    if (*dz && !avp_isdir(dz) && !avp_makedirs(dz, true))
        avp_throw(SH_EXC_OTHER, "OSError: cannot create the directory %s", dz);
    if (sqlite3_open_v2(path, &c->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) sh_sql_fail(c);
    sh_exec(c, SCHEMA);
    migrate_rows(c);
    c->mem = avp_map_new();
}

void sh_db_close(ShCtx *c) {
    if (c->db) {
        sqlite3_exec(c->db, "ROLLBACK", NULL, NULL, NULL); /* an uncommitted transaction (an exception) is dropped */
        sqlite3_close(c->db);
    }
    c->db = NULL;
    if (c->mem) avp_map_free(c->mem);
    c->mem = NULL;
}

/* ---- Indexer ------------------------------------------------------------------------------------------------------ */

typedef AvpStr (*ShLoader)(void *ctx);

static ShIndex *indexer_get(ShCtx *c, AvpStr cache_key, ShLoader loader, void *lctx) {
    AvpVal *mv = avp_map_get(c->mem, cache_key);
    if (mv) return mv->p;
    char *key = avp_sprintf(c->a, "v" PARSER_VERSION ":%.*s", AVP_SF(cache_key));
    sqlite3_stmt *st = sh_prepare(c, "SELECT json FROM blobs WHERE key=?");
    bind_cz(st, 1, key);
    ShIndex *ix = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        AvpStr js = avp_str_dup(c->a, avp_strn((const char *)sqlite3_column_text(st, 0), (size_t)sqlite3_column_bytes(st, 0)));
        sqlite3_finalize(st);
        ix = sh_index_from_json(c->a, js);
    } else {
        sqlite3_finalize(st);
        ix = sh_index_source(c->a, loader(lctx));
        sqlite3_stmt *ins = sh_prepare(c, "INSERT OR IGNORE INTO sources(hash, text) VALUES (?,?)");
        for (size_t i = 0; i < ix->ntexts; i++) {
            bind_str(ins, 1, ix->text_hash[i]);
            bind_str(ins, 2, ix->text[i]);
            step_done(c, ins);
            sqlite3_reset(ins);
        }
        sqlite3_finalize(ins);
        ins = sh_prepare(c, "INSERT OR REPLACE INTO blobs(key, json) VALUES (?,?)");
        bind_cz(ins, 1, key);
        bind_str(ins, 2, sh_index_to_json(c->a, ix));
        step_done(c, ins);
        sqlite3_finalize(ins);
    }
    bool created;
    avp_map_put(c->mem, avp_str_dup(c->a, cache_key), &created)->p = ix;
    return ix;
}

/* ---- reports -> rows --------------------------------------------------------------------------------------------- */

typedef struct {
    AvpStr symbol;
    i64 addr;
    double score;
    i64 size;
} RFunc;
typedef struct {
    AvpStr unit, source_path;
    RFunc *funcs;
    size_t n;
} RUnit;
typedef AVP_VEC(RUnit) RUnitVec;

typedef struct {
    AvpStr symbol;
    i64 addr;
    AvpStr unit, path;
    double score;
    i64 size;
    AvpStr hash, basis, unit_hash;
    i64 lo, hi;
} Row;
typedef AVP_VEC(Row) RowVec;

static bool py_int(const AvpJson *v, i64 *out) {
    if (!v) return false;
    switch (v->kind) {
    case AVP_JSON_INT:
        if (v->big) return false;
        *out = v->u.i;
        return true;
    case AVP_JSON_BOOL:
        *out = v->b;
        return true;
    case AVP_JSON_FLOAT:
        if (!isfinite(v->u.f)) return false;
        *out = (i64)v->u.f;
        return true;
    case AVP_JSON_STRING:
        return avp_parse_int(v->u.str, 10, out, NULL);
    default:
        return false;
    }
}

static bool is_auto_unit(AvpStr name) {
    if (!avp_str_startswith(name, AVP_S("main/auto_"))) return false;
    size_t q = 10, d = 0;
    while (q < name.n && name.p[q] >= '0' && name.p[q] <= '9') q++, d++;
    return d > 0 && q < name.n && name.p[q] == '_';
}

static AvpJson *parse_report(AvpArena *a, AvpStr bytes) {
    AvpJsonError err;
    AvpJson *r = avp_json_parse_ex(a, bytes, AVP_JSON_BYTES, &err);
    if (!r) avp_throw(SH_EXC_OTHER, "json.decoder.JSONDecodeError: %s", avp_json_error_str(a, &err).p);
    if (!avp_json_is_obj(r)) avp_throw(SH_EXC_OTHER, "AttributeError: the report is not an object");
    return r;
}

/* source_units(report) */
static RUnitVec source_units(AvpArena *a, const AvpJson *report) {
    RUnitVec out = {0};
    AvpJson *units = avp_json_getz(report, "units");
    size_t nu = avp_json_len(units);
    for (size_t i = 0; i < nu; i++) {
        AvpJson *u = avp_json_at(units, i);
        AvpJson *md = avp_json_getz(u, "metadata");
        if (!avp_json_truthy(md)) md = NULL;
        AvpStr name = avp_json_get_str(u, "name", AVP_S(""));
        if ((md && avp_json_truthy(avp_json_getz(md, "auto_generated"))) || is_auto_unit(name)) continue;
        RUnit ru = {0};
        ru.unit = name;
        AvpJson *sp = md ? avp_json_getz(md, "source_path") : NULL;
        ru.source_path = avp_json_truthy(sp) ? avp_json_str_or(sp, AVP_S("")) : AVP_S("");
        AvpJson *fs = avp_json_getz(u, "functions");
        size_t nf = avp_json_truthy(fs) ? avp_json_len(fs) : 0;
        ru.funcs = avp_alloc(a, (nf ? nf : 1) * sizeof(RFunc));
        for (size_t k = 0; k < nf; k++) {
            AvpJson *f = avp_json_at(fs, k);
            AvpJson *fm = avp_json_getz(f, "metadata");
            i64 addr;
            if (!py_int(avp_json_truthy(fm) ? avp_json_getz(fm, "virtual_address") : NULL, &addr)) continue;
            AvpJson *sc = avp_json_getz(f, "fuzzy_match_percent");
            double score = 0.0;
            if (sc && !avp_json_is_null(sc)) {
                if (sc->kind == AVP_JSON_STRING) {
                    if (!avp_parse_float(sc->u.str, &score))
                        avp_throw(SH_EXC_OTHER, "ValueError: could not convert string to float");
                } else {
                    score = avp_json_f64(sc, sc->kind == AVP_JSON_BOOL ? (sc->b ? 1.0 : 0.0) : 0.0);
                }
            }
            AvpJson *sz = avp_json_getz(f, "size");
            i64 size = 0;
            if (avp_json_truthy(sz) && !py_int(sz, &size)) avp_throw(SH_EXC_OTHER, "ValueError: invalid size");
            AvpJson *nm = avp_json_getz(f, "name");
            if (!nm) avp_throw(SH_EXC_OTHER, "KeyError: 'name'");
            ru.funcs[ru.n++] = (RFunc){avp_json_str_or(nm, AVP_S("")), addr, score, size};
        }
        avp_vec_push(&out, ru);
    }
    return out;
}

static const char *const SRC_EXTS[] = {".cpp", ".c", ".cp", ".cc", ".s"}; /* .s: assembly source units */

typedef bool (*ShExists)(void *ctx, const char *p);

static const char *guess_path(AvpArena *a, AvpStr unit, ShExists exists, void *ctx) {
    AvpStr base = avp_str_startswith(unit, AVP_S("main/")) ? avp_str_slice(unit, 5, AVP_END) : unit;
    for (size_t k = 0; k < sizeof SRC_EXTS / sizeof SRC_EXTS[0]; k++) {
        char *p = avp_sprintf(a, "src/%.*s%s", AVP_SF(base), SRC_EXTS[k]);
        if (exists(ctx, p)) return p;
    }
    return "";
}

/* resolve(unit, source_path) -> skip (false), or the path and the index (NULL when there is no source) */
typedef bool (*ShResolve)(ShCtx *c, void *ctx, const RUnit *u, AvpStr *path, ShIndex **ix);

static void make_rows(ShCtx *c, const RUnitVec *units, ShResolve resolve, void *rctx, RowVec *rows, AvpStrVec *skipped) {
    AvpArena *a = c->a;
    for (size_t i = 0; i < units->n; i++) {
        const RUnit *u = &units->v[i];
        AvpStr path = {"", 0};
        ShIndex *ix = NULL;
        if (!resolve(c, rctx, u, &path, &ix)) {
            if (skipped) avp_vec_push(skipped, u->unit);
            continue;
        }
        for (size_t k = 0; k < u->n; k++) {
            const RFunc *f = &u->funcs[k];
            Row r = {0};
            r.symbol = f->symbol;
            r.addr = f->addr;
            r.unit = u->unit;
            r.path = path;
            r.score = f->score;
            r.size = f->size;
            if (!ix) {
                r.hash = avp_str(sh_sha1(a, avp_str(avp_sprintf(a, "missing:%.*s", AVP_SF(u->unit)))));
                r.basis = AVP_S("unit");
                r.unit_hash = AVP_S("");
            } else {
                const char *basis;
                const ShIdxFunc *fn = sh_index_lookup(a, ix, f->symbol, f->addr, &basis);
                r.basis = avp_str(basis);
                r.unit_hash = ix->unit_hash;
                if (fn) {
                    r.hash = fn->hash;
                    r.lo = fn->line;
                    r.hi = fn->end;
                } else if (!strcmp(basis, "sinit")) {
                    r.hash = avp_str(sh_sha1(a, avp_str(avp_sprintf(a, "sinit:%.*s", AVP_SF(ix->rest_hash)))));
                } else {
                    r.hash = avp_str(sh_sha1(a, avp_str(avp_sprintf(a, "unit:%.*s", AVP_SF(ix->unit_hash)))));
                }
            }
            avp_vec_push(rows, r);
        }
    }
}

/* insert_snapshot: true when new */
static bool insert_snapshot(ShCtx *c, const char *key, const char *kind, i64 when, const char *subject,
                            const char *report_sha, const char *env_sig, const RowVec *rows) {
    sqlite3_stmt *st = sh_prepare(c, "SELECT id FROM snapshots WHERE key=?");
    bind_cz(st, 1, key);
    if (sqlite3_step(st) == SQLITE_ROW) {
        i64 id = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        if (!strcmp(kind, "worktree")) {
            st = sh_prepare(c, "UPDATE snapshots SET time=? WHERE id=?");
            sqlite3_bind_int64(st, 1, when);
            sqlite3_bind_int64(st, 2, id);
            step_done(c, st);
            sqlite3_finalize(st);
        }
        return false;
    }
    sqlite3_finalize(st);
    st = sh_prepare(c, "INSERT INTO snapshots(key, kind, time, subject, report_sha, env_sig) VALUES (?,?,?,?,?,?)");
    bind_cz(st, 1, key);
    bind_cz(st, 2, kind);
    sqlite3_bind_int64(st, 3, when);
    bind_cz(st, 4, subject);
    bind_cz(st, 5, report_sha);
    bind_cz(st, 6, env_sig);
    step_done(c, st);
    sqlite3_finalize(st);
    i64 sid = sqlite3_last_insert_rowid(c->db);
    st = sh_prepare(c, "INSERT OR REPLACE INTO rows(snap, symbol, addr, unit, path, score, size, hash, basis, unit_hash,"
                       " line_lo, line_hi) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)");
    for (size_t i = 0; i < rows->n; i++) {
        const Row *r = &rows->v[i];
        sqlite3_bind_int64(st, 1, sid);
        bind_str(st, 2, r->symbol);
        sqlite3_bind_int64(st, 3, r->addr);
        bind_str(st, 4, r->unit);
        bind_str(st, 5, r->path);
        sqlite3_bind_double(st, 6, r->score);
        sqlite3_bind_int64(st, 7, r->size);
        bind_str(st, 8, r->hash);
        bind_str(st, 9, r->basis);
        bind_str(st, 10, r->unit_hash);
        sqlite3_bind_int64(st, 11, r->lo);
        sqlite3_bind_int64(st, 12, r->hi);
        step_done(c, st);
        sqlite3_reset(st);
    }
    sqlite3_finalize(st);
    return true;
}

/* ---- record_rev -------------------------------------------------------------------------------------------------- */

typedef struct {
    AvpMap *tree;   /* path -> blob sha (AvpVal.p: char*) */
    AvpMap *wanted; /* unit -> path */
    AvpMap *blobs;  /* sha -> AvpStr* bytes */
    AvpStr cur_blob;
} RevCtx;

static bool tree_has(void *ctx, const char *p) { return avp_map_has(((RevCtx *)ctx)->tree, avp_str(p)); }

static AvpStr rev_loader(void *ctx) {
    RevCtx *rc = ctx;
    AvpVal *v = avp_map_get(rc->blobs, rc->cur_blob);
    return v ? *(AvpStr *)v->p : AVP_S("");
}

static bool rev_resolve(ShCtx *c, void *ctx, const RUnit *u, AvpStr *path, ShIndex **ix) {
    RevCtx *rc = ctx;
    AvpVal *pv = avp_map_get(rc->wanted, u->unit);
    if (!pv) {
        *path = AVP_S("");
        *ix = NULL;
        return true;
    }
    const char *p = pv->p;
    const char *b = avp_map_get(rc->tree, avp_str(p))->p;
    rc->cur_blob = avp_str(b);
    /* blobs are decoded on load: the loader gives the decoded text */
    *ix = indexer_get(c, avp_str(avp_sprintf(c->a, "git:%s", b)), rev_loader, rc);
    *path = avp_str(p);
    return true;
}

static AvpStr git_fail_text(AvpArena *a, const char *what) {
    return avp_str(avp_sprintf(a, "git %s failed", what));
}

/* record_rev: true when the snapshot is new; *nrows the row count */
static bool record_rev(ShCtx *c, const char *rev, const AvpStr *report_bytes, const char *baseline, i64 *nrows) {
    AvpArena *a = c->a;
    *nrows = 0;
    char *sha = shsm_git_rev_parse(a, c->repo, avp_sprintf(a, "%s^{commit}", rev));
    if (!sha) avp_throw(SH_EXC_RUNTIME, "git rev-parse --verify %s^{commit} failed", rev);
    sqlite3_stmt *st = sh_prepare(c, "SELECT 1 FROM snapshots WHERE key=?");
    bind_cz(st, 1, sha);
    bool have = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    if (have) return false;
    AvpStr rb;
    if (report_bytes) {
        rb = *report_bytes;
    } else if (!shsm_git_show_blob(a, c->repo, sha, baseline, &rb)) {
        avp_throw(SH_EXC_RUNTIME, "%.9s has no %s", sha, baseline);
    }
    AvpJson *report = parse_report(a, rb);
    ShsmGitTreeVec tv = {0};
    const char *paths[] = {"src"};
    AvpStr err;
    if (!shsm_git_ls_tree(a, c->repo, sha, paths, 1, &tv, &err))
        avp_throw(SH_EXC_RUNTIME, "git ls-tree -r -z %s -- src failed: %.*s", sha, AVP_SF(err));
    RevCtx rc = {avp_map_new(), avp_map_new(), avp_map_new(), {0}};
    for (size_t i = 0; i < tv.n; i++) {
        if (!avp_str_eqz(tv.v[i].type, "blob")) continue;
        bool created;
        avp_map_put(rc.tree, tv.v[i].path, &created)->p = avp_cz(a, tv.v[i].sha);
    }
    avp_vec_free(&tv);
    RUnitVec units = source_units(a, report);
    for (size_t i = 0; i < units.n; i++) {
        const RUnit *u = &units.v[i];
        const char *p = avp_map_has(rc.tree, u->source_path) ? avp_cz(a, u->source_path) : guess_path(a, u->unit, tree_has, &rc);
        if (*p) {
            bool created;
            avp_map_put(rc.wanted, u->unit, &created)->p = (void *)p;
        }
    }
    /* the blobs that have no cached index, through one cat-file --batch */
    AvpStrVec need = {0};
    AvpSet *needset = avp_set_new();
    sqlite3_stmt *bq = sh_prepare(c, "SELECT 1 FROM blobs WHERE key=?");
    AvpMapIter it = {0};
    while (avp_map_next(rc.wanted, &it)) {
        const char *b = avp_map_get(rc.tree, avp_str(it.val->p))->p;
        bind_cz(bq, 1, avp_sprintf(a, "v" PARSER_VERSION ":git:%s", b));
        bool cached = sqlite3_step(bq) == SQLITE_ROW;
        sqlite3_reset(bq);
        if (!cached && avp_set_add(needset, avp_str(b))) avp_vec_push(&need, avp_str(b));
    }
    sqlite3_finalize(bq);
    avp_set_free(needset);
    if (need.n) {
        const char **names = avp_alloc(a, need.n * sizeof(char *));
        for (size_t i = 0; i < need.n; i++) names[i] = need.v[i].p;
        ShsmGitObjectVec objs = {0};
        if (!shsm_git_cat_batch(a, c->repo, names, need.n, &objs, &err))
            avp_throw(SH_EXC_RUNTIME, "git cat-file --batch failed: %.*s", AVP_SF(err));
        for (size_t i = 0; i < objs.n; i++) {
            if (!objs.v[i].found || !avp_str_eqz(objs.v[i].type, "blob")) continue;
            AvpStr *txt = AVP_NEW(a, AvpStr);
            *txt = sh_decode_utf8(a, objs.v[i].data);
            bool created;
            avp_map_put(rc.blobs, need.v[i], &created)->p = txt;
        }
        avp_vec_free(&objs);
    }
    avp_vec_free(&need);
    RowVec rows = {0};
    make_rows(c, &units, rev_resolve, &rc, &rows, NULL);
    i64 when;
    char *subject;
    if (!shsm_git_commit_info(a, c->repo, sha, &when, &subject))
        avp_throw(SH_EXC_RUNTIME, "%s", git_fail_text(a, "show -s").p);
    char *inc = shsm_git_object_id(a, c->repo, sha, "include");
    char *cfg = shsm_git_object_id(a, c->repo, sha, "configure.py");
    char *env = avp_sprintf(a, "inc=%s;cfg=%s", inc ? inc : "", cfg ? cfg : "");
    bool isnew = insert_snapshot(c, sha, "git", when, subject, sh_sha1(a, rb), env, &rows);
    *nrows = (i64)rows.n;
    avp_vec_free(&rows);
    avp_vec_free(&units);
    avp_map_free(rc.tree);
    avp_map_free(rc.wanted);
    avp_map_free(rc.blobs);
    return isnew;
}

/* ---- record_worktree --------------------------------------------------------------------------------------------- */

typedef struct {
    const char *repo;
    double report_mtime;
    bool include_stale;
    AvpMap *used; /* path -> ck */
    AvpStr text;
} WtCtx;

static bool wt_exists(void *ctx, const char *p) {
    WtCtx *w = ctx;
    return avp_exists(avp_path_join(avp_scratch(), w->repo, p));
}

static AvpStr wt_loader(void *ctx) { return ((WtCtx *)ctx)->text; }

static bool wt_resolve(ShCtx *c, void *ctx, const RUnit *u, AvpStr *path, ShIndex **ix) {
    WtCtx *w = ctx;
    AvpArena *a = c->a;
    const char *p;
    if (u->source_path.n && avp_exists(avp_path_join(a, w->repo, avp_cz(a, u->source_path))))
        p = avp_cz(a, u->source_path);
    else
        p = guess_path(a, u->unit, wt_exists, w);
    if (!*p) {
        *path = AVP_S("");
        *ix = NULL;
        return true;
    }
    char *full = avp_path_join(a, w->repo, p);
    double mt = 0;
    if (!avp_getmtime(full, &mt)) avp_throw(SH_EXC_OTHER, "OSError: cannot stat %s", full);
    if (!w->include_stale && mt > w->report_mtime) return false;
    AvpBytes data = {0};
    if (!avp_read_all(a, full, &data)) avp_throw(SH_EXC_OTHER, "OSError: cannot read %s", full);
    AvpStr text = sh_normalise(a, sh_decode_utf8(a, avp_strn((const char *)data.v, data.n)));
    char *ck = sh_sha1(a, text);
    bool created;
    avp_map_put(w->used, avp_str(p), &created)->p = ck;
    w->text = text;
    *ix = indexer_get(c, avp_str(avp_sprintf(a, "c:%s", ck)), wt_loader, w);
    *path = avp_str(p);
    return true;
}

typedef struct {
    AvpStr p, ck;
} UsedPair;

static int used_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const UsedPair *a = x, *b = y;
    int r = avp_str_cmp(a->p, b->p);
    return r ? r : avp_str_cmp(a->ck, b->ck);
}

static int str_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

/* env_sig_worktree */
static char *env_sig_worktree(AvpArena *a, const char *repo) {
    AvpHasher h;
    avp_hasher_init(&h, AVP_HASH_SHA1);
    char *inc = avp_path_join(a, repo, "include");
    AvpWalk *w = avp_walk_open(inc, true);
    AvpWalkItem item = {0};
    while (avp_walk_next(w, &item)) {
        avp_sort(item.dirs.v, item.dirs.n, sizeof(AvpStr), str_cmp, NULL, false);
        AvpStrVec files = {0};
        for (size_t i = 0; i < item.files.n; i++) avp_vec_push(&files, item.files.v[i]);
        avp_sort(files.v, files.n, sizeof(AvpStr), str_cmp, NULL, false);
        for (size_t i = 0; i < files.n; i++) {
            char *p = avp_path_join(a, avp_cz(a, item.dirpath), avp_cz(a, files.v[i]));
            char *rel = NULL;
            if (!avp_path_relpath(a, p, inc, &rel)) rel = p;
            AvpStr rs = avp_str_replace(a, avp_str(rel), AVP_S("\\"), AVP_S("/"), -1);
            AvpBytes data = {0};
            if (!avp_read_all(a, p, &data)) avp_throw(SH_EXC_OTHER, "OSError: cannot read %s", p);
            u8 dg[20];
            avp_sha1(data.v, data.n, dg);
            avp_hasher_update(&h, rs.p, rs.n);
            avp_hasher_update(&h, "\0", 1);
            avp_hasher_update(&h, dg, 20);
        }
        avp_vec_free(&files);
    }
    avp_walk_close(w);
    u8 dg[20];
    avp_hasher_final(&h, dg);
    char *cfgp = avp_path_join(a, repo, "configure.py");
    const char *cfg = "";
    if (avp_exists(cfgp)) {
        AvpBytes data = {0};
        if (!avp_read_all(a, cfgp, &data)) avp_throw(SH_EXC_OTHER, "OSError: cannot read %s", cfgp);
        cfg = avp_sha1_hex(a, data.v, data.n);
    }
    return avp_sprintf(a, "inc=%s;cfg=%s", avp_hex(a, dg, 20), cfg);
}

static bool record_worktree(ShCtx *c, const char *report_path, bool include_stale, i64 *nrows, AvpStrVec *skipped,
                            char **key_out) {
    AvpArena *a = c->a;
    AvpBytes data = {0};
    if (!avp_read_all(a, report_path, &data))
        avp_throw(SH_EXC_OTHER, "FileNotFoundError: [Errno 2] No such file or directory: '%s'", report_path);
    AvpStr rb = avp_strn((const char *)data.v, data.n);
    double rmt = 0;
    avp_getmtime(report_path, &rmt);
    AvpJson *report = parse_report(a, rb);
    WtCtx w = {c->repo, rmt, include_stale, avp_map_new(), {0}};
    RUnitVec units = source_units(a, report);
    RowVec rows = {0};
    make_rows(c, &units, wt_resolve, &w, &rows, skipped);
    /* key: "worktree:" + sha1(sha1(report) + json.dumps(sorted(used.items())))[:16] */
    AVP_VEC(UsedPair) used = {0};
    AvpMapIter it = {0};
    while (avp_map_next(w.used, &it)) avp_vec_push(&used, ((UsedPair){it.key, avp_str(it.val->p)}));
    avp_sort(used.v, used.n, sizeof(UsedPair), used_cmp, NULL, false);
    AvpBuf jb;
    avp_buf_init(&jb, a);
    char *rsha = sh_sha1(a, rb);
    avp_buf_putz(&jb, rsha);
    AvpJsonW jw;
    avp_jsonw_init(&jw, &jb, NULL);
    avp_jsonw_begin_array(&jw);
    for (size_t i = 0; i < used.n; i++) {
        avp_jsonw_begin_array(&jw);
        avp_jsonw_str(&jw, used.v[i].p);
        avp_jsonw_str(&jw, used.v[i].ck);
        avp_jsonw_end_array(&jw);
    }
    avp_jsonw_end_array(&jw);
    avp_jsonw_finish(&jw);
    char *kh = sh_sha1(a, avp_buf_str(&jb));
    char *key = avp_sprintf(a, "worktree:%.16s", kh);
    i64 when = (i64)floor(avp_time());
    bool isnew = insert_snapshot(c, key, "worktree", when, "working tree", rsha, env_sig_worktree(a, c->repo), &rows);
    *nrows = (i64)rows.n;
    *key_out = key;
    avp_vec_free(&used);
    avp_vec_free(&rows);
    avp_vec_free(&units);
    avp_map_free(w.used);
    return isnew;
}

/* ---- commands ---------------------------------------------------------------------------------------------------- */

int sh_cmd_record(ShCtx *c, const ShRecordOpts *o) {
    AvpArena *a = c->a;
    if (o->rev_given) {
        AvpStr rb, *rbp = NULL;
        if (o->report_given) {
            AvpBytes data = {0};
            if (!avp_read_all(a, o->report, &data))
                avp_throw(SH_EXC_OTHER, "FileNotFoundError: [Errno 2] No such file or directory: '%s'", o->report);
            rb = avp_strn((const char *)data.v, data.n);
            rbp = &rb;
        }
        i64 n;
        sh_exec(c, "BEGIN");
        bool isnew = record_rev(c, o->rev, rbp, o->baseline, &n);
        sh_exec(c, "COMMIT");
        char *sha = shsm_git_rev_parse(a, c->repo, avp_sprintf(a, "%s^{commit}", o->rev));
        if (!sha) avp_throw(SH_EXC_RUNTIME, "git rev-parse --verify %s^{commit} failed", o->rev);
        avp_buf_pyfmt(c->out, "%s %.9s: %d function rows\n", isnew ? "recorded" : "already recorded", sha, n);
        return 0;
    }
    const char *report = o->report_given ? o->report : avp_path_join(a, c->repo, WORKTREE_REPORT);
    i64 n;
    AvpStrVec skipped = {0};
    char *key;
    sh_exec(c, "BEGIN");
    bool isnew = record_worktree(c, report, o->include_stale, &n, &skipped, &key);
    sh_exec(c, "COMMIT");
    avp_buf_pyfmt(c->out, "%s %s: %d function rows\n", isnew ? "recorded" : "unchanged, timestamp refreshed:", key, n);
    if (skipped.n) {
        avp_buf_pyfmt(c->out, "skipped %d unit(s) whose source is newer than the report (use --include-stale): ", (i64)skipped.n);
        for (size_t i = 0; i < skipped.n && i < 8; i++) {
            if (i) avp_buf_putz(c->out, ", ");
            avp_buf_put(c->out, skipped.v[i]);
        }
        if (skipped.n > 8) avp_buf_putz(c->out, " ...");
        avp_buf_putc(c->out, '\n');
    }
    avp_vec_free(&skipped);
    return 0;
}

int sh_cmd_backfill(ShCtx *c, const char *baseline, bool first_parent, bool limit_given, i64 limit) {
    AvpArena *a = c->a;
    AvpStrVec revs = {0};
    AvpStr err;
    if (!shsm_git_log_path_shas(a, c->repo, baseline, first_parent, &revs, &err))
        avp_throw(SH_EXC_RUNTIME, "git log failed: %.*s", AVP_SF(err));
    size_t lo = 0, n = revs.n;
    if (limit_given && limit != 0) {
        /* revs[-limit:] */
        i64 start = -limit;
        if (start < 0) start += (i64)n;
        if (start < 0) start = 0;
        if (start > (i64)n) start = (i64)n;
        lo = (size_t)start;
    }
    i64 new_count = 0;
    for (size_t i = lo; i < n; i++) {
        const char *rev = avp_cz(a, revs.v[i]);
        AvpTry t;
        i64 rows = 0;
        bool isnew = false;
        /* one transaction per recorded revision; a revision that fails leaves its index rows pending, committed with
           the next one (the Python's implicit transaction) or dropped at close */
        if (sqlite3_get_autocommit(c->db)) sh_exec(c, "BEGIN");
        if (AVP_TRY(&t)) {
            isnew = record_rev(c, rev, NULL, baseline, &rows);
            avp_try_end(&t);
        } else {
            if (t.code != SH_EXC_RUNTIME) avp_throw(t.code, "%s", t.msg);
            avp_buf_pyfmt(c->out, "skip %.9s: %s\n", rev, t.msg);
            continue;
        }
        sh_exec(c, "COMMIT");
        new_count += isnew;
        if (isnew) avp_buf_pyfmt(c->out, "recorded %.9s: %d rows\n", rev, rows);
        else avp_buf_pyfmt(c->out, "have     %.9s: \n", rev);
    }
    sqlite3_stmt *st = sh_prepare(c, "SELECT COUNT(*) FROM snapshots");
    sqlite3_step(st);
    i64 total = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    avp_buf_pyfmt(c->out, "%d revision(s) walked, %d new snapshot(s), %d snapshot(s) in the database\n", (i64)(n - lo),
                  new_count, total);
    avp_vec_free(&revs);
    return 0;
}
