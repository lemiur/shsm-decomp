/* solver_stops: Phase 0, the clean stop list. Stops are the non-exact functions of a unit as wall classifies them
 * (wall_classify_result on the stop's reference compile), sorted into the solver's classes by the rules of the
 * brief:
 *   - an INCOMPLETE? verdict, or a function not emitted that the unit source does not define: not-started (skipped);
 *   - not emitted but defined in the source: inline (ours expands it everywhere and drops the copy);
 *   - EQUIVALENT (proved) and EQUIVALENT-LOOKING: register;
 *   - a call finding (missing-in-compiled, extra-in-compiled, different) whose callee the function's source closure
 *     does not name: source-call (the source lacks the call; not a compiler decision); a named one: inline;
 *   - a "compiled calls D, retail does not" side is dropped when D is a retail callee of an intermediate I ours
 *     expanded: the cause is I;
 *   - otherwise wall's class (referent, control flow, register/schedule, other; a call set without a callee
 *     difference is other).
 * Parked-exact units (all functions exact under a flag the other units of the library lack) give one `flag` record
 * per function that loses exactness without the flag (sv_phase0_flags). */
#include "solver.h"
#include "wall.h"

/* ---- the pure rules -------------------------------------------------------------------------------------------- */

static bool kind_is(const char *k, const char *z) { return k && strcmp(k, z) == 0; }

static void add_side(AvpArena *a, SvDecisions *ds, const char *callee, const char *kind, bool retail_calls) {
    for (size_t i = 0; i < ds->n; i++)
        if (ds->v[i].retail_calls == retail_calls && strcmp(ds->v[i].callee, callee) == 0) {
            ds->v[i].count++;
            return;
        }
    SvDecision d = {0};
    d.callee = avp_strdup(a, callee);
    d.kind = kind;
    d.retail_calls = retail_calls;
    d.count = 1;
    avp_vec_push(ds, d);
}

