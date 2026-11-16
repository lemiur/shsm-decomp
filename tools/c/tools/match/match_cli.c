/* match: the command line (tools/match.py _worker, select_units, run_single, run_multi, run, build_parser). */
#include "match.h"

/* str(v) of a JSON value as Python's %s gives it. */
static AvpStr py_str(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    if (v->kind == AVP_JSON_STRING) return v->u.str;
    if (v->kind == AVP_JSON_BOOL) return v->b ? AVP_S("True") : AVP_S("False");
    if (v->kind == AVP_JSON_FLOAT) return avp_repr_f64(a, v->u.f);
    return avp_json_dumps(a, v, NULL);
}

static void out_lines(ShsmOutput *out, AvpStrVec *v) {
    shsm_output_lines(out, v->v, v->n);
    avp_vec_free(v);
}

/* out.data(payload): merge the members of an object into the --json document. */
static void out_data(ShsmOutput *out, const AvpJson *payload) {
    for (size_t i = 0; i < payload->u.obj.n; i++) {
        const AvpJsonMember *m = &payload->u.obj.v[i];
        shsm_output_data(out, avp_cz(avp_arena(), m->key), m->val);
    }
}

/* ---- one unit of a multi-unit run ---------------------------------------------------------------------------------- */

typedef struct {
    const ShsmRepo *r;
    MatchAnalyseOpts opts;
} MultiCtx;

static AvpJson *error_result(AvpArena *a, const char *unit, int code, const char *msg) {
    AvpStr s = shsm_exc_str(a, code, msg);
    AvpStr first = AVP_S("");
    if (s.n) {
        AvpStrVec ls = avp_str_splitlines(s, false);
        first = ls.n ? ls.v[0] : AVP_S("");
        avp_vec_free(&ls);
    } else {
        first = avp_str(shsm_exc_class(code));
    }
    AvpJson *res = avp_json_obj(a);
    avp_json_set(a, res, AVP_S("unit"), avp_json_strz(a, shsm_unit_name(a, unit)));
    avp_json_set(a, res, AVP_S("ok"), avp_json_bool(false));
    AvpJson *err = avp_json_arr(a);
    avp_json_arr_push(a, err, avp_json_str(a, first));
    avp_json_set(a, res, AVP_S("error"), err);
    avp_json_set(a, res, AVP_S("functions"), avp_json_arr(a));
    AvpJson *sec = avp_json_obj(a);
    avp_json_set(a, sec, AVP_S("compile"), match_jf(a, 0.0));
    avp_json_set(a, sec, AVP_S("compare"), match_jf(a, 0.0));
    avp_json_set(a, sec, AVP_S("total"), match_jf(a, 0.0));
    avp_json_set(a, res, AVP_S("seconds"), sec);
    avp_json_set(a, res, AVP_S("status"), avp_json_null());
    avp_json_set(a, res, AVP_S("percent_source"), avp_json_null());
    avp_json_set(a, res, AVP_S("layout"), avp_json_null());
    avp_json_set(a, res, AVP_S("rules"), avp_json_null());
    return res;
}

/* _worker: the result of one unit; one unit's failure must not sink the table. The analysis runs on its own arena;
 * the result is copied to the thread's long-lived one. */
static int multi_worker(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(index);
    MultiCtx *m = ctx;
    const char *unit = *(const char *const *)item;
    AvpArena *scratch = avp_arena_new(0);
    AvpJson *out = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        AvpJson *res = match_analyse_unit(scratch, m->r, unit, &m->opts, NULL);
        out = match_json_copy(avp_arena(), res);
        avp_try_end(&t);
    } else {
        out = error_result(avp_arena(), unit, t.code, t.msg);
    }
    avp_arena_free(scratch);
    *(AvpJson **)result = out;
    return 0;
}

/* ---- selecting units ----------------------------------------------------------------------------------------------- */

static void split_set(AvpStr text, AvpSet *set) {
    AvpStrVec parts = avp_str_split(text, AVP_S(","), -1);
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr s = avp_str_strip(parts.v[i]);
        if (s.n) avp_set_add(set, s);
    }
    avp_vec_free(&parts);
}

