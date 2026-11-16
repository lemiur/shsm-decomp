/* The command line of tools/eqcheck.py (L2258-2495): _work_dir, object_for, _ret_regs, cmd_one, _select_units,
 * _scan_unit, _reason_key, cmd_scan, cmd_selfcheck and main. See eq_cli.h. */
#include "eq_cli.h"

#include "cc_compare.h"
#include "match.h"
#include "shsm_cli.h"
#include "shsm_re.h"
#include "shsm_report.h"
#include "shsm_zlib.h"
#include "wall.h"

/* ---- small helpers --------------------------------------------------------------------------------------------- */

static AvpJson *jget(const AvpJson *o, const char *k) { return avp_json_getz(o, k); }

static const ShsmRepo *repo_of(AvpArena *a, const AvpArgs *args) {
    AvpStr root = avp_args_str(args, "root");
    return shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
}

/* os.path.join(base, *unit_path.split("/")) */
static const char *join_unit_path(AvpArena *a, const char *base, const char *path) {
    AvpStrVec parts = avp_str_split(avp_str(path), AVP_S("/"), -1);
    const char *out = base;
    for (size_t i = 0; i < parts.n; i++) out = avp_path_join(a, out, avp_cz(a, parts.v[i]));
    avp_vec_free(&parts);
    return out;
}

/* collections.Counter as an insertion-ordered object of ints */
static void count_add(AvpArena *a, AvpJson *counts, AvpStr key) {
    AvpJson *v = avp_json_get(counts, key);
    if (v) v->u.i++;
    else avp_json_set(a, counts, key, avp_json_int(a, 1));
}

static i64 count_of(const AvpJson *counts, const char *key) {
    const AvpJson *v = avp_json_getz(counts, key);
    return v ? v->u.i : 0;
}

static int key_cmp(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const AvpJsonMember *p = *(const AvpJsonMember *const *)x, *q = *(const AvpJsonMember *const *)y;
    int c = avp_str_cmp(p->key, q->key);
    if (c) return c;
    return p->val->u.i < q->val->u.i ? -1 : p->val->u.i > q->val->u.i;
}

static int count_cmp(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const AvpJsonMember *p = *(const AvpJsonMember *const *)x, *q = *(const AvpJsonMember *const *)y;
    return p->val->u.i < q->val->u.i ? -1 : p->val->u.i > q->val->u.i;
}

/* sorted(counts.items()) (by_key) or counts.most_common() (stable by count, descending) */
static const AvpJsonMember **count_items(AvpArena *a, const AvpJson *counts, bool by_key) {
    size_t n = counts->u.obj.n;
    const AvpJsonMember **ms = AVP_NEWN(a, const AvpJsonMember *, n + 1);
    for (size_t i = 0; i < n; i++) ms[i] = &counts->u.obj.v[i];
    if (by_key) avp_sort(ms, n, sizeof *ms, key_cmp, NULL, false);
    else avp_sort(ms, n, sizeof *ms, count_cmp, NULL, true);
    return ms;
}

/* ---- _ret_regs, _reason_key ------------------------------------------------------------------------------------ */

EqRet eq_ret_regs(AvpArena *a, const char *text) {
    if (!text || !*text || strcmp(text, "auto") == 0) return EQ_RET_DEFAULT;
    if (strcmp(text, "void") == 0) return (EqRet){NULL, 0};
    AvpStrVec parts = avp_str_split(avp_str(text), AVP_S(","), -1);
    const char **names = AVP_NEWN(a, const char *, parts.n + 1);
    int n = 0;
    for (size_t i = 0; i < parts.n; i++) {
        AvpStr x = avp_str_strip(parts.v[i]);
        if (x.n) names[n++] = avp_cz(a, x);
    }
    avp_vec_free(&parts);
    for (int i = 0; i < n; i++)
        if (eq_loc_index(avp_str(names[i])) < 0) shsm_raise(SHSM_EXC_USAGE, "--ret: unknown register %s", names[i]);
    return (EqRet){names, n};
}

AvpStr eq_reason_key(AvpArena *a, const AvpJson *f) {
    const AvpJson *um = jget(f, "unmodelled");
    if (avp_json_truthy(um)) {
        AvpStrVec w = avp_str_split(um->u.str, AVP_S(""), -1);
        AvpStr first = w.n ? w.v[0] : AVP_S("");
        avp_vec_free(&w);
        return avp_pyfmt(a, "unmodelled: %S", &first);
    }
    const ShsmRe *num = shsm_re("0x[0-9A-Fa-f]+|\\b\\d+\\b", 0);
    AvpStr r = shsm_re_sub(a, num, AVP_S("N"), avp_json_get_str(f, "reason", AVP_S("")), 0);
    i64 k = avp_str_find(r, AVP_S(" ["), 0, AVP_END);
    if (k >= 0) r = avp_strn(r.p, (size_t)k);
    return avp_u8_slice(r, 0, 90);
}

/* ---- object_for ------------------------------------------------------------------------------------------------ */