SvStop sv_classify_stop(AvpArena *a, const AvpJson *d, bool defined_in_source, SvMentionFn mention, void *mctx,
                        SvRetailCallsFn retail_calls, void *rctx) {
    SvStop st = {0};
    AvpStr verdict = avp_json_get_str(d, "verdict", AVP_S(""));
    const AvpJson *name = avp_json_getz(d, "name");
    bool missing = avp_json_get_bool(d, "missing", false) || !name || avp_json_is_null(name);
    AvpStr retail_name = avp_json_get_str(d, "retail_name", AVP_S(""));
    if (avp_str_startswith(verdict, AVP_S("INCOMPLETE"))) {
        st.cls = SV_NOT_STARTED;
        st.rule = "wall's INCOMPLETE? verdict: the body is not complete";
        return st;
    }
    if (missing) {
        if (!defined_in_source) {
            st.cls = SV_NOT_STARTED;
            st.rule = "not emitted, and the unit source does not define it";
            return st;
        }
        st.cls = SV_INLINE;
        st.rule = "not emitted but defined in the unit source: ours expands it at every call and drops the copy";
        SvDecision dec = {0};
        dec.callee = avp_cz(a, retail_name);
        dec.kind = "not-emitted";
        dec.retail_calls = true;
        dec.cls = SV_INLINE;
        dec.via = "";
        dec.count = 1;
        avp_vec_push(&st.decisions, dec);
        return st;
    }
    if (avp_str_startswith(verdict, AVP_S("EQUIVALENT"))) {
        st.cls = SV_REGISTER;
        st.rule = "wall proves or finds the bodies equivalent (EQUIVALENT / EQUIVALENT-LOOKING): register";
        return st;
    }
    const AvpJson *calls = avp_json_getz(d, "calls");
    const AvpJson *findings = calls ? avp_json_getz(calls, "findings") : NULL;
    SvDecisions sides = {0};
    for (size_t i = 0; findings && i < avp_json_len(findings); i++) {
        const AvpJson *f = avp_json_at(findings, i);
        const char *kind = avp_cz(a, avp_json_get_str(f, "kind", AVP_S("")));
        if (kind_is(kind, "different")) {
            add_side(a, &sides, avp_cz(a, avp_json_get_str(f, "retail", AVP_S(""))), kind, true);
            add_side(a, &sides, avp_cz(a, avp_json_get_str(f, "compiled", AVP_S(""))), kind, false);
        } else {
            bool rc = kind_is(kind, "missing-in-compiled") || kind_is(kind, "inlined-in-compiled");
            add_side(a, &sides, avp_cz(a, avp_json_get_str(f, "callee", AVP_S(""))), kind, rc);
        }
    }
    for (size_t i = 0; i < sides.n; i++) {
        SvDecision *s = &sides.v[i];
        const char *via = NULL;
        bool named = mention && mention(mctx, s->callee, &via);
        s->via = named ? via : NULL;
        bool source_kind =
            kind_is(s->kind, "missing-in-compiled") || kind_is(s->kind, "extra-in-compiled") || kind_is(s->kind, "different");
        s->cls = (!named && source_kind) ? SV_SOURCE_CALL : SV_INLINE;
    }
    /* drop "compiled calls D, retail does not" when an intermediate I ours expanded calls D in retail */
    for (size_t i = 0; i < sides.n; i++) {
        SvDecision *s = &sides.v[i];
        if (s->retail_calls) continue;
        for (size_t k = 0; k < sides.n; k++) {
            SvDecision *im = &sides.v[k];
            if (!im->retail_calls || im->cls != SV_INLINE || !retail_calls) continue;
            if (!retail_calls(rctx, im->callee, s->callee)) continue;
            avp_vec_push(&im->explains, avp_sprintf(a, "compiled calls %s, retail does not", s->callee));
            s->kind = NULL; /* dropped */
            break;
        }
    }
    for (size_t i = 0; i < sides.n; i++)
        if (sides.v[i].kind) avp_vec_push(&st.decisions, sides.v[i]);
    avp_vec_free(&sides);
    if (st.decisions.n) {
        st.cls = st.decisions.v[0].cls;
        st.rule = "a call difference wall found";
        return st;
    }
    AvpStr wall = avp_json_get_str(d, "wall", AVP_S(""));
    if (avp_str_eqz(wall, WALL_REFERENT)) st.cls = SV_REFERENT;
    else if (avp_str_eqz(wall, WALL_CONTROL)) st.cls = SV_CONTROL_FLOW;
    else if (avp_str_eqz(wall, WALL_REGISTER)) st.cls = SV_REGISTER;
    else st.cls = SV_OTHER;
    st.rule = avp_str_eqz(wall, WALL_CALL) ? "a call-set wall without a callee difference (order or save helpers)"
                                           : "wall's class";
    return st;
}

/* ---- flag groups ----------------------------------------------------------------------------------------------- */

SvGroups sv_flag_groups(AvpArena *a, const AvpStrVec *cflags) {
    SvGroups out = {0};
    for (size_t i = 0; i < cflags->n; i++) {
        AvpStr w = cflags->v[i];
        if (w.n && w.p[0] == '-' || !out.n) {
            AvpStrVec g = {0};
            avp_vec_push(&g, avp_str_dup(a, w));
            avp_vec_push(&out, g);
        } else {
            avp_vec_push(&out.v[out.n - 1], avp_str_dup(a, w));
        }
    }
    return out;
}

const char *sv_group_text(AvpArena *a, const AvpStrVec *g) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < g->n; i++) {
        if (i) avp_buf_putc(&b, ' ');
        bool q = avp_str_contains(g->v[i], AVP_S(" "));
        if (q) avp_buf_putc(&b, '"');
        avp_buf_put(&b, g->v[i]);
        if (q) avp_buf_putc(&b, '"');
    }
    return b.p ? b.p : "";
}

static bool groups_have(AvpArena *a, const SvGroups *gs, const char *text) {
    for (size_t i = 0; i < gs->n; i++)
        if (strcmp(sv_group_text(a, &gs->v[i]), text) == 0) return true;
    return false;
}

