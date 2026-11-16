/* hashes.py HashDb: build/index/hashes.sqlite (stamp, build, the variable pairs) and var_names(). */
#include "hashes.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/sort.h"

#include <sqlite3.h>
#include <string.h>

static const char *const ARCHIVES[2] = {"data.arc", "igc.arc"};

static const char CREATE_SQL[] =
    "\n"
    "            DROP TABLE IF EXISTS meta; DROP TABLE IF EXISTS corpus; DROP TABLE IF EXISTS hashvals;\n"
    "            DROP TABLE IF EXISTS consts; DROP TABLE IF EXISTS arc; DROP TABLE IF EXISTS vars;\n"
    "            DROP TABLE IF EXISTS derivations; DROP TABLE IF EXISTS truth;\n"
    "            CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT);\n"
    "            CREATE TABLE corpus (text TEXT PRIMARY KEY, origin TEXT, address INTEGER);\n"
    "            CREATE TABLE hashvals (fn TEXT, value INTEGER, text TEXT);\n"
    "            CREATE TABLE consts (value INTEGER, kind TEXT, address INTEGER);\n"
    "            CREATE TABLE arc (value INTEGER, archive TEXT, idx INTEGER);\n"
    "            CREATE TABLE vars (var_addr INTEGER, var TEXT, var_off INTEGER, text TEXT, call TEXT, via TEXT,\n"
    "                               site INTEGER, func TEXT, func_addr INTEGER, sinit INTEGER, unit TEXT);\n"
    "            CREATE TABLE derivations (format TEXT, candidates INTEGER, hits INTEGER, expected REAL, kept "
    "INTEGER);\n"
    "            CREATE TABLE truth (fn TEXT, text TEXT, value INTEGER, archive TEXT, idx INTEGER);\n"
    "        ";
static const char INDEX_SQL[] =
    "\n"
    "            CREATE INDEX hv_value ON hashvals(value); CREATE INDEX hv_text ON hashvals(text);\n"
    "            CREATE INDEX consts_value ON consts(value); CREATE INDEX arc_value ON arc(value);\n"
    "        ";

const char *hashes_db_path(AvpArena *a, const ShsmRepo *r) { return avp_path_join(a, r->index_dir, "hashes.sqlite"); }

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static void exec(sqlite3 *db, const char *sql) {
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) sql_fail(db);
}

static sqlite3_stmt *prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static void step_done(sqlite3 *db, sqlite3_stmt *st) {
    if (sqlite3_step(st) != SQLITE_DONE) sql_fail(db);
    sqlite3_reset(st);
}

static void bind_str(sqlite3_stmt *st, int i, AvpStr s) {
    if (s.p) sqlite3_bind_text(st, i, s.n ? s.p : "", (int)s.n, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, i);
}

static void bind_cz(sqlite3_stmt *st, int i, const char *s) {
    if (s) sqlite3_bind_text(st, i, s, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(st, i);
}

/* _stamp(path): "size:mtime_ns", "-" when os.stat fails. */
static const char *file_stamp(AvpArena *a, const char *path) {
    AvpStat st;
    if (!path || !avp_stat(path, &st)) return "-";
    return avp_sprintf(a, "%lld:%lld", (long long)st.size, (long long)st.mtime_ns);
}

static const char *index_built(AvpArena *a, ShsmAsmIndex *ix) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ix->db, "SELECT key, value FROM meta", -1, &st, NULL) != SQLITE_OK) return "";
    const char *built = "";
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char *k = (const char *)sqlite3_column_text(st, 0);
        if (!k || strcmp(k, "built") != 0) continue;
        switch (sqlite3_column_type(st, 1)) { /* str(value), the last row of the key winning as in dict() */
        case SQLITE_NULL: built = "None"; break;
        case SQLITE_INTEGER: built = avp_sprintf(a, "%lld", (long long)sqlite3_column_int64(st, 1)); break;
        case SQLITE_FLOAT: built = avp_repr_f64(a, sqlite3_column_double(st, 1)).p; break;
        default: built = avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        }
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return "";
    return built;
}

static const char *arc_path(AvpArena *a, const ShsmRepo *r, int i) {
    const char *p = avp_path_join(a, r->root, "orig");
    p = avp_path_join(a, p, r->version);
    p = avp_path_join(a, p, "files");
    return avp_path_join(a, p, ARCHIVES[i]);
}

