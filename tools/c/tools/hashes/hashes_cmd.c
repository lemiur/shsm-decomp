/* hashes.py commands: scan, of, find, vars, stats, functions. */
#include "hashes.h"

#include "avp/fmt.h"
#include "avp/sort.h"
#include "shsm_units.h"

#include <sqlite3.h>
#include <string.h>

/* ---- output ---- */

static void line(HashesOut *o, AvpStr s) {
    if (o->out) shsm_output_line(o->out, s);
    else avp_vec_push(&o->lines, avp_str_dup(avp_arena(), s));
}

static void data(HashesOut *o, const char *key, AvpJson *v) {
    if (o->out) {
        shsm_output_data(o->out, key, v);
        return;
    }
    if (!o->data) o->data = avp_json_obj(avp_arena());
    avp_json_set(avp_arena(), o->data, avp_str(key), v);
}

/* ---- queries ---- */

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static sqlite3_stmt *prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static bool next_row(sqlite3 *db, sqlite3_stmt *st) {
    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) return true;
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(st);
        sql_fail(db);
    }
    sqlite3_finalize(st);
    return false;
}

static AvpStr col_str(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return (AvpStr){NULL, 0};
    return avp_str_dup(a, avp_strn((const char *)sqlite3_column_text(st, c), (size_t)sqlite3_column_bytes(st, c)));
}

static i64 count_query(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = prep(db, sql);
    i64 n = 0;
    if (next_row(db, st)) {
        n = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return n;
}

static AvpJson *opt_str(AvpArena *a, AvpStr s) { return s.p ? avp_json_str(a, s) : avp_json_null(); }

AvpSet *hashes_verified(HashesDb *h) {
    AvpSet *s = avp_set_new();
    sqlite3_stmt *st = prep(h->db, "SELECT DISTINCT fn FROM truth");
    while (next_row(h->db, st)) avp_set_add(s, col_str(avp_scratch(), st, 0));
    return s;
}

static size_t fns_of(const HashesArgs *args, const AvpSet *verified, const HashesFn **out) {
    size_t n = 0;
    for (int i = 0; i < HASHES_NAME_COUNT; i++)
        if (args->unverified || avp_set_has(verified, avp_str(HASHES[i].name))) out[n++] = &HASHES[i];
    return n;
}

static AvpJson *fn_names(AvpArena *a, const HashesFn **fns, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, avp_json_strz(a, fns[i]->name));
    return arr;
}

/* _where(ix, addr, cache): (symbol name, offset, unit) */
typedef struct {
    AvpStr sym;
    i64 off;
    AvpStr unit;
} Where;

static Where where_of(AvpArena *a, ShsmAsmIndex *ix, AvpIMap *cache, i64 addr) {
    bool created;
    AvpVal *v = avp_imap_put(cache, (u64)addr, &created);
    if (created) {
        Where *w = AVP_NEW(a, Where);
        ShsmSymRow s;
        if (shsm_asmindex_containing(a, ix, addr, &s)) {
            *w = (Where){s.name, addr - s.address, s.unit};
        } else {
            const char *u = shsm_asmindex_unit_of(a, ix, addr);
            *w = (Where){{NULL, 0}, 0, avp_str(u ? u : "")};
        }
        v->p = w;
    }
    return *(Where *)v->p;
}

/* ---- scan ---- */

typedef struct {
    i64 start, end;
} Range;
typedef AVP_VEC(Range) Ranges;

static void add_syms(Ranges *r, const ShsmSymRows *syms) {
    for (size_t i = 0; i < syms->n; i++) {
        i64 sz = syms->v[i].size == SHSM_NONE ? 0 : syms->v[i].size;
        avp_vec_push(r, ((Range){syms->v[i].address, syms->v[i].address + sz}));
    }
}

