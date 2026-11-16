/* unit_plan.py: the disk cache (_stamp, load_cached; a C-native cache file, D-unit_plan-3), plan_for and
 * warn_classes. */
#include "unit_plan.h"
#include "witness_lookup.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define UP_CACHE_KIND "unit_plan"
#define UP_CACHE_SCHEMA 2

const char *up_cache_path(AvpArena *a, const ShsmRepo *r) { return avp_path_join(a, r->index_dir, "unit_plan.cbin"); }

/* _stamp(r, ix): the index's listings, the symbol, split, library and unit tables, the class database, and the
 * tool's own sources (avp_tools_stamp, for the Python's own file). */
static AvpStr stamp_of(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix) {
    AvpBuf b;
    avp_buf_init(&b, a);
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(ix->db, "select count(*), max(mtime_ns), sum(size) from files", -1, &st, NULL) != SQLITE_OK)
        shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(ix->db));
    if (sqlite3_step(st) == SQLITE_ROW)
        for (int c = 0; c < 3; c++) {
            if (sqlite3_column_type(st, c) == SQLITE_NULL) avp_buf_putz(&b, "None,");
            else avp_buf_printf(&b, "%lld,", (long long)sqlite3_column_int64(st, c));
        }
    sqlite3_finalize(st);
    return up_stamp_inputs(a, r, avp_cz(a, avp_buf_str(&b)));
}

AvpStr up_cache_stamp(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix) { return stamp_of(a, r, ix); }

AvpStr up_stamp_inputs(AvpArena *a, const ShsmRepo *r, const char *files_row) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "unit_plan/%d|%s", UP_CACHE_SCHEMA, files_row);
    const char *paths[] = {r->symbols_txt, r->splits_txt,  r->libraries_tsv,
                           r->objdiff_json, r->configure_py, r->build_ninja, up_classes_json_path(a, r),
                           r->index_dir ? wt_db_path(a, r) : NULL};
    for (size_t i = 0; i < sizeof paths / sizeof *paths; i++) {
        AvpStat s;
        if (avp_stat(paths[i], &s)) avp_buf_printf(&b, "|%lld,%lld", (long long)s.size, (long long)s.mtime_ns);
        else avp_buf_putz(&b, "|None");
    }
    avp_buf_printf(&b, "|%s", avp_tools_stamp());
    return avp_buf_str(&b);
}

/* ---- the model through the cache format ---- */

static void w_opt(AvpCacheWriter *w, const char *s) {
    avp_cw_bool(w, s != NULL);
    if (s) avp_cw_cz(w, s);
}

static const char *r_opt(AvpArena *a, AvpCacheReader *rd) {
    if (!avp_cr_bool(rd)) return NULL;
    return avp_cz(a, avp_cr_str(rd));
}

static void w_i64s(AvpCacheWriter *w, const AvpI64Vec *v) {
    avp_cw_count(w, v->n);
    for (size_t i = 0; i < v->n; i++) avp_cw_i64(w, v->v[i]);
}

static AvpI64Vec r_i64s(AvpCacheReader *rd) {
    AvpI64Vec v = {0};
    u32 n = avp_cr_count(rd, 8);
    for (u32 i = 0; i < n; i++) avp_vec_push(&v, avp_cr_i64(rd));
    return v;
}