/* --changed with no unit: every unit with a source whose source file or recorded header is newer than the unit's last
 * comparison (its state file, written by every run) or last build (its object), the later of the two. A unit that was
 * never compared nor built has nothing to be newer than and is left out. */
static void select_changed_units(AvpArena *a, const ShsmRepo *r, AvpCzVec *chosen) {
    const ShsmUnits *all = shsm_load_units(r);
    ShsmDeps *deps = shsm_deps_load(a, r->root);
    for (size_t i = 0; i < all->n; i++) {
        const ShsmUnit *u = all->v[i];
        if (!u->source) continue;
        double since = 0, m;
        bool have = false;
        if (avp_getmtime(match_state_path(a, r, u->name), &m)) since = m, have = true;
        if (u->base_obj && avp_getmtime(u->base_obj, &m) && (!have || m > since)) since = m, have = true;
        if (!have) continue;
        const char *obj_rel = u->base_obj ? shsm_repo_rel(a, r, u->base_obj) : "";
        if (shsm_deps_newer_input(a, deps, r->root, obj_rel, u->source, NULL, 0, since))
            avp_vec_push(chosen, (char *)u->name);
    }
}

/* True when the run is `--changed` alone: the units are then chosen by select_changed_units (possibly none). */
static bool changed_all_mode(const AvpArgs *args) {
    return avp_args_bool(args, "changed") && !avp_args_len(args, "targets") && !avp_args_str(args, "status").n &&
           !avp_args_str(args, "lib").n;
}

static void select_units(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, AvpCzVec *chosen, AvpStrVec *syms) {
    if (changed_all_mode(args)) {
        select_changed_units(a, r, chosen);
        return;
    }
    const ShsmUnits *all = shsm_load_units(r);
    AvpStrVec targets = avp_args_strs(args, "targets");
    for (size_t i = 0; i < targets.n; i++) {
        const char *tz = avp_cz(a, targets.v[i]);
        AvpTry t;
        if (AVP_TRY(&t)) {
            const ShsmUnit *u = shsm_get_unit(r, tz);
            avp_vec_push(chosen, (char *)u->name);
            avp_try_end(&t);
        } else {
            if (!shsm_exc_is_key(t.code)) avp_throw(t.code, "%s", t.msg);
            avp_vec_push(syms, targets.v[i]);
        }
    }
    avp_vec_free(&targets);
    AvpStr status = avp_args_str(args, "status"), lib = avp_args_str(args, "lib");
    bool any_diff = avp_args_len(args, "diff") > 0;
    if (status.n || lib.n) {
        AvpSet *statuses = avp_set_new(), *libs = avp_set_new();
        split_set(status, statuses);
        split_set(lib, libs);
        size_t nchosen = chosen->n;
        for (size_t i = 0; i < all->n; i++) {
            const ShsmUnit *u = all->v[i];
            if (!u->source) continue;
            bool dup = false;
            for (size_t k = 0; k < nchosen && !dup; k++) dup = strcmp(chosen->v[k], u->name) == 0;
            if (dup) continue;
            if (avp_set_len(statuses) && !(u->status && avp_set_has(statuses, avp_str(u->status)))) continue;
            if (avp_set_len(libs) && !(u->library && avp_set_has(libs, avp_str(u->library)))) continue;
            avp_vec_push(chosen, (char *)u->name);
        }
        avp_set_free(statuses);
        avp_set_free(libs);
        if (!chosen->n) shsm_raise(SHSM_EXC_USAGE, "no unit with source matches --status/--lib");
    }
    if (!chosen->n) {
        if (syms->n) shsm_raise(SHSM_EXC_USAGE, "unknown unit %.*s", AVP_SF(syms->v[0]));
        shsm_raise(SHSM_EXC_USAGE, "name a unit (or --status / --lib)");
    }
    if (chosen->n > 1 && (syms->n || any_diff)) {
        AvpStr extra;
        if (syms->n) extra = avp_str_join(a, AVP_S(", "), syms->v, syms->n);
        else {
            AvpStrVec rest = {0};
            for (size_t i = 1; i < chosen->n; i++) avp_vec_push(&rest, avp_str(chosen->v[i]));
            extra = avp_str_join(a, AVP_S(", "), rest.v, rest.n);
            avp_vec_free(&rest);
        }
        shsm_raise(SHSM_EXC_USAGE, "symbols and --diff need exactly one unit (unknown unit or extra units: %.*s)",
                   AVP_SF(extra));
    }
}