const char *hashes_stamp(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%d|%s|%s", HASHES_DB_SCHEMA, index_built(a, ix),
                   file_stamp(a, r->retail_dol));
    for (int i = 0; i < 2; i++) avp_buf_printf(&b, "|%s", file_stamp(a, arc_path(a, r, i)));
    return avp_buf_cz(&b);
}

/* ---- the build ---- */

typedef struct {
    AvpStr text;   /* bytes */
    const char *origin;
    i64 address;   /* SHSM_NONE: None */
} CorpusEntry;
typedef AVP_VEC(CorpusEntry) CorpusVec;

typedef struct {
    u32 value;
    const char *archive;
    i64 idx;
} ArcEntry;

static int cmp_bytes(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

typedef struct {
    const ShsmDol *dol;
    AvpArena *a;
    AvpIMap *targets;
} DerefCtx;

static bool deref_fn(void *ctx, bool ptr, i64 address, AvpStr *text, const char **via) {
    DerefCtx *c = ctx;
    AvpStr s;
    if (ptr) {
        AvpStr w;
        if (!hashes_dol_read(c->dol, address, 4, &w)) return false;
        u32 p = (u32)(u8)w.p[0] << 24 | (u32)(u8)w.p[1] << 16 | (u32)(u8)w.p[2] << 8 | (u8)w.p[3];
        if (!hashes_cstring(c->dol, p, &s)) return false;
        *via = "ptr";
    } else {
        if (!hashes_cstring(c->dol, address, &s)) return false;
        *via = "literal";
    }
    *text = hashes_text(c->a, s);
    return true;
}

static bool target_fn(void *ctx, const ShsmAsmRow *row, i64 *addr) {
    DerefCtx *c = ctx;
    AvpVal *v = avp_imap_get(c->targets, (u64)row->address);
    if (!v || v->i == SHSM_NONE) return false;
    *addr = v->i;
    return true;
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_cmp_i64(*(const i64 *)x, *(const i64 *)y);
}

static void read_vars(AvpArena *a, sqlite3 *db, ShsmAsmIndex *ix, const ShsmDol *dol) {
    AvpMap *calls = avp_map_new();
    for (size_t k = 0; k < 5; k++) {
        ShsmSymRows at = shsm_asmindex_at(a, ix, HASHES_CALL_ROUTINES[k].address);
        for (size_t i = 0; i < at.n; i++)
            if (avp_str_eqz(at.v[i].kind, "function"))
                avp_map_put(calls, at.v[i].name, NULL)->p = (void *)&HASHES_CALL_ROUTINES[k].spec;
    }
    AvpISet *sinits = avp_iset_new();
    ShsmTableEntries ctors = shsm_asmindex_ctors(a, ix);
    for (size_t i = 0; i < ctors.n; i++) avp_iset_add(sinits, (u64)ctors.v[i].target_addr);
    AvpISet *funcs = avp_iset_new();
    static const char *const CALL[] = {"call"};
    for (size_t k = 0; k < 5; k++) {
        ShsmRefs refs = shsm_asmindex_refs_to(a, ix, shsm_target_addr(HASHES_CALL_ROUTINES[k].address), CALL, 1);
        for (size_t i = 0; i < refs.n; i++)
            if (refs.v[i].owner_addr != SHSM_NONE) avp_iset_add(funcs, (u64)refs.v[i].owner_addr);
    }
    AvpI64Vec order = {0};
    size_t pos = 0;
    u64 m;
    while (avp_iset_next(funcs, &pos, &m)) avp_vec_push(&order, (i64)m);
    avp_sort(order.v, order.n, sizeof *order.v, cmp_i64, NULL, false);
    sqlite3_stmt *st = prep(db, "INSERT INTO vars VALUES (?,?,?,?,?,?,?,?,?,?,?)");
    for (size_t f = 0; f < order.n; f++) {
        ShsmAsmFile *file = NULL;
        ShsmAsmBlock *blk = NULL;
        AvpTry t;
        if (AVP_TRY(&t)) {
            blk = shsm_asmindex_function(a, ix, shsm_target_addr(order.v[f]), &file);
            avp_try_end(&t);
        } else {
            blk = NULL; /* a listing being rewritten: skip it this round */
        }
        if (!blk) {
            if (file) shsm_asm_free(file);
            continue;
        }
        DerefCtx c = {dol, a, avp_imap_new()};
        ShsmRefs from = shsm_asmindex_refs_from(a, ix, shsm_target_addr(blk->address), NULL, 0);
        for (size_t i = 0; i < from.n; i++) {
            bool created;
            AvpVal *v = avp_imap_put(c.targets, (u64)from.v[i].from_addr, &created);
            if (created) v->i = from.v[i].to_addr;
        }
        i64 sinit = avp_iset_has(sinits, (u64)blk->address) || avp_str_startswith(blk->name, AVP_S("__sinit_"));
        const char *unit = shsm_asmindex_unit_of(a, ix, blk->address);
        HashesReader rd = {&c, target_fn, NULL, deref_fn};
        HashesPairs pairs = hashes_read_pairs(a, blk, calls, &rd);
        for (size_t i = 0; i < pairs.n; i++) {
            const HashesPair *p = &pairs.v[i];
            sqlite3_bind_int64(st, 1, p->var_addr);
            bind_str(st, 2, p->var);
            sqlite3_bind_int64(st, 3, p->var_off);
            bind_str(st, 4, p->text);
            bind_cz(st, 5, p->call);
            bind_cz(st, 6, p->via);
            sqlite3_bind_int64(st, 7, p->site);
            bind_str(st, 8, blk->name);
            sqlite3_bind_int64(st, 9, blk->address);
            sqlite3_bind_int64(st, 10, sinit);
            bind_cz(st, 11, unit ? unit : "");
            step_done(db, st);
        }
        avp_vec_free(&pairs);
        avp_imap_free(c.targets);
        shsm_asm_free(file);
    }
    sqlite3_finalize(st);
    avp_vec_free(&order);
    avp_iset_free(funcs);
    avp_iset_free(sinits);
    avp_map_free(calls);
}

static void build(HashesDb *h, const char *stamp) {
    AvpArena *a = avp_arena_new(0);
    const ShsmRepo *r = h->repo;
    ShsmAsmIndex *ix = h->ix;
    AvpTry t;
    if (AVP_TRY(&t)) {
        h->dol = shsm_dol_load(avp_arena(), r->retail_dol);
        avp_try_end(&t);
    } else {
        shsm_raise(SHSM_EXC_USAGE, "no retail DOL at %s", r->retail_dol ? r->retail_dol : "None");
    }
    const ShsmDol *dol = h->dol;
    sqlite3 *db = h->db;
    exec(db, "BEGIN IMMEDIATE"); /* one transaction (the Python commits each statement of its scripts) */
    exec(db, CREATE_SQL);

    /* the corpus: the DOL's strings, then the asm index's literals */
    CorpusVec corpus = {0};
    AvpMap *in_corpus = avp_map_new();
    HashesDolStrings ds = hashes_dol_strings(a, dol);
    for (size_t i = 0; i < ds.n; i++) {
        avp_map_put(in_corpus, ds.v[i].text, NULL);
        avp_vec_push(&corpus, ((CorpusEntry){ds.v[i].text, "dol", ds.v[i].address}));
    }
    avp_vec_free(&ds);
    {
        sqlite3_stmt *st = prep(ix->db, "SELECT address FROM strings");
        int rc;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            i64 addr = sqlite3_column_int64(st, 0);
            AvpStr s;
            if (!hashes_cstring(dol, addr, &s)) continue;
            bool created;
            avp_map_put(in_corpus, s, &created);
            if (created) avp_vec_push(&corpus, ((CorpusEntry){s, "asm", addr}));
        }
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) sql_fail(ix->db);
    }
    /* the archives' entry tables */
    AVP_VEC(ArcEntry) arc = {0};
    for (int i = 0; i < 2; i++) {
        const char *p = arc_path(a, r, i);
        if (!avp_exists(p)) continue;
        AvpU64Vec tab = hashes_read_arc_table(p);
        for (size_t k = 0; k < tab.n; k++) avp_vec_push(&arc, ((ArcEntry){(u32)tab.v[k], ARCHIVES[i], (i64)k}));
        avp_vec_free(&tab);
    }
    AvpIMap *arcset = avp_imap_new(); /* value -> index into arc of its first entry */
    for (size_t i = 0; i < arc.n; i++) {
        bool created;
        AvpVal *v = avp_imap_put(arcset, arc.v[i].value, &created);
        if (created) v->i = (i64)i;
    }

    /* derivations, judged by the archive entries they reproduce under arcname */
    AvpStrVec texts = {0};
    for (size_t i = 0; i < corpus.n; i++) avp_vec_push(&texts, corpus.v[i].text);
    AvpStrVec fmts = {0}, bases = {0};
    hashes_format_derivations(a, texts.v, texts.n, &fmts, &bases);
    CorpusVec derived = {0};
    AvpMap *in_derived = avp_map_new();
    {
        sqlite3_stmt *st = prep(db, "INSERT INTO derivations VALUES (?,?,?,?,?)");
        for (size_t f = 0; f < fmts.n; f++) {
            AvpStr fmt = fmts.v[f];
            i64 at = avp_str_find(fmt, AVP_S("%s"), 0, AVP_END);
            AvpStr pre = avp_strn(fmt.p, (size_t)at), post = avp_strn(fmt.p + at + 2, fmt.n - (size_t)at - 2);
            AvpMap *cset = avp_map_new();
            for (size_t b = 0; b < bases.n; b++) {
                AvpStr c = avp_str_concat(a, avp_str_concat(a, pre, bases.v[b]), post);
                if (hashes_arcname(c) == hashes_arcname(bases.v[b]) || avp_map_has(in_corpus, c)) continue;
                avp_map_put(cset, c, NULL);
            }
            AvpStrVec cands = avp_map_keys(cset);
            /* py: set order (random per run in the Python); sorted here */
            avp_sort(cands.v, cands.n, sizeof *cands.v, cmp_bytes, NULL, false);
            i64 hits = 0;
            for (size_t k = 0; k < cands.n; k++)
                if (avp_imap_has(arcset, hashes_arcname(cands.v[k]))) hits++;
            double exp = hashes_expected_chance((i64)cands.n, (i64)avp_imap_len(arcset));
            bool keep = avp_imap_len(arcset) && hits >= 2 && (double)hits > 10 * exp;
            AvpStr ft = hashes_text(a, fmt);
            bind_str(st, 1, ft);
            sqlite3_bind_int64(st, 2, (i64)cands.n);
            sqlite3_bind_int64(st, 3, hits);
            sqlite3_bind_double(st, 4, exp);
            sqlite3_bind_int64(st, 5, keep);
            step_done(db, st);
            if (keep) {
                const char *origin = avp_pyfmt(a, "fmt:%S", &ft).p;
                for (size_t k = 0; k < cands.n; k++) {
                    bool created;
                    AvpStr c = avp_str_dup(a, cands.v[k]);
                    avp_map_put(in_derived, c, &created);
                    if (created) avp_vec_push(&derived, ((CorpusEntry){c, origin, SHSM_NONE}));
                }
            }
            avp_vec_free(&cands);
            avp_map_free(cset);
        }
        sqlite3_finalize(st);
    }
    AvpStrVec ctext = {0}; /* _t(b) of every corpus string */
    for (size_t i = 0; i < corpus.n; i++) avp_vec_push(&ctext, hashes_text(a, corpus.v[i].text));
    {
        sqlite3_stmt *st = prep(db, "INSERT OR IGNORE INTO corpus VALUES (?,?,?)");
        for (int pass = 0; pass < 2; pass++) {
            const CorpusVec *cv = pass ? &derived : &corpus;
            for (size_t i = 0; i < cv->n; i++) {
                bind_str(st, 1, pass ? hashes_text(a, cv->v[i].text) : ctext.v[i]);
                bind_cz(st, 2, cv->v[i].origin);
                if (cv->v[i].address == SHSM_NONE) sqlite3_bind_null(st, 3);
                else sqlite3_bind_int64(st, 3, cv->v[i].address);
                step_done(db, st);
            }
        }
        sqlite3_finalize(st);
    }
    {
        sqlite3_stmt *st = prep(db, "INSERT INTO hashvals VALUES (?,?,?)");
        for (int k = 0; k < HASHES_COUNT; k++) {
            for (size_t i = 0; i < corpus.n; i++) {
                bind_cz(st, 1, HASHES[k].name);
                sqlite3_bind_int64(st, 2, HASHES[k].func(corpus.v[i].text));
                bind_str(st, 3, ctext.v[i]);
                step_done(db, st);
            }
        }
        for (size_t i = 0; i < derived.n; i++) {
            bind_cz(st, 1, "arcname");
            sqlite3_bind_int64(st, 2, hashes_arcname(derived.v[i].text));
            bind_str(st, 3, hashes_text(a, derived.v[i].text));
            step_done(db, st);
        }
        sqlite3_finalize(st);
    }
    {
        HashesConsts cs = hashes_code_constants(dol);
        sqlite3_stmt *st = prep(db, "INSERT INTO consts VALUES (?,?,?)");
        for (size_t i = 0; i < cs.n; i++) {
            sqlite3_bind_int64(st, 1, cs.v[i].value);
            bind_cz(st, 2, cs.v[i].kind);
            sqlite3_bind_int64(st, 3, cs.v[i].address);
            step_done(db, st);
        }
        sqlite3_finalize(st);
        avp_vec_free(&cs);
    }
    {
        sqlite3_stmt *st = prep(db, "INSERT INTO arc VALUES (?,?,?)");
        for (size_t i = 0; i < arc.n; i++) {
            sqlite3_bind_int64(st, 1, arc.v[i].value);
            bind_cz(st, 2, arc.v[i].archive);
            sqlite3_bind_int64(st, 3, arc.v[i].idx);
            step_done(db, st);
        }
        sqlite3_finalize(st);
    }
    {
        /* ground truth: corpus strings that reproduce archive entry hashes */
        sqlite3_stmt *st = prep(db, "INSERT INTO truth VALUES (?,?,?,?,?)");
        for (int k = 0; k < HASHES_NAME_COUNT; k++) {
            for (size_t i = 0; i < corpus.n; i++) {
                u32 v = HASHES[k].func(corpus.v[i].text);
                AvpVal *e = avp_imap_get(arcset, v);
                if (!e) continue;
                bind_cz(st, 1, HASHES[k].name);
                bind_str(st, 2, ctext.v[i]);
                sqlite3_bind_int64(st, 3, v);
                bind_cz(st, 4, arc.v[e->i].archive);
                sqlite3_bind_int64(st, 5, arc.v[e->i].idx);
                step_done(db, st);
            }
        }
        sqlite3_finalize(st);
    }
    read_vars(a, db, ix, dol);

    exec(db, INDEX_SQL);
    sqlite3_stmt *st = prep(db, "INSERT INTO meta VALUES ('stamp', ?)");
    bind_cz(st, 1, stamp);
    step_done(db, st);
    sqlite3_finalize(st);
    exec(db, "COMMIT");
    avp_vec_free(&corpus);
    avp_vec_free(&derived);
    avp_vec_free(&texts);
    avp_vec_free(&ctext);
    avp_vec_free(&fmts);
    avp_vec_free(&bases);
    avp_vec_free(&arc);
    avp_imap_free(arcset);
    avp_map_free(in_corpus);
    avp_map_free(in_derived);
    avp_arena_free(a);
}