const char *eq_work_dir(AvpArena *a, const ShsmRepo *r) { return avp_join(a, r->root, "build", "tmp", "eqcheck"); }

EqObject eq_object_for(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, bool built, const char *source,
                       const char *tag) {
    EqObject o = {0};
    if (built) {
        const char *obj = u->base_obj ? avp_path_join(a, r->root, u->base_obj) : NULL;
        if (!obj || !avp_exists(obj))
            shsm_raise(SHSM_EXC_USAGE, "no built object for %s (run the build first or drop --built)", u->name);
        o.obj = obj;
        o.out_dir = join_unit_path(a, avp_path_join(a, eq_work_dir(a, r), "listing"), u->path);
        return o;
    }
    const char *src = source ? source : u->source;
    if (!src || !*src) shsm_raise(SHSM_EXC_USAGE, "%s has no source", u->name);
    src = avp_path_join(a, r->root, src);
    const char *out_dir = join_unit_path(a, avp_path_join(a, eq_work_dir(a, r), avp_sprintf(a, "obj%s", tag)), u->path);
    ShsmRun res = wall_compile_low(a, r, u, src, out_dir);
    if (res.timed_out || res.returncode != 0) {
        AvpStr output = shsm_run_output(a, &res);
        AvpStr why = wall_first_error(a, output);
        if (!why.p || !why.n) {
            AvpStrVec all = avp_str_splitlines(output, false), keep = {0};
            for (size_t i = 0; i < all.n; i++)
                if (avp_str_strip(all.v[i]).n) avp_vec_push(&keep, all.v[i]);
            size_t from = keep.n > 3 ? keep.n - 3 : 0;
            why = avp_str_join(a, AVP_S(" / "), keep.v + from, keep.n - from);
            avp_vec_free(&all);
            avp_vec_free(&keep);
        }
        shsm_raise(SHSM_EXC_USAGE, "compile of %s failed: %.*s", src, AVP_SF(why));
    }
    o.obj = shsm_object_for(a, src, out_dir);
    o.out_dir = NULL;
    return o;
}

/* compare.compare_object(unit, obj, repo=repo, out_dir=out_dir, cache=cache) */
static CcObjectComparison *compare_object(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *obj,
                                          const char *out_dir, bool cache) {
    CcRepo cr = {r->root, r->version};
    const AvpMap *compiled = match_load_listing(a, r, obj, cache, out_dir);
    CcError err = {0};
    CcObjectComparison *oc =
        cc_compare_object(a, &cr, avp_str(u->name), obj, NULL, true, NULL, NULL, compiled, out_dir, NULL, &err);
    if (!oc) match_raise_cc(&err);
    return oc;
}

static CcContext *context_of(const ShsmRepo *r, const ShsmUnit *u) {
    CcRepo cr = {r->root, r->version};
    return cc_repo_context(&cr, avp_str(u->name));
}

/* ---- the single form ------------------------------------------------------------------------------------------- */

static AvpStr py_str(AvpArena *a, const AvpJson *v) { return wall_py_str(a, v); }