/* ---- one unit -------------------------------------------------------------------------------------------------------- */

static int cmp_str(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_str_cmp(*(const AvpStr *)x, *(const AvpStr *)y);
}

static AvpJson *change_json(AvpArena *a, const MatchChange *c) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, c->kind));
    avp_json_set(a, o, AVP_S("name"), avp_json_str(a, c->name));
    avp_json_set(a, o, AVP_S("old"), c->old ? (AvpJson *)c->old : avp_json_null());
    avp_json_set(a, o, AVP_S("new"), c->new_ ? (AvpJson *)c->new_ : avp_json_null());
    return o;
}

static AvpStr pct_strip(AvpArena *a, const AvpJson *rec) { return avp_str_strip(match_pct(a, rec)); }

static int run_single(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const char *unit, const AvpStrVec *syms,
                      ShsmOutput *out) {
    AvpStr file = avp_args_str(args, "file");
    bool built = avp_args_bool(args, "built"), changed = avp_args_bool(args, "changed");
    bool show_all = avp_args_bool(args, "all");
    if (file.p && built) shsm_raise(SHSM_EXC_USAGE, "--file and --built exclude each other");
    AvpJson *old = changed ? match_load_state(a, r, unit) : NULL;
    MatchAnalyseOpts o = {0};
    o.file = file.p ? avp_cz(a, file) : NULL;
    o.built = built;
    o.use_cache = !avp_args_bool(args, "no_cache");
    o.data = avp_args_bool(args, "data");
    o.rules = avp_args_bool(args, "rules");
    o.keep = avp_args_bool(args, "keep");
    o.symbols = syms->v;
    o.nsymbols = syms->n;
    AvpMap *cmp = NULL;
    AvpJson *result = match_analyse_unit(a, r, unit, &o, &cmp);
    const char *stale = match_stale_asm_line(a, r, unit);
    if (stale) shsm_output_linez(out, stale);
    AvpJson *payload = avp_json_obj(a);
    AvpJson *units = avp_json_arr(a);
    avp_json_arr_push(a, units, result);
    avp_json_set(a, payload, AVP_S("units"), units);
    AvpStr uname = avp_json_get_str(result, "unit", AVP_S(""));
    if (!avp_json_truthy(avp_json_getz(result, "ok"))) {
        shsm_output_line(out, avp_pyfmt(a, "%S: compile failed:", &uname));
        const AvpJson *err = avp_json_getz(result, "error");
        for (size_t i = 0; i < avp_json_len(err); i++) {
            AvpStr ln = avp_json_str_or(avp_json_at(err, i), AVP_S(""));
            shsm_output_line(out, avp_pyfmt(a, "  %S", &ln));
        }
        shsm_output_line(out, match_summary_line(a, result));
        out_data(out, payload);
        return SHSM_EXIT_ERROR;
    }
    const AvpJson *recs = avp_json_getz(result, "functions");
    size_t nrecs = avp_json_len(recs);
    if (syms->n) {
        AvpSet *known = avp_set_new();
        for (size_t i = 0; i < nrecs; i++) {
            const AvpJson *rec = avp_json_at(recs, i);
            avp_set_add(known, avp_json_get_str(rec, "name", AVP_S("")));
            const AvpJson *rn = avp_json_getz(rec, "retail_name");
            if (avp_json_truthy(rn)) avp_set_add(known, rn->u.str);
        }
        for (size_t i = 0; i < syms->n; i++)
            if (!avp_set_has(known, syms->v[i]))
                shsm_raise(SHSM_EXC_USAGE, "%.*s is not a function of %.*s (neither side has it)", AVP_SF(syms->v[i]),
                           AVP_SF(uname));
        avp_set_free(known);
    }
    i64 limit = avp_args_int(args, "limit");
    size_t ndiff = avp_args_len(args, "diff");
    if (changed) {
        MatchChange *moved = NULL;
        size_t nmoved = old ? match_changes(a, old, recs, avp_json_truthy(avp_json_getz(result, "restricted")), &moved) : 0;
        AvpJson *ch = avp_json_arr(a);
        for (size_t i = 0; i < nmoved; i++) avp_json_arr_push(a, ch, change_json(a, &moved[i]));
        avp_json_set(a, payload, AVP_S("changes"), ch);
        if (!old) {
            shsm_output_line(out, avp_pyfmt(a, "(no previous run of %S recorded; showing this one)", &uname));
            AvpStrVec ls = match_unit_lines(a, result, show_all, limit);
            out_lines(out, &ls);
        } else if (!nmoved) {
            shsm_output_linez(out, "nothing moved since the last run");
        } else {
            for (size_t i = 0; i < nmoved; i++) {
                const MatchChange *c = &moved[i];
                AvpStr was = AVP_S("-"), now = AVP_S("-"), rt = AVP_S("");
                if (c->old) {
                    AvpStr st = py_str(a, avp_json_getz(c->old, "status")), p = pct_strip(a, c->old);
                    was = avp_pyfmt(a, "%S %S", &st, &p);
                }
                if (c->new_) {
                    AvpStr st = py_str(a, avp_json_getz(c->new_, "status")), p = pct_strip(a, c->new_);
                    now = avp_pyfmt(a, "%S %S", &st, &p);
                }
                if (c->old && c->new_) {
                    AvpStr r0 = py_str(a, avp_json_getz(c->old, "rows")), r1 = py_str(a, avp_json_getz(c->new_, "rows"));
                    AvpStr t0 = py_str(a, avp_json_getz(c->old, "targets"));
                    AvpStr t1 = py_str(a, avp_json_getz(c->new_, "targets"));
                    rt = avp_pyfmt(a, " (rows %S -> %S, tgt %S -> %S)", &r0, &r1, &t0, &t1);
                }
                shsm_output_line(out, avp_pyfmt(a, "%-13s %S: %S -> %S%S", c->kind, &c->name, &was, &now, &rt));
            }
        }
    } else if (!ndiff) {
        AvpStrVec ls = match_unit_lines(a, result, show_all, limit);
        out_lines(out, &ls);
    }
    const AvpJson *rr = avp_json_getz(result, "rules");
    if (avp_json_truthy(rr)) {
        if (!avp_json_truthy(avp_json_getz(rr, "ok"))) {
            AvpStr e = py_str(a, avp_json_getz(rr, "error"));
            shsm_output_line(out, avp_pyfmt(a, "rules: %S", &e));
        } else {
            const AvpJson *note = avp_json_getz(rr, "r13_note");
            if (avp_json_truthy(note)) {
                AvpStr n = py_str(a, note);
                shsm_output_line(out, avp_pyfmt(a, "%s (%S)", MATCH_R13_UNCHECKED, &n));
            }
            const AvpJson *f = avp_json_getz(rr, "findings");
            AvpStrVec fl = {0};
            for (size_t i = 0; i < avp_json_len(f); i++) avp_vec_push(&fl, py_str(a, avp_json_at(f, i)));
            AvpStrVec capped = shsm_cap_lines(a, fl.v, fl.n, limit, show_all, "use --all");
            avp_vec_free(&fl);
            out_lines(out, &capped);
        }
    }
    const AvpJson *lay = avp_json_getz(result, "layout");
    if (avp_json_truthy(lay) && (!avp_str_eqz(avp_json_get_str(lay, "status", AVP_S("")), "ok") || o.data)) {
        const AvpJson *ll = avp_json_getz(lay, "lines");
        AvpStrVec v = {0};
        for (size_t i = 0; i < avp_json_len(ll); i++) {
            AvpStr s = py_str(a, avp_json_at(ll, i));
            avp_vec_push(&v, avp_pyfmt(a, "layout: %S", &s));
        }
        AvpStrVec capped = shsm_cap_lines(a, v.v, v.n, show_all ? 0 : 12, show_all, "use --all");
        avp_vec_free(&v);
        out_lines(out, &capped);
    }
    AvpJson *diffs = NULL;
    for (size_t di = 0; di < ndiff; di++) {
        AvpStr q = avp_args_item(args, "diff", di)->s;
        AvpSet *set = avp_set_new();
        AvpMapIter it = {0};
        while (cmp && avp_map_next(cmp, &it)) {
            avp_set_add(set, it.key);
            avp_set_add(set, ((const CcCompareResult *)it.val->p)->retail_name);
        }
        for (size_t i = 0; i < nrecs; i++) avp_set_add(set, avp_json_get_str(avp_json_at(recs, i), "name", AVP_S("")));
        AvpStrVec names = avp_set_sorted(set);
        AvpStr name = match_resolve_symbol(a, q, names.v, names.n);
        avp_vec_free(&names);
        avp_set_free(set);
        const CcCompareResult *cres = NULL;
        AvpVal *cv = cmp ? avp_map_get(cmp, name) : NULL;
        if (cv) cres = cv->p;
        it.pos = 0;
        while (!cres && cmp && avp_map_next(cmp, &it))
            if (avp_str_eq(((const CcCompareResult *)it.val->p)->retail_name, name)) cres = it.val->p;
        const AvpJson *rec = NULL;
        for (size_t i = 0; i < nrecs && !rec; i++) {
            AvpStr rn = avp_json_get_str(avp_json_at(recs, i), "name", AVP_S(""));
            if (avp_str_eq(rn, name) || (cres && avp_str_eq(rn, cres->name))) rec = avp_json_at(recs, i);
        }
        if (!cres) {
            AvpStr st = rec ? avp_json_get_str(rec, "status", AVP_S("")) : AVP_S("unknown");
            shsm_output_line(out, avp_pyfmt(a, "--- %S: %S, no listing to compare", &name, &st));
            continue;
        }
        AvpStrVec dl = match_diff_lines(a, cres, rec, avp_args_int(args, "context"), avp_args_bool(args, "all_rows"),
                                        limit, show_all);
        out_lines(out, &dl);
        if (!diffs) {
            diffs = avp_json_arr(a);
            avp_json_set(a, payload, AVP_S("diffs"), diffs);
        }
        avp_json_arr_push(a, diffs, cc_compare_result_to_json(a, cres, true));
    }
    shsm_output_line(out, match_summary_line(a, result));
    const char *wd = match_jget_cz(a, result, "workdir");
    if (wd) shsm_output_line(out, avp_pyfmt(a, "kept: %s", shsm_repo_rel(a, r, wd)));
    match_save_state(a, r, unit, result);
    out_data(out, payload);
    return match_exit_code(&result, 1);
}