HashesDb *hashes_db_open(const ShsmRepo *r, ShsmAsmIndex *ix, bool rebuild) {
    AvpArena *a = avp_arena();
    if (!avp_makedirs(r->index_dir, true)) shsm_raise_io();
    HashesDb *h = AVP_NEW(a, HashesDb);
    h->repo = r;
    h->ix = ix;
    h->path = hashes_db_path(a, r);
    if (sqlite3_open_v2(h->path, &h->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        const char *msg = h->db ? avp_strdup(a, sqlite3_errmsg(h->db)) : "unable to open database file";
        sqlite3_close(h->db);
        shsm_raise(SHSM_EXC_SQLITE, "%s", msg);
    }
    sqlite3_busy_timeout(h->db, 60000);
    const char *stamp = hashes_stamp(a, r, ix);
    const char *cur = NULL;
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(h->db, "SELECT key, value FROM meta", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *k = (const char *)sqlite3_column_text(st, 0);
            if (k && strcmp(k, "stamp") == 0)
                cur = sqlite3_column_type(st, 1) == SQLITE_NULL ? NULL
                                                                 : avp_strdup(a, (const char *)sqlite3_column_text(st, 1));
        }
        sqlite3_finalize(st);
    }
    if (rebuild || !cur || strcmp(cur, stamp) != 0) build(h, stamp);
    return h;
}

