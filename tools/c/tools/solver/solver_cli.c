/* solver_cli: the command. Selects the units (every NonMatching unit by default, the case's unit under --case),
 * compiles each one's reference, scratch copy, pipeline control, preprocessed text and (parked-exact units) the
 * variants without each unit-only flag in parallel, then runs Phases 0 to 3 in unit order on one thread
 * (levers' tokenizer is not thread-safe), and writes records.json, records.tsv and causes.tsv. */
#include "sibling.h"
#include "sibling_lookup.h"
#include "solver.h"
#include "witness_lookup.h"

#include <stdlib.h>

/* ---- the flag control cache ------------------------------------------------------------------------------------ */

int sv_run_flag_control(SvRun *run, SvUnitWork *w, const char *flag, const SvFlags *without, const SvFlags *with,
                        const char **why) {
    AvpArena *a = run->a;
    AvpBuf key;
    avp_buf_init(&key, a);
    avp_buf_putz(&key, w->u->mw_version ? w->u->mw_version : "?");
    for (size_t i = 0; i < w->u->cflags.n; i++) {
        avp_buf_putc(&key, '\x1f');
        avp_buf_put(&key, w->u->cflags.v[i]);
    }
    avp_buf_printf(&key, "\x1e%s\x1e%s", flag, without ? "drop" : "add");
    bool created = false;
    AvpVal *v = avp_map_put(run->flag_controls, avp_buf_str(&key), &created);
    if (!created) {
        *why = "cached";
        return (int)v->i;
    }
    /* NULL on either side is the unit's own command line: a dropped flag passes `without`, an added one `with` */
    v->i = sv_control_flag(&w->sc, a, without, with, why);
    return (int)v->i;
}

/* ---- the unit job ---------------------------------------------------------------------------------------------- */

typedef struct {
    const ShsmRepo *r;
    bool case_mode;
} JobCtx;

static int unit_job(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(index);
    AVP_UNUSED(result);
    const JobCtx *jc = ctx;
    SvUnitWork *w = *(SvUnitWork *const *)item;
    AvpArena *a = avp_arena();
    double t0 = avp_time();
    if (!w->text0.p) {
        AvpBytes b = {0};
        const char *path = avp_path_join(a, jc->r->root, w->u->source);
        if (!avp_read_all(a, path, &b)) {
            w->error = avp_sprintf(a, "cannot read %s", path);
            return 0;
        }
        w->text0 = avp_strn((const char *)b.v, b.n);
    }
    w->ref = jc->case_mode ? sv_build(&w->sc, a, w->text0, "ref", NULL)
                           : sv_build_file(&w->sc, a, avp_path_join(a, jc->r->root, w->u->source), "ref", NULL);
    if (!w->ref->ok) {
        w->error = w->ref->error;
        return 0;
    }
    w->base = sv_build(&w->sc, a, w->text0, "base", NULL);
    w->pipeline_ok = sv_control_pipeline(&w->sc, a, w->text0, &w->pipeline_why);
    SvFlags pp = {0};
    pp.preprocess = true;
    SvBuild *p = sv_build(&w->sc, a, w->text0, "pp", &pp);
    if (p->ok) w->preprocessed = p->preprocessed;
    else w->pp_error = p->error;
    /* parked exact: every function exact, nothing of retail left unpaired */
    w->all_exact = w->ref->oc->retail_only.n == 0 && avp_map_len(w->ref->oc->results) > 0;
    AvpMapIter it = {0};
    while (w->all_exact && avp_map_next(w->ref->oc->results, &it))
        if (!((const CcCompareResult *)it.val->p)->exact) w->all_exact = false;
    if (w->all_exact)
        for (size_t k = 0; k < w->unit_only.n; k++) {
            SvFlags fl = {0};
            fl.groups = &w->unit_only.v[k];
            fl.ngroups = 1;
            avp_vec_push(&w->dropped, sv_build(&w->sc, a, w->text0, avp_sprintf(a, "drop%zu", k), &fl));
        }
    w->seconds = avp_time() - t0;
    return 0;
}

/* ---- selection ------------------------------------------------------------------------------------------------- */

