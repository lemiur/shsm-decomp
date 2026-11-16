/* unit_plan.py: the model (Func, Datum, Model, load_model, is_thunk, class_groups, _flag_align), the library
 * ranges (find_library, parse_range) and the tag names. */
#include "unit_plan.h"

#include "shsm_libraries.h"
#include "shsm_re.h"
#include "shsm_symbols.h"
#include "shsm_units.h"
#include "witness_lookup.h"

#include <sqlite3.h>
#include <string.h>

const char *const UP_TAG_NAMES[UP_NTAGS] = {"sinit",   "dupfloat", "dupfloat~", "dupstr", "dupstr~",
                                            "file",    "file~",    "weak",      "nocross", "textpad",
                                            "datapad", "order",    "class",     "ps2",     "ps2~",
                                            "inclass", "ps2run",   "known"};

AvpStrVec up_tag_names(UpTags ts) {
    AvpStrVec v = {0};
    for (int t = 0; t < UP_NTAGS; t++)
        if (ts & UP_BIT(t)) avp_vec_push(&v, avp_str(UP_TAG_NAMES[t]));
    avp_sort_strs(v.v, v.n);
    return v;
}

AvpStr up_tag_key(AvpArena *a, UpTags ts) {
    AvpStrVec v = up_tag_names(ts);
    AvpStr k = avp_str_join(a, AVP_S("+"), v.v, v.n);
    avp_vec_free(&v);
    return k;
}

/* ---- the model ---- */

static int cmp_fn_addr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = ((const UpFunc *)x)->address, b = ((const UpFunc *)y)->address;
    return a < b ? -1 : a > b;
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = *(const i64 *)x, b = *(const i64 *)y;
    return a < b ? -1 : a > b;
}

static int cmp_datum_addr(const void *x, const void *y, void *ctx) {
    const UpDatum *d = ctx;
    i64 a = d[*(const size_t *)x].address, b = d[*(const size_t *)y].address;
    return a < b ? -1 : a > b;
}

UpModel *up_model_new(AvpArena *a, const UpFunc *fns, size_t n, const UpDatum *data, size_t nd,
                      const i64 (*local_calls)[2], size_t nl, const i64 (*align)[2], size_t nal, const UpClass *classes,
                      size_t nc, const char *class_note) {
    UpModel *m = AVP_NEW(a, UpModel);
    m->fns = AVP_NEWN(a, UpFunc, n ? n : 1);
    if (n) memcpy(m->fns, fns, n * sizeof *fns);
    m->n = n;
    avp_sort(m->fns, n, sizeof *m->fns, cmp_fn_addr, NULL, false);
    m->data = AVP_NEWN(a, UpDatum, nd ? nd : 1);
    if (nd) memcpy(m->data, data, nd * sizeof *data);
    m->ndata = nd;
    m->data_at = avp_imap_new_cap(nd);
    for (size_t i = 0; i < nd; i++) avp_imap_put(m->data_at, (u64)data[i].address, NULL)->i = (i64)i;
    m->local_calls = AVP_NEWN(a, UpPair, nl ? nl : 1);
    if (nl) memcpy(m->local_calls, local_calls, nl * sizeof *local_calls);
    m->nlocal = nl;
    m->align = avp_imap_new();
    for (size_t i = 0; i < nal; i++) avp_imap_put(m->align, (u64)align[i][0], NULL)->i = align[i][1];
    m->classes = AVP_NEWN(a, UpClass, nc ? nc : 1);
    if (nc) memcpy(m->classes, classes, nc * sizeof *classes);
    m->nclasses = nc;
    m->class_note = class_note;
    /* pads: per section (dict order of first datum), the data that follow a hole */
    AvpMap *by_sec = avp_map_new();
    AVP_VEC(AvpPtrVec *) groups = {0};
    for (size_t i = 0; i < nd; i++) {
        bool created;
        AvpVal *v = avp_map_put(by_sec, avp_str(data[i].section), &created);
        if (created) {
            v->i = (i64)groups.n;
            avp_vec_push(&groups, AVP_NEW(a, AvpPtrVec));
        }
        avp_vec_push(groups.v[v->i], (void *)(uintptr_t)i);
    }
    m->npads = groups.n;
    m->pads = AVP_NEWN(a, UpPads, groups.n ? groups.n : 1);
    AvpMapIter it = {0};
    size_t k = 0;
    while (avp_map_next(by_sec, &it)) {
        AvpPtrVec *g = groups.v[it.val->i];
        size_t *ix = AVP_NEWN(a, size_t, g->n ? g->n : 1);
        for (size_t i = 0; i < g->n; i++) ix[i] = (size_t)(uintptr_t)g->v[i];
        avp_sort(ix, g->n, sizeof *ix, cmp_datum_addr, (void *)data, false);
        UpPads *p = &m->pads[k++];
        p->section = data[ix[0]].section;
        for (size_t i = 0; i + 1 < g->n; i++) {
            const UpDatum *x = &data[ix[i]], *y = &data[ix[i + 1]];
            if (x->address + x->size < y->address) avp_vec_push(&p->points, y->address);
        }
        avp_sort(p->points.v, p->points.n, sizeof *p->points.v, cmp_i64, NULL, false);
        avp_vec_free(g);
    }
    avp_vec_free(&groups);
    avp_map_free(by_sec);
    return m;
}