void up_model_write(AvpCacheWriter *w, const UpModel *m) {
    avp_cw_count(w, m->n);
    for (size_t i = 0; i < m->n; i++) {
        const UpFunc *f = &m->fns[i];
        avp_cw_i64(w, f->address);
        avp_cw_i64(w, f->size);
        avp_cw_cz(w, f->name);
        avp_cw_cz(w, f->scope);
        avp_cw_bool(w, f->sinit);
        w_i64s(w, &f->taken);
        w_i64s(w, &f->data);
        w_opt(w, f->unit);
        avp_cw_cz(w, f->lib);
        avp_cw_count(w, f->strings.n);
        for (size_t k = 0; k < f->strings.n; k++) avp_cw_str(w, f->strings.v[k]);
        avp_cw_i64(w, f->thunk);
        avp_cw_bool(w, f->has_ps2);
        avp_cw_i64(w, f->ps2);
    }
    avp_cw_count(w, m->ndata);
    for (size_t i = 0; i < m->ndata; i++) {
        const UpDatum *d = &m->data[i];
        avp_cw_i64(w, d->address);
        avp_cw_i64(w, d->size);
        avp_cw_cz(w, d->section);
        avp_cw_cz(w, d->scope);
        avp_cw_bool(w, d->value.p != NULL);
        if (d->value.p) avp_cw_str(w, d->value);
        avp_cw_u8(w, (u8)d->kind);
    }
    avp_cw_count(w, m->nlocal);
    for (size_t i = 0; i < m->nlocal; i++) {
        avp_cw_i64(w, m->local_calls[i][0]);
        avp_cw_i64(w, m->local_calls[i][1]);
    }
    avp_cw_count(w, avp_imap_len(m->align));
    AvpIMapIter it = {0};
    while (avp_imap_next(m->align, &it)) {
        avp_cw_i64(w, (i64)it.key);
        avp_cw_i64(w, it.val->i);
    }
    avp_cw_count(w, m->nclasses);
    for (size_t i = 0; i < m->nclasses; i++) {
        avp_cw_cz(w, m->classes[i].name);
        w_i64s(w, &m->classes[i].addrs);
    }
    w_opt(w, m->class_note);
}

UpModel *up_model_read(AvpArena *a, AvpCacheReader *rd) {
    AVP_VEC(UpFunc) fns = {0};
    u32 n = avp_cr_count(rd, 16);
    for (u32 i = 0; i < n && avp_cr_ok(rd); i++) {
        UpFunc f = {0};
        f.address = avp_cr_i64(rd);
        f.size = avp_cr_i64(rd);
        f.name = avp_cz(a, avp_cr_str(rd));
        f.scope = avp_cz(a, avp_cr_str(rd));
        f.sinit = avp_cr_bool(rd);
        f.taken = r_i64s(rd);
        f.data = r_i64s(rd);
        f.unit = r_opt(a, rd);
        f.lib = avp_cz(a, avp_cr_str(rd));
        u32 ns = avp_cr_count(rd, 4);
        for (u32 k = 0; k < ns; k++) {
            AvpStr s = avp_cr_str(rd);
            char *q = avp_alloc(a, s.n + 1);
            if (s.n) memcpy(q, s.p, s.n);
            avp_vec_push(&f.strings, avp_strn(q, s.n));
        }
        f.thunk = avp_cr_i64(rd);
        f.has_ps2 = avp_cr_bool(rd);
        f.ps2 = avp_cr_i64(rd);
        avp_vec_push(&fns, f);
    }
    AVP_VEC(UpDatum) data = {0};
    u32 nd = avp_cr_count(rd, 16);
    for (u32 i = 0; i < nd && avp_cr_ok(rd); i++) {
        UpDatum d = {0};
        d.address = avp_cr_i64(rd);
        d.size = avp_cr_i64(rd);
        d.section = avp_cz(a, avp_cr_str(rd));
        d.scope = avp_cz(a, avp_cr_str(rd));
        if (avp_cr_bool(rd)) {
            AvpStr v = avp_cr_str(rd);
            char *q = avp_alloc(a, v.n + 1);
            if (v.n) memcpy(q, v.p, v.n);
            d.value = avp_strn(q, v.n);
        }
        d.kind = (UpKind)avp_cr_u8(rd);
        avp_vec_push(&data, d);
    }
    AVP_VEC(i64) lc = {0}, al = {0};
    u32 nl = avp_cr_count(rd, 16);
    for (u32 i = 0; i < 2 * nl && avp_cr_ok(rd); i++) avp_vec_push(&lc, avp_cr_i64(rd));
    u32 na = avp_cr_count(rd, 16);
    for (u32 i = 0; i < 2 * na && avp_cr_ok(rd); i++) avp_vec_push(&al, avp_cr_i64(rd));
    AVP_VEC(UpClass) cls = {0};
    u32 nc = avp_cr_count(rd, 8);
    for (u32 i = 0; i < nc && avp_cr_ok(rd); i++) {
        UpClass c = {0};
        c.name = avp_cz(a, avp_cr_str(rd));
        c.addrs = r_i64s(rd);
        avp_vec_push(&cls, c);
    }
    const char *note = r_opt(a, rd);
    if (!avp_cr_ok(rd) || avp_cr_remaining(rd)) return NULL;
    UpModel *m = up_model_new(a, fns.v, fns.n, data.v, data.n, (const i64(*)[2])lc.v, lc.n / 2,
                              (const i64(*)[2])al.v, al.n / 2, cls.v, cls.n, note);
    avp_vec_free(&fns);
    avp_vec_free(&data);
    avp_vec_free(&lc);
    avp_vec_free(&al);
    avp_vec_free(&cls);
    return m;
}