static bool hex_like_address(AvpStr f) { /* ^(0x)?8[0-9A-Fa-f]{7}$ */
    if (avp_str_startswith(f, AVP_S("0x"))) f = avp_strn(f.p + 2, f.n - 2);
    if (f.n == 9 && f.p[8] == '\n') f.n = 8;
    if (f.n != 8 || f.p[0] != '8') return false;
    for (size_t i = 1; i < 8; i++) {
        char c = f.p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

/* _selection(ix, args): false for everything. */
static bool selection(AvpArena *a, HashesDb *h, const HashesArgs *args, Ranges *ranges) {
    ShsmAsmIndex *ix = h->ix;
    for (size_t i = 0; i < args->units.n; i++) {
        ShsmSymRows syms = shsm_asmindex_unit_symbols(a, ix, avp_cz(a, args->units.v[i]), NULL);
        if (!syms.n) shsm_raise(SHSM_EXC_USAGE, "unknown or empty unit: %.*s", AVP_SF(args->units.v[i]));
        add_syms(ranges, &syms);
    }
    if (args->lib) {
        const ShsmUnits *us = shsm_load_units(h->repo);
        AvpStr lib = avp_str(args->lib);
        AvpStr prefix = avp_str_concat(a, avp_str_rstrip_chars(lib, AVP_S("/")), AVP_S("/"));
        size_t found = 0;
        for (size_t i = 0; i < us->n; i++) {
            const ShsmUnit *u = us->v[i];
            if (!(strcmp(u->library ? u->library : "", args->lib) == 0 || avp_str_startswith(avp_str(u->path), prefix)))
                continue;
            found++;
        }
        if (!found) shsm_raise(SHSM_EXC_USAGE, "no unit in library %s", args->lib);
        for (size_t i = 0; i < us->n; i++) {
            const ShsmUnit *u = us->v[i];
            if (!(strcmp(u->library ? u->library : "", args->lib) == 0 || avp_str_startswith(avp_str(u->path), prefix)))
                continue;
            ShsmSymRows syms = shsm_asmindex_unit_symbols(a, ix, u->path, NULL);
            add_syms(ranges, &syms);
        }
    }
    for (size_t i = 0; i < args->ranges.n; i++) {
        AvpStr r = args->ranges.v[i];
        AvpStrVec parts = avp_str_split(r, AVP_S("-"), -1);
        i64 x = 0, y = 0;
        bool ok = parts.n == 2 && avp_parse_int(parts.v[0], 16, &x, NULL) && avp_parse_int(parts.v[1], 16, &y, NULL);
        avp_vec_free(&parts);
        if (!ok) shsm_raise(SHSM_EXC_USAGE, "bad range %.*s (START-END, hex)", AVP_SF(r));
        avp_vec_push(ranges, ((Range){x, y}));
    }
    for (size_t i = 0; i < args->functions.n; i++) {
        AvpStr f = args->functions.v[i];
        ShsmSymRow s;
        bool got = shsm_asmindex_symbol(a, ix, f, &s);
        i64 v;
        if (!got && hex_like_address(f) && avp_parse_int(f, 16, &v, NULL)) got = shsm_asmindex_containing(a, ix, v, &s);
        if (!got) shsm_raise(SHSM_EXC_USAGE, "unknown symbol: %.*s", AVP_SF(f));
        avp_vec_push(ranges, ((Range){s.address, s.address + (s.size == SHSM_NONE ? 0 : s.size)}));
    }
    return ranges->n > 0;
}

static bool in_ranges(const Ranges *r, bool any, i64 a) {
    if (!any) return true;
    for (size_t i = 0; i < r->n; i++)
        if (r->v[i].start <= a && a < r->v[i].end) return true;
    return false;
}

typedef struct {
    u32 value;
    const char *fn;
    AvpStr text, origin;
    double expected;
    const char *strength, *kind;
    i64 address; /* SHSM_NONE: None */
    AvpStr symbol, unit;
    i64 offset;
    bool arc;
    const char *archive;
    i64 index;
} Hit;

static int cmp_hit(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const Hit *p = x, *q = y;
    int c = avp_cmp_f64(p->expected, q->expected);
    if (c) return c;
    if (p->arc != q->arc) return p->arc ? 1 : -1;
    c = avp_cmp_i64(p->address == SHSM_NONE ? 0 : p->address, q->address == SHSM_NONE ? 0 : q->address);
    if (c) return c;
    c = strcmp(p->arc ? p->archive : "", q->arc ? q->archive : "");
    if (c) return c;
    return avp_cmp_i64(p->arc ? p->index : 0, q->arc ? q->index : 0);
}

typedef struct {
    u32 value;
    const char *kind;
    i64 address;
} Site;
typedef struct {
    u32 value;
    const char *archive;
    i64 idx;
} ArcRow;

int hashes_cmd_scan(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    AvpArena *a = avp_arena();
    sqlite3 *db = h->db;
    const HashesFn *fns[HASHES_NAME_COUNT];
    size_t nf = fns_of(args, verified, fns);
    Ranges ranges = {0};
    bool any = selection(a, h, args, &ranges);
    i64 corpus = count_query(db, "SELECT COUNT(*) FROM corpus");
    AvpMap *origin = avp_map_new();
    sqlite3_stmt *st = prep(db, "SELECT text, origin FROM corpus");
    while (next_row(db, st)) avp_map_put(origin, col_str(a, st, 0), NULL)->p = avp_cz(a, col_str(a, st, 1));
    AvpMap *population = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(origin, &it)) {
        const char *o = it.val->p;
        bool base = o && (strcmp(o, "dol") == 0 || strcmp(o, "asm") == 0);
        avp_map_put(population, avp_str(base ? "base" : o ? o : "None"), NULL)->i += 1;
    }
    AVP_VEC(Site) sites = {0};
    st = prep(db, "SELECT value, kind, address FROM consts");
    while (next_row(db, st)) {
        i64 addr = sqlite3_column_int64(st, 2);
        if (!in_ranges(&ranges, any, addr)) continue;
        Site s = {(u32)sqlite3_column_int64(st, 0), NULL, addr};
        s.kind = avp_cz(a, col_str(a, st, 1));
        avp_vec_push(&sites, s);
    }
    AvpISet *cands = avp_iset_new();
    for (size_t i = 0; i < sites.n; i++)
        if (!hashes_excluded(sites.v[i].value)) avp_iset_add(cands, sites.v[i].value);
    AVP_VEC(ArcRow) arcs = {0};
    if (!args->no_arc && !any) {
        st = prep(db, "SELECT value, archive, idx FROM arc");
        while (next_row(db, st))
            avp_vec_push(&arcs, ((ArcRow){(u32)sqlite3_column_int64(st, 0), avp_cz(a, col_str(a, st, 1)),
                                          sqlite3_column_int64(st, 2)}));
    }
    AvpISet *arcvals = avp_iset_new();
    for (size_t i = 0; i < arcs.n; i++) avp_iset_add(arcvals, arcs.v[i].value);
    AVP_VEC(Hit) hits = {0};
    AvpIMap *cache = avp_imap_new();
    for (size_t f = 0; f < nf; f++) {
        AvpIMap *table = avp_imap_new();
        st = prep(db, "SELECT value, text FROM hashvals WHERE fn = ?");
        sqlite3_bind_text(st, 1, fns[f]->name, -1, SQLITE_TRANSIENT);
        while (next_row(db, st)) {
            bool created;
            AvpVal *v = avp_imap_put(table, (u64)sqlite3_column_int64(st, 0), &created);
            if (created) v->p = AVP_NEW(a, AvpStrVec);
            avp_vec_push((AvpStrVec *)v->p, col_str(a, st, 1));
        }
        for (int pass = 0; pass < 2; pass++) {
            size_t n = pass ? arcs.n : sites.n;
            for (size_t i = 0; i < n; i++) {
                u32 v = pass ? arcs.v[i].value : sites.v[i].value;
                if (!pass && !avp_iset_has(cands, v)) continue;
                AvpVal *tv = avp_imap_get(table, v);
                if (!tv) continue;
                const AvpStrVec *texts = tv->p;
                Where w = {{NULL, 0}, 0, {NULL, 0}};
                if (!pass) w = where_of(a, h->ix, cache, sites.v[i].address);
                for (size_t k = 0; k < texts->n; k++) {
                    Hit x = {0};
                    x.value = v;
                    x.fn = fns[f]->name;
                    x.text = texts->v[k];
                    AvpVal *ov = avp_map_get(origin, x.text);
                    const char *o = ov ? ov->p : "?";
                    x.origin = avp_str(o ? o : "None");
                    bool base = o && (strcmp(o, "dol") == 0 || strcmp(o, "asm") == 0);
                    AvpVal *pv = avp_map_get(population, avp_str(base ? "base" : o ? o : "None"));
                    i64 nvals = pass ? (i64)avp_iset_len(arcvals) : (i64)avp_iset_len(cands);
                    x.expected = hashes_expected_chance(pv ? pv->i : corpus, nvals);
                    x.strength = hashes_strength(x.expected);
                    if (!pass) {
                        x.kind = sites.v[i].kind;
                        x.address = sites.v[i].address;
                        x.symbol = w.sym;
                        x.offset = w.off;
                        x.unit = w.unit;
                    } else {
                        x.kind = "arc entry";
                        x.address = SHSM_NONE;
                        x.symbol = avp_pyfmt(a, "%s[%d]", arcs.v[i].archive, arcs.v[i].idx);
                        x.offset = 0;
                        x.unit = AVP_S("");
                        x.arc = true;
                        x.archive = arcs.v[i].archive;
                        x.index = arcs.v[i].idx;
                    }
                    avp_vec_push(&hits, x);
                }
            }
        }
        avp_imap_free(table);
    }
    avp_sort(hits.v, hits.n, sizeof *hits.v, cmp_hit, NULL, false);
    AvpJson *jh = avp_json_arr(a);
    for (size_t i = 0; i < hits.n; i++) {
        const Hit *x = &hits.v[i];
        AvpStr where = (x->address != SHSM_NONE && x->address)
                           ? avp_pyfmt(a, "%08X %s+0x%X", x->address, x->symbol.p ? avp_cz(a, x->symbol) : "?", x->offset)
                           : x->symbol;
        AvpStr unit = (x->unit.p && x->unit.n) ? x->unit : AVP_S("-");
        AvpStr shown = hashes_show(a, x->text);
        line(out, avp_pyfmt(a, "%-6s %08X %-9s %-12s %-40S %-28S %S  (chance %.3g; %S)", x->strength, x->value, x->fn,
                            x->kind, &where, &unit, &shown, x->expected, &x->origin));
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("value"), avp_json_int(a, x->value));
        avp_json_set(a, o, AVP_S("fn"), avp_json_strz(a, x->fn));
        avp_json_set(a, o, AVP_S("text"), avp_json_str(a, x->text));
        avp_json_set(a, o, AVP_S("origin"), avp_json_str(a, x->origin));
        avp_json_set(a, o, AVP_S("expected"), avp_json_float(a, x->expected));
        avp_json_set(a, o, AVP_S("strength"), avp_json_strz(a, x->strength));
        avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, x->kind));
        avp_json_set(a, o, AVP_S("address"), x->address == SHSM_NONE ? avp_json_null() : avp_json_int(a, x->address));
        avp_json_set(a, o, AVP_S("symbol"), opt_str(a, x->symbol));
        avp_json_set(a, o, AVP_S("offset"), avp_json_int(a, x->offset));
        avp_json_set(a, o, AVP_S("unit"), opt_str(a, x->unit));
        if (x->arc) {
            avp_json_set(a, o, AVP_S("archive"), avp_json_strz(a, x->archive));
            avp_json_set(a, o, AVP_S("index"), avp_json_int(a, x->index));
        }
        avp_json_arr_push(a, jh, o);
    }
    data(out, "hits", jh);
    data(out, "constants_examined", avp_json_int(a, (i64)avp_iset_len(cands)));
    data(out, "sites", avp_json_int(a, (i64)sites.n));
    data(out, "arc_entries", avp_json_int(a, (i64)avp_iset_len(arcvals)));
    data(out, "functions", fn_names(a, fns, nf));
    data(out, "corpus", avp_json_int(a, corpus));
    int rc = SHSM_EXIT_OK;
    if (!hits.n) {
        AvpStr tail = avp_iset_len(arcvals) ? avp_pyfmt(a, " and %d archive entries", avp_iset_len(arcvals)) : AVP_S("");
        line(out, avp_pyfmt(a, "no hash of a corpus string among %d candidate constants%S", avp_iset_len(cands), &tail));
        rc = SHSM_EXIT_FINDING;
    }
    avp_vec_free(&hits);
    avp_vec_free(&sites);
    avp_vec_free(&arcs);
    avp_vec_free(&ranges);
    avp_iset_free(cands);
    avp_iset_free(arcvals);
    avp_imap_free(cache);
    avp_map_free(origin);
    avp_map_free(population);
    return rc;
}