static int cmd_one(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit *u = shsm_get_unit(r, avp_cz(a, avp_args_str(args, "unit")));
    AvpStr file = avp_args_str(args, "file");
    EqObject o = file.p ? eq_object_for(a, r, u, false, avp_path_abspath(a, avp_cz(a, file)), "-file")
                        : eq_object_for(a, r, u, avp_args_bool(args, "built"), NULL, "");
    CcObjectComparison *oc = compare_object(a, r, u, o.obj, o.out_dir, false);
    const CcCompareResult *res = wall_find(avp_args_str(args, "symbol"), oc->results, oc->retail);
    if (!res) {
        AvpStr sym = avp_args_str(args, "symbol");
        shsm_raise(SHSM_EXC_USAGE, "%.*s is not emitted by the compiled object", AVP_SF(sym));
    }
    CcContext *cctx = context_of(r, u);
    ShsmOutput *out = shsm_output_open(args, 1000000, -1, -1);
    if (res->exact) {
        shsm_output_line(out, avp_pyfmt(a, "%S: byte-exact", &res->name));
        shsm_output_linez(out, "EQUIVALENT (byte-exact)");
        shsm_output_data(out, "name", avp_json_str(a, res->name));
        shsm_output_data(out, "verdict", avp_json_strz(a, "EQUIVALENT"));
        shsm_output_data(out, "reason", avp_json_strz(a, "byte-exact"));
        shsm_output_data(out, "line", avp_json_strz(a, "EQUIVALENT (byte-exact)"));
        shsm_output_close(out);
        return SHSM_EXIT_OK;
    }
    AvpStr rets = avp_args_str(args, "ret");
    EqRet ret = eq_ret_regs(a, rets.p ? avp_cz(a, rets) : NULL);
    EqLive *live = eq_live_new(r, 4);
    AvpJson *rr;
    AvpTry t;
    if (AVP_TRY(&t)) {
        rr = eq_prove_functions(a, res->retail_fn, res->compiled_fn, cctx, ret, avp_args_bool(args, "explain"), res,
                                live);
        avp_try_end(&t);
    } else {
        eq_live_free(live);
        eq_reraise(eq_exc_caught(&t));
    }
    eq_live_free(live);
    AvpStr secs = avp_pyfmt(a, "%.2f s", avp_json_f64(jget(rr, "seconds"), 0.0));
    shsm_output_line(out, avp_pyfmt(a, "%S (retail %S): %d / %d rows, %S", &res->name, &res->retail_name,
                                    (i64)res->retail_fn->nrows, (i64)res->compiled_fn->nrows, &secs));
    const AvpJson *blocks = jget(rr, "blocks");
    if (avp_json_truthy(blocks)) {
        const AvpJson *sw = jget(rr, "swapped");
        AvpStr tail = AVP_S("");
        if (avp_json_truthy(sw)) {
            size_t n = avp_json_len(sw);
            AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
            for (size_t i = 0; i < n; i++) items[i] = py_str(a, avp_json_at(sw, i));
            AvpStr j = avp_str_join(a, AVP_S(", "), items, n);
            tail = avp_pyfmt(a, ", conditions crossed at retail blocks %S", &j);
        }
        shsm_output_line(out, avp_pyfmt(a, "blocks paired: %d%S", (i64)avp_json_len(blocks), &tail));
    }
    const AvpJson *ex = jget(rr, "explain");
    for (size_t i = 0; avp_json_is_arr(ex) && i < avp_json_len(ex); i++) {
        const AvpJson *e = avp_json_at(ex, i);
        AvpStr rows[2];
        const char *keys[2] = {"retail_rows", "compiled_rows"};
        for (int k = 0; k < 2; k++) {
            const AvpJson *l = jget(e, keys[k]);
            size_t n = avp_json_len(l);
            AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
            for (size_t m = 0; m < n; m++) items[m] = py_str(a, avp_json_at(l, m));
            rows[k] = avp_str_join(a, AVP_S("-"), items, n);
        }
        AvpStr rb = py_str(a, jget(e, "retail_block")), cb = py_str(a, jget(e, "compiled_block"));
        shsm_output_line(out, avp_pyfmt(a, "-- retail block %S %S / compiled block %S %S", &rb, &rows[0], &cb, &rows[1]));
        const char *sides[2] = {"retail", "compiled"};
        for (int s = 0; s < 2; s++) {
            const AvpJson *l = jget(e, sides[s]);
            for (size_t m = 0; m < avp_json_len(l); m++) {
                AvpStr x = py_str(a, avp_json_at(l, m));
                shsm_output_line(out, avp_pyfmt(a, "   %-8s %S", sides[s], &x));
            }
        }
        const AvpJson *cond = jget(e, "cond");
        const AvpJson *c0 = avp_json_at(cond, 0), *c1 = avp_json_at(cond, 1);
        if (avp_json_truthy(c0) || avp_json_truthy(c1)) {
            AvpStr s0 = py_str(a, c0), s1 = py_str(a, c1);
            shsm_output_line(out, avp_pyfmt(a, "   cond     retail %S | compiled %S", &s0, &s1));
        }
    }
    shsm_output_line(out, avp_json_get_str(rr, "line", AVP_S("")));
    shsm_output_data(out, "name", avp_json_str(a, res->name));
    shsm_output_data(out, "retail_name", avp_json_str(a, res->retail_name));
    for (size_t i = 0; i < rr->u.obj.n; i++) shsm_output_data(out, avp_cz(a, rr->u.obj.v[i].key), rr->u.obj.v[i].val);
    shsm_output_close(out);
    const AvpJson *v = jget(rr, "verdict");
    return avp_json_is_str(v) && avp_str_eqz(v->u.str, "EQUIVALENT") ? SHSM_EXIT_OK : SHSM_EXIT_FINDING;
}

/* ---- scan and selfcheck ---------------------------------------------------------------------------------------- */

/* _select_units(args, repo, default_status) */
static size_t select_units(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const char *default_status,
                           const ShsmUnit ***out) {
    const ShsmUnits *all = shsm_load_units(r);
    AvpPtrVec sel = {0};
    size_t nu = avp_args_is_none(args, "unit") ? 0 : avp_args_len(args, "unit");
    if (nu) {
        AvpStrVec us = avp_args_strs(args, "unit");
        for (size_t i = 0; i < us.n; i++) avp_vec_push(&sel, (void *)shsm_get_unit(r, avp_cz(a, us.v[i])));
        avp_vec_free(&us);
    } else {
        AvpStr status = avp_args_str(args, "status"), lib = avp_args_str(args, "lib");
        if (!status.n) status = lib.n ? AVP_S("") : avp_str(default_status);
        for (size_t i = 0; i < all->n; i++) {
            const ShsmUnit *u = all->v[i];
            if (!u->source || !*u->source || shsm_unit_is_asm(u)) continue; /* assembled, not compiled */
            if (status.n && !(u->status && avp_str_eqz(status, u->status))) continue;
            if (lib.n && !(u->library && avp_str_eqz(lib, u->library))) continue;
            avp_vec_push(&sel, (void *)u);
        }
        if (!sel.n) shsm_raise(SHSM_EXC_USAGE, "no units selected");
    }
    const ShsmUnit **v = AVP_NEWN(a, const ShsmUnit *, sel.n + 1);
    for (size_t i = 0; i < sel.n; i++) v[i] = sel.v[i];
    size_t n = sel.n;
    avp_vec_free(&sel);
    *out = v;
    return n;
}

