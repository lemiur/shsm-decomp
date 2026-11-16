/* The retail asm index: tools/common/asmindex.py. The SQL text of every statement is the Python's, so that SQLite
 * plans (and orders ties) the same way for both languages. */
#include "shsm_asmindex.h"

#include "avp/clock.h"
#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/pool.h"
#include "avp/sort.h"
#include "shsm_cli.h"

#include <sqlite3.h>
#include <string.h>

static const char *const SCHEMA_STATEMENTS[] = {
    "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)",
    "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL, unit TEXT NOT NULL, "
    "size INTEGER, mtime_ns INTEGER)",
    "CREATE TABLE IF NOT EXISTS symbols (name TEXT NOT NULL, address INTEGER, size INTEGER, kind TEXT NOT NULL, "
    "section TEXT, scope TEXT, hidden INTEGER, unit TEXT, file_id INTEGER NOT NULL, line_start INTEGER, "
    "line_end INTEGER, byte_start INTEGER, byte_end INTEGER, demangled TEXT, nrows INTEGER)",
    "CREATE TABLE IF NOT EXISTS refs (from_sym TEXT, from_addr INTEGER, to_sym TEXT NOT NULL, to_off INTEGER, "
    "kind TEXT NOT NULL, file_id INTEGER NOT NULL, owner_addr INTEGER, to_base INTEGER, to_addr INTEGER, "
    "section TEXT, xres INTEGER)",
    "CREATE TABLE IF NOT EXISTS strings (owner TEXT, address INTEGER, text TEXT, kind TEXT, "
    "file_id INTEGER NOT NULL, owner_addr INTEGER)",
    "CREATE TABLE IF NOT EXISTS floats (owner TEXT, address INTEGER, kind TEXT, value REAL, text TEXT, "
    "file_id INTEGER NOT NULL, owner_addr INTEGER)",
    "CREATE TABLE IF NOT EXISTS sections (name TEXT, start INTEGER, end INTEGER, file_id INTEGER NOT NULL)",
    "CREATE INDEX IF NOT EXISTS symbols_name ON symbols(name)",
    "CREATE INDEX IF NOT EXISTS symbols_addr ON symbols(address)",
    "CREATE INDEX IF NOT EXISTS symbols_file ON symbols(file_id)",
    "CREATE INDEX IF NOT EXISTS refs_to ON refs(to_sym)",
    "CREATE INDEX IF NOT EXISTS refs_from ON refs(from_sym)",
    "CREATE INDEX IF NOT EXISTS refs_file ON refs(file_id)",
    "CREATE INDEX IF NOT EXISTS refs_owner ON refs(owner_addr)",
    "CREATE INDEX IF NOT EXISTS refs_base ON refs(to_base)",
    "CREATE INDEX IF NOT EXISTS refs_addr ON refs(to_addr)",
    "CREATE INDEX IF NOT EXISTS refs_from_addr ON refs(from_addr)",
    "CREATE INDEX IF NOT EXISTS strings_owner ON strings(owner)",
    "CREATE INDEX IF NOT EXISTS strings_addr ON strings(address)",
    "CREATE INDEX IF NOT EXISTS strings_file ON strings(file_id)",
    "CREATE INDEX IF NOT EXISTS floats_owner ON floats(owner)",
    "CREATE INDEX IF NOT EXISTS floats_owner_addr ON floats(owner_addr)",
    "CREATE INDEX IF NOT EXISTS floats_file ON floats(file_id)",
    "CREATE INDEX IF NOT EXISTS sections_file ON sections(file_id)",
};
static const char *const TABLES[] = {"symbols", "refs", "strings", "floats", "sections"};
static const char *const ALL_TABLES[] = {"symbols", "refs", "strings", "floats", "sections", "files", "meta"};
#define COUNT(x) (sizeof(x) / sizeof *(x))

static const char *const RESOLVE_STATEMENTS[] = {
    "DROP TABLE IF EXISTS temp._g",
    "CREATE TEMP TABLE _g (name TEXT PRIMARY KEY, address INTEGER, kind TEXT)",
    "INSERT INTO _g (name, address) SELECT name, CASE "
    "WHEN COUNT(DISTINCT CASE WHEN scope IN ('global', 'weak') THEN address END) = 1 "
    "THEN MAX(CASE WHEN scope IN ('global', 'weak') THEN address END) "
    "WHEN COUNT(DISTINCT address) = 1 THEN MAX(address) END FROM symbols WHERE kind != 'label' GROUP BY name",
    "INSERT OR IGNORE INTO _g (name, address) SELECT name, CASE WHEN COUNT(DISTINCT address) = 1 THEN MAX(address) "
    "END FROM symbols WHERE kind = 'label' GROUP BY name",
    "UPDATE _g SET kind = (SELECT s.kind FROM symbols s WHERE s.name = _g.name AND s.address = _g.address "
    "ORDER BY s.kind = 'label' LIMIT 1) WHERE address IS NOT NULL",
    "UPDATE refs SET to_base = (SELECT address FROM _g WHERE _g.name = refs.to_sym) WHERE xres = 1",
    "UPDATE refs SET to_addr = to_base + to_off WHERE xres = 1 AND kind != 'jumptable'",
    "UPDATE refs SET kind = 'ptr', to_off = to_addr - to_base WHERE xres = 1 AND kind = 'jumptable' "
    "AND to_base IS NOT NULL AND to_addr IS NOT NULL "
    "AND (SELECT kind FROM _g WHERE _g.name = refs.to_sym) NOT IN ('function', 'label')",
    "DROP TABLE temp._g",
};

#define SYM_COLS                                                                                                     \
    "s.name, s.address, s.size, s.kind, s.section, s.scope, s.hidden, s.unit, f.path, s.line_start, s.line_end, "    \
    "s.byte_start, s.byte_end, s.demangled, s.nrows"
#define SYM_FROM "symbols s JOIN files f ON f.id = s.file_id"
#define REF_COLS "from_sym, from_addr, to_sym, to_off, kind, owner_addr, to_base, to_addr, section"

/* ---- sqlite helpers ------------------------------------------------------------------------------------------- */

AVP_NORETURN static void sql_fail(sqlite3 *db) {
    shsm_raise(SHSM_EXC_SQLITE, "%s", db ? sqlite3_errmsg(db) : "out of memory");
}

static sqlite3_stmt *prep(ShsmAsmIndex *ix, const char *sql) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(ix->db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(ix->db);
    return st;
}
static void bind_i(sqlite3_stmt *st, int i, i64 v) {
    if (v == SHSM_NONE) sqlite3_bind_null(st, i);
    else sqlite3_bind_int64(st, i, v);
}
static void bind_s(sqlite3_stmt *st, int i, AvpStr s) {
    if (!s.p) sqlite3_bind_null(st, i);
    else sqlite3_bind_text(st, i, s.n ? s.p : "", (int)s.n, SQLITE_TRANSIENT);
}
static void bind_z(sqlite3_stmt *st, int i, const char *s) { bind_s(st, i, s ? avp_str(s) : (AvpStr){NULL, 0}); }
/* true: a row; false: done. Raises on an error (and finalizes). */
static bool step(ShsmAsmIndex *ix, sqlite3_stmt *st) {
    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    char msg[512];
    snprintf(msg, sizeof msg, "%s", sqlite3_errmsg(ix->db));
    sqlite3_finalize(st);
    shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
}
static void exec(ShsmAsmIndex *ix, const char *sql) {
    sqlite3_stmt *st = prep(ix, sql);
    while (step(ix, st)) {}
    sqlite3_finalize(st);
}
static i64 col_i(sqlite3_stmt *st, int i) {
    return sqlite3_column_type(st, i) == SQLITE_NULL ? SHSM_NONE : sqlite3_column_int64(st, i);
}
static AvpStr col_s(AvpArena *a, sqlite3_stmt *st, int i) {
    if (sqlite3_column_type(st, i) == SQLITE_NULL) return (AvpStr){NULL, 0};
    const char *p = (const char *)sqlite3_column_text(st, i);
    int n = sqlite3_column_bytes(st, i);
    return avp_strn(avp_memdup(a, p ? p : "", (size_t)n + 1), (size_t)n);
}
static i64 scalar_i(ShsmAsmIndex *ix, const char *sql) {
    sqlite3_stmt *st = prep(ix, sql);
    i64 v = step(ix, st) ? col_i(st, 0) : SHSM_NONE;
    sqlite3_finalize(st);
    return v;
}

static const char *intern_kind(AvpArena *a, AvpStr k) {
    const char *list[] = {SHSM_REF_CALL, SHSM_REF_BRANCH, SHSM_REF_HA,  SHSM_REF_LO,
                          SHSM_REF_HI,   SHSM_REF_SDA21,  SHSM_REF_PTR, SHSM_REF_JUMPTABLE};
    for (size_t i = 0; i < COUNT(list); i++)
        if (k.n == strlen(list[i]) && memcmp(k.p, list[i], k.n) == 0) return list[i];
    return k.p ? avp_cz(a, k) : NULL;
}