/* ---- of / find ---- */

typedef struct {
    bool arc;
    const char *kind;
    i64 address;
    const char *archive;
    i64 index;
} Occ;
typedef AVP_VEC(Occ) Occs;

static Occs occurrences(AvpArena *a, HashesDb *h, u32 v) {
    Occs occ = {0};
    if (!hashes_excluded(v)) {
        sqlite3_stmt *st = prep(h->db, "SELECT kind, address FROM consts WHERE value = ?");
        sqlite3_bind_int64(st, 1, v);
        while (next_row(h->db, st))
            avp_vec_push(&occ, ((Occ){false, avp_cz(a, col_str(a, st, 0)), sqlite3_column_int64(st, 1), NULL, 0}));
    }
    sqlite3_stmt *st = prep(h->db, "SELECT archive, idx FROM arc WHERE value = ?");
    sqlite3_bind_int64(st, 1, v);
    while (next_row(h->db, st))
        avp_vec_push(&occ, ((Occ){true, "arc entry", 0, avp_cz(a, col_str(a, st, 0)), sqlite3_column_int64(st, 1)}));
    return occ;
}

static AvpJson *occ_json(AvpArena *a, const Occs *occ) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < occ->n; i++) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, occ->v[i].kind));
        if (occ->v[i].arc) {
            avp_json_set(a, o, AVP_S("archive"), avp_json_strz(a, occ->v[i].archive));
            avp_json_set(a, o, AVP_S("index"), avp_json_int(a, occ->v[i].index));
        } else {
            avp_json_set(a, o, AVP_S("address"), avp_json_int(a, occ->v[i].address));
        }
        avp_json_arr_push(a, arr, o);
    }
    return arr;
}