i64 up_model_index(const UpModel *m, i64 address) {
    size_t lo = 0, hi = m->n;
    while (lo < hi) { /* bisect_right */
        size_t mid = (lo + hi) / 2;
        if (address < m->fns[mid].address) hi = mid;
        else lo = mid + 1;
    }
    if (lo && m->fns[lo - 1].address == address) return (i64)lo - 1;
    return -1;
}

const UpDatum *up_model_datum(const UpModel *m, i64 address) {
    AvpVal *v = avp_imap_get(m->data_at, (u64)address);
    return v ? &m->data[v->i] : NULL;
}

/* ---- is_thunk, _flag_align ---- */

bool up_is_thunk(const AvpStr *mn, const AvpStr *ops, size_t n) {
    if (n < 2 || !avp_str_eqz(mn[n - 1], "b")) return false;
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    bool ok = true;
    for (size_t i = 0; ok && i + 1 < n; i++) {
        if (!avp_str_eqz(mn[i], "addi") && !avp_str_eqz(mn[i], "subi")) {
            ok = false;
            break;
        }
        AvpStrVec parts = avp_str_split(ops[i].p ? ops[i] : AVP_S(""), AVP_S(","), -1);
        ok = parts.n >= 2 && avp_str_eqz(avp_str_strip(parts.v[0]), "r3") && avp_str_eqz(avp_str_strip(parts.v[1]), "r3");
        avp_vec_free(&parts);
    }
    avp_arena_reset(s, mk);
    return ok;
}

i64 up_flag_align(const AvpStrVec *cflags) {
    for (size_t i = 0; i < cflags->n; i++) {
        if (!avp_str_eqz(cflags->v[i], "-func_align")) continue;
        if (i + 1 >= cflags->n) return 0;
        i64 v = 0;
        bool ovf = false;
        if (!avp_parse_int(cflags->v[i + 1], 10, &v, &ovf) || ovf)
            shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 10: %s",
                       avp_cz(avp_scratch(), avp_repr_str(avp_scratch(), cflags->v[i + 1])));
        return v;
    }
    for (size_t i = 0; i < cflags->n; i++)
        if (avp_str_startswith(cflags->v[i], AVP_S("-O4"))) return 16;
    return 0;
}

/* ---- class_groups ---- */

static bool json_truthy(const AvpJson *v) {
    if (!v) return false;
    switch (v->kind) {
    case AVP_JSON_NULL: return false;
    case AVP_JSON_BOOL: return v->b;
    case AVP_JSON_INT: return v->big || v->u.i != 0;
    case AVP_JSON_FLOAT: return v->u.f != 0.0;
    case AVP_JSON_STRING: return v->u.str.n != 0;
    case AVP_JSON_ARRAY: return v->u.arr.n != 0;
    default: return v->u.obj.n != 0;
    }
}

static const AvpJson *jget(const AvpJson *o, const char *k) {
    return o && o->kind == AVP_JSON_OBJECT ? avp_json_get(o, avp_str(k)) : NULL;
}

/* _int(a): int(a, 16) for a str, int(a) otherwise; false for None. */
static bool json_int(const AvpJson *v, i64 *out) {
    if (!v) return false;
    switch (v->kind) {
    case AVP_JSON_STRING: {
        bool ovf = false;
        return avp_parse_int(v->u.str, 16, out, &ovf) && !ovf;
    }
    case AVP_JSON_INT:
        if (v->big) return false;
        *out = v->u.i;
        return true;
    case AVP_JSON_FLOAT:
        if (!(v->u.f == v->u.f) || v->u.f > 9e18 || v->u.f < -9e18) return false;
        *out = (i64)v->u.f;
        return true;
    case AVP_JSON_BOOL: *out = v->b; return true;
    default: return false;
    }
}