static size_t select_units(AvpArena *a, const AvpArgs *args, const ShsmRepo *r, const char *case_unit,
                           const ShsmUnit ***out) {
    const ShsmUnits *all = shsm_load_units(r);
    AvpPtrVec sel = {0};
    size_t nu = avp_args_is_none(args, "units") ? 0 : avp_args_len(args, "units");
    if (case_unit) {
        avp_vec_push(&sel, (void *)shsm_get_unit(r, case_unit));
    } else if (nu) {
        AvpStrVec us = avp_args_strs(args, "units");
        for (size_t i = 0; i < us.n; i++) avp_vec_push(&sel, (void *)shsm_get_unit(r, avp_cz(a, us.v[i])));
        avp_vec_free(&us);
    } else {
        AvpStr lib = avp_args_str(args, "lib");
        for (size_t i = 0; i < all->n; i++) {
            const ShsmUnit *u = all->v[i];
            if (!u->source || !*u->source || shsm_unit_is_asm(u)) continue;
            if (!u->status || strcmp(u->status, "NonMatching") != 0) continue;
            if (lib.n && !(u->library && avp_str_eqz(lib, u->library))) continue;
            avp_vec_push(&sel, (void *)u);
        }
    }
    for (size_t i = 0; i < sel.n; i++) {
        const ShsmUnit *u = sel.v[i];
        if (!u->source || !*u->source || shsm_unit_is_asm(u))
            shsm_raise(SHSM_EXC_USAGE, "%s has no compiled source", u->name);
    }
    if (!sel.n) shsm_raise(SHSM_EXC_USAGE, "no units selected");
    const ShsmUnit **v = AVP_NEWN(a, const ShsmUnit *, sel.n + 1);
    for (size_t i = 0; i < sel.n; i++) v[i] = sel.v[i];
    size_t n = sel.n;
    avp_vec_free(&sel);
    *out = v;
    return n;
}

static SvGroups unit_only_of(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u) {
    const ShsmUnits *all = shsm_load_units(r);
    AvpPtrVec others = {0};
    for (size_t i = 0; i < all->n; i++) {
        const ShsmUnit *o = all->v[i];
        if (o == u || !o->has_cflags || !o->source || shsm_unit_is_asm(o)) continue;
        if (!u->library || !o->library || strcmp(u->library, o->library) != 0) continue;
        avp_vec_push(&others, (void *)o);
    }
    SvGroups g = sv_unit_only_flags(a, u, (const ShsmUnit *const *)others.v, others.n);
    avp_vec_free(&others);
    return g;
}

/* ---- evidence indexes ------------------------------------------------------------------------------------------ */

static void open_indexes(SvRun *run) {
    AvpArena *a = run->a;
    run->witness_present = avp_exists(wt_db_path(a, run->r));
    if (!run->witness_present)
        avp_vec_push(&run->run_notes, avp_str("witness index missing (build/index/witness.sqlite): no PS2 evidence"));
    AvpTry t;
    if (AVP_TRY(&t)) {
        run->sibling = sibling_lookup_open(a, run->r);
        if (run->sibling) {
            Sibling *sb = sibling_open(run->r, SIBLING_DEFAULT_NAME);
            if (sb && sibling_has_index(sb)) {
                SibMapSymVec un = sibling_unused_all(a, sb);
                run->sibling_unused = avp_set_new();
                for (size_t i = 0; i < un.n; i++)
                    if (un.v[i].name) avp_set_add(run->sibling_unused, avp_str(un.v[i].name));
                avp_vec_free(&un);
            }
            run->sibling_db = sb;
        }
        avp_try_end(&t);
    } else {
        run->sibling = NULL;
        avp_vec_push(&run->run_notes, avp_str(avp_sprintf(a, "sibling index unreadable: %s", t.msg ? t.msg : "?")));
    }
    if (!run->sibling)
        avp_vec_push(&run->run_notes, avp_str("sibling index missing or unpaired: no Gormiti map evidence"));
}

/* ---- the case's expectations ----------------------------------------------------------------------------------- */