void hashes_db_close(HashesDb *h) {
    if (h && h->db) sqlite3_close(h->db);
    if (h) h->db = NULL;
}

/* ---- var_names ---- */

AvpJson *hashes_var_name_json(AvpArena *a, const HashesVarName *v) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("var_addr"), avp_json_int(a, v->var_addr));
    avp_json_set(a, o, AVP_S("var"), v->var ? avp_json_strz(a, v->var) : avp_json_null());
    avp_json_set(a, o, AVP_S("var_off"), avp_json_int(a, v->var_off));
    avp_json_set(a, o, AVP_S("text"), v->text ? avp_json_strz(a, v->text) : avp_json_null());
    avp_json_set(a, o, AVP_S("call"), v->call ? avp_json_strz(a, v->call) : avp_json_null());
    avp_json_set(a, o, AVP_S("function"), v->function ? avp_json_strz(a, v->function) : avp_json_null());
    avp_json_set(a, o, AVP_S("function_addr"), avp_json_int(a, v->function_addr));
    avp_json_set(a, o, AVP_S("sinit"), avp_json_bool(v->sinit));
    avp_json_set(a, o, AVP_S("unit"), v->unit ? avp_json_strz(a, v->unit) : avp_json_null());
    avp_json_set(a, o, AVP_S("kind"), v->kind ? avp_json_strz(a, v->kind) : avp_json_null());
    return o;
}