static bool jstr_is(const AvpJson *v, const char *s) {
    return v && v->kind == AVP_JSON_STRING && avp_str_eqz(v->u.str, s);
}

typedef struct {
    AvpArena *a;
    const UpFunc *order;
    size_t n;
    AvpIMap *by_addr; /* address -> last index */
    const char *name;
} Mine;

static bool mine(const Mine *c, i64 addr) {
    AvpVal *v = avp_imap_get(c->by_addr, (u64)addr);
    if (!v) return false;
    const UpFunc *f = &c->order[v->i];
    if (f->scope && strcmp(f->scope, "weak") == 0) return false;
    const char *mc = up_mangled_class(c->a, f->name);
    return mc == NULL || strcmp(mc, c->name) == 0;
}

static bool vec_has(const AvpI64Vec *v, i64 x) {
    for (size_t i = 0; i < v->n; i++)
        if (v->v[i] == x) return true;
    return false;
}

UpClass *up_class_groups(AvpArena *a, const AvpJson *db, const UpFunc *fns, size_t n, size_t *nout) {
    UpFunc *order = AVP_NEWN(a, UpFunc, n ? n : 1);
    if (n) memcpy(order, fns, n * sizeof *fns);
    avp_sort(order, n, sizeof *order, cmp_fn_addr, NULL, false);
    AvpIMap *by_addr = avp_imap_new_cap(n);
    for (size_t i = 0; i < n; i++) avp_imap_put(by_addr, (u64)order[i].address, NULL)->i = (i64)i;
    AvpIMap *thunks_to = avp_imap_new(); /* target -> AvpI64Vec* */
    for (size_t i = 0; i < n; i++) {
        if (order[i].thunk == UP_NONE) continue;
        bool created;
        AvpVal *v = avp_imap_put(thunks_to, (u64)order[i].thunk, &created);
        if (created) v->p = AVP_NEW(a, AvpI64Vec);
        avp_vec_push((AvpI64Vec *)v->p, order[i].address);
    }
    AVP_VEC(UpClass) out = {0};
    const AvpJson *classes = jget(db, "classes");
    size_t ncl = classes && classes->kind == AVP_JSON_ARRAY ? classes->u.arr.n : 0;
    for (size_t ci = 0; ci < ncl; ci++) {
        const AvpJson *c = classes->u.arr.v[ci];
        const AvpJson *vt = jget(c, "vtable");
        if (!json_truthy(vt)) continue;
        const AvpJson *nm = jget(c, "name");
        if (!json_truthy(nm)) nm = jget(c, "key");
        const char *name = json_truthy(nm) && nm->kind == AVP_JSON_STRING ? avp_cz(a, nm->u.str) : "?";
        AvpI64Vec cand = {0};
        const AvpJson *words = jget(vt, "words");
        if (words && words->kind == AVP_JSON_ARRAY)
            for (size_t i = 0; i < words->u.arr.n; i++) {
                const AvpJson *w = words->u.arr.v[i];
                const AvpJson *st = jget(w, "status");
                i64 x;
                if ((jstr_is(st, "override") || jstr_is(st, "new")) && json_int(jget(w, "target_address"), &x))
                    avp_vec_push(&cand, x);
            }
        const AvpJson *members = jget(c, "members");
        if (members && members->kind == AVP_JSON_ARRAY)
            for (size_t i = 0; i < members->u.arr.n; i++) {
                const AvpJson *x = members->u.arr.v[i];
                i64 v;
                if (!jstr_is(jget(x, "role"), "slot") && json_int(jget(x, "address"), &v)) avp_vec_push(&cand, v);
            }
        Mine mc = {a, order, n, by_addr, name};
        AvpI64Vec own = {0};
        for (size_t i = 0; i < cand.n; i++)
            if (!vec_has(&own, cand.v[i]) && mine(&mc, cand.v[i])) avp_vec_push(&own, cand.v[i]);
        size_t base = own.n;
        for (size_t i = 0; i < base; i++) {
            AvpVal *tv = avp_imap_get(thunks_to, (u64)own.v[i]);
            if (!tv) continue;
            AvpI64Vec *ts = tv->p;
            for (size_t k = 0; k < ts->n; k++)
                if (!vec_has(&own, ts->v[k]) && mine(&mc, ts->v[k])) avp_vec_push(&own, ts->v[k]);
        }
        size_t with_thunks = own.n;
        for (size_t i = 0; i < n; i++) {
            if (!order[i].sinit || vec_has(&own, order[i].address)) continue;
            bool hit = false;
            for (size_t k = 0; !hit && k < order[i].taken.n; k++)
                for (size_t j = 0; !hit && j < with_thunks; j++) hit = order[i].taken.v[k] == own.v[j];
            if (hit) avp_vec_push(&own, order[i].address);
        }
        avp_vec_free(&cand);
        if (own.n < 2) {
            avp_vec_free(&own);
            continue;
        }
        i64 lo = INT64_MAX, hi = INT64_MIN;
        for (size_t i = 0; i < own.n; i++) {
            i64 ix = avp_imap_get(by_addr, (u64)own.v[i])->i;
            if (ix < lo) lo = ix;
            if (ix > hi) hi = ix;
        }
        if (hi - lo > UP_CLASS_SPAN) {
            avp_vec_free(&own);
            continue;
        }
        avp_sort(own.v, own.n, sizeof *own.v, cmp_i64, NULL, false);
        avp_vec_push(&out, ((UpClass){name, own}));
    }
    avp_imap_free(by_addr);
    avp_imap_free(thunks_to);
    *nout = out.n;
    UpClass *res = AVP_NEWN(a, UpClass, out.n ? out.n : 1);
    if (out.n) memcpy(res, out.v, out.n * sizeof *res);
    avp_vec_free(&out);
    return res;
}