static bool expect_held(const SvExpect *e, const SvRecords *recs) {
    bool any = false;
    if (!strcmp(e->key, "contains") || !strcmp(e->key, "not-contains")) {
        /* the readable blocks of the records */
        AvpArena *a = avp_scratch();
        AvpArenaMark mk = avp_arena_mark(a);
        AvpStrVec lines = {0};
        for (size_t i = 0; i < recs->n; i++) sv_record_lines(a, recs->v[i], &lines);
        for (size_t i = 0; i < lines.n && !any; i++) any = avp_str_contains(lines.v[i], avp_str(e->value));
        avp_vec_free(&lines);
        avp_arena_reset(a, mk);
        return !strcmp(e->key, "not-contains") ? !any : any;
    }
    if (!strcmp(e->key, "candidates")) {
        i64 n = 0;
        for (size_t i = 0; i < recs->n; i++) n += (i64)recs->v[i]->candidates.n;
        return n == strtoll(e->value, NULL, 10);
    }
    if (!strcmp(e->key, "winner")) {
        /* the verdict-giving candidate: exact and clean, else closer and clean */
        for (size_t i = 0; i < recs->n; i++) {
            const SvRecord *r = recs->v[i];
            for (int pass = 0; pass < 2; pass++)
                for (size_t k = 0; k < r->candidates.n; k++) {
                    const SvCandidate *c = &r->candidates.v[k];
                    bool clean = c->compiled && !c->broken.n && !c->rules.n;
                    bool win = pass == 0 ? clean && c->exact
                                         : clean && c->rows_after >= 0 && c->rows_after < c->rows_before;
                    if (win && r->verdict == (pass == 0 ? SV_MATCH : SV_CLOSER) && !strcmp(c->form, e->value))
                        any = true;
                }
        }
        return any;
    }
    for (size_t i = 0; i < recs->n; i++) {
        const SvRecord *r = recs->v[i];
        if (!strcmp(e->key, "class") && !strcmp(SV_CLASS_NAMES[r->cls], e->value)) any = true;
        if ((!strcmp(e->key, "verdict") || !strcmp(e->key, "verdict-not")) &&
            !strcmp(SV_VERDICT_NAMES[r->verdict], e->value))
            any = true;
        if (!strcmp(e->key, "callee") && r->callee && !strcmp(r->callee, e->value)) any = true;
    }
    return !strcmp(e->key, "verdict-not") ? !any : any;
}

/* ---- --apply --------------------------------------------------------------------------------------------------- */

static const char *out_dir(AvpArena *a, AvpArgs *args, const ShsmRepo *r) {
    AvpStr outdir_s = avp_args_str(args, "out");
    return outdir_s.p ? avp_path_abspath(a, avp_cz(a, outdir_s)) : avp_join(a, r->root, "build", "solver");
}

/* Candidate N of --function's last run, written into the unit's source; the follow-up the project requires. */
static int apply_cmd(AvpArena *a, AvpArgs *args, const ShsmRepo *r) {
    AvpStr fn = avp_args_str(args, "function");
    if (!fn.p || !fn.n) {
        fprintf(stderr, "solver.exe: --apply N needs --function SYM (the function of the run to read)\n");
        return SHSM_EXIT_ERROR;
    }
    if (!avp_args_is_none(args, "case")) {
        fprintf(stderr, "solver.exe: --apply does not combine with --case (a case edits the scratch text)\n");
        return SHSM_EXIT_ERROR;
    }
    SvApplied ap = sv_apply(a, r->root, out_dir(a, args, r), avp_cz(a, fn), avp_args_int(args, "apply"));
    if (!ap.ok) {
        fprintf(stderr, "solver.exe: --apply refused: %s\n", ap.error);
        return SHSM_EXIT_ERROR;
    }
    ShsmOutput *out = shsm_output_open(args, 0, -1, -1);
    AvpStrVec lines = {0};
    avp_vec_push(&lines, avp_str(avp_sprintf(a, "applied candidate #%lld of %.*s (%s): %s", (long long)avp_args_int(args, "apply"),
                                             AVP_SF(fn), ap.form, ap.desc)));
    avp_vec_push(&lines, avp_str(avp_sprintf(a, "rows %lld -> %lld%s; wrote %s (the only file written; nothing committed)",
                                             (long long)ap.rows_before, (long long)ap.rows_after,
                                             ap.exact ? ", exact" : "", ap.source)));
    avp_vec_push(&lines, AVP_S("a draft until the project's checks pass; run, in order:"));
    avp_vec_push(&lines, avp_str(avp_sprintf(a, "  build/tools/match.exe %s", ap.unit)));
    avp_vec_push(&lines, avp_str(avp_sprintf(a, "  build/tools/check_rules.exe --units %s", ap.unit)));
    avp_vec_push(&lines, AVP_S("  build/tools/gate.exe"));
    shsm_output_lines(out, lines.v, lines.n);
    avp_vec_free(&lines);
    shsm_output_close(out);
    return SHSM_EXIT_OK;
}