UpState *up_load_cached(AvpArena *a, const ShsmRepo *r, ShsmAsmIndex *ix, bool build) {
    AvpStr stamp = stamp_of(a, r, ix);
    const char *path = up_cache_path(a, r);
    UpModel *m = NULL;
    AvpCache c;
    if (avp_cache_open(&c, path, UP_CACHE_KIND, UP_CACHE_SCHEMA, stamp.p, stamp.n)) {
        m = up_model_read(a, &c.r);
        avp_cache_close(&c);
    }
    if (!m) {
        if (!build) return NULL;
        m = up_load_model(a, ix, r);
        AvpCacheWriter w;
        avp_cw_init(&w);
        up_model_write(&w, m);
        if (avp_makedirs(r->index_dir, true))
            (void)avp_cache_write_w(path, UP_CACHE_KIND, UP_CACHE_SCHEMA, stamp.p, stamp.n, &w);
        avp_cw_free(&w);
    }
    UpState *s = AVP_NEW(a, UpState);
    s->m = m;
    s->ev = up_gap_evidence(a, m);
    s->lab = up_truth_splits(a, m, r, NULL);
    s->calib = up_evaluate(a, m, &s->ev, s->lab).calib;
    return s;
}

/* ---- warn_classes, plan_for ---- */

void up_warn_classes(const UpModel *m) {
    static AvpSet *warned;
    if (!m->class_note) return;
    if (!warned) warned = avp_set_new();
    if (avp_set_has(warned, avp_str(m->class_note))) return;
    avp_set_add(warned, avp_str(m->class_note));
    fflush(stdout);
    fprintf(stderr, "unit_plan.exe: warning: %s\n", m->class_note);
    fflush(stderr);
}

UpPlanDoc *up_plan_for(AvpArena *a, i64 start, i64 end, const ShsmRepo *r, const char *min_conf, ShsmAsmIndex *ix,
                       bool build, bool refresh) {
    if (!r) r = shsm_repo_find(NULL);
    bool own = ix == NULL;
    if (own) ix = shsm_asmindex_open(&(ShsmAsmIndexOpts){.repo = r, .readonly = true, .no_refresh = !refresh});
    UpState *s = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        s = up_load_cached(a, r, ix, build);
        avp_try_end(&t);
    } else {
        if (own) shsm_asmindex_close(ix);
        avp_throw(t.code, "%s", t.msg);
    }
    if (own) shsm_asmindex_close(ix);
    if (!s) return NULL;
    up_warn_classes(s->m);
    double min_p = up_min_p(min_conf);
    if (min_p < 0) shsm_raise(SHSM_EXC_KEY, "%s", min_conf);
    UpPlanDoc *d = AVP_NEW(a, UpPlanDoc);
    d->start = start;
    d->end = end;
    d->accuracy = s->calib;
    d->header = avp_cz(a, up_accuracy_header(a, s->calib));
    d->units = up_plan(a, s->m, start, end, s->calib, &s->ev, min_p, s->lab);
    d->class_note = s->m->class_note;
    return d;
}

AvpJson *up_plan_doc_json(AvpArena *a, const UpPlanDoc *d) {
    AvpJson *o = avp_json_obj(a);
    AvpJson *rng = avp_json_arr(a);
    avp_json_arr_push(a, rng, avp_json_int(a, d->start));
    avp_json_arr_push(a, rng, avp_json_int(a, d->end));
    avp_json_set(a, o, AVP_S("range"), rng);
    avp_json_set(a, o, AVP_S("accuracy"), up_calib_tag_json(a, d->accuracy));
    avp_json_set(a, o, AVP_S("header"), avp_json_strz(a, d->header));
    AvpJson *units = avp_json_arr(a);
    for (size_t i = 0; i < d->units.n; i++) avp_json_arr_push(a, units, up_unit_json(a, &d->units.v[i]));
    avp_json_set(a, o, AVP_S("units"), units);
    avp_json_set(a, o, AVP_S("class_note"), d->class_note ? avp_json_strz(a, d->class_note) : avp_json_null());
    return o;
}