/* ---- opening ------------------------------------------------------------------------------------------------- */

static ShsmAsmIndex *ix_new(const ShsmRepo *repo, const char *db_path, const char *asm_dir, bool readonly) {
    AvpArena *a = avp_arena_new(0);
    ShsmAsmIndex *ix = AVP_NEW(a, ShsmAsmIndex);
    ix->arena = a;
    ix->repo = repo;
    ix->asm_dir = avp_strdup(a, asm_dir ? asm_dir : repo->asm_dir);
    ix->db_path = avp_strdup(a, db_path ? db_path : avp_path_join(a, repo->index_dir, "asm.sqlite"));
    ix->readonly = readonly;
    return ix;
}

void shsm_asmindex_close(ShsmAsmIndex *ix) {
    if (!ix) return;
    if (ix->db) sqlite3_close_v2(ix->db);
    ix->db = NULL;
    avp_arena_free(ix->arena);
}

i64 shsm_asmindex_total_changes(ShsmAsmIndex *ix) { return sqlite3_total_changes64(ix->db); }

static void connect_rw(ShsmAsmIndex *ix) {
    AvpStr dir = avp_path_dirname(ix->db_path);
    if (dir.n) avp_makedirs(avp_cz(ix->arena, dir), true);
    if (sqlite3_open_v2(ix->db_path, &ix->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        char msg[512];
        snprintf(msg, sizeof msg, "%s", ix->db ? sqlite3_errmsg(ix->db) : "out of memory");
        sqlite3_close_v2(ix->db);
        ix->db = NULL;
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_busy_timeout(ix->db, 120000);
    sqlite3_stmt *st = prep(ix, "PRAGMA journal_mode");
    bool wal = step(ix, st) && _stricmp((const char *)sqlite3_column_text(st, 0), "wal") == 0;
    sqlite3_finalize(st);
    if (!wal) exec(ix, "PRAGMA journal_mode=WAL");
    exec(ix, "PRAGMA synchronous=NORMAL");
    exec(ix, "PRAGMA cache_size=-262144"); /* 256 MiB: a cold build touches every index page (no effect on the rows) */
}

static void connect_ro(ShsmAsmIndex *ix) {
    if (!avp_exists(ix->db_path)) shsm_raise(SHSM_EXC_SQLITE, "no index at %s", ix->db_path);
    const char *abs = avp_path_abspath(ix->arena, ix->db_path);
    if (sqlite3_open_v2(abs, &ix->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        char msg[512];
        snprintf(msg, sizeof msg, "%s", ix->db ? sqlite3_errmsg(ix->db) : "out of memory");
        sqlite3_close_v2(ix->db);
        ix->db = NULL;
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_busy_timeout(ix->db, 120000);
}

static i64 schema_version(ShsmAsmIndex *ix) {
    sqlite3_stmt *st = prep(ix, "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'meta'");
    bool has = step(ix, st);
    sqlite3_finalize(st);
    if (!has) return SHSM_NONE;
    st = prep(ix, "SELECT value FROM meta WHERE key = 'schema'");
    i64 v = SHSM_NONE;
    if (step(ix, st)) {
        AvpStr s = col_s(avp_scratch(), st, 0);
        i64 x;
        if (s.p && avp_parse_int(s, 10, &x, NULL)) v = x;
    }
    sqlite3_finalize(st);
    return v;
}

static void begin_immediate(ShsmAsmIndex *ix) {
    if (!sqlite3_get_autocommit(ix->db)) exec(ix, "COMMIT");
    exec(ix, "BEGIN IMMEDIATE");
}
static void rollback(ShsmAsmIndex *ix) {
    if (!sqlite3_get_autocommit(ix->db)) sqlite3_exec(ix->db, "ROLLBACK", NULL, NULL, NULL);
}

/* ---- scanning and parsing -------------------------------------------------------------------------------------- */

typedef struct {
    const char *path, *rel, *unit;
    i64 size, mtime_ns;
} Work;
typedef AVP_VEC(Work) WorkVec;

typedef struct {
    const char *rel, *unit;
    ShsmAsmFile *af;
    i64 *str_owner, *flt_owner; /* owner_addr of each string / float */
    int err;
    char msg[512];
} ParseRes;

static WorkVec scan(ShsmAsmIndex *ix) {
    ix->listings = shsm_asm_listings(ix->arena, ix->asm_dir);
    WorkVec out = {0};
    for (size_t i = 0; i < ix->listings->rels.n; i++) {
        AvpStat st;
        const char *p = avp_cz(ix->arena, ix->listings->paths.v[i]);
        if (!avp_stat(p, &st)) continue; /* a split rewriting the directory right now */
        const char *rel = avp_cz(ix->arena, ix->listings->rels.v[i]);
        size_t rn = strlen(rel);
        Work w = {p, rel, avp_cz(ix->arena, avp_strn(rel, rn >= 2 ? rn - 2 : 0)), st.size, st.mtime_ns};
        avp_vec_push(&out, w);
    }
    return out;
}

typedef struct {
    AvpStrVec stale; /* rel paths, disk order */
    AvpStrVec gone;  /* rel paths, files-table order */
} Diff;

static Diff diff(ShsmAsmIndex *ix, const WorkVec *disk) {
    AvpArena *a = ix->arena;
    AvpMap *known = avp_map_new();
    typedef struct {
        i64 size, mtime;
        bool size_null, mtime_null;
    } Known;
    AVP_VEC(Known) kv = {0};
    AvpStrVec order = {0};
    sqlite3_stmt *st = prep(ix, "SELECT path, id, size, mtime_ns FROM files");
    while (step(ix, st)) {
        AvpStr path = col_s(a, st, 0);
        Known k = {sqlite3_column_int64(st, 2), sqlite3_column_int64(st, 3),
                   sqlite3_column_type(st, 2) == SQLITE_NULL, sqlite3_column_type(st, 3) == SQLITE_NULL};
        bool created;
        AvpVal *v = avp_map_put(known, path, &created);
        if (created) avp_vec_push(&order, path);
        v->i = (i64)kv.n;
        avp_vec_push(&kv, k);
    }
    sqlite3_finalize(st);
    Diff d = {0};
    AvpSet *on_disk = avp_set_new();
    for (size_t i = 0; i < disk->n; i++) {
        AvpStr rel = avp_str(disk->v[i].rel);
        avp_set_add(on_disk, rel);
        AvpVal *v = avp_map_get(known, rel);
        if (!v) {
            avp_vec_push(&d.stale, rel);
            continue;
        }
        Known *k = &kv.v[v->i];
        if (k->size_null || k->mtime_null || k->size != disk->v[i].size || k->mtime != disk->v[i].mtime_ns)
            avp_vec_push(&d.stale, rel);
    }
    for (size_t i = 0; i < order.n; i++)
        if (!avp_set_has(on_disk, order.v[i])) avp_vec_push(&d.gone, order.v[i]);
    avp_set_free(on_disk);
    avp_map_free(known);
    avp_vec_free(&kv);
    avp_vec_free(&order);
    return d;
}

/* asmindex._owner_finder over one listing's blocks */
typedef struct {
    AvpMap *by_name; /* name -> index into lists */
    AVP_VEC(ShsmAsmBlock *) * lists;
    size_t nlists;
} Owners;

static i64 owner_find(const Owners *o, AvpStr name, i64 address) {
    if (!name.p) return SHSM_NONE;
    AvpVal *v = avp_map_get(o->by_name, name);
    if (!v) return SHSM_NONE;
    ShsmAsmBlock **lst = o->lists[v->i].v;
    size_t n = o->lists[v->i].n;
    if (n == 1) return lst[0]->address;
    for (size_t i = 0; i < n; i++) {
        i64 sz = lst[i]->size == SHSM_NONE ? 0 : lst[i]->size;
        if (lst[i]->address <= address && address < lst[i]->address + sz) return lst[i]->address;
    }
    return lst[0]->address;
}

static int parse_one(void *ctx, size_t index, const void *item, void *result) {
    (void)ctx;
    (void)index;
    const Work *w = item;
    ParseRes *r = result;
    r->rel = w->rel;
    r->unit = w->unit;
    AvpTry t;
    if (AVP_TRY(&t)) {
        ShsmAsmFile *af = shsm_asm_parse_file(w->path, w->unit, SHSM_ASM_NO_ROWS);
        avp_try_end(&t);
        r->af = af;
    } else {
        r->err = t.code ? t.code : SHSM_EXC_OS;
        snprintf(r->msg, sizeof r->msg, "%s", t.msg);
        return 1;
    }
    ShsmAsmFile *af = r->af;
    Owners o = {avp_map_new(), NULL, 0};
    o.lists = avp_alloc(af->arena, sizeof *o.lists * (af->blocks.n + 1));
    for (size_t i = 0; i < af->blocks.n; i++) {
        ShsmAsmBlock *b = af->blocks.v[i];
        if (strcmp(b->kind, "label") == 0 || b->address == SHSM_NONE) continue;
        bool created;
        AvpVal *v = avp_map_put(o.by_name, b->name, &created);
        if (created) v->i = (i64)o.nlists++;
        avp_vec_push(&o.lists[v->i], b);
    }
    r->str_owner = avp_alloc(af->arena, sizeof(i64) * (af->strings.n + 1));
    r->flt_owner = avp_alloc(af->arena, sizeof(i64) * (af->floats.n + 1));
    for (size_t i = 0; i < af->strings.n; i++)
        r->str_owner[i] = owner_find(&o, af->strings.v[i].owner, af->strings.v[i].address);
    for (size_t i = 0; i < af->floats.n; i++)
        r->flt_owner[i] = owner_find(&o, af->floats.v[i].owner, af->floats.v[i].address);
    for (size_t i = 0; i < o.nlists; i++) avp_vec_free(&o.lists[i]);
    avp_map_free(o.by_name);
    return 0;
}

static int cmp_size_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = ((const Work *)x)->size, b = ((const Work *)y)->size;
    return a > b ? -1 : a < b ? 1 : 0;
}

static ParseRes *parse_all(WorkVec *work, int jobs) {
    if (!work->n) return NULL;
    if (jobs <= 0) {
        jobs = avp_cpu_count();
        if (jobs < 1) jobs = 1;
        if (jobs > 32) jobs = 32;
    }
    ParseRes *res = avp_xcalloc(work->n, sizeof *res);
    int threads = 1;
    if (jobs > 1 && work->n > 4) {
        avp_sort(work->v, work->n, sizeof *work->v, cmp_size_desc, NULL, false); /* largest first (stable) */
        threads = jobs < (int)work->n ? jobs : (int)work->n;
    }
    i64 bad = avp_pool_map(NULL, threads, parse_one, NULL, work->v, work->n, sizeof *work->v, res, sizeof *res);
    if (bad >= 0) {
        int code = res[bad].err;
        char msg[512];
        snprintf(msg, sizeof msg, "%s", res[bad].msg);
        for (size_t i = 0; i < work->n; i++) shsm_asm_free(res[i].af);
        avp_xfree(res);
        avp_throw(code, "%s", msg);
    }
    return res;
}

static void free_results(ParseRes *res, size_t n) {
    for (size_t i = 0; i < n; i++) shsm_asm_free(res[i].af);
    avp_xfree(res);
}

/* asmindex._write: replace the rows of the given listings, re-resolve, stamp 'built' (inside a transaction) */
static void write_results(ShsmAsmIndex *ix, ParseRes *res, size_t nres, const AvpStrVec *gone, const WorkVec *disk) {
    sqlite3_stmt *sel = prep(ix, "SELECT id FROM files WHERE path = ?");
    sqlite3_stmt *del[COUNT(TABLES)];
    for (size_t t = 0; t < COUNT(TABLES); t++)
        del[t] = prep(ix, avp_sprintf(avp_scratch(), "DELETE FROM %s WHERE file_id = ?", TABLES[t]));
    sqlite3_stmt *delf = prep(ix, "DELETE FROM files WHERE id = ?");
    size_t ndel = gone->n + nres;
    for (size_t i = 0; i < ndel; i++) {
        AvpStr rel = i < gone->n ? gone->v[i] : avp_str(res[i - gone->n].rel);
        sqlite3_reset(sel);
        bind_s(sel, 1, rel);
        if (!step(ix, sel)) continue;
        i64 id = sqlite3_column_int64(sel, 0);
        sqlite3_reset(sel);
        for (size_t t = 0; t < COUNT(TABLES); t++) {
            sqlite3_reset(del[t]);
            sqlite3_bind_int64(del[t], 1, id);
            step(ix, del[t]);
        }
        sqlite3_reset(delf);
        sqlite3_bind_int64(delf, 1, id);
        step(ix, delf);
    }
    sqlite3_finalize(sel);
    for (size_t t = 0; t < COUNT(TABLES); t++) sqlite3_finalize(del[t]);
    sqlite3_finalize(delf);

    AvpMap *stamps = avp_map_new();
    for (size_t i = 0; i < disk->n; i++) avp_map_put(stamps, avp_str(disk->v[i].rel), NULL)->i = (i64)i;
    sqlite3_stmt *insf = prep(ix, "INSERT INTO files (path, unit, size, mtime_ns) VALUES (?, ?, ?, ?)");
    sqlite3_stmt *inss = prep(ix, "INSERT INTO symbols VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    sqlite3_stmt *insr = prep(ix, "INSERT INTO refs (from_sym, from_addr, to_sym, to_off, kind, file_id, owner_addr, "
                                  "to_base, to_addr, section, xres) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    sqlite3_stmt *insstr = prep(ix, "INSERT INTO strings (owner, address, text, kind, file_id, owner_addr) "
                                    "VALUES (?, ?, ?, ?, ?, ?)");
    sqlite3_stmt *insflt = prep(ix, "INSERT INTO floats (owner, address, kind, value, text, file_id, owner_addr) "
                                    "VALUES (?, ?, ?, ?, ?, ?, ?)");
    sqlite3_stmt *inssec = prep(ix, "INSERT INTO sections (name, start, end, file_id) VALUES (?, ?, ?, ?)");
    for (size_t i = 0; i < nres; i++) {
        ParseRes *r = &res[i];
        const Work *w = &disk->v[avp_map_get(stamps, avp_str(r->rel))->i];
        sqlite3_reset(insf);
        bind_z(insf, 1, r->rel);
        bind_z(insf, 2, r->unit);
        sqlite3_bind_int64(insf, 3, w->size);
        sqlite3_bind_int64(insf, 4, w->mtime_ns);
        step(ix, insf);
        i64 fid = sqlite3_last_insert_rowid(ix->db);
        ShsmAsmFile *af = r->af;
        for (size_t k = 0; k < af->blocks.n; k++) {
            ShsmAsmBlock *b = af->blocks.v[k];
            sqlite3_stmt *s = inss;
            sqlite3_reset(s);
            bind_s(s, 1, b->name);
            bind_i(s, 2, b->address);
            bind_i(s, 3, b->size);
            bind_z(s, 4, b->kind);
            bind_s(s, 5, b->section);
            bind_s(s, 6, b->scope);
            sqlite3_bind_int64(s, 7, b->hidden ? 1 : 0);
            bind_z(s, 8, r->unit);
            sqlite3_bind_int64(s, 9, fid);
            sqlite3_bind_int64(s, 10, b->line_start);
            sqlite3_bind_int64(s, 11, b->line_end);
            sqlite3_bind_int64(s, 12, b->byte_start);
            sqlite3_bind_int64(s, 13, b->byte_end);
            bind_s(s, 14, b->demangled);
            bind_i(s, 15, strcmp(b->kind, "function") == 0 ? (i64)b->nrows : SHSM_NONE);
            step(ix, s);
        }
        for (size_t k = 0; k < af->refs.n; k++) {
            ShsmAsmRef *f = &af->refs.v[k];
            sqlite3_stmt *s = insr;
            sqlite3_reset(s);
            bind_s(s, 1, f->from_sym);
            sqlite3_bind_int64(s, 2, f->from_addr);
            bind_s(s, 3, f->to_sym);
            sqlite3_bind_int64(s, 4, f->to_off);
            bind_z(s, 5, f->kind);
            sqlite3_bind_int64(s, 6, fid);
            bind_i(s, 7, f->owner_addr);
            bind_i(s, 8, f->to_base);
            bind_i(s, 9, f->to_addr);
            bind_s(s, 10, f->section);
            sqlite3_bind_int64(s, 11, f->to_base != SHSM_NONE ? 0 : 1);
            step(ix, s);
        }
        for (size_t k = 0; k < af->strings.n; k++) {
            ShsmAsmString *x = &af->strings.v[k];
            sqlite3_stmt *s = insstr;
            sqlite3_reset(s);
            bind_s(s, 1, x->owner);
            sqlite3_bind_int64(s, 2, x->address);
            bind_s(s, 3, x->text);
            bind_z(s, 4, x->kind);
            sqlite3_bind_int64(s, 5, fid);
            bind_i(s, 6, r->str_owner[k]);
            step(ix, s);
        }
        for (size_t k = 0; k < af->floats.n; k++) {
            ShsmAsmFloat *x = &af->floats.v[k];
            sqlite3_stmt *s = insflt;
            sqlite3_reset(s);
            bind_s(s, 1, x->owner);
            sqlite3_bind_int64(s, 2, x->address);
            bind_z(s, 3, x->kind);
            if (x->has_value) sqlite3_bind_double(s, 4, x->value);
            else sqlite3_bind_null(s, 4);
            bind_s(s, 5, x->text);
            sqlite3_bind_int64(s, 6, fid);
            bind_i(s, 7, r->flt_owner[k]);
            step(ix, s);
        }
        for (size_t k = 0; k < af->sections.n; k++) {
            ShsmAsmSection *x = &af->sections.v[k];
            sqlite3_stmt *s = inssec;
            sqlite3_reset(s);
            bind_s(s, 1, x->name);
            sqlite3_bind_int64(s, 2, x->start);
            sqlite3_bind_int64(s, 3, x->end);
            sqlite3_bind_int64(s, 4, fid);
            step(ix, s);
        }
    }
    sqlite3_finalize(insf);
    sqlite3_finalize(inss);
    sqlite3_finalize(insr);
    sqlite3_finalize(insstr);
    sqlite3_finalize(insflt);
    sqlite3_finalize(inssec);
    avp_map_free(stamps);
    for (size_t i = 0; i < COUNT(RESOLVE_STATEMENTS); i++) exec(ix, RESOLVE_STATEMENTS[i]);
    char t[32];
    avp_float_repr(avp_time(), t);
    sqlite3_stmt *m = prep(ix, "INSERT OR REPLACE INTO meta VALUES ('built', ?)");
    bind_z(m, 1, t);
    step(ix, m);
    sqlite3_finalize(m);
}

static WorkVec work_of(const WorkVec *disk, const AvpStrVec *rels) {
    AvpMap *at = avp_map_new();
    for (size_t i = 0; i < disk->n; i++) avp_map_put(at, avp_str(disk->v[i].rel), NULL)->i = (i64)i;
    WorkVec w = {0};
    for (size_t i = 0; i < rels->n; i++) avp_vec_push(&w, disk->v[avp_map_get(at, rels->v[i])->i]);
    avp_map_free(at);
    return w;
}

/* A refresh that parses many listings (the first open after a re-split or a regenerated asm tree re-reads most of the
 * 1,300 files: minutes) says so before it starts; a caller that waited on a silent index open looked hung. */
static void notice_stderr(const char *line) {
    fprintf(stderr, "%s\n", line);
    fflush(stderr);
}
void (*shsm_asmindex_notice)(const char *line) = notice_stderr;
size_t shsm_asmindex_notice_min = 25;

static void announce_parse(ShsmAsmIndex *ix, size_t parsing, size_t total, size_t gone) {
    if (!shsm_asmindex_notice || parsing < shsm_asmindex_notice_min) return;
    shsm_asmindex_notice(avp_sprintf(avp_scratch(),
                                     "asmindex: refreshing %s: %zu of %zu listing(s) changed (%zu gone); parsing them "
                                     "can take minutes",
                                     ix->db_path, parsing, total, gone));
}

/* _ensure_schema: nothing when current; else rebuild the whole index in one transaction */
static bool ensure_schema(ShsmAsmIndex *ix) {
    if (schema_version(ix) == SHSM_ASMINDEX_SCHEMA_VERSION) return false;
    begin_immediate(ix);
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (schema_version(ix) == SHSM_ASMINDEX_SCHEMA_VERSION) {
            avp_try_end(&t);
            rollback(ix);
            return false;
        }
        sqlite3_stmt *st = prep(ix, "SELECT name FROM sqlite_master WHERE type = 'table' AND name IN "
                                    "('files', 'symbols', 'refs')");
        bool fresh = !step(ix, st);
        sqlite3_finalize(st);
        for (size_t i = 0; i < COUNT(ALL_TABLES); i++)
            exec(ix, avp_sprintf(avp_scratch(), "DROP TABLE IF EXISTS %s", ALL_TABLES[i]));
        for (size_t i = 0; i < COUNT(SCHEMA_STATEMENTS); i++) exec(ix, SCHEMA_STATEMENTS[i]);
        if (!fresh && avp_isdir(ix->asm_dir)) {
            double t0 = avp_perf_counter();
            WorkVec disk = scan(ix);
            WorkVec work = disk;
            work.v = avp_memdup(ix->arena, disk.v, sizeof *disk.v * (disk.n + 1));
            announce_parse(ix, work.n, disk.n, 0);
            ParseRes *res = parse_all(&work, 0);
            AvpStrVec none = {0};
            write_results(ix, res, work.n, &none, &disk);
            free_results(res, work.n);
            ix->rebuilt = true;
            ix->rebuilt_files = (i64)work.n;
            ix->rebuilt_seconds = avp_py_round(avp_perf_counter() - t0, 3);
            avp_vec_free(&disk);
        }
        exec(ix, "INSERT OR REPLACE INTO meta VALUES ('schema', '2')");
        exec(ix, "COMMIT");
        avp_try_end(&t);
    } else {
        rollback(ix);
        avp_throw(t.code, "%s", t.msg);
    }
    return true;
}

void shsm_asmindex_clear(ShsmAsmIndex *ix) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        exec(ix, "BEGIN");
        for (size_t i = 0; i < COUNT(TABLES); i++) exec(ix, avp_sprintf(avp_scratch(), "DELETE FROM %s", TABLES[i]));
        exec(ix, "DELETE FROM files");
        exec(ix, "COMMIT");
        avp_try_end(&t);
    } else {
        rollback(ix);
        avp_throw(t.code, "%s", t.msg);
    }
}

static void listing_info(ShsmAsmIndex *ix, ShsmRefresh *r) {
    r->ignored = (AvpStrVec){0};
    r->note = NULL;
    if (ix->listings) {
        for (size_t i = 0; i < ix->listings->stale.n; i++) avp_vec_push(&r->ignored, ix->listings->stale.v[i]);
        r->note = ix->listings->note;
    }
}

const ShsmRefresh *shsm_asmindex_refresh(ShsmAsmIndex *ix, int jobs) {
    double t0 = avp_perf_counter();
    if (!avp_isdir(ix->asm_dir))
        shsm_raise(SHSM_EXC_FILE_NOT_FOUND, "no retail asm at %s (run configure.py and ninja first)", ix->asm_dir);
    WorkVec disk = scan(ix);
    Diff d = diff(ix, &disk);
    if ((d.stale.n || d.gone.n) && ix->readonly)
        shsm_raise(SHSM_EXC_SQLITE, "read-only index is out of date; open it with refresh=True");
    WorkVec work = work_of(&disk, &d.stale);
    announce_parse(ix, work.n, disk.n, d.gone.n);
    ParseRes *res = parse_all(&work, jobs);
    double t_parse = avp_perf_counter();
    ShsmRefresh *r = &ix->last_refresh;
    if (ix->has_last_refresh) avp_vec_free(&r->ignored);
    ix->has_last_refresh = true;
    r->scanned = (i64)disk.n;
    r->parse_seconds = avp_py_round(t_parse - t0, 3);
    if (!work.n && !d.gone.n) {
        r->parsed = r->removed = 0;
        r->seconds = avp_py_round(avp_perf_counter() - t0, 3);
        listing_info(ix, r);
    } else {
        begin_immediate(ix);
        AvpTry t;
        if (AVP_TRY(&t)) {
            write_results(ix, res, work.n, &d.gone, &disk);
            exec(ix, "COMMIT");
            avp_try_end(&t);
        } else {
            rollback(ix);
            free_results(res, work.n);
            avp_throw(t.code, "%s", t.msg);
        }
        r->parsed = (i64)work.n;
        r->removed = (i64)d.gone.n;
        r->seconds = avp_py_round(avp_perf_counter() - t0, 3);
        listing_info(ix, r);
    }
    if (res) free_results(res, work.n);
    avp_vec_free(&work);
    avp_vec_free(&disk);
    avp_vec_free(&d.stale);
    avp_vec_free(&d.gone);
    return r;
}

static bool stale_files(ShsmAsmIndex *ix) {
    if (!avp_isdir(ix->asm_dir)) return false;
    WorkVec disk = scan(ix);
    Diff d = diff(ix, &disk);
    bool any = d.stale.n || d.gone.n;
    avp_vec_free(&disk);
    avp_vec_free(&d.stale);
    avp_vec_free(&d.gone);
    return any;
}

bool shsm_asmindex_is_current(ShsmAsmIndex *ix) { return !stale_files(ix); }

static void noop_refresh(ShsmAsmIndex *ix) {
    ShsmRefresh *r = &ix->last_refresh;
    r->scanned = scalar_i(ix, "SELECT COUNT(*) FROM files");
    r->parsed = r->removed = 0;
    r->parse_seconds = r->seconds = 0.0;
    listing_info(ix, r);
    ix->has_last_refresh = true;
}

static ShsmAsmIndex *open_writer(const ShsmRepo *repo, const char *db_path, const char *asm_dir) {
    ShsmAsmIndex *ix = ix_new(repo, db_path, asm_dir, false);
    AvpTry t;
    if (AVP_TRY(&t)) {
        connect_rw(ix);
        ensure_schema(ix);
        avp_try_end(&t);
    } else {
        shsm_asmindex_close(ix);
        avp_throw(t.code, "%s", t.msg);
    }
    return ix;
}

ShsmAsmIndex *shsm_asmindex_open(const ShsmAsmIndexOpts *o) {
    const ShsmRepo *repo = o->repo ? o->repo : shsm_repo_find(o->root);
    bool refresh = !o->no_refresh;
    if (o->readonly && !o->rebuild) {
        ShsmAsmIndex *ix = ix_new(repo, o->db_path, o->asm_dir, true);
        AvpTry t;
        bool ok = false;
        if (AVP_TRY(&t)) {
            connect_ro(ix);
            if (schema_version(ix) == SHSM_ASMINDEX_SCHEMA_VERSION && !(refresh && stale_files(ix))) {
                if (refresh) noop_refresh(ix);
                ok = true;
            }
            avp_try_end(&t);
        } else if (t.code != SHSM_EXC_SQLITE) {
            shsm_asmindex_close(ix);
            avp_throw(t.code, "%s", t.msg);
        }
        if (ok) return ix;
        shsm_asmindex_close(ix);
        ShsmAsmIndex *w = open_writer(repo, o->db_path, o->asm_dir);
        bool had = false;
        ShsmRefresh last = {0};
        ShsmListings *lst = NULL;
        if (AVP_TRY(&t)) {
            if (refresh) {
                last = *shsm_asmindex_refresh(w, o->jobs);
                had = true;
                lst = w->listings;
            }
            avp_try_end(&t);
        } else {
            shsm_asmindex_close(w);
            avp_throw(t.code, "%s", t.msg);
        }
        ix = ix_new(repo, o->db_path, o->asm_dir, true);
        if (had) { /* copy what the writer's arena holds before closing it */
            ix->last_refresh = last;
            ix->last_refresh.ignored = (AvpStrVec){0};
            for (size_t i = 0; i < last.ignored.n; i++)
                avp_vec_push(&ix->last_refresh.ignored, avp_str_dup(ix->arena, last.ignored.v[i]));
            ix->last_refresh.note = last.note ? avp_strdup(ix->arena, last.note) : NULL;
            ix->has_last_refresh = true;
            avp_vec_free(&last.ignored);
            w->has_last_refresh = false;
            (void)lst;
        }
        shsm_asmindex_close(w);
        if (AVP_TRY(&t)) {
            connect_ro(ix);
            avp_try_end(&t);
        } else {
            shsm_asmindex_close(ix);
            avp_throw(t.code, "%s", t.msg);
        }
        return ix;
    }
    ShsmAsmIndex *ix = open_writer(repo, o->db_path, o->asm_dir);
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (o->rebuild) shsm_asmindex_clear(ix);
        if (refresh || o->rebuild) shsm_asmindex_refresh(ix, o->jobs);
        avp_try_end(&t);
    } else {
        shsm_asmindex_close(ix);
        avp_throw(t.code, "%s", t.msg);
    }
    return ix;
}