/* ---- load_model ---- */

static const char *const DATA_SECTIONS[] = {".data", ".rodata", ".bss", ".sdata", ".sbss", ".sdata2", ".sbss2"};

static bool is_data_section(const char *s) {
    if (!s) return false;
    for (size_t i = 0; i < sizeof DATA_SECTIONS / sizeof *DATA_SECTIONS; i++)
        if (strcmp(s, DATA_SECTIONS[i]) == 0) return true;
    return false;
}

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static sqlite3_stmt *prep(sqlite3 *db, const char *q) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static const char *col_text(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return NULL;
    return avp_cz(a, avp_strn((const char *)sqlite3_column_text(st, c), (size_t)sqlite3_column_bytes(st, c)));
}

static AvpStr col_str(AvpArena *a, sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return (AvpStr){NULL, 0};
    const char *p = (const char *)sqlite3_column_text(st, c);
    size_t n = (size_t)sqlite3_column_bytes(st, c);
    char *q = avp_alloc(a, n + 1);
    memcpy(q, p, n);
    return avp_strn(q, n);
}

static i64 col_int(sqlite3_stmt *st, int c) {
    return sqlite3_column_type(st, c) == SQLITE_NULL ? UP_NONE : sqlite3_column_int64(st, c);
}

/* "%r" % value of a floats.value cell */
static const char *col_repr(AvpArena *a, sqlite3_stmt *st, int c) {
    switch (sqlite3_column_type(st, c)) {
    case SQLITE_NULL: return "None";
    case SQLITE_INTEGER: return avp_sprintf(a, "%lld", (long long)sqlite3_column_int64(st, c));
    case SQLITE_FLOAT: {
        char buf[32];
        avp_float_repr(sqlite3_column_double(st, c), buf);
        return avp_strdup(a, buf);
    }
    default: return avp_cz(a, avp_repr_str(a, avp_str(col_text(a, st, c))));
    }
}

typedef struct {
    AvpI64Vec refs, taken;
    AvpStrVec strings;
} Acc;

typedef struct {
    const char *name, *scope, *unit;
    i64 address, size;
} FnRow;

static bool name_like(AvpStr s) {
    ShsmReMatch mm;
    return shsm_re_search(shsm_re("(?:\\.(?:cpp|c|h|xaml)$)|^C[A-Z][A-Za-z0-9]*[a-z][A-Za-z0-9]*$", 0), s, 0, AVP_END,
                          &mm);
}

static void sort_unique(AvpI64Vec *v) {
    avp_sort(v->v, v->n, sizeof *v->v, cmp_i64, NULL, false);
    size_t k = 0;
    for (size_t i = 0; i < v->n; i++)
        if (!k || v->v[k - 1] != v->v[i]) v->v[k++] = v->v[i];
    v->n = k;
}