enum { MODE_COMPILE, MODE_COMPILE_BUILT, MODE_BUILT, MODE_IDENTITY };

/* One _scan_unit job and its result. The result (functions, error) lives in the job's own arena; the compile,
 * comparison and proofs use a work arena freed when the unit is done. */
typedef struct {
    const ShsmRepo *r;
    const ShsmUnit *unit;
    int mode;
    double rate;
    EqRet ret;
    AvpArena *ra;
    AvpJson *functions; /* the records */
    AvpStr error;       /* .p NULL: none */
    double seconds;
} UnitJob;

static AvpJson *copy_or_null(AvpArena *a, const AvpJson *v) { return v ? match_json_copy(a, v) : avp_json_null(); }

/* _scan_unit(job) */
static int scan_unit(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(ctx);
    AVP_UNUSED(index);
    AVP_UNUSED(result);
    UnitJob *j = *(UnitJob *const *)item;
    double t0 = avp_time();
    bool want_exact = j->mode == MODE_BUILT || j->mode == MODE_IDENTITY;
    AvpArena *wa = avp_arena_new(0);
    EqLive *live = NULL;
    j->functions = avp_json_arr(j->ra);
    AvpTry t;
    if (AVP_TRY(&t)) {
        const ShsmRepo *r = j->r;
        const ShsmUnit *u = j->unit;
        CcContext *cctx = context_of(r, u);
        typedef struct {
            AvpStr name;
            const CcFn *rfn, *cfn;
            bool exact;
            const CcCompareResult *res;
        } Item;
        AVP_VEC(Item) items = {0};
        if (j->mode == MODE_IDENTITY) {
            CcRepo cr = {r->root, r->version};
            CcError err = {0};
            AvpMap *retail = cc_load_retail(&cr, avp_str(u->name), NULL, false, &err);
            if (!retail) match_raise_cc(&err);
            AvpMapIter it = {0};
            while (avp_map_next(retail, &it)) {
                Item x = {it.key, it.val->p, it.val->p, true, NULL};
                avp_vec_push(&items, x);
            }
        } else {
            EqObject o = eq_object_for(wa, r, u, j->mode != MODE_COMPILE, NULL, "");
            CcObjectComparison *oc = compare_object(wa, r, u, o.obj, o.out_dir, true);
            AvpMapIter it = {0};
            while (avp_map_next(oc->results, &it)) {
                const CcCompareResult *res = it.val->p;
                if (!res->retail_fn || !res->compiled_fn) continue;
                Item x = {it.key, res->retail_fn, res->compiled_fn, res->exact, res};
                avp_vec_push(&items, x);
            }
        }
        AvpJson *out = avp_json_arr(j->ra);
        live = eq_live_new(r, 4);
        for (size_t i = 0; i < items.n; i++) {
            const Item *x = &items.v[i];
            if (x->exact != want_exact) continue;
            if (j->rate < 1.0 && (shsm_zlib_crc32(x->name.p, x->name.n, 0) / 4294967296.0) >= j->rate) continue;
            AvpArenaMark mk = avp_arena_mark(wa);
            AvpJson *pr = eq_prove_functions(wa, x->rfn, x->cfn, cctx, j->ret, false, x->res, live);
            AvpArena *ra = j->ra;
            AvpJson *f = avp_json_obj(ra);
            avp_json_set(ra, f, AVP_S("unit"), avp_json_strz(ra, u->name));
            avp_json_set(ra, f, AVP_S("function"), avp_json_str(ra, x->name));
            avp_json_set(ra, f, AVP_S("rows"), avp_json_int(ra, (i64)x->rfn->nrows));
            avp_json_set(ra, f, AVP_S("verdict"), copy_or_null(ra, jget(pr, "verdict")));
            avp_json_set(ra, f, AVP_S("reason"), copy_or_null(ra, jget(pr, "reason")));
            avp_json_set(ra, f, AVP_S("line"), copy_or_null(ra, jget(pr, "line")));
            avp_json_set(ra, f, AVP_S("unmodelled"), copy_or_null(ra, jget(pr, "unmodelled")));
            avp_json_set(ra, f, AVP_S("seconds"), copy_or_null(ra, jget(pr, "seconds")));
            avp_json_arr_push(ra, out, f);
            avp_arena_reset(wa, mk);
        }
        avp_vec_free(&items);
        eq_live_free(live);
        live = NULL;
        j->functions = out;
        avp_try_end(&t);
    } else {
        /* one bad unit must not stop the scan */
        if (live) eq_live_free(live);
        const EqExc *e = eq_exc_caught(&t);
        const char *s = eq_exc_str(j->ra, e);
        AvpStr first = AVP_S("");
        if (*s) {
            AvpStrVec ls = avp_str_splitlines(avp_str(s), false);
            if (ls.n) first = avp_str(avp_cz(j->ra, ls.v[0]));
            avp_vec_free(&ls);
        }
        j->error = avp_pyfmt(j->ra, "%s: %S", eq_exc_class(e->code), &first);
        j->functions = avp_json_arr(j->ra);
    }
    avp_arena_free(wa);
    j->seconds = avp_py_round(avp_time() - t0, 2);
    return 0;
}