AvpJson *shsm_refresh_json(AvpArena *a, const ShsmRefresh *r) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("scanned"), avp_json_int(a, r->scanned));
    avp_json_set(a, o, AVP_S("parsed"), avp_json_int(a, r->parsed));
    avp_json_set(a, o, AVP_S("removed"), avp_json_int(a, r->removed));
    avp_json_set(a, o, AVP_S("parse_seconds"), avp_json_float(a, r->parse_seconds));
    avp_json_set(a, o, AVP_S("seconds"), avp_json_float(a, r->seconds));
    AvpJson *ign = avp_json_arr(a);
    for (size_t i = 0; i < r->ignored.n; i++) avp_json_arr_push(a, ign, avp_json_str(a, r->ignored.v[i]));
    avp_json_set(a, o, AVP_S("ignored"), ign);
    avp_json_set(a, o, AVP_S("note"), r->note ? avp_json_str(a, avp_str(r->note)) : avp_json_null());
    return o;
}

/* ---- stats ----------------------------------------------------------------------------------------------------- */

static void kind_counts(AvpArena *a, ShsmAsmIndex *ix, const char *sql, void *vec) {
    AVP_VEC(ShsmKindCount) *v = vec;
    sqlite3_stmt *st = prep(ix, sql);
    while (step(ix, st)) {
        ShsmKindCount k = {col_s(a, st, 0), sqlite3_column_int64(st, 1)};
        avp_vec_push(v, k);
    }
    sqlite3_finalize(st);
}