/* ---- the command ----------------------------------------------------------------------------------------------- */

static int cmd(void *ctx, AvpArgs *args) {
    AVP_UNUSED(ctx);
    AvpArena *a = avp_arena();
    double t0 = avp_time();
    AvpStr root = avp_args_str(args, "root");
    const ShsmRepo *r = shsm_repo_find(root.p ? avp_cz(a, root) : NULL);
    if (!avp_args_is_none(args, "apply")) return apply_cmd(a, args, r);
    SvRun *run = AVP_NEW(a, SvRun);
    run->a = a;
    run->r = r;
    run->rulings = sv_rulings_load(a, r);
    run->diagnose_only = avp_args_bool(args, "diagnose_only");
    /* --trace: the kinds of compiler trace (default all); --no-trace: none */
    {
        AvpStr tl = avp_args_str(args, "trace");
        bool none = avp_args_bool(args, "no_trace");
        for (int k = 0; k < SV_NTRACE; k++) run->trace_kind[k] = !none && !tl.p;
        if (tl.p && !none) {
            AvpStrVec ws = avp_str_split(tl, AVP_S(","), -1);
            for (size_t i = 0; i < ws.n; i++) {
                AvpStr w = avp_str_strip(ws.v[i]);
                int hit = -1;
                for (int k = 0; k < SV_NTRACE; k++)
                    if (avp_str_eqz(w, SV_TRACE_NAMES[k])) hit = k;
                if (hit < 0) shsm_raise(SHSM_EXC_USAGE, "--trace takes inline, regalloc, sched (comma-separated), not %.*s", AVP_SF(w));
                run->trace_kind[hit] = true;
            }
        }
        run->trace = false;
        for (int k = 0; k < SV_NTRACE; k++) run->trace = run->trace || run->trace_kind[k];
    }
    mwt_set_quiet(true); /* a traced compile's console output goes to its trace folder (compiler.txt) */
    run->flag_controls = avp_map_new();
    run->limit = avp_args_is_none(args, "limit") ? 0 : avp_args_int(args, "limit");
    /* --minutes: the per-stop budget of Phase 2's candidates and probes (default 2) */
    run->stop_seconds = 60.0 * (avp_args_is_none(args, "minutes") ? 2.0 : avp_args_float(args, "minutes"));
    run->jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    AvpStr fn = avp_args_str(args, "function");
    run->function = fn.p ? avp_cz(a, fn) : NULL;

    SvCase *cs = NULL;
    AvpStr case_path = avp_args_str(args, "case");
    if (case_path.p) {
        cs = AVP_NEW(a, SvCase);
        AvpBytes b = {0};
        if (!avp_read_all(a, avp_cz(a, case_path), &b)) shsm_raise_io();
        const char *err = NULL;
        if (!sv_case_parse(a, avp_strn((const char *)b.v, b.n), avp_cz(a, case_path), cs, &err))
            shsm_raise(SHSM_EXC_USAGE, "%s", err);
        run->function = cs->function;
    }
    const ShsmUnit **sel = NULL;
    size_t n = select_units(a, args, r, cs ? cs->unit : NULL, &sel);
    run->symtab = shsm_load_symbols(r, true);
    AvpTry t;
    if (AVP_TRY(&t)) {
        run->report = cs ? NULL : shsm_load_report(r); /* a case edits the source: the report does not describe it */
        avp_try_end(&t);
    } else {
        run->report = NULL;
    }

    SvUnitWork **works = AVP_NEWN(a, SvUnitWork *, n + 1);
    for (size_t i = 0; i < n; i++) {
        SvUnitWork *w = AVP_NEW(a, SvUnitWork);
        w->u = sel[i];
        sv_scorer_init(a, &w->sc, r, sel[i]);
        w->unit_only = unit_only_of(a, r, sel[i]);
        if (cs) {
            AvpBytes b = {0};
            const char *path = avp_path_join(a, r->root, sel[i]->source);
            if (!avp_read_all(a, path, &b)) shsm_raise_io();
            const char *err = NULL;
            if (!sv_case_apply(a, cs, avp_strn((const char *)b.v, b.n), &w->text0, &err))
                shsm_raise(SHSM_EXC_USAGE, "%s", err);
        }
        works[i] = w;
    }
    i64 jobs = avp_args_is_none(args, "jobs") ? 0 : avp_args_int(args, "jobs");
    if (!jobs) jobs = shsm_default_jobs();
    JobCtx jc = {r, cs != NULL};
    double tc = avp_time();
    if (jobs <= 1 || n <= 1)
        for (size_t i = 0; i < n; i++) unit_job(&jc, i, &works[i], NULL);
    else
        avp_pool_map(NULL, (int)AVP_MIN((i64)n, jobs), unit_job, &jc, works, n, sizeof(void *), NULL, 0);
    double compile_secs = avp_time() - tc;

    open_indexes(run);
    SvRecords recs = {0};
    AvpJson *controls = avp_json_obj(a);
    for (size_t i = 0; i < n; i++) {
        SvUnitWork *w = works[i];
        AvpJson *c = avp_json_obj(a);
        avp_json_set(a, c, AVP_S("pipeline"), avp_json_bool(w->pipeline_ok));
        avp_json_set(a, c, AVP_S("why"), w->pipeline_ok || !w->pipeline_why ? avp_json_null()
                                                                              : avp_json_strz(a, w->pipeline_why));
        avp_json_set(a, controls, avp_str(w->u->name), c);
        if (w->error) {
            avp_vec_push(&run->run_notes, avp_str(avp_sprintf(a, "%s: %s", w->u->name, w->error)));
            continue;
        }
        if (run->deadline && avp_time() > run->deadline) {
            run->deadline_hit = true;
            avp_vec_push(&run->run_notes, avp_str(avp_sprintf(a, "%s: deadline reached, not processed", w->u->name)));
            continue;
        }
        w->src = sv_source_parse(a, w->text0);
        if (w->preprocessed.p) w->pp = sv_source_parse(a, w->preprocessed);
        else
            avp_vec_push(&run->run_notes, avp_str(avp_sprintf(a, "%s: preprocessing failed (%s): mentions read from the "
                                                                 "unit source only",
                                                              w->u->name, w->pp_error ? w->pp_error : "?")));
        size_t first_rec = recs.n;
        sv_phase0_flags(run, w, &recs);
        sv_phase0_stops(run, w, &recs);
        const char *sha = avp_sha1_hex(a, w->text0.p, w->text0.n);
        for (size_t k = first_rec; k < recs.n; k++) {
            recs.v[k]->source = w->u->source;
            recs.v[k]->source_sha1 = sha;
            recs.v[k]->from_case = cs != NULL;
        }
    }
    double secs = avp_time() - t0;

    /* outputs */
    sv_number_records(&recs);
    SvCauses causes = sv_group_causes(a, &recs);
    SvTiming timing = sv_timing(&recs);
    const char *outdir = out_dir(a, args, r);
    if (!avp_makedirs(outdir, true)) shsm_raise_io();
    AvpJson *doc = avp_json_obj(a);
    avp_json_set(a, doc, AVP_S("version"), avp_json_int(a, 1));
    avp_json_set(a, doc, AVP_S("seconds"), avp_json_float(a, avp_py_round(secs, 2)));
    avp_json_set(a, doc, AVP_S("compile_seconds"), avp_json_float(a, avp_py_round(compile_secs, 2)));
    AvpJson *units = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, units, avp_json_strz(a, works[i]->u->name));
    avp_json_set(a, doc, AVP_S("units"), units);
    avp_json_set(a, doc, AVP_S("controls"), controls);
    i64 by_class[SV_NCLASS] = {0}, by_verdict[SV_NVERDICT] = {0};
    for (size_t i = 0; i < recs.n; i++) {
        by_class[recs.v[i]->cls]++;
        by_verdict[recs.v[i]->verdict]++;
    }
    AvpJson *counts = avp_json_obj(a), *cc = avp_json_obj(a), *cv = avp_json_obj(a);
    for (int k = 0; k < SV_NCLASS; k++) avp_json_set(a, cc, avp_str(SV_CLASS_NAMES[k]), avp_json_int(a, by_class[k]));
    for (int k = 0; k < SV_NVERDICT; k++)
        avp_json_set(a, cv, avp_str(SV_VERDICT_NAMES[k]), avp_json_int(a, by_verdict[k]));
    avp_json_set(a, counts, AVP_S("class"), cc);
    avp_json_set(a, counts, AVP_S("verdict"), cv);
    avp_json_set(a, doc, AVP_S("counts"), counts);
    AvpJson *notes = avp_json_arr(a);
    for (size_t i = 0; i < run->run_notes.n; i++) avp_json_arr_push(a, notes, avp_json_str(a, run->run_notes.v[i]));
    if (run->deadline_hit) avp_json_arr_push(a, notes, avp_json_strz(a, "--minutes deadline reached"));
    avp_json_set(a, doc, AVP_S("notes"), notes);
    AvpJson *jr = avp_json_arr(a);
    for (size_t i = 0; i < recs.n; i++) avp_json_arr_push(a, jr, sv_record_json(a, recs.v[i]));
    avp_json_set(a, doc, AVP_S("records"), jr);
    AvpJson *jc2 = avp_json_arr(a);
    for (size_t i = 0; i < causes.n; i++) {
        AvpJson *x = avp_json_obj(a);
        avp_json_set(a, x, AVP_S("cause"), avp_json_strz(a, causes.v[i].cause));
        avp_json_set(a, x, AVP_S("count"), avp_json_int(a, (i64)causes.v[i].count));
        AvpJson *fs = avp_json_arr(a);
        for (size_t k = 0; k < causes.v[i].functions.n; k++)
            avp_json_arr_push(a, fs, avp_json_strz(a, causes.v[i].functions.v[k]));
        avp_json_set(a, x, AVP_S("functions"), fs);
        avp_json_arr_push(a, jc2, x);
    }
    avp_json_set(a, doc, AVP_S("causes"), jc2);
    AvpJson *jt = avp_json_obj(a);
    avp_json_set(a, jt, AVP_S("stops"), avp_json_int(a, (i64)timing.stops));
    avp_json_set(a, jt, AVP_S("solved"), avp_json_int(a, (i64)timing.solved));
    avp_json_set(a, jt, AVP_S("total_solved_seconds"), avp_json_float(a, avp_py_round(timing.total_solved, 2)));
    avp_json_set(a, jt, AVP_S("median_seconds"), avp_json_float(a, avp_py_round(timing.median, 2)));
    avp_json_set(a, jt, AVP_S("max_seconds"), avp_json_float(a, avp_py_round(timing.max, 2)));
    avp_json_set(a, jt, AVP_S("budget_hit"), avp_json_int(a, (i64)timing.budget_hit));
    avp_json_set(a, jt, AVP_S("trace"), avp_json_bool(run->trace));
    avp_json_set(a, jt, AVP_S("trace_seconds"), avp_json_float(a, avp_py_round(run->trace_seconds, 2)));
    avp_json_set(a, jt, AVP_S("traces"), avp_json_int(a, (i64)run->ntraces));
    avp_json_set(a, jt, AVP_S("traces_failed"), avp_json_int(a, (i64)run->traces_failed));
    avp_json_set(a, jt, AVP_S("traces_skipped"), avp_json_int(a, (i64)run->traces_skipped));
    avp_json_set(a, jt, AVP_S("decl_swaps_dropped"), avp_json_int(a, (i64)run->decl_dropped));
    {
        AvpJson *kinds = avp_json_obj(a);
        for (int k = 0; k < SV_NTRACE; k++)
            avp_json_set(a, kinds, avp_str(SV_TRACE_NAMES[k]),
                         run->trace_kind[k] ? avp_json_float(a, avp_py_round(run->trace_kind_seconds[k], 2)) : avp_json_null());
        avp_json_set(a, jt, AVP_S("trace_kind_seconds"), kinds);
    }
    avp_json_set(a, doc, AVP_S("timing"), jt);
    bool case_failed = false;
    if (cs) {
        AvpJson *ex = avp_json_arr(a);
        for (size_t i = 0; i < cs->expects.n; i++) {
            bool held = expect_held(&cs->expects.v[i], &recs);
            case_failed = case_failed || !held;
            AvpJson *x = avp_json_obj(a);
            avp_json_set(a, x, AVP_S("expect"), avp_json_strz(a, avp_sprintf(a, "%s %s", cs->expects.v[i].key,
                                                                             cs->expects.v[i].value)));
            avp_json_set(a, x, AVP_S("held"), avp_json_bool(held));
            avp_json_arr_push(a, ex, x);
        }
        avp_json_set(a, doc, AVP_S("case"), ex);
    }
    AvpJsonDumpOpts jo = shsm_json_opts();
    AvpStr dumped = avp_json_dumps(a, doc, &jo);
    if (!avp_write_file(avp_path_join(a, outdir, "records.json"), dumped.p, dumped.n)) shsm_raise_io();
    AvpStr tsv = sv_records_tsv(a, &recs), ctsv = sv_causes_tsv(a, &causes);
    if (!avp_write_file(avp_path_join(a, outdir, "records.tsv"), tsv.p, tsv.n)) shsm_raise_io();
    if (!avp_write_file(avp_path_join(a, outdir, "causes.tsv"), ctsv.p, ctsv.n)) shsm_raise_io();
    if (!sv_apply_save(a, outdir, &recs)) shsm_raise_io();

    ShsmOutput *out = shsm_output_open(args, 0, -1, -1);
    AvpStrVec lines = {0};
    for (size_t i = 0; i < recs.n; i++) {
        sv_record_lines(a, recs.v[i], &lines);
        avp_vec_push(&lines, AVP_S(""));
    }
    AvpBuf cb;
    avp_buf_init(&cb, a);
    avp_buf_printf(&cb, "classes:");
    for (int k = 0; k < SV_NCLASS; k++)
        if (by_class[k]) avp_buf_printf(&cb, " %s %lld", SV_CLASS_NAMES[k], (long long)by_class[k]);
    avp_buf_printf(&cb, "; verdicts:");
    for (int k = 0; k < SV_NVERDICT; k++)
        if (by_verdict[k]) avp_buf_printf(&cb, " %s %lld", SV_VERDICT_NAMES[k], (long long)by_verdict[k]);
    avp_vec_push(&lines, avp_buf_str(&cb));
    for (size_t i = 0; i < run->run_notes.n; i++)
        avp_vec_push(&lines, avp_str(avp_sprintf(a, "note: %.*s", AVP_SF(run->run_notes.v[i]))));
    if (cs)
        for (size_t i = 0; i < cs->expects.n; i++)
            avp_vec_push(&lines, avp_str(avp_sprintf(a, "expect %s %s: %s", cs->expects.v[i].key, cs->expects.v[i].value,
                                                     expect_held(&cs->expects.v[i], &recs) ? "held" : "FAILED")));
    avp_vec_push(&lines, avp_str(avp_sprintf(a, "%zu record(s) from %zu unit(s) in %.1f s (compiles %.1f s); %s",
                                             recs.n, n, secs, compile_secs, outdir)));
    AvpBuf kb;
    avp_buf_init(&kb, a);
    for (int k = 0; k < SV_NTRACE; k++)
        avp_buf_printf(&kb, "%s%s %s", k ? ", " : "", SV_TRACE_NAMES[k],
                       run->trace_kind[k] ? avp_sprintf(a, "%.1f s", run->trace_kind_seconds[k]) : "off");
    const char *tr = run->trace ? avp_sprintf(a, "; tracing %.1f s [%s] (%zu traced compile(s), %zu failed, %zu skipped by a "
                                                 "budget; %zu unevidenced declaration swap(s) dropped by the same-sweep "
                                                 "filter)",
                                              run->trace_seconds, kb.p, run->ntraces, run->traces_failed, run->traces_skipped,
                                              run->decl_dropped)
                                : "; tracing off (--no-trace)";
    if (timing.solved)
        avp_vec_push(&lines, avp_str(avp_sprintf(a, "time: %zu solved stop(s) (candidates compiled): total %.1f s, median %.1f "
                                                    "s, maximum %.1f s per stop; %zu stop(s) hit the --minutes budget%s",
                                                 timing.solved, timing.total_solved, timing.median, timing.max,
                                                 timing.budget_hit, tr)));
    else
        avp_vec_push(&lines, avp_str(avp_sprintf(a, "time: no stop ran candidates; %zu stop(s) hit the --minutes budget%s",
                                                 timing.budget_hit, tr)));
    avp_vec_push(&lines, AVP_S(""));
    sv_tables(a, &recs, &causes, &lines);
    shsm_output_lines(out, lines.v, lines.n);
    avp_vec_free(&lines);
    shsm_output_data(out, "document", doc);
    shsm_output_close(out);
    if (run->sibling_db) sibling_close(run->sibling_db);
    return case_failed ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}