/* ---- several units ----------------------------------------------------------------------------------------------------- */

typedef struct {
    AvpJson *r;
    int a;
    double b, c;
} Ranked;

static int by_badness(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const Ranked *p = x, *q = y;
    if (p->a != q->a) return p->a < q->a ? -1 : 1;
    if (p->b != q->b) return p->b < q->b ? -1 : 1;
    if (p->c != q->c) return p->c < q->c ? -1 : 1;
    return 0;
}

static int run_multi(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const AvpCzVec *chosen, ShsmOutput *out) {
    if (avp_args_str(args, "file").p) shsm_raise(SHSM_EXC_USAGE, "--file needs exactly one unit");
    double t0 = avp_time();
    MultiCtx m = {0};
    m.r = r;
    m.opts.use_cache = !avp_args_bool(args, "no_cache");
    m.opts.data = avp_args_bool(args, "data");
    m.opts.rules = avp_args_bool(args, "rules");
    m.opts.built = avp_args_bool(args, "built");
    i64 jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    if (!jobs) jobs = shsm_default_jobs();
    bool changed = avp_args_bool(args, "changed");
    size_t n = chosen->n;
    AvpJson **olds = AVP_NEWN(a, AvpJson *, n + 1);
    if (changed)
        for (size_t i = 0; i < n; i++) olds[i] = match_load_state(a, r, chosen->v[i]);
    AvpJson **results = AVP_NEWN(a, AvpJson *, n + 1);
    if (jobs <= 1 || n <= 1) {
        for (size_t i = 0; i < n; i++) multi_worker(&m, i, &chosen->v[i], &results[i]);
    } else {
        avp_pool_map(NULL, (int)AVP_MIN((i64)n, jobs), multi_worker, &m, chosen->v, n, sizeof(char *), results,
                     sizeof(AvpJson *));
    }
    Ranked *rk = AVP_NEWN(a, Ranked, n + 1);
    for (size_t i = 0; i < n; i++) {
        rk[i].r = results[i];
        if (!avp_json_truthy(avp_json_getz(results[i], "ok"))) {
            rk[i].a = -1;
            continue;
        }
        MatchSummary s = match_summarise(results[i]);
        rk[i].a = s.matched ? 1 : 0;
        rk[i].b = s.total ? (double)s.exact / (double)s.total : 1.0;
        const AvpJson *wp = s.worst ? avp_json_getz(s.worst, "percent") : NULL;
        rk[i].c = avp_json_is_num(wp) ? avp_json_f64(wp, 0.0) : 100.0;
    }
    avp_sort(rk, n, sizeof *rk, by_badness, NULL, false);
    AvpStrVec rows = {0};
    i64 n_ok = 0, n_failed = 0, fexact = 0, ftotal = 0;
    AvpJson *units = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        AvpJson *res = rk[i].r;
        avp_json_arr_push(a, units, res);
        AvpStr unit = avp_json_get_str(res, "unit", AVP_S(""));
        const AvpJson *stv = avp_json_getz(res, "status");
        AvpStr status = avp_json_truthy(stv) ? stv->u.str : AVP_S("?");
        if (!avp_json_truthy(avp_json_getz(res, "ok"))) {
            n_failed++;
            const AvpJson *err = avp_json_getz(res, "error");
            AvpStr e0 = avp_json_len(err) ? py_str(a, avp_json_at(err, 0)) : AVP_S("");
            avp_vec_push(&rows, avp_pyfmt(a, "%-11S %-9s %-44S %S", &status, "ERROR", &unit, &e0));
        } else {
            MatchSummary s = match_summarise(res);
            if (s.matched) n_ok++;
            fexact += s.exact;
            ftotal += s.total;
            AvpStr worst = AVP_S("");
            if (s.worst) {
                AvpStr p = pct_strip(a, s.worst), wn = avp_json_get_str(s.worst, "name", AVP_S(""));
                worst = avp_pyfmt(a, "%S %S", &p, &wn);
            }
            AvpStrVec flags = {0};
            if (s.layout_bad) {
                AvpStr ls = py_str(a, avp_json_getz(avp_json_getz(res, "layout"), "status"));
                avp_vec_push(&flags, avp_pyfmt(a, "layout %S", &ls));
            }
            if (s.counts[MATCH_S_PENDING]) avp_vec_push(&flags, avp_pyfmt(a, "%d pending", s.counts[MATCH_S_PENDING]));
            AvpStr fl = AVP_S("");
            if (flags.n) {
                AvpStr j = avp_str_join(a, AVP_S(", "), flags.v, flags.n);
                fl = avp_pyfmt(a, "  [%S]", &j);
            }
            avp_vec_free(&flags);
            avp_vec_push(&rows, avp_pyfmt(a, "%-11S %4d/%-4d %-44S %S%S", &status, s.exact, s.total, &unit, &worst, &fl));
            if (changed) {
                const AvpJson *old = NULL;
                for (size_t k = 0; k < n; k++)
                    if (avp_str_eqz(unit, chosen->v[k])) {
                        old = olds[k];
                        break;
                    }
                MatchChange *moved = NULL;
                size_t nmoved = old ? match_changes(a, old, avp_json_getz(res, "functions"), false, &moved) : 0;
                for (size_t k = 0; k < nmoved; k++) {
                    AvpStr was = moved[k].old ? pct_strip(a, moved[k].old) : AVP_S("-");
                    AvpStr now = moved[k].new_ ? pct_strip(a, moved[k].new_) : AVP_S("-");
                    avp_vec_push(&rows, avp_pyfmt(a, "    %-13s %S %S -> %S", moved[k].kind, &moved[k].name, &was, &now));
                }
            }
        }
        match_save_state(a, r, avp_cz(a, unit), res);
    }
    AvpStrVec capped = shsm_cap_lines(a, rows.v, rows.n, avp_args_int(args, "limit"), avp_args_bool(args, "all"),
                                      "use --all");
    avp_vec_free(&rows);
    out_lines(out, &capped);
    shsm_output_line(out, avp_pyfmt(a, "%d units: %d fully exact, %d failed to compile; functions %d/%d exact; %.1fs with -j %d",
                                    (i64)n, n_ok, n_failed, fexact, ftotal, avp_time() - t0, jobs));
    shsm_output_data(out, "units", units);
    shsm_output_data(out, "seconds", match_jf(a, avp_py_round(avp_time() - t0, 3)));
    return match_exit_code(results, n);
}