static i64 jobs_of(const AvpArgs *args) {
    i64 jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    return jobs ? jobs : shsm_default_jobs();
}

/* cli.pmap(_scan_unit, jobs, jobs=args.jobs, processes=True): the units on threads (DEVIATIONS D-eqcheck-8) */
static UnitJob **run_units(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const ShsmUnit **sel, size_t n,
                           int mode, double rate, EqRet ret) {
    UnitJob **jobs = AVP_NEWN(a, UnitJob *, n + 1);
    for (size_t i = 0; i < n; i++) {
        UnitJob *j = AVP_NEW(a, UnitJob);
        j->r = r;
        j->unit = sel[i];
        j->mode = mode;
        j->rate = rate;
        j->ret = ret;
        j->ra = avp_arena_new(0);
        jobs[i] = j;
    }
    i64 nj = jobs_of(args);
    if (nj <= 1 || n <= 1) {
        for (size_t i = 0; i < n; i++) scan_unit(NULL, i, &jobs[i], NULL);
    } else {
        avp_pool_map(NULL, (int)AVP_MIN((i64)n, nj), scan_unit, NULL, jobs, n, sizeof(UnitJob *), NULL, 0);
    }
    return jobs;
}

/* the outs' error dicts: {"unit", "error", "functions": [], "seconds"} */
static AvpJson *error_list(AvpArena *a, UnitJob **jobs, size_t n) {
    AvpJson *errors = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) {
        if (!jobs[i]->error.n) continue;
        AvpJson *e = avp_json_obj(a);
        avp_json_set(a, e, AVP_S("unit"), avp_json_strz(a, jobs[i]->unit->name));
        avp_json_set(a, e, AVP_S("error"), avp_json_str(a, jobs[i]->error));
        avp_json_set(a, e, AVP_S("functions"), avp_json_arr(a));
        avp_json_set(a, e, AVP_S("seconds"), avp_json_float(a, jobs[i]->seconds));
        avp_json_arr_push(a, errors, e);
    }
    return errors;
}

static AvpJson *all_functions(AvpArena *a, UnitJob **jobs, size_t n) {
    AvpJson *funcs = avp_json_arr(a);
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < avp_json_len(jobs[i]->functions); k++)
            avp_json_arr_push(a, funcs, avp_json_at(jobs[i]->functions, k));
    return funcs;
}

static AvpJson *unit_names(AvpArena *a, const ShsmUnit **sel, size_t n) {
    AvpJson *l = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, l, avp_json_strz(a, sel[i]->name));
    return l;
}

static int verdict_order(AvpStr v) {
    if (avp_str_eqz(v, "EQUIVALENT")) return 0;
    if (avp_str_eqz(v, "DIFFERENT")) return 1;
    if (avp_str_eqz(v, "NOT PROVEN")) return 2;
    return 9;
}

static int scan_row_cmp(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const AvpJson *p = *(const AvpJson *const *)x, *q = *(const AvpJson *const *)y;
    int a = verdict_order(avp_json_get_str(p, "verdict", AVP_S(""))),
        b = verdict_order(avp_json_get_str(q, "verdict", AVP_S("")));
    if (a != b) return a < b ? -1 : 1;
    int c = avp_str_cmp(avp_json_get_str(p, "unit", AVP_S("")), avp_json_get_str(q, "unit", AVP_S("")));
    if (c) return c;
    return avp_str_cmp(avp_json_get_str(p, "function", AVP_S("")), avp_json_get_str(q, "function", AVP_S("")));
}

static AvpStr counts_text(AvpArena *a, const AvpJson *counts) {
    size_t n = counts->u.obj.n;
    const AvpJsonMember **ms = count_items(a, counts, true);
    AvpStr *items = AVP_NEWN(a, AvpStr, n + 1);
    for (size_t i = 0; i < n; i++) items[i] = avp_pyfmt(a, "%S %d", &ms[i]->key, ms[i]->val->u.i);
    return avp_str_join(a, AVP_S(", "), items, n);
}

static AvpJson *verdict_counts(AvpArena *a, const AvpJson *funcs) {
    AvpJson *counts = avp_json_obj(a);
    for (size_t i = 0; i < avp_json_len(funcs); i++)
        count_add(a, counts, py_str(a, jget(avp_json_at(funcs, i), "verdict")));
    return counts;
}