static AvpStr occ_text(AvpArena *a, HashesDb *h, AvpIMap *cache, const Occ *o) {
    if (o->arc) return avp_pyfmt(a, "%s[%d]", o->archive, o->index);
    Where w = where_of(a, h->ix, cache, o->address);
    const char *unit = w.unit.p ? avp_cz(a, w.unit) : NULL;
    return avp_pyfmt(a, "%s %08X %s+0x%X %s", o->kind, o->address, w.sym.p ? avp_cz(a, w.sym) : "?", w.off, unit);
}

static AvpJson *excluded_json(AvpArena *a, u32 v) {
    const char *e = hashes_excluded(v);
    return e ? avp_json_strz(a, e) : avp_json_null();
}

int hashes_cmd_of(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    AvpArena *a = avp_arena();
    AvpJson *res = avp_json_arr(a);
    AvpIMap *cache = avp_imap_new();
    for (size_t i = 0; i < args->strings.n; i++) {
        AvpStr s = args->strings.v[i];
        AvpStr b = hashes_bytes(a, s);
        for (int k = 0; k < HASHES_COUNT; k++) {
            const HashesFn *fn = &HASHES[k];
            u32 v = fn->func(b);
            Occs occ = occurrences(a, h, v);
            bool ver = avp_set_has(verified, avp_str(fn->name));
            AvpJson *o = avp_json_obj(a);
            avp_json_set(a, o, AVP_S("string"), avp_json_str(a, s));
            avp_json_set(a, o, AVP_S("fn"), avp_json_strz(a, fn->name));
            avp_json_set(a, o, AVP_S("value"), avp_json_int(a, v));
            avp_json_set(a, o, AVP_S("verified"), avp_json_bool(ver));
            avp_json_set(a, o, AVP_S("name_hash"), avp_json_bool(fn->name_hash));
            avp_json_set(a, o, AVP_S("excluded"), excluded_json(a, v));
            avp_json_set(a, o, AVP_S("occurrences"), occ_json(a, &occ));
            avp_json_arr_push(a, res, o);
            const char *flag = ver ? "" : fn->name_hash ? " (unverified)" : " (checksum)";
            AvpBuf wb;
            avp_buf_init(&wb, a);
            for (size_t j = 0; j < occ.n && j < 6; j++) {
                if (j) avp_buf_putz(&wb, "; ");
                avp_buf_put(&wb, occ_text(a, h, cache, &occ.v[j]));
            }
            if (!occ.n) avp_buf_putz(&wb, "-");
            if (occ.n > 6) avp_buf_pyfmt(&wb, "; ... %d more", occ.n - 6);
            AvpStr where = avp_buf_str(&wb);
            AvpStr shown = hashes_show(a, s);
            line(out, avp_pyfmt(a, "%-24S %-9s %08X%s  %S", &shown, fn->name, v, flag, &where));
            avp_vec_free(&occ);
        }
    }
    data(out, "results", res);
    avp_imap_free(cache);
    return SHSM_EXIT_OK;
}