ShsmIndexStats shsm_asmindex_stats(AvpArena *a, ShsmAsmIndex *ix) {
    ShsmIndexStats s = {0};
    s.files = scalar_i(ix, "SELECT COUNT(*) FROM files");
    s.symbols = scalar_i(ix, "SELECT COUNT(*) FROM symbols");
    s.refs = scalar_i(ix, "SELECT COUNT(*) FROM refs");
    s.strings = scalar_i(ix, "SELECT COUNT(*) FROM strings");
    s.floats = scalar_i(ix, "SELECT COUNT(*) FROM floats");
    s.sections = scalar_i(ix, "SELECT COUNT(*) FROM sections");
    s.refs_resolved = scalar_i(ix, "SELECT COUNT(*) FROM refs WHERE to_base IS NOT NULL");
    kind_counts(a, ix, "SELECT kind, COUNT(*) FROM symbols GROUP BY kind", &s.symbols_by_kind);
    kind_counts(a, ix, "SELECT kind, COUNT(*) FROM refs GROUP BY kind", &s.refs_by_kind);
    return s;
}

AvpJson *shsm_stats_json(AvpArena *a, const ShsmIndexStats *s) {
    AvpJson *o = avp_json_obj(a);
    const char *names[] = {"files", "symbols", "refs", "strings", "floats", "sections", "refs_resolved"};
    i64 vals[] = {s->files, s->symbols, s->refs, s->strings, s->floats, s->sections, s->refs_resolved};
    for (size_t i = 0; i < COUNT(names); i++) avp_json_set(a, o, avp_str(names[i]), avp_json_int(a, vals[i]));
    for (int k = 0; k < 2; k++) {
        AvpJson *d = avp_json_obj(a);
        const ShsmKindCount *v = k ? s->refs_by_kind.v : s->symbols_by_kind.v;
        size_t n = k ? s->refs_by_kind.n : s->symbols_by_kind.n;
        for (size_t i = 0; i < n; i++) avp_json_set(a, d, v[i].kind.p ? v[i].kind : AVP_S("None"), avp_json_int(a, v[i].count));
        avp_json_set(a, o, k ? AVP_S("refs_by_kind") : AVP_S("symbols_by_kind"), d);
    }
    return o;
}

