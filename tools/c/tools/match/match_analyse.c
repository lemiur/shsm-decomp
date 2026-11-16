/* match: one unit (tools/match.py analyse_unit, row_percent, _record), summaries, exit codes and changes. */
#include "match.h"

#include "shsm_mangle.h"

/* ---- helper threads (the Python's rules and objdiff threads) --------------------------------------------------- */

typedef struct {
    const ShsmRepo *r;
    const char *unit, *file;
    AvpArena *arena;
    AvpJson *result; /* NULL: the thread raised */
    AvpThread *thread;
} RulesJob;

static void rules_thread(void *arg) {
    RulesJob *j = arg;
    AvpTry t;
    if (AVP_TRY(&t)) {
        j->result = match_run_rules(j->arena, j->r, j->unit, j->file);
        avp_try_end(&t);
    } else {
        j->result = NULL;
    }
}

typedef struct {
    const ShsmRepo *r;
    const char *target, *obj, *work;
    AvpMap *pct; /* NULL: no objdiff figures */
    AvpThread *thread;
} OdJob;

static void od_thread(void *arg) {
    OdJob *j = arg;
    AvpTry t;
    if (AVP_TRY(&t)) {
        j->pct = match_objdiff_percents(j->r, j->target, j->obj, j->work);
        avp_try_end(&t);
    } else {
        j->pct = NULL;
    }
}

/* ---- records ------------------------------------------------------------------------------------------------------ */

static double row_percent(const CcCompareResult *c) {
    size_t n = AVP_MAX(c->counts.retail_rows, c->counts.compiled_rows);
    if (!n) n = 1;
    size_t good = 0;
    for (size_t i = 0; i < c->npairs; i++)
        if (!cc_pair_differs(&c->pairs[i])) good++;
    return 100.0 * (double)good / (double)n;
}

static AvpJson *record(AvpArena *a, AvpStr name, const char *status, const AvpStr *retail_name,
                       const CcCompareResult *cres, i64 size, bool has_address, i64 address, const double *pct,
                       const char *pct_source) {
    char *namez = avp_cz(a, name);
    AvpJson *rec = avp_json_obj(a);
    avp_json_set(a, rec, AVP_S("name"), avp_json_str(a, name));
    avp_json_set(a, rec, AVP_S("retail_name"), retail_name ? avp_json_str(a, *retail_name) : avp_json_null());
    avp_json_set(a, rec, AVP_S("demangled"), avp_json_strz(a, shsm_demangle_or_name(a, namez)));
    avp_json_set(a, rec, AVP_S("status"), avp_json_strz(a, status));
    i64 rows = 0, targets = 0, unresolved = 0;
    if (cres) {
        rows = (i64)(cres->counts.replace + cres->counts.delete_ + cres->counts.insert);
        targets = (i64)cres->counts.target_mismatches;
        unresolved = (i64)cres->counts.unresolved;
    }
    avp_json_set(a, rec, AVP_S("rows"), match_ji(a, rows));
    avp_json_set(a, rec, AVP_S("targets"), match_ji(a, targets));
    avp_json_set(a, rec, AVP_S("unresolved"), match_ji(a, unresolved));
    avp_json_set(a, rec, AVP_S("percent"), pct ? match_jf(a, *pct) : avp_json_null());
    avp_json_set(a, rec, AVP_S("percent_source"), match_jstr(a, pct_source));
    avp_json_set(a, rec, AVP_S("size"), match_ji(a, size));
    avp_json_set(a, rec, AVP_S("address"), has_address ? match_ji(a, address) : avp_json_null());
    if (cres && cres->raw && cres->raw->notes.n) {
        AvpJson *vr = avp_json_arr(a);
        for (size_t i = 0; i < cres->raw->notes.n; i++) avp_json_arr_push(a, vr, avp_json_str(a, cres->raw->notes.v[i].detail));
        avp_json_set(a, rec, AVP_S("value_rows"), vr);
    }
    return rec;
}

typedef struct {
    AvpStr name;
    const CcFn *fn;
} Named;

static int by_start(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const Named *)x)->fn->start, ((const Named *)y)->fn->start);
}