static u32 parse_hex(AvpStr s) {
    AvpStr t = (s.n >= 2 && s.p[0] == '0' && (s.p[1] == 'x' || s.p[1] == 'X')) ? avp_strn(s.p + 2, s.n - 2) : s;
    i64 v;
    bool overflow = false;
    if (!avp_parse_int(t, 16, &v, &overflow)) {
        if (overflow) shsm_raise(SHSM_EXC_USAGE, "not a 32-bit value: %.*s", AVP_SF(s));
        shsm_raise(SHSM_EXC_USAGE, "not a hex value: %.*s", AVP_SF(s));
    }
    if (v < 0 || v > 0xFFFFFFFFll) shsm_raise(SHSM_EXC_USAGE, "not a 32-bit value: %.*s", AVP_SF(s));
    return (u32)v;
}

int hashes_cmd_find(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    AvpArena *a = avp_arena();
    const HashesFn *fns[HASHES_NAME_COUNT];
    size_t nf = fns_of(args, verified, fns);
    AvpSet *words = hashes_read_words(&args->words);
    AvpJson *res = avp_json_arr(a);
    AvpIMap *cache = avp_imap_new();
    bool any = false;
    for (size_t i = 0; i < args->values.n; i++) {
        u32 v = parse_hex(args->values.v[i]);
        AvpJson *found = avp_json_arr(a);
        AVP_VEC(AvpStr) flines = {0};
        for (size_t f = 0; f < nf; f++) {
            sqlite3_stmt *st = prep(h->db, "SELECT text FROM hashvals WHERE fn = ? AND value = ?");
            sqlite3_bind_text(st, 1, fns[f]->name, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, v);
            AvpStrVec texts = {0}, sources = {0};
            while (next_row(h->db, st)) {
                avp_vec_push(&texts, col_str(a, st, 0));
                avp_vec_push(&sources, AVP_S("corpus"));
            }
            size_t pos = 0;
            AvpStr w;
            while (avp_set_next(words, &pos, &w)) {
                if (fns[f]->func(w) != v) continue;
                avp_vec_push(&texts, hashes_text(a, w));
                avp_vec_push(&sources, AVP_S("words"));
            }
            for (size_t k = 0; k < texts.n; k++) {
                AvpJson *o = avp_json_obj(a);
                avp_json_set(a, o, AVP_S("fn"), avp_json_strz(a, fns[f]->name));
                avp_json_set(a, o, AVP_S("text"), avp_json_str(a, texts.v[k]));
                avp_json_set(a, o, AVP_S("source"), avp_json_str(a, sources.v[k]));
                avp_json_arr_push(a, found, o);
                AvpStr shown = hashes_show(a, texts.v[k]);
                avp_vec_push(&flines, avp_pyfmt(a, "%08X %-9s %S  [%S]", v, fns[f]->name, &shown, &sources.v[k]));
            }
            avp_vec_free(&texts);
            avp_vec_free(&sources);
        }
        Occs occ = occurrences(a, h, v);
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("value"), avp_json_int(a, v));
        avp_json_set(a, o, AVP_S("excluded"), excluded_json(a, v));
        avp_json_set(a, o, AVP_S("matches"), found);
        avp_json_set(a, o, AVP_S("occurrences"), occ_json(a, &occ));
        avp_json_arr_push(a, res, o);
        if (!flines.n) {
            const char *e = hashes_excluded(v);
            AvpStr why = e ? avp_pyfmt(a, " (value excluded: %s)", e) : AVP_S("");
            line(out, avp_pyfmt(a, "%08X: no corpus string%S", v, &why));
        }
        any |= flines.n > 0;
        for (size_t k = 0; k < flines.n; k++) line(out, flines.v[k]);
        for (size_t k = 0; k < occ.n && k < 10; k++) {
            AvpStr t = occ_text(a, h, cache, &occ.v[k]);
            line(out, avp_pyfmt(a, "    at %S", &t));
        }
        avp_vec_free(&flines);
        avp_vec_free(&occ);
    }
    data(out, "results", res);
    data(out, "functions", fn_names(a, fns, nf));
    data(out, "words", avp_json_int(a, (i64)avp_set_len(words)));
    avp_set_free(words);
    avp_imap_free(cache);
    return any ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
}

/* ---- vars ---- */

typedef struct {
    const HashesVarRow *r;
    const char *kind, *status;
    AvpStr cls, fcls;
    int order;
} VarItem;

static int status_order(const char *s) { return strcmp(s, "mismatch") == 0 ? 0 : strcmp(s, "new") == 0 ? 1 : 2; }

static int cmp_var_item(const void *x, const void *y, void *ctx) {
    const VarItem *p = x, *q = y;
    if (ctx) {
        AvpArena *a = avp_scratch();
        int c = avp_str_cmp(avp_str_lower(a, p->cls), avp_str_lower(a, q->cls));
        if (c) return c;
    }
    int c = p->order - q->order;
    if (c) return c;
    return avp_cmp_i64(p->r->var_addr, q->r->var_addr);
}

static AvpJson *opt_cz(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }

static int cmp_str_keys(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

static AvpStr counts_text(AvpArena *a, const AvpMap *m, bool terms) {
    AvpStrVec keys = avp_map_keys(m);
    avp_sort(keys.v, keys.n, sizeof *keys.v, cmp_str_keys, NULL, false);
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < keys.n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        const char *k = avp_cz(a, keys.v[i]);
        avp_buf_pyfmt(&b, "%s %d", terms ? hashes_call_term(k) : k, avp_map_get(m, keys.v[i])->i);
    }
    avp_vec_free(&keys);
    return b.p ? avp_buf_str(&b) : AVP_S("");
}

static AvpJson *counts_json(AvpArena *a, const AvpMap *m) {
    AvpJson *o = avp_json_obj(a);
    AvpMapIter it = {0};
    while (avp_map_next(m, &it)) avp_json_set(a, o, it.key, avp_json_int(a, it.val->i));
    return o;
}

static AvpStr str_or_none(const char *s) { return avp_str(s ? s : "None"); }

int hashes_cmd_vars_rows(const HashesArgs *args, const HashesVarRows *rows, const AvpMap *names, HashesOut *out) {
    AvpArena *a = avp_arena();
    AVP_VEC(VarItem) res = {0};
    AvpStr pats[2];
    size_t np = 0;
    if (args->pattern && *args->pattern) pats[np++] = avp_str_lower(a, avp_str(args->pattern));
    if (args->grep && *args->grep) pats[np++] = avp_str_lower(a, avp_str(args->grep));
    for (size_t i = 0; i < rows->n; i++) {
        const HashesVarRow *r = &rows->v[i];
        VarItem d = {.r = r};
        d.kind = hashes_call_term(r->call);
        d.status = hashes_var_status(names, r->var ? avp_str(r->var) : (AvpStr){NULL, 0}, str_or_none(r->text));
        hashes_pair_classes(a, avp_str(r->function), str_or_none(r->text), avp_str(r->call), &d.cls, &d.fcls);
        d.order = status_order(d.status);
        if (np) {
            AvpStr parts[4] = {str_or_none(r->var), str_or_none(r->text), str_or_none(r->function),
                               str_or_none(r->unit)};
            AvpStr hay = avp_str_lower(a, avp_str_join(a, AVP_S(" "), parts, 4));
            bool ok = true;
            for (size_t k = 0; k < np && ok; k++) ok = avp_str_contains(hay, pats[k]);
            if (!ok) continue;
        }
        if (args->cls && *args->cls) {
            AvpStr c = avp_str_lower(a, avp_str(args->cls));
            if (!avp_str_eq(c, avp_str_lower(a, d.cls)) && !avp_str_eq(c, avp_str_lower(a, d.fcls))) continue;
        }
        if (args->call && *args->call && !(r->call && strcmp(r->call, args->call) == 0)) continue;
        if (args->status && *args->status && strcmp(d.status, args->status) != 0) continue;
        avp_vec_push(&res, d);
    }
    AvpMap *counts = avp_map_new(), *calls = avp_map_new(), *by_class = avp_map_new();
    for (size_t i = 0; i < res.n; i++) avp_map_put(counts, avp_str(res.v[i].status), NULL)->i += 1;
    for (size_t i = 0; i < res.n; i++) avp_map_put(calls, str_or_none(res.v[i].r->call), NULL)->i += 1;
    AvpStr ct = counts_text(a, counts, false), cl = counts_text(a, calls, true);
    line(out, avp_pyfmt(a, "%d pairs (%S; %S)", res.n, &ct, &cl));
    for (size_t i = 0; i < res.n; i++) avp_map_put(by_class, res.v[i].cls, NULL)->i += 1;
    VarItem *sorted = avp_xmalloc((res.n ? res.n : 1) * sizeof *sorted);
    if (res.n) memcpy(sorted, res.v, res.n * sizeof *sorted);
    avp_sort(sorted, res.n, sizeof *sorted, cmp_var_item, args->by_class ? (void *)1 : NULL, false);
    AvpStr last = {NULL, 0};
    for (size_t i = 0; i < res.n; i++) {
        const VarItem *d = &sorted[i];
        if (args->by_class && (!last.p || !avp_str_eq(d->cls, last))) {
            last = d->cls;
            line(out, avp_pyfmt(a, "class %S (%d)", &last, avp_map_get(by_class, last)->i));
        }
        AvpStr var = str_or_none(d->r->var);
        if (d->r->var_off) var = avp_pyfmt(a, "%S+0x%X", &var, d->r->var_off);
        AvpStr shown = hashes_show(a, str_or_none(d->r->text));
        bool literal = d->r->via && strcmp(d->r->via, "literal") == 0;
        line(out, avp_pyfmt(a, "%-8s %-21s %-22S %-36S %s%s  in %s (%s)", d->status, d->kind, &var, &shown,
                            literal ? "" : "[via pointer] ", d->r->sinit ? "sinit" : "code", d->r->function,
                            d->r->unit));
    }
    avp_xfree(sorted);
    AvpJson *pairs = avp_json_arr(a);
    for (size_t i = 0; i < res.n; i++) {
        const VarItem *d = &res.v[i];
        const HashesVarRow *r = d->r;
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("var_addr"), avp_json_int(a, r->var_addr));
        avp_json_set(a, o, AVP_S("var"), opt_cz(a, r->var));
        avp_json_set(a, o, AVP_S("var_off"), avp_json_int(a, r->var_off));
        avp_json_set(a, o, AVP_S("text"), opt_cz(a, r->text));
        avp_json_set(a, o, AVP_S("call"), opt_cz(a, r->call));
        avp_json_set(a, o, AVP_S("via"), opt_cz(a, r->via));
        avp_json_set(a, o, AVP_S("site"), avp_json_int(a, r->site));
        avp_json_set(a, o, AVP_S("function"), opt_cz(a, r->function));
        avp_json_set(a, o, AVP_S("function_addr"), avp_json_int(a, r->function_addr));
        avp_json_set(a, o, AVP_S("sinit"), avp_json_bool(r->sinit != 0));
        avp_json_set(a, o, AVP_S("unit"), opt_cz(a, r->unit));
        avp_json_set(a, o, AVP_S("kind"), opt_cz(a, d->kind));
        avp_json_set(a, o, AVP_S("status"), avp_json_strz(a, d->status));
        avp_json_set(a, o, AVP_S("class"), avp_json_str(a, d->cls));
        avp_json_set(a, o, AVP_S("function_class"), avp_json_str(a, d->fcls));
        avp_json_arr_push(a, pairs, o);
    }
    data(out, "pairs", pairs);
    data(out, "counts", counts_json(a, counts));
    data(out, "calls", counts_json(a, calls));
    data(out, "classes", counts_json(a, by_class));
    bool filtered = (args->status && *args->status) || (args->call && *args->call) || (args->cls && *args->cls) ||
                    (args->pattern && *args->pattern) || (args->grep && *args->grep);
    int rc = filtered && !res.n ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
    avp_vec_free(&res);
    avp_map_free(counts);
    avp_map_free(calls);
    avp_map_free(by_class);
    return rc;
}