/* ---- symbols ---------------------------------------------------------------------------------------------------- */

static ShsmSymRow sym_row(AvpArena *a, sqlite3_stmt *st) {
    ShsmSymRow s;
    s.name = col_s(a, st, 0);
    s.address = col_i(st, 1);
    s.size = col_i(st, 2);
    s.kind = col_s(a, st, 3);
    s.section = col_s(a, st, 4);
    s.scope = col_s(a, st, 5);
    s.hidden = sqlite3_column_int64(st, 6) != 0;
    s.unit = col_s(a, st, 7);
    s.file = col_s(a, st, 8);
    s.line_start = col_i(st, 9);
    s.line_end = col_i(st, 10);
    s.byte_start = col_i(st, 11);
    s.byte_end = col_i(st, 12);
    s.demangled = col_s(a, st, 13);
    s.nrows = col_i(st, 14);
    return s;
}

AvpJson *shsm_symrow_json(AvpArena *a, const ShsmSymRow *s) {
    AvpJson *o = avp_json_obj(a);
#define S_(k, v) avp_json_set(a, o, AVP_S(k), (v))
    S_("name", shsm_json_opt_str(a, s->name));
    S_("address", shsm_json_opt_int(a, s->address));
    S_("size", shsm_json_opt_int(a, s->size));
    S_("kind", shsm_json_opt_str(a, s->kind));
    S_("section", shsm_json_opt_str(a, s->section));
    S_("scope", shsm_json_opt_str(a, s->scope));
    S_("hidden", avp_json_bool(s->hidden));
    S_("unit", shsm_json_opt_str(a, s->unit));
    S_("file", shsm_json_opt_str(a, s->file));
    S_("line_start", shsm_json_opt_int(a, s->line_start));
    S_("line_end", shsm_json_opt_int(a, s->line_end));
    S_("byte_start", shsm_json_opt_int(a, s->byte_start));
    S_("byte_end", shsm_json_opt_int(a, s->byte_end));
    S_("demangled", shsm_json_opt_str(a, s->demangled));
    S_("nrows", shsm_json_opt_int(a, s->nrows));
#undef S_
    return o;
}

const char *shsm_symrow_unit_name(AvpArena *a, const ShsmSymRow *s) {
    return avp_sprintf(a, "main/%.*s", AVP_SF(s->unit));
}

/* _syms(where, params, order, limit); params: 'i' int, 's' string */
typedef struct {
    char t;
    i64 i;
    AvpStr s;
} Param;
static void bind_params(sqlite3_stmt *st, const Param *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (p[i].t == 'i') bind_i(st, (int)i + 1, p[i].i);
        else bind_s(st, (int)i + 1, p[i].s);
    }
}
static ShsmSymRows syms(AvpArena *a, ShsmAsmIndex *ix, const char *where, const Param *p, size_t np,
                        const char *order, i64 limit) {
    if (!order) order = "s.address, s.kind = 'label'";
    const char *q = avp_sprintf(avp_scratch(), "SELECT %s FROM %s WHERE %s ORDER BY %s", SYM_COLS, SYM_FROM, where, order);
    if (limit != SHSM_NONE) q = avp_sprintf(avp_scratch(), "%s LIMIT %lld", q, (long long)limit);
    sqlite3_stmt *st = prep(ix, q);
    bind_params(st, p, np);
    ShsmSymRows out = {0};
    while (step(ix, st)) avp_vec_push(&out, sym_row(a, st));
    sqlite3_finalize(st);
    return out;
}