static int cmd_scan(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit **sel;
    size_t nsel = select_units(a, args, r, "NonMatching", &sel);
    bool built = avp_args_bool(args, "built");
    AvpStr rets = avp_args_str(args, "ret");
    EqRet ret = eq_ret_regs(a, rets.p ? avp_cz(a, rets) : NULL);
    double t0 = avp_time();
    UnitJob **jobs = run_units(a, args, r, sel, nsel, built ? MODE_COMPILE_BUILT : MODE_COMPILE, 1.0, ret);
    AvpJson *funcs = all_functions(a, jobs, nsel), *errors = error_list(a, jobs, nsel);
    AvpJson *counts = verdict_counts(a, funcs);
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    AvpStr ct = counts_text(a, counts);
    shsm_output_line(out, avp_pyfmt(a, "%d unit(s), %d non-exact function(s) in %.1f s: %S", (i64)nsel,
                                    (i64)avp_json_len(funcs), avp_time() - t0, &ct));
    for (size_t i = 0; i < avp_json_len(errors); i++) {
        const AvpJson *e = avp_json_at(errors, i);
        AvpStr un = avp_json_get_str(e, "unit", AVP_S("")), er = avp_json_get_str(e, "error", AVP_S(""));
        shsm_output_line(out, avp_pyfmt(a, "error: %S: %S", &un, &er));
    }
    size_t nf = avp_json_len(funcs);
    const AvpJson **order = AVP_NEWN(a, const AvpJson *, nf + 1);
    for (size_t i = 0; i < nf; i++) order[i] = avp_json_at(funcs, i);
    avp_sort(order, nf, sizeof *order, scan_row_cmp, NULL, false);
    for (size_t i = 0; i < nf; i++) {
        const AvpJson *f = order[i];
        AvpStr un = avp_u8_slice(avp_str_replace(a, avp_json_get_str(f, "unit", AVP_S("")), AVP_S("main/"), AVP_S(""), -1),
                                 0, 34);
        AvpStr fn = avp_u8_slice(avp_json_get_str(f, "function", AVP_S("")), 0, 44);
        AvpStr ln = avp_u8_slice(py_str(a, jget(f, "line")), 0, 160);
        shsm_output_line(out, avp_pyfmt(a, "%-34S %-44S %4d  %S", &un, &fn, wall_py_int(jget(f, "rows")), &ln));
    }
    AvpJson *units = unit_names(a, sel, nsel);
    shsm_output_data(out, "units", units);
    shsm_output_data(out, "counts", counts);
    shsm_output_data(out, "errors", errors);
    shsm_output_data(out, "functions", funcs);
    shsm_output_close(out);
    const char *path = avp_join(a, r->root, "build", "eqcheck.json");
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("version"), avp_json_int(a, 1));
    avp_json_set(a, doc, AVP_S("built"), avp_json_bool(built));
    avp_json_set(a, doc, AVP_S("units"), units);
    avp_json_set(a, doc, AVP_S("counts"), counts);
    avp_json_set(a, doc, AVP_S("errors"), errors);
    avp_json_set(a, doc, AVP_S("functions"), funcs);
    AvpJsonDumpOpts dopts = shsm_json_opts();
    AvpStr text = avp_json_dumps(a, doc, &dopts);
    const char *tmp = avp_sprintf(a, "%s.tmp", path);
    if (!avp_write_text(tmp, text, AVP_TM_UTF8)) shsm_raise_io();
    if (!avp_replace(tmp, path)) shsm_raise_io();
    for (size_t i = 0; i < nsel; i++) avp_arena_free(jobs[i]->ra);
    return (count_of(counts, "DIFFERENT") || count_of(counts, "NOT PROVEN")) ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