static AvpJson *seconds_obj(AvpArena *a, double compile, double compare, double total) {
    AvpJson *s = avp_json_obj(a);
    avp_json_set(a, s, AVP_S("compile"), match_jf(a, avp_py_round(compile, 3)));
    avp_json_set(a, s, AVP_S("compare"), match_jf(a, avp_py_round(compare, 3)));
    avp_json_set(a, s, AVP_S("total"), match_jf(a, avp_py_round(total, 3)));
    return s;
}

/* The body of analyse_unit after the work directory exists (the Python's try block). */
static void analyse_body(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *source, const char *work,
                         const MatchAnalyseOpts *o, AvpJson *result, double t0, AvpMap **cmp_out, OdJob *od,
                         RulesJob *rj) {
    const char *obj;
    double t_compile;
    if (shsm_unit_is_asm(u) && !o->built) {
        /* an assembly source unit: ninja assembles it with build.ninja's own rule; its listing is compared as for C */
        MatchCompiled comp = match_assemble_unit(a, r, u);
        t_compile = comp.seconds;
        avp_json_set(a, result, AVP_S("cached"), avp_json_bool(false));
        if (!comp.ok) {
            avp_json_set(a, result, AVP_S("ok"), avp_json_bool(false));
            AvpStrVec lines = match_first_error_lines(a, comp.output, MATCH_ERROR_LINES);
            AvpJson *err = avp_json_arr(a);
            for (size_t i = 0; i < lines.n; i++) avp_json_arr_push(a, err, avp_json_str(a, lines.v[i]));
            if (!lines.n) avp_json_arr_push(a, err, avp_json_strz(a, "assembler failed with no output"));
            avp_vec_free(&lines);
            avp_json_set(a, result, AVP_S("error"), err);
            avp_json_set(a, result, AVP_S("seconds"), seconds_obj(a, t_compile, 0.0, avp_time() - t0));
            return;
        }
        obj = comp.obj;
    } else if (o->built) {
        if (!u->base_obj || !avp_exists(u->base_obj))
            shsm_raise(SHSM_EXC_USAGE, "no built object %s (run ninja)", u->base_obj ? u->base_obj : u->name);
        obj = u->base_obj; /* read only; its listing goes to the work directory */
        t_compile = 0.0;
    } else {
        const char *extra[2];
        size_t nextra = 0;
        if (o->file) {
            const char *src_dir = avp_cz(a, avp_path_dirname(avp_path_join(a, r->root, u->source ? u->source : "")));
            if (u->source && strcmp(avp_path_normcase(a, avp_cz(a, avp_path_dirname(source))),
                                    avp_path_normcase(a, src_dir)) != 0) {
                char *rel = NULL;
                if (!avp_path_relpath(a, src_dir, r->root, &rel)) shsm_raise_io();
                extra[0] = "-i";
                extra[1] = rel;
                nextra = 2;
            }
        }
        MatchCompiled comp = match_compile_unit(a, r, u, source, work, extra, nextra, o->use_cache);
        t_compile = comp.seconds;
        avp_json_set(a, result, AVP_S("cached"), avp_json_bool(comp.cached));
        if (!comp.ok) {
            avp_json_set(a, result, AVP_S("ok"), avp_json_bool(false));
            AvpStrVec lines = match_first_error_lines(a, comp.output, MATCH_ERROR_LINES);
            AvpJson *err = avp_json_arr(a);
            for (size_t i = 0; i < lines.n; i++) avp_json_arr_push(a, err, avp_json_str(a, lines.v[i]));
            if (!lines.n) avp_json_arr_push(a, err, avp_json_strz(a, "compiler failed with no output"));
            avp_vec_free(&lines);
            avp_json_set(a, result, AVP_S("error"), err);
            avp_json_set(a, result, AVP_S("seconds"), seconds_obj(a, t_compile, 0.0, avp_time() - t0));
            return;
        }
        obj = comp.obj;
        avp_json_set(a, result, AVP_S("inputs"), o->file ? avp_json_null() : match_state_inputs(a, r, u, source, comp.deps));
    }
    double t1 = avp_time();
    od->r = r;
    od->target = u->target_obj;
    od->obj = obj;
    od->work = work;
    od->thread = avp_thread_start(od_thread, od, 0);
    AvpMap *compiled = match_load_listing(a, r, obj, o->use_cache, work);
    AvpSet *wanted = NULL;
    if (o->nsymbols) {
        wanted = avp_set_new();
        for (size_t i = 0; i < o->nsymbols; i++) avp_set_add(wanted, o->symbols[i]);
    }
    CcRepo cr = {r->root, r->version};
    CcError err = {0};
    AvpStr uname = avp_str(u->name);
    const AvpMap *retail = cc_load_retail(&cr, uname, NULL, false, &err); /* gap_ padding blocks left out */
    if (!retail) match_raise_cc(&err);
    AvpSet *only = NULL;
    AvpMapIter it = {0};
    if (wanted) {
        only = avp_set_new();
        while (avp_map_next(compiled, &it)) {
            AvpStr rn = cc_retail_name_for(it.key, retail);
            if (avp_set_has(wanted, it.key) || (rn.p && avp_set_has(wanted, rn))) avp_set_add(only, it.key);
        }
    }
    CcObjectComparison *oc = cc_compare_object(a, &cr, uname, obj, only, true, NULL, NULL, compiled, NULL, NULL, &err);
    if (!oc) match_raise_cc(&err);
    AvpMap *cmp = oc->results;
    /* One compiled function per retail function: the one of the same name, else the first in the object (another
     * with the same fn_<addr> prefix is reported as EXTRA). */
    AvpMap *by_retail = avp_map_new();
    it.pos = 0;
    while (avp_map_next(compiled, &it)) {
        AvpVal *cv = avp_map_get(cmp, it.key);
        if (!cv) continue;
        const CcCompareResult *c = cv->p;
        AvpVal *bv = avp_map_get(by_retail, c->retail_name);
        if (!bv || avp_str_eq(it.key, c->retail_name)) avp_map_put(by_retail, c->retail_name, NULL)->p = (void *)&c->name;
    }
    AvpSet *extra = avp_set_new();
    for (size_t i = 0; i < oc->compiled_only.n; i++) avp_set_add(extra, oc->compiled_only.v[i]);
    it.pos = 0;
    while (avp_map_next(compiled, &it)) {
        AvpVal *cv = avp_map_get(cmp, it.key);
        if (!cv) continue;
        const CcCompareResult *c = cv->p;
        const AvpStr *owner = avp_map_get(by_retail, c->retail_name)->p;
        if (!avp_str_eq(*owner, it.key)) avp_set_add(extra, it.key);
    }
    avp_thread_join(od->thread);
    od->thread = NULL;
    AvpMap *pcts = od->pct;
    const char *psrc = pcts ? "objdiff" : "rows";
    avp_json_set(a, result, AVP_S("percent_source"), avp_json_strz(a, psrc));
    AvpSet *retail_only = avp_set_new();
    for (size_t i = 0; i < oc->retail_only.n; i++) avp_set_add(retail_only, oc->retail_only.v[i]);
    size_t nret = avp_map_len(oc->retail);
    Named *rs = AVP_NEWN(a, Named, nret + 1);
    size_t k = 0;
    it.pos = 0;
    while (avp_map_next(oc->retail, &it)) rs[k++] = (Named){it.key, it.val->p};
    avp_sort(rs, k, sizeof *rs, by_start, NULL, false);
    AvpJson *recs = avp_json_arr(a);
    for (size_t i = 0; i < k; i++) {
        AvpStr rname = rs[i].name;
        AvpVal *bv = avp_map_get(by_retail, rname);
        const AvpStr *cname = bv ? bv->p : NULL;
        if (!cname && !avp_set_has(retail_only, rname)) continue; /* paired, but outside a symbol restriction */
        if (wanted && !avp_set_has(wanted, rname) && (!cname || !avp_set_has(wanted, *cname))) continue;
        i64 size = (i64)rs[i].fn->nrows * 4;
        if (!cname) {
            double zero = 0.0;
            avp_json_arr_push(a, recs, record(a, rname, MATCH_MISSING, &rname, NULL, size, true, rs[i].fn->start, &zero, psrc));
            continue;
        }
        const CcCompareResult *cres = avp_map_get(cmp, *cname)->p;
        double pct;
        AvpVal *pv = pcts ? avp_map_get(pcts, *cname) : NULL;
        pct = pv ? pv->f : row_percent(cres);
        pct = avp_py_round(pct, 4);
        avp_json_arr_push(a, recs, record(a, *cname, cres->exact ? MATCH_EXACT : MATCH_DIFF, &rname, cres, size, true,
                                          rs[i].fn->start, &pct, psrc));
    }
    it.pos = 0;
    while (avp_map_next(compiled, &it)) {
        if (!avp_set_has(extra, it.key) || (wanted && !avp_set_has(wanted, it.key))) continue;
        const CcFn *cfn = it.val->p;
        avp_json_arr_push(a, recs, record(a, it.key, MATCH_EXTRA, NULL, NULL, (i64)cfn->nrows * 4, false, 0, NULL, NULL));
    }
    if (pcts) avp_map_free(pcts);
    avp_json_set(a, result, AVP_S("functions"), recs);
    double t_cmp = avp_time() - t1;
    if (o->data) avp_json_set(a, result, AVP_S("layout"), match_run_layout(a, r, u->name, obj));
    if (rj->thread) {
        avp_thread_join(rj->thread);
        rj->thread = NULL;
        AvpJson *rr;
        if (rj->result) rr = match_json_copy(a, rj->result);
        else {
            rr = avp_json_obj(a);
            avp_json_set(a, rr, AVP_S("ok"), avp_json_bool(false));
            avp_json_set(a, rr, AVP_S("pending"), avp_json_obj(a));
            avp_json_set(a, rr, AVP_S("findings"), avp_json_arr(a));
            avp_json_set(a, rr, AVP_S("error"), avp_json_strz(a, "rule checker failed"));
        }
        avp_json_set(a, result, AVP_S("rules"), rr);
        const AvpJson *pending = avp_json_getz(rr, "pending");
        for (size_t i = 0; i < avp_json_len(recs); i++) {
            AvpJson *rec = avp_json_at(recs, i);
            if (!avp_str_eqz(avp_json_get_str(rec, "status", AVP_S("")), MATCH_EXACT)) continue;
            const char *q = shsm_qualified(a, avp_cz(a, avp_json_get_str(rec, "name", AVP_S(""))));
            AvpJson *rules = avp_json_get(pending, avp_str(q));
            if (!rules) continue;
            avp_json_set(a, rec, AVP_S("status"), avp_json_strz(a, MATCH_PENDING));
            avp_json_set(a, rec, AVP_S("match_rules"), rules);
        }
    }
    avp_json_set(a, result, AVP_S("seconds"), seconds_obj(a, t_compile, t_cmp, avp_time() - t0));
    if (cmp_out) *cmp_out = cmp;
}