ShsmSymRows shsm_asmindex_symbols_named(AvpArena *a, ShsmAsmIndex *ix, AvpStr name) {
    Param p = {'s', 0, name};
    ShsmSymRows rows = syms(a, ix, "s.name = ?", &p, 1, NULL, SHSM_NONE);
    if (rows.n) return rows;
    AvpBuf b = {0};
    for (size_t i = 0; i < name.n; i++) {
        char c = name.p[i];
        if (c == '[') avp_buf_putz(&b, "[[]");
        else if (c == '*') avp_buf_putz(&b, "[*]");
        else if (c == '?') avp_buf_putz(&b, "[?]");
        else avp_buf_putc(&b, c);
    }
    avp_buf_putz(&b, "_[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]");
    Param g = {'s', 0, avp_str_dup(a, avp_buf_str(&b))};
    avp_buf_free(&b);
    ShsmSymRows all = syms(a, ix, "s.name GLOB ?", &g, 1, NULL, SHSM_NONE);
    ShsmSymRows out = {0};
    for (size_t i = 0; i < all.n; i++) {
        ShsmSymRow *s = &all.v[i];
        if (s->address == SHSM_NONE) continue;
        char suf[24];
        int n = snprintf(suf, sizeof suf, "_%08llX", (unsigned long long)s->address);
        if (s->name.n >= (size_t)n && memcmp(s->name.p + s->name.n - n, suf, (size_t)n) == 0) avp_vec_push(&out, *s);
    }
    avp_vec_free(&all);
    return out;
}

static bool is_label(const ShsmSymRow *s) { return s->kind.p && s->kind.n == 5 && memcmp(s->kind.p, "label", 5) == 0; }
static bool kind_is(const ShsmSymRow *s, const char *k) {
    return s->kind.p && s->kind.n == strlen(k) && memcmp(s->kind.p, k, s->kind.n) == 0;
}

static int cmp_symbol_key(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmSymRow *a = x, *b = y;
    int la = is_label(a), lb = is_label(b);
    if (la != lb) return la - lb;
    i64 aa = a->address == SHSM_NONE ? 0 : a->address, ab = b->address == SHSM_NONE ? 0 : b->address;
    return aa < ab ? -1 : aa > ab ? 1 : 0;
}

bool shsm_asmindex_symbol(AvpArena *a, ShsmAsmIndex *ix, AvpStr name, ShsmSymRow *out) {
    ShsmSymRows l = shsm_asmindex_symbols_named(a, ix, name);
    avp_sort(l.v, l.n, sizeof *l.v, cmp_symbol_key, NULL, false);
    bool ok = l.n > 0;
    if (ok) *out = l.v[0];
    avp_vec_free(&l);
    return ok;
}

ShsmSymRows shsm_asmindex_at(AvpArena *a, ShsmAsmIndex *ix, i64 address) {
    Param p = {'i', address, {0}};
    return syms(a, ix, "s.address = ?", &p, 1, "s.kind = 'label', s.size DESC", SHSM_NONE);
}

bool shsm_asmindex_symbol_at(AvpArena *a, ShsmAsmIndex *ix, i64 address, ShsmSymRow *out) {
    ShsmSymRows l = shsm_asmindex_at(a, ix, address);
    bool ok = l.n > 0;
    if (ok) *out = l.v[0];
    avp_vec_free(&l);
    return ok;
}

bool shsm_asmindex_containing(AvpArena *a, ShsmAsmIndex *ix, i64 address, ShsmSymRow *out) {
    Param p[3] = {{'i', address, {0}}, {'i', address - 0x200000, {0}}, {'i', address, {0}}};
    ShsmSymRows l = syms(a, ix, "s.address <= ? AND s.address > ? AND s.kind != 'label' AND s.address + s.size > ?",
                         p, 3, "s.size ASC", SHSM_NONE);
    bool ok = l.n > 0;
    if (ok) *out = l.v[0];
    avp_vec_free(&l);
    return ok;
}

const char *shsm_asmindex_unit_of(AvpArena *a, ShsmAsmIndex *ix, i64 address) {
    ShsmSymRow s;
    if (shsm_asmindex_containing(a, ix, address, &s) || shsm_asmindex_symbol_at(a, ix, address, &s))
        return shsm_symrow_unit_name(a, &s);
    return NULL;
}

static bool ends_with(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && memcmp(s + n - m, suf, m) == 0;
}

ShsmSymRows shsm_asmindex_unit_symbols(AvpArena *a, ShsmAsmIndex *ix, const char *unit, const char *kind) {
    const char *u = strncmp(unit, "main/", 5) == 0 ? unit + 5 : unit;
    if (ends_with(u, ".s")) u = avp_cz(a, avp_strn(u, strlen(u) - 2));
    else if (ends_with(u, ".c") || ends_with(u, ".cpp") || ends_with(u, ".cp")) {
        AvpStr root, ext;
        avp_path_splitext(u, &root, &ext);
        u = avp_cz(a, root);
    }
    if (kind && *kind) {
        Param p[2] = {{'s', 0, avp_str(u)}, {'s', 0, avp_str(kind)}};
        return syms(a, ix, "s.unit = ? AND s.kind = ?", p, 2, NULL, SHSM_NONE);
    }
    Param p = {'s', 0, avp_str(u)};
    return syms(a, ix, "s.unit = ?", &p, 1, NULL, SHSM_NONE);
}

ShsmSymRows shsm_asmindex_functions(AvpArena *a, ShsmAsmIndex *ix) {
    return syms(a, ix, "s.kind = 'function'", NULL, 0, NULL, SHSM_NONE);
}

ShsmSymRows shsm_asmindex_search(AvpArena *a, ShsmAsmIndex *ix, AvpStr pattern, const char *kind, i64 limit) {
    if (kind && *kind) {
        Param p[2] = {{'s', 0, pattern}, {'s', 0, avp_str(kind)}};
        return syms(a, ix, "s.name LIKE ? AND s.kind = ?", p, 2, NULL, limit);
    }
    Param p = {'s', 0, pattern};
    return syms(a, ix, "s.name LIKE ?", &p, 1, NULL, limit);
}

/* ---- blocks ---------------------------------------------------------------------------------------------------- */

static bool resolve_block(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, const char *kind, ShsmSymRow *out) {
    if (t.t == 2) t = shsm_target_addr(t.sym->address); /* a SymRow is not an int in Python: by its name */
    if (t.t == 1) {
        ShsmSymRows l = shsm_asmindex_at(a, ix, t.addr);
        for (size_t i = 0; i < l.n; i++)
            if (kind_is(&l.v[i], kind)) {
                *out = l.v[i];
                avp_vec_free(&l);
                return true;
            }
        avp_vec_free(&l);
        ShsmSymRow c;
        if (shsm_asmindex_containing(a, ix, t.addr, &c) && kind_is(&c, kind)) {
            *out = c;
            return true;
        }
        return false;
    }
    ShsmSymRows l = shsm_asmindex_symbols_named(a, ix, t.name);
    bool ok = false;
    for (size_t i = 0; i < l.n && !ok; i++)
        if (kind_is(&l.v[i], kind)) {
            *out = l.v[i];
            ok = true;
        }
    avp_vec_free(&l);
    return ok;
}

ShsmAsmBlock *shsm_asmindex_read(ShsmAsmIndex *ix, const ShsmSymRow *sym, ShsmAsmFile **file) {
    AvpArena *a = avp_scratch();
    char *rel = avp_cz(a, sym->file);
    for (char *c = rel; *c; c++)
        if (*c == '/') *c = '\\';
    const char *path = avp_path_join(a, ix->asm_dir, rel);
    return shsm_asm_read_block(path, sym->byte_start, sym->byte_end, sym->section, file);
}

ShsmAsmBlock *shsm_asmindex_function(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, ShsmAsmFile **file) {
    ShsmSymRow s;
    *file = NULL;
    return resolve_block(a, ix, t, "function", &s) ? shsm_asmindex_read(ix, &s, file) : NULL;
}

ShsmAsmBlock *shsm_asmindex_object(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, ShsmAsmFile **file) {
    ShsmSymRow s;
    *file = NULL;
    return resolve_block(a, ix, t, "object", &s) ? shsm_asmindex_read(ix, &s, file) : NULL;
}

/* ---- references ---------------------------------------------------------------------------------------------- */