SvGroups sv_unit_only_flags(AvpArena *a, const ShsmUnit *u, const ShsmUnit *const *others, size_t nothers) {
    SvGroups out = {0};
    if (!u->has_cflags || !nothers) return out;
    SvGroups mine = sv_flag_groups(a, &u->cflags);
    SvGroups *theirs = AVP_NEWN(a, SvGroups, nothers + 1);
    for (size_t k = 0; k < nothers; k++) theirs[k] = sv_flag_groups(a, &others[k]->cflags);
    for (size_t i = 0; i < mine.n; i++) {
        const char *t = sv_group_text(a, &mine.v[i]);
        bool anywhere = false;
        for (size_t k = 0; k < nothers && !anywhere; k++) anywhere = groups_have(a, &theirs[k], t);
        if (!anywhere && !groups_have(a, &out, t)) avp_vec_push(&out, mine.v[i]);
    }
    return out;
}

/* ---- the unit drivers ------------------------------------------------------------------------------------------ */

static void fill_from_wall(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpJson *d, const CcCompareResult *res) {
    AvpArena *a = run->a;
    AvpStr wall = avp_json_get_str(d, "wall", AVP_S(""));
    rec->wall = wall.n ? avp_cz(a, wall) : NULL;
    rec->wall_verdict = avp_cz(a, avp_json_get_str(d, "verdict", AVP_S("")));
    const AvpJson *cl = avp_json_getz(d, "classes");
    for (size_t i = 0; cl && i < avp_json_len(cl); i++)
        avp_vec_push(&rec->wall_classes, avp_cz(a, avp_json_str_or(avp_json_at(cl, i), AVP_S(""))));
    const AvpJson *rows = avp_json_getz(d, "rows");
    if (rows) {
        rec->rows_retail = avp_json_get_i64(rows, "retail", -1);
        rec->rows_compiled = avp_json_get_i64(rows, "compiled", -1);
        rec->rows_differing = avp_json_get_i64(rows, "differing", -1);
    }
    if (run->report) {
        const ShsmFunctionReport *fr = shsm_report_function(run->report, rec->function, w->u->name);
        if (fr) {
            rec->has_percent = true;
            rec->percent = avp_py_round(fr->fuzzy, 2);
        }
    }
    /* the first differing row of wall's first class */
    const AvpJson *ev = avp_json_getz(d, "row_evidence");
    const AvpJson *list = ev && rec->wall ? avp_json_getz(ev, rec->wall) : NULL;
    if (!list && ev && avp_json_is_obj(ev) && ev->u.obj.n) list = ev->u.obj.v[0].val;
    const AvpJson *first = list && avp_json_len(list) ? avp_json_at(list, 0) : NULL;
    if (first) {
        i64 ri = avp_json_get_i64(first, "retail", -1), ci = avp_json_get_i64(first, "compiled", -1);
        if (res && res->retail_fn && ri >= 0 && (size_t)ri < res->retail_fn->nrows)
            rec->site_addr = res->retail_fn->rows[ri].addr;
        rec->site_row = ci;
        AvpStr rt = avp_json_get_str(first, "retail_text", AVP_S("")), ct = avp_json_get_str(first, "compiled_text", AVP_S(""));
        if (ct.n) rec->ours = avp_cz(a, ct);
        if (rt.n) rec->retail = avp_cz(a, rt);
    } else if (res) {
        for (size_t i = 0; i < res->npairs; i++)
            if (cc_pair_differs(&res->pairs[i])) {
                if (res->pairs[i].retail) rec->site_addr = res->pairs[i].retail->address;
                if (res->pairs[i].compiled) rec->site_row = (i64)res->pairs[i].compiled->index;
                break;
            }
    }
}

typedef struct {
    SvRun *run;
    SvUnitWork *w;
    SvMentions mentions;
    bool known;
} MentionCtx;

static bool mention_fn(void *ctx, const char *callee, const char **via) {
    MentionCtx *m = ctx;
    if (!m->known) {
        *via = "?";
        return true;
    }
    const char *nm = sv_source_name(m->run->a, callee);
    if (!nm) return false;
    AvpVal *v = avp_map_get(m->mentions.names, avp_str(nm));
    if (!v) return false;
    *via = v->p ? v->p : "";
    return true;
}