static void finish(const MatchAnalyseOpts *o, AvpArena *a, AvpJson *result, const char *work, OdJob *od, RulesJob *rj) {
    if (od->thread) {
        avp_thread_join(od->thread);
        od->thread = NULL;
        if (od->pct) avp_map_free(od->pct);
    }
    if (rj->thread) {
        avp_thread_join(rj->thread);
        rj->thread = NULL;
    }
    if (rj->arena) {
        avp_arena_free(rj->arena);
        rj->arena = NULL;
    }
    if (o->keep) avp_json_set(a, result, AVP_S("workdir"), avp_json_strz(a, work));
    else {
        avp_rmtree(work, AVP_RMTREE_IGNORE_ERRORS);
        avp_io_clear();
    }
}

AvpJson *match_analyse_unit(AvpArena *a, const ShsmRepo *r, const char *unit, const MatchAnalyseOpts *o, AvpMap **cmp) {
    double t0 = avp_time();
    if (cmp) *cmp = NULL;
    const ShsmUnit *u = shsm_get_unit(r, unit);
    if (!u->source && !o->file) shsm_raise(SHSM_EXC_USAGE, "%s has no source (asm unit)", u->name);
    if (o->file && shsm_unit_is_asm(u))
        shsm_raise(SHSM_EXC_USAGE, "--file does not apply to %s: an assembly source unit (%s) is assembled by ninja",
                   u->name, u->source);
    const char *source = o->file ? avp_path_abspath(a, o->file) : avp_path_join(a, r->root, u->source);
    if (!avp_exists(source)) shsm_raise(SHSM_EXC_USAGE, "source %s does not exist", source);
    AvpJson *result = avp_json_obj(a);
    avp_json_set(a, result, AVP_S("unit"), avp_json_strz(a, u->name));
    const char *shown;
    if (o->built) {
        if (!u->base_obj) shsm_raise(SHSM_EXC_TYPE, "expected str, bytes or os.PathLike object, not NoneType");
        shown = shsm_repo_rel(a, r, u->base_obj);
    } else {
        shown = shsm_repo_rel(a, r, source);
    }
    avp_json_set(a, result, AVP_S("source"), avp_json_strz(a, shown));
    avp_json_set(a, result, AVP_S("status"), match_jstr(a, u->status));
    avp_json_set(a, result, AVP_S("library"), match_jstr(a, u->library));
    avp_json_set(a, result, AVP_S("ok"), avp_json_bool(true));
    avp_json_set(a, result, AVP_S("error"), avp_json_arr(a));
    avp_json_set(a, result, AVP_S("restricted"), avp_json_bool(o->nsymbols > 0));
    avp_json_set(a, result, AVP_S("functions"), avp_json_arr(a));
    avp_json_set(a, result, AVP_S("cached"), avp_json_bool(false));
    avp_json_set(a, result, AVP_S("percent_source"), avp_json_null());
    avp_json_set(a, result, AVP_S("layout"), avp_json_null());
    avp_json_set(a, result, AVP_S("rules"), avp_json_null());
    avp_json_set(a, result, AVP_S("inputs"), avp_json_null());
    const char *base = avp_join(a, match_dir(a, r), "work");
    if (!avp_makedirs(base, true)) shsm_raise_io();
    AvpStr prefix = avp_str_replace(a, avp_str(shsm_unit_path(a, u->name)), AVP_S("/"), AVP_S("__"), -1);
    char *work = avp_mkdtemp(a, avp_sprintf(a, "%.*s-", AVP_SF(prefix)), "", base);
    if (!work) shsm_raise_io();
    OdJob od = {0};
    RulesJob rj = {0};
    if (o->rules) {
        rj.r = r;
        rj.unit = u->name;
        rj.file = o->file;
        rj.arena = avp_arena_new(0);
        rj.thread = avp_thread_start(rules_thread, &rj, 0);
    }
    AvpTry t;
    if (AVP_TRY(&t)) {
        analyse_body(a, r, u, source, work, o, result, t0, cmp, &od, &rj);
        avp_try_end(&t);
    } else {
        finish(o, a, result, work, &od, &rj);
        avp_throw(t.code, "%s", t.msg);
    }
    finish(o, a, result, work, &od, &rj);
    return result;
}