int hashes_cmd_vars(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    (void)verified;
    AvpMap *names = hashes_source_names(h->repo);
    HashesVarRows rows = hashes_db_var_rows(avp_arena(), h);
    int rc = hashes_cmd_vars_rows(args, &rows, names, out);
    avp_vec_free(&rows);
    return rc;
}

/* ---- stats / functions ---- */

int hashes_cmd_stats(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    (void)args;
    (void)verified;
    AvpArena *a = avp_arena();
    sqlite3 *db = h->db;
    AvpMap *corpus = avp_map_new();
    sqlite3_stmt *st = prep(db, "SELECT origin, COUNT(*) FROM corpus GROUP BY origin");
    while (next_row(db, st)) avp_map_put(corpus, str_or_none(avp_cz(a, col_str(a, st, 0))), NULL)->i = sqlite3_column_int64(st, 1);
    i64 total = 0;
    AvpMapIter it = {0};
    while (avp_map_next(corpus, &it)) total += it.val->i;
    i64 arcn = count_query(db, "SELECT COUNT(DISTINCT value) FROM arc");
    i64 nsites = 0;
    AvpISet *cands = avp_iset_new();
    st = prep(db, "SELECT value FROM consts");
    while (next_row(db, st)) {
        nsites++;
        u32 v = (u32)sqlite3_column_int64(st, 0);
        if (!hashes_excluded(v)) avp_iset_add(cands, v);
    }
    i64 truth[HASHES_NAME_COUNT], hits[HASHES_NAME_COUNT];
    for (int k = 0; k < HASHES_NAME_COUNT; k++) {
        st = prep(db, "SELECT COUNT(DISTINCT text) FROM truth WHERE fn = ?");
        sqlite3_bind_text(st, 1, HASHES[k].name, -1, SQLITE_TRANSIENT);
        truth[k] = 0;
        if (next_row(db, st)) {
            truth[k] = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
    }
    AvpVal *dv = avp_map_get(corpus, AVP_S("dol")), *av = avp_map_get(corpus, AVP_S("asm"));
    i64 base = (dv ? dv->i : 0) + (av ? av->i : 0);
    for (int k = 0; k < HASHES_NAME_COUNT; k++) {
        st = prep(db, "SELECT COUNT(*) FROM hashvals hv JOIN consts c ON c.value = hv.value WHERE hv.fn = ?");
        sqlite3_bind_text(st, 1, HASHES[k].name, -1, SQLITE_TRANSIENT);
        i64 n = 0;
        if (next_row(db, st)) {
            n = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        st = prep(db, "SELECT DISTINCT hv.value FROM hashvals hv JOIN consts c ON c.value = hv.value WHERE hv.fn = ?");
        sqlite3_bind_text(st, 1, HASHES[k].name, -1, SQLITE_TRANSIENT);
        i64 c = 0;
        while (next_row(db, st))
            if (avp_iset_has(cands, (u64)sqlite3_column_int64(st, 0))) c++;
        hits[k] = n ? c : 0;
    }
    AvpMap *vars_n = avp_map_new();
    st = prep(db, "SELECT call, COUNT(*) FROM vars GROUP BY call");
    while (next_row(db, st)) avp_map_put(vars_n, str_or_none(avp_cz(a, col_str(a, st, 0))), NULL)->i = sqlite3_column_int64(st, 1);
    typedef struct {
        AvpStr format;
        i64 cands, hits;
        double expected;
        i64 kept;
    } Der;
    AVP_VEC(Der) der = {0};
    st = prep(db, "SELECT format, candidates, hits, expected, kept FROM derivations ORDER BY hits DESC");
    while (next_row(db, st))
        avp_vec_push(&der, ((Der){col_str(a, st, 0), sqlite3_column_int64(st, 1), sqlite3_column_int64(st, 2),
                                  sqlite3_column_double(st, 3), sqlite3_column_int64(st, 4)}));
    AvpStr ct = counts_text(a, corpus, false);
    line(out, avp_pyfmt(a, "corpus: %d strings (%S)", total, &ct));
    line(out, avp_pyfmt(a, "constants: %d sites, %d distinct candidate values after exclusions; %d archive entry hashes",
                        nsites, avp_iset_len(cands), arcn));
    line(out, avp_pyfmt(a, "ground truth (corpus strings reproducing archive entry hashes; chance at %d x %d / 2^32 = "
                           "%.3g):",
                        base, arcn, hashes_expected_chance(base, arcn)));
    for (int k = 0; k < HASHES_NAME_COUNT; k++)
        line(out, avp_pyfmt(a, "  %-9s %4d pairs  %s; constant hits %d", HASHES[k].name, truth[k],
                            truth[k] ? "used" : "not used", hits[k]));
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t nk = 0;
    for (size_t i = 0; i < der.n; i++) {
        if (!der.v[i].kept) continue;
        if (nk++) avp_buf_putz(&b, ", ");
        AvpStr shown = hashes_show(a, der.v[i].format);
        avp_buf_pyfmt(&b, "%S (%d hits, chance %.2g)", &shown, der.v[i].hits, der.v[i].expected);
    }
    AvpStr kept = nk ? avp_buf_str(&b) : AVP_S("none");
    line(out, avp_pyfmt(a, "derivations kept: %S", &kept));
    AvpBuf vb;
    avp_buf_init(&vb, a);
    size_t vi = 0;
    it = (AvpMapIter){0};
    while (avp_map_next(vars_n, &it)) {
        if (vi++) avp_buf_putz(&vb, ", ");
        avp_buf_pyfmt(&vb, "%S %d", &it.key, it.val->i);
    }
    AvpStr vt = vi ? avp_buf_str(&vb) : AVP_S("");
    line(out, avp_pyfmt(a, "variable pairs: %S", &vt));
    data(out, "corpus", counts_json(a, corpus));
    data(out, "corpus_total", avp_json_int(a, total));
    data(out, "sites", avp_json_int(a, nsites));
    data(out, "candidates", avp_json_int(a, (i64)avp_iset_len(cands)));
    data(out, "arc_entries", avp_json_int(a, arcn));
    AvpJson *gt = avp_json_obj(a), *ch = avp_json_obj(a);
    for (int k = 0; k < HASHES_NAME_COUNT; k++) {
        avp_json_set(a, gt, avp_str(HASHES[k].name), avp_json_int(a, truth[k]));
        avp_json_set(a, ch, avp_str(HASHES[k].name), avp_json_int(a, hits[k]));
    }
    data(out, "ground_truth", gt);
    data(out, "constant_hits", ch);
    data(out, "variables", counts_json(a, vars_n));
    AvpJson *dj = avp_json_arr(a);
    for (size_t i = 0; i < der.n; i++) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("format"), opt_str(a, der.v[i].format));
        avp_json_set(a, o, AVP_S("candidates"), avp_json_int(a, der.v[i].cands));
        avp_json_set(a, o, AVP_S("hits"), avp_json_int(a, der.v[i].hits));
        avp_json_set(a, o, AVP_S("expected"), avp_json_float(a, der.v[i].expected));
        avp_json_set(a, o, AVP_S("kept"), avp_json_bool(der.v[i].kept != 0));
        avp_json_arr_push(a, dj, o);
    }
    data(out, "derivations", dj);
    AvpJson *sj = avp_json_arr(a);
    st = prep(db, "SELECT fn, text, value, archive, idx FROM truth ORDER BY archive, idx LIMIT 20");
    while (next_row(db, st)) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("fn"), opt_str(a, col_str(a, st, 0)));
        avp_json_set(a, o, AVP_S("text"), opt_str(a, col_str(a, st, 1)));
        avp_json_set(a, o, AVP_S("value"), avp_json_int(a, sqlite3_column_int64(st, 2)));
        avp_json_set(a, o, AVP_S("archive"), opt_str(a, col_str(a, st, 3)));
        avp_json_set(a, o, AVP_S("index"), avp_json_int(a, sqlite3_column_int64(st, 4)));
        avp_json_arr_push(a, sj, o);
    }
    data(out, "samples", sj);
    avp_vec_free(&der);
    avp_iset_free(cands);
    avp_map_free(corpus);
    avp_map_free(vars_n);
    return SHSM_EXIT_OK;
}

int hashes_cmd_functions(const HashesArgs *args, HashesDb *h, const AvpSet *verified, HashesOut *out) {
    (void)args;
    (void)h;
    (void)verified;
    AvpArena *a = avp_arena();
    AvpJson *arr = avp_json_arr(a);
    for (int k = 0; k < HASHES_COUNT; k++) {
        const HashesFn *fn = &HASHES[k];
        line(out, avp_pyfmt(a, "%-9s %-50s %s%s", fn->name, fn->routine, fn->evidence,
                            fn->name_hash ? "" : " [not a name hash]"));
        avp_json_arr_push(a, arr, hashes_fn_json(a, fn));
    }
    data(out, "functions", arr);
    return SHSM_EXIT_OK;
}