/* ---- run --------------------------------------------------------------------------------------------------------------- */

static int run(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    AvpStr root = avp_args_str(args, "root");
    const ShsmRepo *r = shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
    AvpCzVec chosen = {0};
    AvpStrVec syms = {0};
    select_units(a, args, r, &chosen, &syms);
    ShsmOutput *out = shsm_output_open(args, 0, -1, -1);
    int rc;
    bool all_changed = changed_all_mode(args);
    if (all_changed) {
        shsm_output_line(out, avp_pyfmt(a, "--changed: %d units have a source or header newer than their last run or build",
                                        (i64)chosen.n));
        if (!chosen.n) {
            shsm_output_close(out);
            return 0;
        }
    }
    if (!all_changed && chosen.n == 1 && !avp_args_str(args, "status").n && !avp_args_str(args, "lib").n)
        rc = run_single(a, args, r, chosen.v[0], &syms, out);
    else
        rc = run_multi(a, args, r, &chosen, out);
    shsm_output_close(out);
    return rc;
}

static AvpArgParser *build_parser(void) {
    AvpArgParser *ap = shsm_cli_parser(
        "match.exe", "Compile a unit privately and compare every function with retail (the matching inner loop).", true,
        true, true);
    avp_arg(ap, &(AvpArgDesc){.names = "targets", .nargs = AVP_NARGS_STAR, .metavar = "UNIT|SYMBOL",
                              .help = "unit (main/game/PadRumble, game/PadRumble or the source path), then optional "
                                      "symbols"});
    avp_arg(ap, &(AvpArgDesc){.names = "--diff", .action = AVP_ARG_APPEND, .metavar = "SYMBOL",
                              .help = "aligned listing of this function (repeatable; demangled or partial names "
                                      "accepted)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-C --context", .type = AVP_T_INT, .dflt = AVP_DEF_INT(3), .metavar = "N",
                              .help = "rows of context in --diff (default 3)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--all-rows", .action = AVP_ARG_STORE_TRUE,
                              .help = "--diff lists every row, not only the differences"});
    avp_arg(ap, &(AvpArgDesc){.names = "--changed", .action = AVP_ARG_STORE_TRUE,
                              .help = "print only what moved since the last run of the unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "--file", .metavar = "PATH", .help = "compile PATH in place of the unit's source"});
    avp_arg(ap, &(AvpArgDesc){.names = "--built", .action = AVP_ARG_STORE_TRUE,
                              .help = "compare the object of the last ninja build (no compile)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--status", .metavar = "S[,S]",
                              .help = "every unit with this configure.py status (NonMatching ...)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--lib", .metavar = "L[,L]",
                              .help = "every unit of this library (build.ninja library name)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE, .metavar = "N",
                              .help = "parallel units (default: CPU count)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--data", .action = AVP_ARG_STORE_TRUE,
                              .help = "also run gatecheck layout on the object"});
    avp_arg(ap, &(AvpArgDesc){.names = "--rules", .action = AVP_ARG_STORE_TRUE,
                              .help = "also run the rule checker; exact + [match] = MATCH-PENDING"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-cache", .action = AVP_ARG_STORE_TRUE,
                              .help = "always compile and disassemble"});
    avp_arg(ap, &(AvpArgDesc){.names = "--keep", .action = AVP_ARG_STORE_TRUE,
                              .help = "keep the private work directory (object, listing)"});
    return ap;
}

int match_main(int argc, char **argv) { return shsm_cli_main(build_parser(), argc, argv, run, NULL); }