/* ---- summaries ------------------------------------------------------------------------------------------------- */

int match_status_index(AvpStr s) {
    static const char *const names[] = {MATCH_EXACT, MATCH_DIFF, MATCH_MISSING, MATCH_EXTRA, MATCH_PENDING};
    for (int i = 0; i < MATCH_S_COUNT; i++)
        if (avp_str_eqz(s, names[i])) return i;
    return -1;
}

static double pct_or0(const AvpJson *rec) {
    const AvpJson *p = avp_json_getz(rec, "percent");
    return avp_json_is_num(p) ? avp_json_f64(p, 0.0) : 0.0;
}

MatchSummary match_summarise(const AvpJson *result) {
    MatchSummary s = {0};
    const AvpJson *recs = avp_json_getz(result, "functions");
    double worst = 0.0;
    for (size_t i = 0; avp_json_is_arr(recs) && i < avp_json_len(recs); i++) {
        const AvpJson *rec = avp_json_at(recs, i);
        int k = match_status_index(avp_json_get_str(rec, "status", AVP_S("")));
        if (k >= 0) s.counts[k]++;
        if (k == MATCH_S_EXTRA) continue;
        s.total++;
        if (k == MATCH_S_EXACT) continue;
        double p = pct_or0(rec);
        if (!s.worst || p < worst) {
            s.worst = rec;
            worst = p;
        }
    }
    s.exact = s.counts[MATCH_S_EXACT];
    const AvpJson *lay = avp_json_getz(result, "layout");
    s.layout_bad = !avp_json_is_null(lay) && !avp_str_eqz(avp_json_get_str(lay, "status", AVP_S("")), "ok");
    s.matched = avp_json_truthy(avp_json_getz(result, "ok")) && s.exact == s.total && !s.layout_bad;
    return s;
}