static int cmp_pair(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const i64 *a = x, *b = y;
    if (a[0] != b[0]) return a[0] < b[0] ? -1 : 1;
    return a[1] < b[1] ? -1 : a[1] > b[1];
}

UpModel *up_load_model(AvpArena *a, ShsmAsmIndex *ix, const ShsmRepo *r) {
    const ShsmSymbolTable *symtab = shsm_load_symbols(r, true);
    const ShsmUnits *us = shsm_load_units(r);
    sqlite3 *db = ix->db;
    /* data */
    AVP_VEC(UpDatum) data = {0};
    AvpIMap *data_at = avp_imap_new();
    sqlite3_stmt *st = prep(db, "select name, address, size, section, scope from symbols where kind = 'object'");
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *sec = col_text(a, st, 3);
        if (!is_data_section(sec)) continue;
        const char *name = col_text(a, st, 0);
        i64 addr = col_int(st, 1), size = col_int(st, 2);
        const char *scope = col_text(a, st, 4);
        const ShsmSymbol *sym = name ? shsm_sym_by_name(symtab, avp_str(name)) : NULL;
        UpDatum d = {addr, size == UP_NONE ? 0 : size, sec, scope && *scope ? scope : "global", {NULL, 0},
                     sym && sym->data && strcmp(sym->data, "string") == 0 ? UP_KIND_STRING : UP_KIND_OTHER};
        bool created;
        AvpVal *v = avp_imap_put(data_at, (u64)addr, &created);
        if (created) {
            v->i = (i64)data.n;
            avp_vec_push(&data, d);
        } else {
            data.v[v->i] = d;
        }
    }
    sqlite3_finalize(st);
    /* floats: a datum with exactly one float value */
    AvpIMap *fl = avp_imap_new(); /* owner -> count in i, first repr kept apart */
    AvpIMap *fl_val = avp_imap_new();
    st = prep(db, "select owner_addr, value from floats");
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (sqlite3_column_type(st, 0) == SQLITE_NULL) continue;
        u64 owner = (u64)sqlite3_column_int64(st, 0);
        bool created;
        avp_imap_put(fl, owner, &created)->i += 1;
        if (created) avp_imap_put(fl_val, owner, NULL)->p = (void *)col_repr(a, st, 1);
    }
    sqlite3_finalize(st);
    AvpIMapIter fit = {0};
    while (avp_imap_next(fl, &fit)) {
        AvpVal *dv = avp_imap_get(data_at, fit.key);
        if (dv && fit.val->i == 1) {
            data.v[dv->i].kind = UP_KIND_FLOAT;
            data.v[dv->i].value = avp_str(avp_imap_get(fl_val, fit.key)->p);
        }
    }
    avp_imap_free(fl);
    avp_imap_free(fl_val);
    /* strings */
    AvpIMap *text_at = avp_imap_new();
    st = prep(db, "select address, text from strings");
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (sqlite3_column_type(st, 0) == SQLITE_NULL) continue;
        u64 addr = (u64)sqlite3_column_int64(st, 0);
        AvpStr *text = AVP_NEW(a, AvpStr);
        *text = col_str(a, st, 1);
        avp_imap_put(text_at, addr, NULL)->p = text;
        AvpVal *dv = avp_imap_get(data_at, addr);
        if (dv) {
            data.v[dv->i].kind = UP_KIND_STRING;
            data.v[dv->i].value = *text;
        }
    }
    sqlite3_finalize(st);
    /* sinits */
    AvpIMap *sinits = avp_imap_new();
    ShsmTableEntries ct = shsm_asmindex_ctors(a, ix);
    for (size_t i = 0; i < ct.n; i++)
        if (ct.v[i].target_addr != SHSM_NONE) avp_imap_put(sinits, (u64)ct.v[i].target_addr, NULL);
    /* functions */
    AVP_VEC(FnRow) rows = {0};
    st = prep(db, "select name, address, size, scope, unit from symbols where kind = 'function'"
                  " and section = '.text' order by address");
    while (sqlite3_step(st) == SQLITE_ROW) {
        FnRow fr = {col_text(a, st, 0), col_text(a, st, 3), col_text(a, st, 4), col_int(st, 1), col_int(st, 2)};
        avp_vec_push(&rows, fr);
    }
    sqlite3_finalize(st);
    size_t nr = rows.n;
    i64 *starts = AVP_NEWN(a, i64, nr ? nr : 1), *ends = AVP_NEWN(a, i64, nr ? nr : 1);
    AvpIMap *local_fn = avp_imap_new(), *fn_set = avp_imap_new(), *end_of = avp_imap_new();
    for (size_t i = 0; i < nr; i++) {
        starts[i] = rows.v[i].address;
        ends[i] = rows.v[i].address + (rows.v[i].size == UP_NONE || rows.v[i].size == 0 ? 4 : rows.v[i].size);
        if (rows.v[i].scope && strcmp(rows.v[i].scope, "local") == 0) avp_imap_put(local_fn, (u64)starts[i], NULL);
        avp_imap_put(fn_set, (u64)starts[i], NULL);
        avp_imap_put(end_of, (u64)starts[i], NULL)->i = ends[i];
    }