static ShsmRefs refs_q(AvpArena *a, ShsmAsmIndex *ix, const char *where, const Param *p, size_t np,
                       const char *const *kinds, size_t nk) {
    AvpBuf q = {0};
    avp_buf_printf(&q, "SELECT %s FROM refs WHERE %s", REF_COLS, where);
    if (nk) {
        avp_buf_putz(&q, " AND kind IN (");
        for (size_t i = 0; i < nk; i++) avp_buf_putz(&q, i ? ",?" : "?");
        avp_buf_putc(&q, ')');
    }
    avp_buf_putz(&q, " ORDER BY from_addr");
    sqlite3_stmt *st = prep(ix, avp_buf_cz(&q));
    avp_buf_free(&q);
    bind_params(st, p, np);
    for (size_t i = 0; i < nk; i++) bind_z(st, (int)(np + i + 1), kinds[i]);
    ShsmRefs out = {0};
    while (step(ix, st)) {
        ShsmAsmRef r;
        r.from_sym = col_s(a, st, 0);
        r.from_addr = col_i(st, 1);
        r.to_sym = col_s(a, st, 2);
        r.to_off = col_i(st, 3);
        r.kind = intern_kind(a, col_s(a, st, 4));
        r.owner_addr = col_i(st, 5);
        r.to_base = col_i(st, 6);
        r.to_addr = col_i(st, 7);
        r.section = col_s(a, st, 8);
        avp_vec_push(&out, r);
    }
    sqlite3_finalize(st);
    return out;
}

static i64 block_addr(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t) {
    if (t.t == 2) return t.sym->address;
    ShsmSymRows l = shsm_asmindex_at(a, ix, t.addr);
    for (size_t i = 0; i < l.n; i++)
        if (!is_label(&l.v[i])) {
            i64 r = l.v[i].address;
            avp_vec_free(&l);
            return r;
        }
    avp_vec_free(&l);
    ShsmSymRow c;
    return shsm_asmindex_containing(a, ix, t.addr, &c) ? c.address : t.addr;
}

ShsmRefs shsm_asmindex_refs_to(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, const char *const *kinds, size_t nk) {
    if (t.t) {
        Param p = {'i', t.t == 2 ? t.sym->address : t.addr, {0}};
        return refs_q(a, ix, "to_base = ?", &p, 1, kinds, nk);
    }
    Param p = {'s', 0, t.name};
    return refs_q(a, ix, "to_sym = ?", &p, 1, kinds, nk);
}

ShsmRefs shsm_asmindex_refs_at(AvpArena *a, ShsmAsmIndex *ix, i64 address, const char *const *kinds, size_t nk) {
    Param p = {'i', address, {0}};
    return refs_q(a, ix, "to_addr = ?", &p, 1, kinds, nk);
}

ShsmRefs shsm_asmindex_refs_from(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, const char *const *kinds, size_t nk) {
    if (t.t) {
        Param p = {'i', block_addr(a, ix, t), {0}};
        return refs_q(a, ix, "owner_addr = ?", &p, 1, kinds, nk);
    }
    Param p = {'s', 0, t.name};
    return refs_q(a, ix, "from_sym = ?", &p, 1, kinds, nk);
}

static const char *const CODE_KINDS[] = {SHSM_REF_CALL, SHSM_REF_BRANCH};
static const char *const CODE_PTR_KINDS[] = {SHSM_REF_CALL, SHSM_REF_BRANCH, SHSM_REF_PTR};
static const char *const DATA_KINDS[] = {SHSM_REF_HA, SHSM_REF_LO, SHSM_REF_HI, SHSM_REF_SDA21};
static const char *const PTR_KINDS[] = {SHSM_REF_PTR};

ShsmCallers shsm_asmindex_callers(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, bool include_pointers) {
    ShsmRefs r = shsm_asmindex_refs_to(a, ix, t, include_pointers ? CODE_PTR_KINDS : CODE_KINDS,
                                       include_pointers ? 3 : 2);
    ShsmCallers out = {0};
    for (size_t i = 0; i < r.n; i++) {
        ShsmCaller c = {r.v[i].from_sym, r.v[i].from_addr, r.v[i].kind};
        avp_vec_push(&out, c);
    }
    avp_vec_free(&r);
    return out;
}

ShsmCallees shsm_asmindex_callees(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t) {
    ShsmRefs r = shsm_asmindex_refs_from(a, ix, t, CODE_KINDS, 2);
    ShsmCallees out = {0};
    AvpMap *seen = avp_map_new();
    for (size_t i = 0; i < r.n; i++) {
        AvpStr key = avp_str(avp_sprintf(avp_scratch(), "%s\x01%.*s", r.v[i].kind, AVP_SF(r.v[i].to_sym)));
        bool created;
        AvpVal *v = avp_map_put(seen, key, &created);
        if (created) {
            v->i = (i64)out.n;
            ShsmCallee c = {r.v[i].to_sym, r.v[i].kind, 0};
            avp_vec_push(&out, c);
        }
        out.v[v->i].count++;
    }
    avp_map_free(seen);
    avp_vec_free(&r);
    return out;
}

ShsmCallEdges shsm_asmindex_call_graph(AvpArena *a, ShsmAsmIndex *ix, const char *const *kinds, size_t nk) {
    if (!kinds) {
        kinds = CODE_KINDS;
        nk = 2;
    }
    AvpBuf q = {0};
    avp_buf_putz(&q, "SELECT from_sym, owner_addr, to_sym, to_base, kind, from_addr FROM refs WHERE kind IN (");
    for (size_t i = 0; i < nk; i++) avp_buf_putz(&q, i ? ",?" : "?");
    avp_buf_putz(&q, ") ORDER BY from_addr");
    sqlite3_stmt *st = prep(ix, avp_buf_cz(&q));
    avp_buf_free(&q);
    for (size_t i = 0; i < nk; i++) bind_z(st, (int)i + 1, kinds[i]);
    ShsmCallEdges out = {0};
    while (step(ix, st)) {
        ShsmCallEdge e = {col_s(a, st, 0), col_i(st, 1), col_s(a, st, 2), col_i(st, 3),
                          intern_kind(a, col_s(a, st, 4)), col_i(st, 5)};
        avp_vec_push(&out, e);
    }
    sqlite3_finalize(st);
    return out;
}

static bool opt_str_eq(AvpStr x, AvpStr y) { return (!x.p || !y.p) ? (!x.p && !y.p) : avp_str_eq(x, y); }

static ShsmDataRefs data_q(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, bool with_addr) {
    ShsmRefs r = shsm_asmindex_refs_from(a, ix, t, DATA_KINDS, 4);
    ShsmDataRefs out = {0};
    for (size_t i = 0; i < r.n; i++) {
        ShsmDataRef d = {r.v[i].to_sym, r.v[i].to_off, with_addr ? r.v[i].to_addr : SHSM_NONE};
        bool dup = false;
        for (size_t k = 0; k < out.n && !dup; k++)
            dup = opt_str_eq(out.v[k].sym, d.sym) && out.v[k].offset == d.offset && out.v[k].address == d.address;
        if (!dup) avp_vec_push(&out, d);
    }
    avp_vec_free(&r);
    return out;
}
ShsmDataRefs shsm_asmindex_data_refs(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t) { return data_q(a, ix, t, false); }
ShsmDataRefs shsm_asmindex_data_targets(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t) { return data_q(a, ix, t, true); }

static bool str_row_q(AvpArena *a, ShsmAsmIndex *ix, sqlite3_stmt *st, ShsmStrRow *out) {
    bool ok = step(ix, st);
    if (ok) {
        out->owner = col_s(a, st, 0);
        out->address = col_i(st, 1);
        out->text = col_s(a, st, 2);
    }
    sqlite3_finalize(st);
    return ok;
}
static bool string_at(AvpArena *a, ShsmAsmIndex *ix, i64 address, ShsmStrRow *out) {
    sqlite3_stmt *st = prep(ix, "SELECT owner, address, text FROM strings WHERE address = ? LIMIT 1");
    bind_i(st, 1, address);
    return str_row_q(a, ix, st, out);
}
static bool string_by_name(AvpArena *a, ShsmAsmIndex *ix, AvpStr sym, i64 off, ShsmStrRow *out) {
    sqlite3_stmt *st = prep(ix, "SELECT address FROM symbols WHERE name = ? AND kind = 'object' LIMIT 1");
    bind_s(st, 1, sym);
    bool has = step(ix, st);
    i64 base = has ? col_i(st, 0) : SHSM_NONE;
    sqlite3_finalize(st);
    if (!has || base == SHSM_NONE) return false;
    st = prep(ix, "SELECT owner, address, text FROM strings WHERE owner = ? AND address = ?");
    bind_s(st, 1, sym);
    bind_i(st, 2, base + off);
    return str_row_q(a, ix, st, out);
}
static bool str_row_eq(const ShsmStrRow *x, const ShsmStrRow *y) {
    return opt_str_eq(x->owner, y->owner) && x->address == y->address && opt_str_eq(x->text, y->text);
}