static int cmd_selfcheck(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    const ShsmRepo *r = repo_of(a, args);
    const ShsmUnit **sel;
    size_t nsel = select_units(a, args, r, "Matching", &sel);
    double rate = 1.0;
    i64 sample = avp_args_int(args, "sample");
    if (sample) {
        i64 total = 0;
        AvpTry t;
        if (AVP_TRY(&t)) {
            ShsmReport *rep = shsm_load_report(r);
            AvpSet *names = avp_set_new();
            for (size_t i = 0; i < nsel; i++) avp_set_add(names, avp_str(sel[i]->name));
            for (size_t i = 0; i < rep->nunits; i++) {
                const ShsmUnitReport *ur = rep->units[i];
                for (size_t k = 0; k < ur->nfunctions; k++) {
                    const ShsmFunctionReport *f = &ur->functions[k];
                    if (avp_set_has(names, avp_str(f->unit)) && f->fuzzy >= 100.0) total++;
                }
            }
            avp_set_free(names);
            avp_try_end(&t);
        } else {
            total = 0;
        }
        rate = total ? AVP_MIN(1.0, (double)sample / (double)total) : 1.0;
    }
    bool identity = avp_args_bool(args, "identity");
    const char *mode = identity ? "identity" : "built";
    AvpStr rets = avp_args_str(args, "ret");
    EqRet ret = eq_ret_regs(a, rets.p ? avp_cz(a, rets) : NULL);
    double t0 = avp_time();
    UnitJob **jobs = run_units(a, args, r, sel, nsel, identity ? MODE_IDENTITY : MODE_BUILT, rate, ret);
    AvpJson *funcs = all_functions(a, jobs, nsel), *errors = error_list(a, jobs, nsel);
    AvpJson *counts = verdict_counts(a, funcs), *reasons = avp_json_obj(a), *unmod = avp_json_obj(a);
    AvpJson *bad = avp_json_arr(a), *not_proven = avp_json_arr(a);
    size_t n = avp_json_len(funcs);
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(funcs, i);
        AvpStr v = py_str(a, jget(f, "verdict"));
        if (avp_str_eqz(v, "NOT PROVEN")) {
            count_add(a, reasons, eq_reason_key(a, f));
            avp_json_arr_push(a, not_proven, f);
        }
    }
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(funcs, i);
        const AvpJson *um = jget(f, "unmodelled");
        if (avp_json_truthy(um)) {
            AvpStrVec w = avp_str_split(um->u.str, AVP_S(""), -1);
            count_add(a, unmod, w.n ? w.v[0] : AVP_S(""));
            avp_vec_free(&w);
        }
    }
    for (size_t i = 0; i < n; i++) {
        AvpJson *f = avp_json_at(funcs, i);
        if (avp_str_eqz(py_str(a, jget(f, "verdict")), "DIFFERENT")) avp_json_arr_push(a, bad, f);
    }
    ShsmOutput *out = shsm_output_open(args, -1, -1, -1);
    shsm_output_line(out, avp_pyfmt(a, "selfcheck (%s) of %d unit(s): %d byte-exact function(s) in %.1f s", mode,
                                    (i64)nsel, (i64)n, avp_time() - t0));
    i64 eqv = count_of(counts, "EQUIVALENT");
    shsm_output_line(out, avp_pyfmt(a, "  proved %d (%.1f%%), not proven %d, DIFFERENT %d", eqv,
                                    n ? 100.0 * (double)eqv / (double)n : 0.0, count_of(counts, "NOT PROVEN"),
                                    count_of(counts, "DIFFERENT")));
    for (size_t i = 0; i < avp_json_len(errors); i++) {
        const AvpJson *e = avp_json_at(errors, i);
        AvpStr un = avp_json_get_str(e, "unit", AVP_S("")), er = avp_json_get_str(e, "error", AVP_S(""));
        shsm_output_line(out, avp_pyfmt(a, "error: %S: %S", &un, &er));
    }
    for (size_t i = 0; i < avp_json_len(bad); i++) {
        const AvpJson *f = avp_json_at(bad, i);
        AvpStr un = avp_json_get_str(f, "unit", AVP_S("")), fn = avp_json_get_str(f, "function", AVP_S(""));
        AvpStr ln = avp_u8_slice(py_str(a, jget(f, "line")), 0, 200);
        shsm_output_line(out, avp_pyfmt(a, "ENGINE BUG: %S %S: %S", &un, &fn, &ln));
    }
    shsm_output_linez(out, "not-proven reasons:");
    const AvpJsonMember **rs = count_items(a, reasons, false);
    for (size_t i = 0; i < reasons->u.obj.n && i < 25; i++)
        shsm_output_line(out, avp_pyfmt(a, "  %5d  %S", rs[i]->val->u.i, &rs[i]->key));
    shsm_output_linez(out, "unmodelled instructions:");
    const AvpJsonMember **us = count_items(a, unmod, false);
    size_t nu = AVP_MIN(unmod->u.obj.n, (size_t)30);
    AvpStr *items = AVP_NEWN(a, AvpStr, nu + 1);
    for (size_t i = 0; i < nu; i++) items[i] = avp_pyfmt(a, "%S %d", &us[i]->key, us[i]->val->u.i);
    AvpStr ul = avp_str_join(a, AVP_S(", "), items, nu);
    shsm_output_line(out, avp_pyfmt(a, "  %S", &ul));
    shsm_output_data(out, "mode", avp_json_strz(a, mode));
    shsm_output_data(out, "units", avp_json_int(a, (i64)nsel));
    shsm_output_data(out, "functions", avp_json_int(a, (i64)n));
    shsm_output_data(out, "counts", counts);
    shsm_output_data(out, "reasons", reasons);
    shsm_output_data(out, "unmodelled", unmod);
    shsm_output_data(out, "bugs", bad);
    shsm_output_data(out, "errors", errors);
    shsm_output_data(out, "not_proven", avp_args_bool(args, "all") ? not_proven : avp_json_null());
    shsm_output_close(out);
    bool fail = avp_json_len(bad) || avp_json_len(errors);
    for (size_t i = 0; i < nsel; i++) avp_arena_free(jobs[i]->ra);
    return fail ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