#define FN_AT(addr, out)                                                                                               \
    do {                                                                                                               \
        size_t lo_ = 0, hi_ = nr;                                                                                      \
        while (lo_ < hi_) {                                                                                            \
            size_t mid_ = (lo_ + hi_) / 2;                                                                             \
            if ((addr) < starts[mid_]) hi_ = mid_;                                                                     \
            else lo_ = mid_ + 1;                                                                                       \
        }                                                                                                              \
        (out) = (lo_ && (addr) < ends[lo_ - 1]) ? starts[lo_ - 1] : UP_NONE;                                           \
    } while (0)
    AvpIMap *acc = avp_imap_new();
    AVP_VEC(i64) lc = {0}; /* pairs */
    st = prep(db, "select from_addr, to_base, to_addr, kind from refs where to_base is not null"
                  " and kind in ('call', 'branch', 'ha', 'l', 'h', 'sda21')");
    while (sqlite3_step(st) == SQLITE_ROW) {
        i64 frm = col_int(st, 0), tgt = col_int(st, 1), to = col_int(st, 2);
        const char *kind = (const char *)sqlite3_column_text(st, 3);
        if (frm == UP_NONE) continue;
        i64 src;
        FN_AT(frm, src);
        if (src == UP_NONE) continue;
        bool cb = strcmp(kind, "call") == 0 || strcmp(kind, "branch") == 0;
        if (cb) {
            if (avp_imap_has(local_fn, (u64)tgt) && tgt != src) {
                avp_vec_push(&lc, src);
                avp_vec_push(&lc, tgt);
            }
            continue;
        }
        bool created;
        if (avp_imap_has(data_at, (u64)tgt)) {
            AvpVal *v = avp_imap_put(acc, (u64)src, &created);
            if (created) v->p = AVP_NEW(a, Acc);
            Acc *ac = v->p;
            avp_vec_push(&ac->refs, tgt);
            AvpVal *tv = to == UP_NONE ? NULL : avp_imap_get(text_at, (u64)to);
            if (strcmp(kind, "ha") != 0 && tv && ((AvpStr *)tv->p)->p) {
                AvpStr s = *(AvpStr *)tv->p;
                if (name_like(s)) {
                    bool seen = false;
                    for (size_t k = 0; !seen && k < ac->strings.n; k++) seen = avp_str_eq(ac->strings.v[k], s);
                    if (!seen) avp_vec_push(&ac->strings, s);
                }
            }
        } else if ((strcmp(kind, "l") == 0 || strcmp(kind, "sda21") == 0) && avp_imap_has(fn_set, (u64)tgt)) {
            AvpVal *v = avp_imap_put(acc, (u64)src, &created);
            if (created) v->p = AVP_NEW(a, Acc);
            avp_vec_push(&((Acc *)v->p)->taken, tgt);
        }
    }
    sqlite3_finalize(st);
    /* this-adjust thunks */
    AvpIMap *branch_at = avp_imap_new();
    st = prep(db, "select from_addr, to_base from refs where kind = 'branch' and to_base is not null");
    while (sqlite3_step(st) == SQLITE_ROW) {
        i64 frm = col_int(st, 0), tgt = col_int(st, 1);
        if (frm == UP_NONE) continue;
        i64 src;
        FN_AT(frm, src);
        if (src == UP_NONE || src == tgt || !avp_imap_has(fn_set, (u64)tgt)) continue;
        i64 e = avp_imap_get(end_of, (u64)src)->i;
        if (e - src <= UP_THUNK_MAX && frm + 4 == e) avp_imap_put(branch_at, (u64)src, NULL)->i = tgt;
    }
    sqlite3_finalize(st);