typedef struct {
    SvRun *run;
    SvUnitWork *w;
} RetailCtx;

static bool retail_calls_fn(void *ctx, const char *intermediate, const char *callee) {
    RetailCtx *rc = ctx;
    AvpArena *a = rc->run->a;
    const CcFn *fn = NULL;
    if (rc->w->ref && rc->w->ref->oc) {
        AvpVal *v = avp_map_get(rc->w->ref->oc->retail, avp_str(intermediate));
        if (v) fn = v->p;
    }
    if (!fn && rc->run->symtab) {
        const ShsmSymbol *s = shsm_sym_by_name(rc->run->symtab, avp_str(intermediate));
        const char *path = s ? shsm_sym_unit_of(rc->run->symtab, s->address) : NULL;
        if (path) {
            CcRepo cr = {rc->run->r->root, rc->run->r->version};
            CcError err = {0};
            AvpMap *m = cc_load_retail(&cr, avp_str(shsm_unit_name(a, path)), NULL, false, &err);
            AvpVal *v = m ? avp_map_get(m, avp_str(intermediate)) : NULL;
            if (v) fn = v->p;
        }
    }
    if (!fn) return false;
    WallCall *calls = NULL;
    size_t n = wall_calls(a, fn->rows, fn->nrows, &calls);
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(calls[i].name, callee)) return true;
    return false;
}

static bool wanted(const SvRun *run, AvpStr name, AvpStr retail_name) {
    if (!run->function) return true;
    return avp_str_eqz(name, run->function) || avp_str_eqz(retail_name, run->function);
}

static void baseline_control(SvRun *run, SvUnitWork *w, SvRecord *rec, const char *symbol) {
    const char *why = NULL;
    if (!w->pipeline_ok) {
        rec->control_failed = true;
        sv_note(run->a, rec, "pipeline control failed: %s", w->pipeline_why ? w->pipeline_why : "?");
        return;
    }
    if (!sv_control_baseline(run->a, w->ref, w->base, symbol, &why)) {
        rec->control_failed = true;
        sv_note(run->a, rec, "baseline control failed: %s", why);
    }
}

static const char *phase_note(SvClass cls) {
    switch (cls) {
    case SV_REGISTER: return "phase not built (Phase 2: register / schedule)";
    case SV_CONTROL_FLOW: return "no control-flow candidates: Phase 3 needs an emitted function and passing controls";
    case SV_REFERENT: return "phase not built (referent)";
    default: return "phase not built";
    }
}