static const char *col_cz(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return NULL;
    return avp_strdup(a, (const char *)sqlite3_column_text(st, c));
}

HashesVarNames *hashes_var_names(AvpArena *a, const ShsmRepo *r, const char *unit, const i64 *addresses,
                                 size_t naddresses, const i64 *functions, size_t nfunctions) {
    if (!r) r = shsm_repo_find(NULL);
    const char *path = hashes_db_path(a, r);
    if (!avp_isfile(path)) return NULL;
    const char *want = unit ? shsm_unit_name(a, unit) : NULL;
    sqlite3 *db = NULL;
    const char *uri = avp_sprintf(a, "file:%s?mode=ro", avp_str_replace(a, avp_str(path), AVP_S("\\"), AVP_S("/"), -1).p);
    if (sqlite3_open_v2(uri, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }
    sqlite3_busy_timeout(db, 5000);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "SELECT var_addr, var, var_off, text, call, func, func_addr, sinit, unit FROM vars "
                               "ORDER BY var_addr, site", -1, &st, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }
    HashesVarNames *out = AVP_NEW(a, HashesVarNames);
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        HashesVarName v = {0};
        v.var_addr = sqlite3_column_int64(st, 0);
        v.var = col_cz(a, st, 1);
        v.var_off = sqlite3_column_int64(st, 2);
        v.text = col_cz(a, st, 3);
        v.call = col_cz(a, st, 4);
        v.function = col_cz(a, st, 5);
        v.function_addr = sqlite3_column_int64(st, 6);
        v.sinit = sqlite3_column_int64(st, 7) != 0;
        v.unit = col_cz(a, st, 8);
        bool keep = (want && v.unit && strcmp(v.unit, want) == 0) || (!want && !v.unit);
        for (size_t i = 0; !keep && i < naddresses; i++) keep = addresses[i] == v.var_addr;
        for (size_t i = 0; !keep && i < nfunctions; i++) keep = functions[i] == v.function_addr;
        if (!keep) continue;
        v.kind = hashes_call_term(v.call);
        avp_vec_push(out, v);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    if (rc != SQLITE_DONE) {
        avp_vec_free(out);
        return NULL;
    }
    return out;
}

/* ---- rows for the commands ---- */

HashesVarRows hashes_db_var_rows(AvpArena *a, HashesDb *h) {
    HashesVarRows out = {0};
    sqlite3_stmt *st = prep(h->db, "SELECT var_addr, var, var_off, text, call, via, site, func, func_addr, sinit, "
                                   "unit FROM vars ORDER BY var_addr, site");
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        HashesVarRow v = {0};
        v.var_addr = sqlite3_column_int64(st, 0);
        v.var = col_cz(a, st, 1);
        v.var_off = sqlite3_column_int64(st, 2);
        v.text = col_cz(a, st, 3);
        v.call = col_cz(a, st, 4);
        v.via = col_cz(a, st, 5);
        v.site = sqlite3_column_int64(st, 6);
        v.function = col_cz(a, st, 7);
        v.function_addr = sqlite3_column_int64(st, 8);
        v.sinit = sqlite3_column_int64(st, 9);
        v.unit = col_cz(a, st, 10);
        avp_vec_push(&out, v);
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) sql_fail(h->db);
    return out;
}