int sv_main(int argc, char **argv) {
    AvpArgParser *ap = shsm_cli_parser(
        "solver.exe",
        "The decision solver: for every stop (a non-exact function of the chosen units, default every NonMatching "
        "unit) state which compiler decision diverged and the condition retail's decision implies, from evidence "
        "and, unless --no-trace, in the compiler's own numbers (the inliner's, the register allocator's and the scheduler's traces). Phase 0 (stop list, controls, parked-exact flag records), Phase 1 "
        "(inline / call-set decisions), Phase 2 (register / schedule: the order test, DWARF comparison, candidates, probes) and Phase 3 "
        "(control flow: wall's control finding, branch-structure candidates) are built. Ends with a table per record and a table of causes. "
        "Works on scratch copies under build/tmp/solver; never writes src/ or include/.",
        true, true, false);
    avp_arg(ap, &(AvpArgDesc){.names = "units", .nargs = AVP_NARGS_STAR, .metavar = "UNIT",
                              .help = "units (default: every NonMatching unit)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--lib", .help = "only the NonMatching units of this library"});
    avp_arg(ap, &(AvpArgDesc){.names = "--function", .metavar = "SYM", .help = "only this function"});
    avp_arg(ap, &(AvpArgDesc){.names = "--limit", .type = AVP_T_INT, .dflt = AVP_DEF_NONE, .metavar = "N",
                              .help = "at most N stops"});
    avp_arg(ap, &(AvpArgDesc){.names = "--diagnose-only", .action = AVP_ARG_STORE_TRUE,
                              .help = "classify and state conditions; compile no probe"});
    avp_arg(ap, &(AvpArgDesc){.names = "--trace", .metavar = "KINDS",
                              .help = "the compiler traces to make, comma-separated: inline, regalloc, sched (default "
                                      "all)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--no-trace", .action = AVP_ARG_STORE_TRUE,
                              .help = "state no condition in the compiler's numbers: no inliner, register "
                                      "allocator or scheduler trace (mwtrace's library) is made; the register model "
                                      "check is then the declaration-order rule"});
    avp_arg(ap, &(AvpArgDesc){.names = "--minutes", .type = AVP_T_FLOAT, .dflt = AVP_DEF_NONE, .metavar = "M",
                              .help = "the time budget of each register stop's candidates and probes, in minutes "
                                      "(default 2)"});
    avp_arg(ap, &(AvpArgDesc){.names = "-j --jobs", .type = AVP_T_INT, .dflt = AVP_DEF_NONE, .metavar = "N",
                              .help = "parallel unit compiles (default: CPU count)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--out", .metavar = "DIR", .help = "output folder (default build/solver)"});
    avp_arg(ap, &(AvpArgDesc){.names = "--case", .metavar = "FILE",
                              .help = "a case file: unit, function, expectations, find/replace edits applied to "
                                      "the scratch copy"});
    avp_arg(ap, &(AvpArgDesc){.names = "--apply", .type = AVP_T_INT, .dflt = AVP_DEF_NONE, .metavar = "N",
                              .help = "write candidate N of --function's last run (build/solver/apply) into its unit's "
                                      "source: refuses a probe, a candidate that is not exact or closer, one with "
                                      "broken functions or rule findings, a --case run and a source that changed "
                                      "since the run; never commits"});
    return shsm_cli_main(ap, argc, argv, cmd, NULL);
}