/* One stop: its records (one per call decision, else one). */
static void stop_records(SvRun *run, SvUnitWork *w, const AvpJson *d, const CcCompareResult *res, AvpStr retail_name, double t_stop0,
                         SvRecords *out) {
    AvpArena *a = run->a;
    const char *fname = avp_cz(a, retail_name);
    double t_prev = t_stop0;
    bool defined = false;
    LvFnDef def = {0}, ppdef = {0};
    if (w->src) defined = sv_source_find(a, w->src, fname, &def);
    MentionCtx mc = {run, w, {0}, false};
    if (w->pp && sv_source_find(a, w->pp, fname, &ppdef)) {
        mc.mentions = sv_source_mentions(a, w->pp, &ppdef);
        mc.known = mc.mentions.ok;
    } else if (defined) {
        mc.mentions = sv_source_mentions(a, w->src, &def);
        mc.known = mc.mentions.ok;
    }
    RetailCtx rc = {run, w};
    SvStop st = sv_classify_stop(a, d, defined, mention_fn, &mc, retail_calls_fn, &rc);
    const ShsmUnit *u = w->u;
    bool surviving = sv_rulings_surviving(&run->rulings, u->name, u->path) != NULL;
    if (!st.decisions.n) {
        SvRecord *rec = sv_record_new(a, u->name, fname, st.cls);
        fill_from_wall(run, w, rec, d, res);
        rec->surviving_source = surviving;
        if (st.cls == SV_NOT_STARTED) {
            rec->condition = NULL;
            sv_note(a, rec, "skipped: %s", st.rule);
        } else {
            rec->condition = rec->wall_verdict ? avp_sprintf(a, "wall: %s", rec->wall_verdict) : NULL;
            baseline_control(run, w, rec, fname);
            bool reg = st.cls == SV_REGISTER;
            for (size_t k = 0; st.cls == SV_OTHER && k < rec->wall_classes.n; k++)
                if (!strcmp(rec->wall_classes.v[k], WALL_REGISTER)) reg = true;
            if (st.cls == SV_CONTROL_FLOW && res && !rec->control_failed) {
                sv_phase3_control(run, w, rec, d, res);
            } else if (reg && res && !rec->control_failed) {
                if (st.cls == SV_OTHER) sv_evidence(a, rec, "wall lists register/schedule among its classes: Phase 2");
                sv_phase2_register(run, w, rec, d, res);
            } else {
                sv_note(a, rec, "%s", phase_note(st.cls));
                if (surviving) sv_note(a, rec, "surviving source: no respelling candidates");
            }
        }
        sv_evidence(a, rec, "Phase 0: %s", st.rule);
        sv_assign_verdict(rec);
        rec->seconds = avp_time() - t_prev;
        avp_vec_push(out, rec);
        return;
    }
    for (size_t i = 0; i < st.decisions.n; i++) {
        SvDecision *dec = &st.decisions.v[i];
        SvRecord *rec = sv_record_new(a, u->name, fname, dec->cls);
        fill_from_wall(run, w, rec, d, res);
        rec->callee = dec->callee;
        rec->direction = dec->retail_calls ? SV_DIR_OURS_EXPANDED : SV_DIR_RETAIL_EXPANDED;
        rec->surviving_source = surviving;
        rec->phase_built = true;
        sv_evidence(a, rec, "Phase 0: %s (%s, %d finding(s))", st.rule, dec->kind, dec->count);
        for (size_t k = 0; k < dec->explains.n; k++) sv_evidence(a, rec, "also explains: %s", dec->explains.v[k]);
        if (!mc.known) sv_note(a, rec, "the function's definition was not found in the source: whether it names the "
                                       "callee is unknown");
        baseline_control(run, w, rec, fname);
        if (dec->cls == SV_SOURCE_CALL) {
            rec->cause = avp_sprintf(a, "source-call %s", dec->callee);
            if (dec->retail_calls) {
                rec->ours = avp_sprintf(a, "does not call %s", dec->callee);
                rec->retail = avp_sprintf(a, "calls %s (%d more time(s))", dec->callee, dec->count);
                rec->condition = avp_sprintf(a, "the source must call %s: retail calls it, and neither the function's "
                                                "body nor a function of the same preprocessed text it names mentions "
                                                "%s (not a compiler decision)",
                                             dec->callee, sv_source_name(a, dec->callee));
            } else {
                rec->ours = avp_sprintf(a, "calls %s (%d more time(s))", dec->callee, dec->count);
                rec->retail = avp_sprintf(a, "does not call %s", dec->callee);
                rec->condition = avp_sprintf(a, "ours calls %s although the source closure never names %s (an "
                                                "implicit call); retail does not call it",
                                             dec->callee, sv_source_name(a, dec->callee));
            }
            if (surviving) sv_note(a, rec, "surviving source: no respelling candidates");
            sv_assign_verdict(rec);
        } else {
            sv_phase1_decision(run, w, rec, dec, res);
            if (surviving) sv_note(a, rec, "surviving source: no respelling candidates");
            sv_assign_verdict(rec);
        }
        double now = avp_time();
        rec->seconds = now - t_prev;
        t_prev = now;
        avp_vec_push(out, rec);
    }
}

/* --function naming an exact function: one line, no record, quoting the RULE-EXCEPTION comments of its body. */
static void exact_function_note(SvRun *run, SvUnitWork *w, const char *fname) {
    AvpArena *a = run->a;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%s: exact in %s", fname, w->u->name);
    AvpTry t;
    if (AVP_TRY(&t)) {
        LvCtx *x = lv_ctx_new(a, w->text0, fname);
        for (i32 i = 0; i < x->comments.n; i++) {
            LvS c = {x->comments.v[i].t, x->comments.v[i].n};
            if (!lv_s_contains(c, LVS("RULE-EXCEPTION"))) continue;
            AvpStr one = avp_str_replace(a, lv_strip(c), AVP_S("\r"), AVP_S(""), -1);
            one = avp_str_replace(a, one, AVP_S("\n"), AVP_S(" "), -1);
            avp_buf_printf(&b, "; its source carries: %.*s", AVP_SF(one));
        }
        avp_try_end(&t);
    }
    avp_vec_push(&run->run_notes, avp_buf_str(&b));
}