/* ---- command line ---------------------------------------------------------------------------------------------- */

static bool limit_conv(void *ctx, AvpArena *a, AvpStr in, AvpArgValue *out, AvpStr *msg) {
    AVP_UNUSED(ctx);
    i64 n = 0;
    if (!shsm_limit_type(a, in, &n, msg)) return false;
    out->kind = AVP_V_INT;
    out->i = n;
    return true;
}

/* cli.parser(description) with an epilog */
static AvpArgParser *shared_parser(const char *prog, const char *description, const char *epilog) {
    AvpArgParser *p = avp_args_parser(NULL, &(AvpArgParserDesc){.prog = prog, .description = description,
                                                               .epilog = epilog});
    avp_arg(p, &(AvpArgDesc){.names = "--root", .metavar = "DIR",
                             .help = "checkout to work on (default: the one around the cwd)"});
    avp_arg(p, &(AvpArgDesc){.names = "--json", .action = AVP_ARG_STORE_TRUE,
                             .help = "print one JSON document instead of text"});
    avp_arg(p, &(AvpArgDesc){.names = "--all", .action = AVP_ARG_STORE_TRUE, .help = "do not cap the output"});
    avp_arg(p, &(AvpArgDesc){.names = "--limit", .type = AVP_T_CUSTOM, .conv = limit_conv, .type_name = "limit_type",
                             .dflt = AVP_DEF_INT(SHSM_DEFAULT_LIMIT), .metavar = "N",
                             .help = "cap text output at N lines (default 40; 0 or 'all': no cap)"});
    avp_arg(p, &(AvpArgDesc){.names = "--debug", .action = AVP_ARG_STORE_TRUE, .help = "print tracebacks"});
    return p;
}

#define RET_HELP "return registers compared (default r3,r4,f1; 'void' for none)"

int eq_main(int argc, char **argv) {
    if (argc > 1 && (strcmp(argv[1], "scan") == 0 || strcmp(argv[1], "selfcheck") == 0)) {
        bool scan = strcmp(argv[1], "scan") == 0;
        AvpArena *a = avp_arena();
        AvpArgParser *ap = shared_parser(scan ? "eqcheck.exe scan" : "eqcheck.exe selfcheck",
                                         avp_sprintf(a, "Prove functions equivalent to retail (%s).", argv[1]), NULL);
        avp_arg(ap, &(AvpArgDesc){.names = "--unit", .action = AVP_ARG_APPEND, .help = "unit (repeatable)"});
        avp_arg(ap, &(AvpArgDesc){.names = "--lib", .help = "configure.py library name"});
        avp_arg(ap, &(AvpArgDesc){.names = "--status",
                                  .help = scan ? "unit status (default: NonMatching)" : "unit status (default: Matching)"});
        avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE,
                                  .help = "parallel workers"});
        avp_arg(ap, &(AvpArgDesc){.names = "--ret", .help = RET_HELP});
        if (scan) {
            avp_arg(ap, &(AvpArgDesc){.names = "--built", .action = AVP_ARG_STORE_TRUE,
                                      .help = "read the last build's objects"});
            return shsm_cli_main(ap, argc - 1, argv + 1, cmd_scan, NULL);
        }
        avp_arg(ap, &(AvpArgDesc){.names = "--sample", .type = AVP_T_INT, .dflt = AVP_DEF_INT(0),
                                  .help = "prove about N functions"});
        avp_arg(ap, &(AvpArgDesc){.names = "--identity", .action = AVP_ARG_STORE_TRUE,
                                  .help = "prove retail against retail"});
        return shsm_cli_main(ap, argc - 1, argv + 1, cmd_selfcheck, NULL);
    }
    AvpArgParser *ap = shared_parser(
        "eqcheck.exe", "Prove one compiled function equivalent to retail by symbolic execution.",
        "keyword forms: eqcheck.exe {scan,selfcheck} ...  scan proves every non-exact function of the chosen units into "
        "build/eqcheck.json; selfcheck proves byte-exact functions against themselves. `eqcheck.exe scan --help` and "
        "`eqcheck.exe selfcheck --help` show their options.");
    avp_arg(ap, &(AvpArgDesc){.names = "unit"});
    avp_arg(ap, &(AvpArgDesc){.names = "symbol"});
    AvpArgMutex *g = avp_args_mutex(ap, false);
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--built", .action = AVP_ARG_STORE_TRUE,
                                   .help = "read the last build's object"});
    avp_arg_mutex(g, &(AvpArgDesc){.names = "--file", .help = "compile this variant source with the unit's flags"});
    avp_arg(ap, &(AvpArgDesc){.names = "--ret", .help = RET_HELP});
    avp_arg(ap, &(AvpArgDesc){.names = "--explain", .action = AVP_ARG_STORE_TRUE,
                              .help = "print the per-block effect lists"});
    return shsm_cli_main(ap, argc, argv, cmd_one, NULL);
}