ShsmStrRows shsm_asmindex_strings_of(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t, bool follow_pointers) {
    ShsmRefs r = shsm_asmindex_refs_from(a, ix, t, DATA_KINDS, 4);
    ShsmRefs r2 = shsm_asmindex_refs_from(a, ix, t, PTR_KINDS, 1);
    avp_vec_extend(&r, r2.v, r2.n);
    avp_vec_free(&r2);
    ShsmStrRows out = {0};
    for (size_t i = 0; i < r.n; i++) {
        ShsmAsmRef *x = &r.v[i];
        ShsmStrRow row;
        bool has = x->to_addr != SHSM_NONE ? string_at(a, ix, x->to_addr, &row)
                                           : string_by_name(a, ix, x->to_sym, x->to_off, &row);
        if (!has && follow_pointers && x->to_addr != SHSM_NONE && x->kind != SHSM_REF_PTR) {
            Param p = {'i', x->to_addr, {0}};
            ShsmRefs ps = refs_q(a, ix, "from_addr = ?", &p, 1, PTR_KINDS, 1);
            for (size_t k = 0; k < ps.n; k++)
                if (ps.v[k].to_addr != SHSM_NONE) {
                    has = string_at(a, ix, ps.v[k].to_addr, &row);
                    break;
                }
            avp_vec_free(&ps);
        }
        if (!has) continue;
        bool dup = false;
        for (size_t k = 0; k < out.n && !dup; k++) dup = str_row_eq(&out.v[k], &row);
        if (!dup) avp_vec_push(&out, row);
    }
    avp_vec_free(&r);
    return out;
}

ShsmFloatRows shsm_asmindex_floats_of(AvpArena *a, ShsmAsmIndex *ix, ShsmTarget t) {
    ShsmRefs r = shsm_asmindex_refs_from(a, ix, t, DATA_KINDS, 4);
    ShsmFloatRows out = {0};
    for (size_t i = 0; i < r.n; i++) {
        sqlite3_stmt *st;
        if (r.v[i].to_base != SHSM_NONE) {
            st = prep(ix, "SELECT owner, address, kind, value FROM floats WHERE owner_addr = ? ORDER BY address");
            bind_i(st, 1, r.v[i].to_base);
        } else {
            st = prep(ix, "SELECT owner, address, kind, value FROM floats WHERE owner = ? ORDER BY address");
            bind_s(st, 1, r.v[i].to_sym);
        }
        while (step(ix, st)) {
            ShsmFloatRow f = {col_s(a, st, 0), col_i(st, 1), col_s(a, st, 2),
                              sqlite3_column_type(st, 3) != SQLITE_NULL, sqlite3_column_double(st, 3)};
            bool dup = false;
            for (size_t k = 0; k < out.n && !dup; k++) {
                ShsmFloatRow *g = &out.v[k];
                dup = opt_str_eq(g->owner, f.owner) && g->address == f.address && opt_str_eq(g->kind, f.kind) &&
                      g->has_value == f.has_value && (!f.has_value || g->value == f.value);
            }
            if (!dup) avp_vec_push(&out, f);
        }
        sqlite3_finalize(st);
    }
    avp_vec_free(&r);
    return out;
}

ShsmStrRows shsm_asmindex_find_strings(AvpArena *a, ShsmAsmIndex *ix, AvpStr needle, i64 limit) {
    const char *q = avp_sprintf(avp_scratch(),
                                "SELECT owner, address, text FROM strings WHERE instr(text, ?) > 0 ORDER BY address "
                                "LIMIT %lld",
                                (long long)limit);
    sqlite3_stmt *st = prep(ix, q);
    bind_s(st, 1, needle);
    ShsmStrRows out = {0};
    while (step(ix, st)) {
        ShsmStrRow r = {col_s(a, st, 0), col_i(st, 1), col_s(a, st, 2)};
        avp_vec_push(&out, r);
    }
    sqlite3_finalize(st);
    return out;
}

ShsmStrRows shsm_asmindex_users_of_string(AvpArena *a, ShsmAsmIndex *ix, AvpStr needle, i64 limit) {
    ShsmStrRows found = shsm_asmindex_find_strings(a, ix, needle, limit);
    ShsmStrRows out = {0};
    for (size_t i = 0; i < found.n; i++) {
        sqlite3_stmt *st = prep(ix, "SELECT DISTINCT from_sym FROM refs WHERE to_addr = ?");
        bind_i(st, 1, found.v[i].address);
        while (step(ix, st)) {
            ShsmStrRow r = {col_s(a, st, 0), found.v[i].address, found.v[i].text};
            avp_vec_push(&out, r);
        }
        sqlite3_finalize(st);
    }
    avp_vec_free(&found);
    return out;
}

/* ---- tables and sections ---------------------------------------------------------------------------------------- */

static ShsmTableEntries table_q(AvpArena *a, ShsmAsmIndex *ix, const char *prefix) {
    sqlite3_stmt *st = prep(ix, "SELECT from_addr, to_sym, to_addr, section FROM refs WHERE kind = 'ptr' AND "
                                "(section = ? OR section LIKE ?) ORDER BY from_addr");
    bind_z(st, 1, prefix);
    bind_z(st, 2, avp_sprintf(avp_scratch(), "%s$%%", prefix));
    ShsmTableEntries out = {0};
    while (step(ix, st)) {
        ShsmTableEntry e = {col_i(st, 0), col_s(a, st, 1), col_i(st, 2), col_s(a, st, 3)};
        avp_vec_push(&out, e);
    }
    sqlite3_finalize(st);
    return out;
}
ShsmTableEntries shsm_asmindex_ctors(AvpArena *a, ShsmAsmIndex *ix) { return table_q(a, ix, ".ctors"); }
ShsmTableEntries shsm_asmindex_dtors(AvpArena *a, ShsmAsmIndex *ix) { return table_q(a, ix, ".dtors"); }

ShsmSectionRanges shsm_asmindex_section_ranges(AvpArena *a, ShsmAsmIndex *ix, bool merge) {
    sqlite3_stmt *st = prep(ix, "SELECT name, start, end FROM sections ORDER BY start");
    ShsmSectionRanges rows = {0};
    while (step(ix, st)) {
        ShsmAsmSection s = {col_s(a, st, 0), col_i(st, 1), col_i(st, 2)};
        avp_vec_push(&rows, s);
    }
    sqlite3_finalize(st);
    if (!merge) return rows;
    ShsmSectionRanges out = {0};
    AvpMap *at = avp_map_new();
    for (size_t i = 0; i < rows.n; i++) {
        AvpStr base = rows.v[i].name;
        const char *d = base.p ? memchr(base.p, '$', base.n) : NULL;
        if (d) base.n = (size_t)(d - base.p);
        bool created;
        AvpVal *v = avp_map_put(at, base, &created);
        if (created) {
            v->i = (i64)out.n;
            ShsmAsmSection s = {base, rows.v[i].start, rows.v[i].end};
            avp_vec_push(&out, s);
        } else {
            ShsmAsmSection *s = &out.v[v->i];
            if (rows.v[i].start < s->start) s->start = rows.v[i].start;
            if (rows.v[i].end > s->end) s->end = rows.v[i].end;
        }
    }
    avp_map_free(at);
    avp_vec_free(&rows);
    return out;
}

AvpStr shsm_asmindex_section_of(AvpArena *a, ShsmAsmIndex *ix, i64 address) {
    sqlite3_stmt *st = prep(ix, "SELECT name FROM sections WHERE start <= ? AND end > ? LIMIT 1");
    bind_i(st, 1, address);
    bind_i(st, 2, address);
    AvpStr r = step(ix, st) ? col_s(a, st, 0) : (AvpStr){NULL, 0};
    sqlite3_finalize(st);
    return r;
}

AvpJson *shsm_asmindex_sql(AvpArena *a, ShsmAsmIndex *ix, const char *query, const AvpJson *params) {
    sqlite3_stmt *st = prep(ix, query);
    size_t np = params ? avp_json_len(params) : 0;
    for (size_t i = 0; i < np; i++) {
        const AvpJson *v = avp_json_at(params, i);
        int k = (int)i + 1;
        if (avp_json_is_null(v)) sqlite3_bind_null(st, k);
        else if (avp_json_is_int(v)) sqlite3_bind_int64(st, k, avp_json_i64(v, 0));
        else if (avp_json_is_num(v)) sqlite3_bind_double(st, k, avp_json_f64(v, 0));
        else if (avp_json_is_str(v)) bind_s(st, k, v->u.str);
        else sqlite3_bind_null(st, k);
    }
    AvpJson *rows = avp_json_arr(a);
    int nc = sqlite3_column_count(st);
    while (step(ix, st)) {
        AvpJson *row = avp_json_arr(a);
        for (int c = 0; c < nc; c++) {
            switch (sqlite3_column_type(st, c)) {
            case SQLITE_INTEGER: avp_json_arr_push(a, row, avp_json_int(a, sqlite3_column_int64(st, c))); break;
            case SQLITE_FLOAT: avp_json_arr_push(a, row, avp_json_float(a, sqlite3_column_double(st, c))); break;
            case SQLITE_NULL: avp_json_arr_push(a, row, avp_json_null()); break;
            default: avp_json_arr_push(a, row, avp_json_str(a, col_s(a, st, c))); break;
            }
        }
        avp_json_arr_push(a, rows, row);
    }
    sqlite3_finalize(st);
    return rows;
}