void sv_phase0_stops(SvRun *run, SvUnitWork *w, SvRecords *out) {
    AvpArena *a = run->a;
    if (!w->ref || !w->ref->oc) return;
    AvpSet *seen = avp_set_new();
    AvpMapIter it = {0};
    while (avp_map_next(w->ref->oc->results, &it)) {
        const CcCompareResult *res = it.val->p;
        avp_set_add(seen, res->retail_name);
        if (res->exact && run->function && wanted(run, it.key, res->retail_name)) {
            exact_function_note(run, w, avp_cz(a, res->retail_name));
            continue;
        }
        if (res->exact || !wanted(run, it.key, res->retail_name)) continue;
        if (run->limit && run->stops >= run->limit) break;
        if (run->deadline && avp_time() > run->deadline) {
            run->deadline_hit = true;
            break;
        }
        run->stops++;
        double t_stop0 = avp_time();
        AvpJson *d = wall_classify_result(a, w->u->name, res, run->r, run->symtab);
        stop_records(run, w, d, res, res->retail_name, t_stop0, out);
    }
    AvpMapIter jt = {0};
    while (avp_map_next(w->ref->oc->retail, &jt)) {
        const CcFn *rfn = jt.val->p;
        if (avp_set_has(seen, jt.key) || !rfn->nrows || cc_is_gap(jt.key)) continue;
        if (!wanted(run, jt.key, jt.key)) continue;
        if (run->limit && run->stops >= run->limit) break;
        run->stops++;
        double t_stop0 = avp_time();
        AvpJson *d = avp_json_obj(a);
        avp_json_set(a, d, AVP_S("name"), avp_json_null());
        avp_json_set(a, d, AVP_S("retail_name"), avp_json_str(a, jt.key));
        avp_json_set(a, d, AVP_S("exact"), avp_json_bool(false));
        avp_json_set(a, d, AVP_S("wall"), avp_json_strz(a, WALL_CALL));
        AvpJson *cl = avp_json_arr(a);
        avp_json_arr_push(a, cl, avp_json_strz(a, WALL_CALL));
        avp_json_set(a, d, AVP_S("classes"), cl);
        avp_json_set(a, d, AVP_S("verdict"), avp_json_strz(a, "WALL call set: not emitted by the compiled object "
                                                              "(inlined everywhere, or not defined)"));
        AvpJson *rows = avp_json_obj(a);
        avp_json_set(a, rows, AVP_S("retail"), avp_json_int(a, (i64)rfn->nrows));
        avp_json_set(a, rows, AVP_S("compiled"), avp_json_int(a, 0));
        avp_json_set(a, rows, AVP_S("differing"), avp_json_int(a, (i64)rfn->nrows));
        avp_json_set(a, d, AVP_S("rows"), rows);
        avp_json_set(a, d, AVP_S("missing"), avp_json_bool(true));
        stop_records(run, w, d, NULL, jt.key, t_stop0, out);
    }
    avp_set_free(seen);
}