int match_exit_code(AvpJson *const *results, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (!avp_json_truthy(avp_json_getz(results[i], "ok"))) return SHSM_EXIT_ERROR;
    for (size_t i = 0; i < n; i++)
        if (!match_summarise(results[i]).matched) return SHSM_EXIT_FINDING;
    return SHSM_EXIT_OK;
}

/* ---- changes since the last run ---------------------------------------------------------------------------------- */

static double num_or(const AvpJson *rec, const char *key, double dflt) {
    const AvpJson *v = avp_json_getz(rec, key);
    return avp_json_is_num(v) ? avp_json_f64(v, dflt) : dflt;
}

size_t match_changes(AvpArena *a, const AvpJson *old, const AvpJson *records, bool restricted, MatchChange **out) {
    const AvpJson *old_f = old ? avp_json_getz(old, "functions") : NULL;
    if (!avp_json_is_obj(old_f)) old_f = NULL;
    AVP_VEC(MatchChange) v = {0};
    AvpSet *seen = avp_set_new();
    static const char *const keys[] = {"status", "rows", "targets", "percent", "unresolved"};
    for (size_t i = 0; avp_json_is_arr(records) && i < avp_json_len(records); i++) {
        const AvpJson *rec = avp_json_at(records, i);
        AvpStr name = avp_json_get_str(rec, "name", AVP_S(""));
        avp_set_add(seen, name);
        const AvpJson *o = old_f ? avp_json_get(old_f, name) : NULL;
        if (!o) {
            avp_vec_push(&v, ((MatchChange){"new", name, NULL, rec}));
            continue;
        }
        bool same = true;
        for (size_t k = 0; k < AVP_ARRAY_LEN(keys) && same; k++)
            same = match_json_eq(avp_json_getz(o, keys[k]), avp_json_getz(rec, keys[k]));
        if (same) continue;
        bool was = avp_str_eqz(avp_json_get_str(o, "status", AVP_S("")), MATCH_EXACT);
        bool now = avp_str_eqz(avp_json_get_str(rec, "status", AVP_S("")), MATCH_EXACT);
        const char *kind;
        if (now && !was) kind = "newly exact";
        else if (was && !now) kind = "newly broken";
        else {
            double op = num_or(o, "percent", 0.0), np = num_or(rec, "percent", 0.0);
            double ob = num_or(o, "rows", 0) + num_or(o, "targets", 0), nb = num_or(rec, "rows", 0) + num_or(rec, "targets", 0);
            if (np > op || (np == op && nb < ob)) kind = "improved";
            else if (np < op || (np == op && nb > ob)) kind = "regressed";
            else kind = "changed";
        }
        avp_vec_push(&v, ((MatchChange){kind, name, o, rec}));
    }
    if (!restricted && old_f)
        for (size_t i = 0; i < old_f->u.obj.n; i++)
            if (!avp_set_has(seen, old_f->u.obj.v[i].key))
                avp_vec_push(&v, ((MatchChange){"gone", old_f->u.obj.v[i].key, old_f->u.obj.v[i].val, NULL}));
    avp_set_free(seen);
    MatchChange *res = AVP_NEWN(a, MatchChange, v.n + 1);
    if (v.n) memcpy(res, v.v, v.n * sizeof *res);
    size_t n = v.n;
    avp_vec_free(&v);
    *out = res;
    return n;
}