#undef FN_AT
    AvpIMap *thunks = avp_imap_new();
    AvpIMapIter bit = {0};
    while (avp_imap_next(branch_at, &bit)) {
        ShsmAsmFile *file = NULL;
        ShsmAsmBlock *blk = shsm_asmindex_function(a, ix, shsm_target_addr((i64)bit.key), &file);
        if (blk) {
            size_t n = blk->rows.n;
            AvpStr *mn = AVP_NEWN(a, AvpStr, n ? n : 1), *ops = AVP_NEWN(a, AvpStr, n ? n : 1);
            for (size_t i = 0; i < n; i++) {
                mn[i] = blk->rows.v[i].mnemonic;
                ops[i] = blk->rows.v[i].operands;
            }
            if (up_is_thunk(mn, ops, n)) avp_imap_put(thunks, bit.key, NULL)->i = bit.val->i;
        }
        if (file) shsm_asm_free(file);
    }
    avp_imap_free(branch_at);
    /* the functions */
    const ShsmLibraries *libs = shsm_load_libraries(r);
    AVP_VEC(UpFunc) fns = {0};
    AVP_VEC(i64) align = {0};
    for (size_t i = 0; i < nr; i++) {
        FnRow *fr = &rows.v[i];
        if (fr->name && (strncmp(fr->name, "gap_", 4) == 0 || strncmp(fr->name, "pad_", 4) == 0)) continue;
        const ShsmUnit *u = fr->unit && *fr->unit ? shsm_units_lookup(us, shsm_unit_name(a, fr->unit)) : NULL;
        const char *lib = shsm_libraries_lookup(a, libs, fr->address);
        UpFunc f = {0};
        f.address = fr->address;
        f.size = fr->size == UP_NONE || fr->size == 0 ? 4 : fr->size;
        f.name = fr->name ? fr->name : "";
        f.scope = fr->scope && *fr->scope ? fr->scope : "global";
        f.sinit = avp_imap_has(sinits, (u64)fr->address);
        AvpVal *av = avp_imap_get(acc, (u64)fr->address);
        if (av) {
            Acc *ac = av->p;
            f.taken = ac->taken;
            sort_unique(&f.taken);
            f.data = ac->refs;
            sort_unique(&f.data);
            f.strings = ac->strings;
        }
        f.unit = u && shsm_unit_has_source(u) ? u->path : NULL;
        f.lib = lib && *lib ? lib : "unknown";
        AvpVal *tv = avp_imap_get(thunks, (u64)fr->address);
        f.thunk = tv ? tv->i : UP_NONE;
        avp_vec_push(&fns, f);
        if (u && u->has_cflags && u->cflags.n && up_flag_align(&u->cflags) == 16) {
            avp_vec_push(&align, fr->address);
            avp_vec_push(&align, 16);
        }
    }
    const char *note = NULL;
    const AvpJson *cdb = up_classes_load(a, r, &note);
    size_t nc = 0;
    UpClass *cls = json_truthy(cdb) ? up_class_groups(a, cdb, fns.v, fns.n, &nc) : NULL;
    avp_sort(lc.v, lc.n / 2, 2 * sizeof *lc.v, cmp_pair, NULL, false);
    size_t k = 0;
    for (size_t i = 0; i < lc.n / 2; i++)
        if (!k || lc.v[2 * (k - 1)] != lc.v[2 * i] || lc.v[2 * (k - 1) + 1] != lc.v[2 * i + 1]) {
            lc.v[2 * k] = lc.v[2 * i];
            lc.v[2 * k + 1] = lc.v[2 * i + 1];
            k++;
        }
    /* PS2 link order from the witness database; no database, or no pair, leaves has_ps2 false (no evidence) */
    WtOrderRank *ranks = NULL;
    size_t nranks = 0;
    if (wt_lookup_order(a, r, &ranks, &nranks)) {
        AvpIMap *rank_of = avp_imap_new();
        for (size_t q = 0; q < nranks; q++) {
            bool created;
            avp_imap_put(rank_of, (u64)ranks[q].wii, &created)->i = ranks[q].rank;
        }
        for (size_t i = 0; i < fns.n; i++) {
            AvpVal *rv = avp_imap_get(rank_of, (u64)fns.v[i].address);
            if (rv) {
                fns.v[i].has_ps2 = true;
                fns.v[i].ps2 = rv->i;
            }
        }
        avp_imap_free(rank_of);
    }
    UpModel *m = up_model_new(a, fns.v, fns.n, data.v, data.n, (const i64(*)[2])lc.v, k, (const i64(*)[2])align.v,
                              align.n / 2, cls, nc, note);
    avp_imap_free(data_at);
    avp_imap_free(text_at);
    avp_imap_free(sinits);
    avp_imap_free(local_fn);
    avp_imap_free(fn_set);
    avp_imap_free(end_of);
    avp_imap_free(acc);
    avp_imap_free(thunks);
    avp_vec_free(&rows);
    avp_vec_free(&fns);
    avp_vec_free(&data);
    avp_vec_free(&lc);
    avp_vec_free(&align);
    return m;
}