void sv_phase0_flags(SvRun *run, SvUnitWork *w, SvRecords *out) {
    AvpArena *a = run->a;
    if (!w->all_exact || !w->unit_only.n) return;
    for (size_t k = 0; k < w->unit_only.n; k++) {
        const AvpStrVec *g = &w->unit_only.v[k];
        const char *flag = sv_group_text(a, g);
        const SvBuild *b = k < w->dropped.n ? w->dropped.v[k] : NULL;
        bool lever = sv_rulings_is_lever_flag(&run->rulings, flag);
        SvFlags without = {0};
        without.groups = g;
        without.ngroups = 1;
        const char *cwhy = NULL;
        int ctl = sv_run_flag_control(run, w, flag, &without, NULL, &cwhy);
        if (!b || !b->ok) {
            avp_vec_push(&run->run_notes,
                         avp_str(avp_sprintf(a, "%s: compiling without %s failed: %s", w->u->name, flag,
                                             b && b->error ? b->error : "not compiled")));
            continue;
        }
        size_t lost = 0, total = 0;
        AvpMapIter it = {0};
        while (avp_map_next(w->ref->oc->results, &it)) total++;
        it = (AvpMapIter){0};
        while (avp_map_next(w->ref->oc->results, &it)) {
            const CcCompareResult *res = it.val->p;
            const CcCompareResult *nres = sv_result(b, avp_cz(a, res->retail_name));
            if (nres && nres->exact) continue;
            if (!wanted(run, it.key, res->retail_name)) continue;
            lost++;
            const char *fname = avp_cz(a, res->retail_name);
            SvRecord *rec = sv_record_new(a, w->u->name, fname, SV_FLAG);
            rec->callee = flag;
            rec->cause = avp_sprintf(a, "flag %s", flag);
            rec->phase_built = true;
            if (nres) {
                AvpJson *d = wall_classify_result(a, w->u->name, nres, run->r, run->symtab);
                fill_from_wall(run, w, rec, d, nres);
                const AvpJson *calls = avp_json_getz(d, "calls");
                const AvpJson *readings = calls ? avp_json_getz(calls, "readings") : NULL;
                for (size_t i = 0; readings && i < avp_json_len(readings); i++)
                    sv_evidence(a, rec, "without %s, wall: %.*s", flag,
                                AVP_SF(avp_json_str_or(avp_json_at(readings, i), AVP_S(""))));
                if (!readings || !avp_json_len(readings))
                    sv_evidence(a, rec, "without %s, wall: %s (no call finding)", flag, rec->wall_verdict);
            } else {
                rec->ours = avp_sprintf(a, "not emitted without %s", flag);
                sv_evidence(a, rec, "without %s the object does not emit %s", flag, fname);
            }
            rec->ours = rec->ours ? avp_sprintf(a, "without %s: %s", flag, rec->ours) : NULL;
            rec->retail = rec->retail ? rec->retail : "the unit's bytes (exact with the flag)";
            rec->condition = avp_sprintf(a, "%s is exact only under the unit's per-file flag %s, which no other unit "
                                            "of library %s has: compiled without it, it differs",
                                         fname, flag, w->u->library ? w->u->library : "?");
            SvProbe p = {0};
            p.lever = avp_sprintf(a, "flag %s (the unit's per-file flag)", flag);
            p.result = avp_sprintf(a, "exact with it; %s without it", nres ? avp_sprintf(a, "%lld row(s) differ",
                                                                                    (long long)rec->rows_differing)
                                                                     : "not emitted");
            p.closes = true;
            p.lever_ruled = lever;
            avp_vec_push(&rec->probes, p);
            if (lever) sv_evidence(a, rec, "solver.tsv: lever-flag %s", flag);
            else sv_note(a, rec, "the flag is not a lever-flag row of solver.tsv");
            if (ctl == 1) sv_evidence(a, rec, "flag control: %s changes the generated control source", flag);
            else if (ctl == 0) sv_note(a, rec, "flag control: %s leaves the generated control source unchanged", flag);
            else {
                rec->control_failed = true;
                sv_note(a, rec, "flag control failed: %s", cwhy ? cwhy : "?");
            }
            if (!w->pipeline_ok) {
                rec->control_failed = true;
                sv_note(a, rec, "pipeline control failed: %s", w->pipeline_why ? w->pipeline_why : "?");
            }
            sv_trace_flag_record(run, w, rec, g);
            sv_assign_verdict(rec);
            avp_vec_push(out, rec);
        }
        if (!lost)
            avp_vec_push(&run->run_notes, avp_str(avp_sprintf(a, "%s: dropping %s changes no function's exactness "
                                                                 "(%zu functions)",
                                                              w->u->name, flag, total)));
    }
}