/* ---- library ranges ---- */

UpLibRange up_find_library(AvpArena *a, const char *lib, const ShsmRepo *r, const void *libsp) {
    const ShsmLibraries *libs = libsp;
    if (!libs) {
        AvpTry t;
        if (AVP_TRY(&t)) {
            libs = shsm_load_libraries(r);
            avp_try_end(&t);
        } else {
            if (t.code == SHSM_EXC_VALUE) shsm_raise(SHSM_EXC_USAGE, "%s", t.msg);
            avp_throw(t.code, "%s", t.msg);
        }
    }
    AvpStr low = avp_str_strip_chars(avp_str_lower(a, avp_str(lib)), AVP_S("/"));
    for (int test = 0; test < 3; test++) {
        AVP_VEC(const ShsmLibRow *) rows = {0};
        for (size_t i = 0; i < libs->rows.n; i++) {
            const ShsmLibRow *row = &libs->rows.v[i];
            AvpStr n = avp_str_lower(a, row->library);
            bool hit;
            if (test == 0) hit = avp_str_eq(n, low);
            else if (test == 1) {
                AvpStrVec parts = avp_str_rsplit(n, AVP_S("/"), 1);
                hit = avp_str_eq(parts.v[parts.n - 1], low);
                avp_vec_free(&parts);
            } else hit = avp_str_eq(shsm_library_group_of(avp_cz(a, n)), low);
            if (hit) avp_vec_push(&rows, row);
        }
        if (!rows.n) continue;
        UpLibRange res = {0};
        for (size_t i = 0; i < rows.n; i++) {
            bool seen = false;
            for (size_t k = 0; !seen && k < res.names.n; k++) seen = avp_str_eq(res.names.v[k], rows.v[i]->library);
            if (!seen) avp_vec_push(&res.names, rows.v[i]->library);
        }
        for (size_t i = 0; i + 1 < rows.n; i++)
            if (rows.v[i + 1]->start != rows.v[i]->end) {
                AvpStr joined = avp_str_join(a, AVP_S(", "), res.names.v, res.names.n);
                shsm_raise(SHSM_EXC_USAGE, "%s names %zu libraries that are not contiguous: %s",
                           avp_cz(a, avp_repr_str(a, avp_str(lib))), res.names.n, avp_cz(a, joined));
            }
        res.start = rows.v[0]->start;
        res.end = rows.v[rows.n - 1]->end;
        avp_vec_free(&rows);
        return res;
    }
    shsm_raise(SHSM_EXC_USAGE, "no library of %s matches %s", libs->path ? libs->path : "libraries.tsv",
               avp_cz(a, avp_repr_str(a, avp_str(lib))));
}

void up_parse_range(AvpArena *a, const char *text, i64 *start, i64 *end) {
    ShsmReMatch mm;
    AvpStr t = avp_str(text);
    if (!shsm_re_fullmatch(shsm_re("(?:0x)?([0-9A-Fa-f]{1,8})\\s*-\\s*(?:0x)?([0-9A-Fa-f]{1,8})", 0), t, 0, AVP_END,
                           &mm))
        shsm_raise(SHSM_EXC_USAGE, "range must be START-END in hex, got %s", avp_cz(a, avp_repr_str(a, t)));
    i64 s = 0, e = 0;
    bool ovf;
    avp_parse_int(shsm_re_group(&mm, 1), 16, &s, &ovf);
    avp_parse_int(shsm_re_group(&mm, 2), 16, &e, &ovf);
    if (e <= s) shsm_raise(SHSM_EXC_USAGE, "empty range %s", avp_cz(a, avp_repr_str(a, t)));
    *start = s;
    *end = e;
}
