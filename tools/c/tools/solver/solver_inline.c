/* solver_inline: Phase 1, the inline / call-set decisions. For each differing callee C of a stop:
 *   - ours expanded C, retail calls it: where retail defines C (symbols.txt and splits.txt): another unit gives the
 *     condition "C's body must not be visible to this unit" (unit boundary); the same unit after the caller gives
 *     definition order; the same unit before the caller means the compiler declined, and the probe-flag rows of
 *     solver.tsv are compiled as probes;
 *   - retail expanded C, ours calls it: the condition "C's body must be visible and inlinable before the caller",
 *     with where our source defines C and whether retail keeps a copy;
 *   - witness (PS2) and sibling (Gormiti map) evidence where their indexes exist (a missing index is a note);
 *   - the model check: a callee reported "ours expanded" must be named in the caller's source closure and absent from
 *     our object's call set; a failure is a MODEL-GAP.
 * No source candidates in this phase: moving a definition across units is not a one-function edit. The compiler's
 * own numbers (the inliner's rule, cost, limit and statements at the site) come from the inline trace
 * (sv_trace_inline_record, solver_trace.c); a gap of its self-check after the inliner reclassifies the stop. */
#include "sibling_lookup.h"
#include "solver.h"
#include "wall.h"
#include "witness_lookup.h"

static size_t count_calls(AvpArena *a, const CcFn *fn, const char *callee) {
    if (!fn) return 0;
    WallCall *calls = NULL;
    size_t n = wall_calls(a, fn->rows, fn->nrows, &calls), k = 0;
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(calls[i].name, callee)) k++;
    return k;
}

/* The first differing pair whose side names the callee, else the first differing pair. */
static void decision_site(SvRecord *rec, const CcCompareResult *res, const char *callee) {
    if (!res) return;
    AvpStr c = avp_str(callee);
    i64 first = -1;
    for (size_t i = 0; i < res->npairs; i++) {
        const CcPair *p = &res->pairs[i];
        if (!cc_pair_differs(p)) continue;
        if (first < 0) first = (i64)i;
        bool names = (p->retail && avp_str_contains(p->retail->text, c)) ||
                     (p->compiled && avp_str_contains(p->compiled->text, c));
        if (names) {
            first = (i64)i;
            break;
        }
    }
    if (first < 0) return;
    const CcPair *p = &res->pairs[first];
    rec->site_addr = p->retail ? p->retail->address : -1;
    rec->site_row = p->compiled ? (i64)p->compiled->index : -1;
}

static const char *unit_of_addr(SvRun *run, u64 addr) {
    const char *path = run->symtab ? shsm_sym_unit_of(run->symtab, addr) : NULL;
    return path ? shsm_unit_name(run->a, path) : NULL;
}

static void witness_evidence(SvRun *run, SvRecord *rec, const ShsmSymbol *caller, const ShsmSymbol *callee) {
    AvpArena *a = run->a;
    if (!run->witness_present) {
        sv_note(a, rec, "witness: index missing (build/index/witness.sqlite), no PS2 evidence");
        return;
    }
    WtLookup wl = {0};
    if (rec->direction == SV_DIR_OURS_EXPANDED && callee) {
        if (wt_lookup(a, run->r, (i64)callee->address, 3, &wl) && wl.ps2 != WT_NONE)
            sv_evidence(a, rec, "witness: the PS2 build has its own copy of %s (PS2 0x%08llX, tier %s)", callee->name,
                        (unsigned long long)wl.ps2, wl.tier ? wl.tier : "?");
        else
            sv_evidence(a, rec, "witness: no PS2 counterpart known for %s", rec->callee);
        avp_vec_free(&wl.helpers);
    } else if (caller) {
        if (wt_lookup(a, run->r, (i64)caller->address, 50, &wl)) {
            bool hit = false;
            for (size_t i = 0; i < wl.helpers.n; i++)
                if (wl.helpers.v[i].wii_copy && strcmp(wl.helpers.v[i].wii_copy, rec->callee) == 0) hit = true;
            if (hit)
                sv_evidence(a, rec, "witness: the PS2 build also expands %s in this function", rec->callee);
            else
                sv_evidence(a, rec, "witness: %zu PS2 helper(s) inlined here, none is %s", wl.helpers.n, rec->callee);
            avp_vec_free(&wl.helpers);
        } else {
            sv_evidence(a, rec, "witness: the PS2 build gives nothing for %s", rec->function);
        }
    }
}

static void sibling_evidence(SvRun *run, SvRecord *rec) {
    AvpArena *a = run->a;
    if (!run->sibling) {
        sv_note(a, rec, "sibling: no paired sibling index (build/index/sibling/gormiti), no map evidence");
        return;
    }
    SibLookupSame same = {0};
    if (sibling_lookup_same_named(run->sibling, rec->callee, &same))
        sv_evidence(a, rec, "sibling: Gormiti has a function %s (%lld bytes) in %s%s%s", rec->callee,
                    (long long)same.size, same.lib && *same.lib ? same.lib : "", same.lib && *same.lib ? " " : "",
                    same.object ? same.object : "?");
    if (run->sibling_unused && avp_set_has(run->sibling_unused, avp_str(rec->callee)))
        sv_evidence(a, rec, "sibling: Gormiti's map lists %s as UNUSED (its linker dead-stripped the copy)",
                    rec->callee);
}

/* The probe-flag probes of a "compiler declined" decision. */
void sv_probe_flags(SvRun *run, SvUnitWork *w, SvRecord *rec, bool self) {
    AvpArena *a = run->a;
    if (run->diagnose_only) {
        sv_note(a, rec, "--diagnose-only: probes not compiled");
        return;
    }
    if (!w->base || !w->base->ok) return;
    for (size_t i = 0; i < run->rulings.n; i++) {
        const SvRuling *ru = &run->rulings.v[i];
        if (ru->kind != SV_RULE_PROBE_FLAG) continue;
        if (run->deadline && avp_time() > run->deadline) {
            run->deadline_hit = true;
            sv_note(a, rec, "deadline reached before probe %s", ru->arg1);
            continue;
        }
        AvpStrVec words = sv_flag_words(a, ru->arg1);
        const char **ex = AVP_NEWN(a, const char *, words.n + 1);
        for (size_t k = 0; k < words.n; k++) ex[k] = avp_cz(a, words.v[k]);
        SvFlags fl = {0};
        fl.extra = ex;
        fl.nextra = words.n;
        SvProbe p = {0};
        p.lever = avp_sprintf(a, "flag %s", ru->arg1);
        p.lever_ruled = sv_rulings_is_lever_flag(&run->rulings, ru->arg1);
        SvBuild *b = sv_build(&w->sc, a, w->text0, NULL, &fl);
        if (!b->ok) {
            p.error = true;
            p.result = avp_sprintf(a, "error: %s", b->error);
            avp_vec_push(&rec->probes, p);
            continue;
        }
        p.no_effect = sv_objects_equal(w->base->obj, b->obj);
        LvScore *s0 = sv_lvscore(a, w->base, rec->function), *s1 = sv_lvscore(a, b, rec->function);
        AvpCzVec broken = {0};
        if (self) {
            /* the target is not emitted in the baseline: is it emitted now, and does anything else break? */
            const CcCompareResult *t = sv_result(b, rec->function);
            AvpMapIter it = {0};
            while (avp_map_next(w->base->oc->results, &it)) {
                const CcCompareResult *c = it.val->p, *c1 = sv_result(b, avp_cz(a, c->retail_name));
                if (c->exact && (!c1 || !c1->exact)) avp_vec_push(&broken, avp_cz(a, c->retail_name));
            }
            if (!t)
                p.result = "the function is still not emitted";
            else
                p.result = avp_sprintf(a, "emits the function (%lld differing row(s)%s)", (long long)s1->rows,
                                       s1->exact ? ", exact" : "");
            p.closes = t && s1->exact && !broken.n;
            p.improves = t && !s1->exact && !broken.n;
        } else {
            const char *st = lv_classify(a, s0, s1, &broken);
            p.closes = !strcmp(st, "improved") && s1->exact;
            p.improves = !strcmp(st, "improved") && !s1->exact;
            p.result = avp_sprintf(a, "%s: target rows %lld -> %lld%s", st, (long long)s0->rows, (long long)s1->rows,
                                   s1->ok && s1->exact ? " (exact)" : "");
        }
        if (broken.n) {
            AvpBuf bb;
            avp_buf_init(&bb, a);
            for (size_t k = 0; k < broken.n; k++) avp_buf_printf(&bb, "%s%s", k ? ", " : "", broken.v[k]);
            p.result = avp_sprintf(a, "%s; breaks %s", p.result, bb.p);
        }
        if (p.no_effect) {
            const char *why = NULL;
            int ctl = sv_run_flag_control(run, w, ru->arg1, NULL, &fl, &why);
            p.result = ctl == 1 ? "no effect on the object (the flag changes the control source)"
                       : ctl == 0 ? "no effect on the object (the flag leaves the control source unchanged too)"
                                  : avp_sprintf(a, "no effect on the object (flag control failed: %s)", why ? why : "?");
            p.closes = p.improves = false;
        }
        sv_trace_probe_flag(run, w, rec, &p, &words);
        avp_vec_push(&rec->probes, p);
    }
}

void sv_phase1_decision(SvRun *run, SvUnitWork *w, SvRecord *rec, const SvDecision *dec, const CcCompareResult *res) {
    AvpArena *a = run->a;
    const char *C = dec->callee;
    bool self = strcmp(C, rec->function) == 0;
    const ShsmSymbol *caller = run->symtab ? shsm_sym_by_name(run->symtab, avp_str(rec->function)) : NULL;
    const ShsmSymbol *callee = run->symtab ? shsm_sym_by_name(run->symtab, avp_str(C)) : NULL;
    const char *callee_unit = callee ? unit_of_addr(run, callee->address) : NULL;
    const char *here = w->u->name;
    const char *srcname = sv_source_name(a, C);
    rec->cause = avp_sprintf(a, "inline %s", C);
    decision_site(rec, res, C);

    /* our source: where the unit source defines the callee and the caller */
    LvFnDef cdef = {0}, fdef = {0};
    bool c_here = w->src && sv_source_find(a, w->src, C, &cdef);
    bool f_here = w->src && sv_source_find(a, w->src, rec->function, &fdef);
    i64 c_line = c_here ? sv_source_line(w->src, cdef.head) : 0, f_line = f_here ? sv_source_line(w->src, fdef.head) : 0;
    if (c_here) {
        AvpBuf q;
        avp_buf_init(&q, a);
        for (i32 k = 0; k < cdef.nquals; k++) avp_buf_printf(&q, "%s%s", k ? " " : "", cdef.quals[k]);
        sv_evidence(a, rec, "our source defines %s at line %lld%s%s%s", srcname, (long long)c_line,
                    q.n ? " (declaration words: " : "", q.n ? q.p : "", q.n ? ")" : "");
    } else if (dec->via && *dec->via && strcmp(dec->via, "?") != 0) {
        sv_evidence(a, rec, "our source names %s through %s (preprocessed text)", srcname, dec->via);
    } else if (!c_here) {
        sv_evidence(a, rec, "the unit source does not define %s (a header, or another unit)", srcname);
    }

    if (rec->direction == SV_DIR_OURS_EXPANDED) {
        size_t ours_calls = self ? 0 : count_calls(a, res ? res->compiled_fn : NULL, C);
        size_t retail_calls = self ? 0 : count_calls(a, res ? res->retail_fn : NULL, C);
        rec->ours = self ? avp_sprintf(a, "expands %s at every call and emits no copy of it", C)
                         : avp_sprintf(a, "expands %s here (%zu call(s) left)", C, ours_calls);
        rec->retail = callee ? avp_sprintf(a, "%s %s (0x%08llX, %s)", self ? "keeps an out-of-line" : "calls",
                                           self ? "copy" : C, (unsigned long long)callee->address,
                                           callee_unit ? callee_unit : "unit unknown")
                             : avp_sprintf(a, "calls %s", C);
        /* model check */
        if (!dec->via) {
            rec->model_gap = true;
            sv_note(a, rec, "model check: %s is not named in the function's source or a same-text function it names",
                    srcname ? srcname : C);
        } else if (!strcmp(dec->via, "?")) {
            rec->model_gap = true;
            sv_note(a, rec, "model check: the caller's definition was not found, so the expansion cannot be checked");
        } else if (!self && ours_calls) {
            rec->model_gap = true;
            sv_note(a, rec, "model check: ours still calls %s %zu time(s) (retail %zu): the difference is not one "
                            "expansion of it",
                    C, ours_calls, retail_calls);
        } else if (self && sv_result(w->ref, C)) {
            rec->model_gap = true;
            sv_note(a, rec, "model check: our object emits %s after all", C);
        }
        bool declined = false;
        if (!callee) {
            rec->condition = avp_sprintf(a, "retail calls %s, which symbols.txt does not name: where retail defines it "
                                            "is unknown",
                                         C);
        } else if (!callee_unit) {
            rec->condition = avp_sprintf(a, "retail calls %s at 0x%08llX, a range splits.txt assigns to no unit: "
                                            "whether its body was visible to this unit is unknown",
                                         C, (unsigned long long)callee->address);
        } else if (strcmp(callee_unit, here) != 0) {
            rec->condition = avp_sprintf(a, "%s's body must not be visible to this unit: retail defines it in %s "
                                            "(unit boundary) and calls it; ours sees its body%s%s",
                                         C, callee_unit, dec->via && *dec->via ? " through " : "",
                                         dec->via && *dec->via ? dec->via : "");
        } else if (self) {
            declined = true;
            rec->condition = avp_sprintf(a, "%s must stay out of line in this unit: retail keeps its copy at 0x%08llX "
                                            "and calls it; ours expands every call and drops the copy (the inline "
                                            "decision's cost is not read)",
                                         C, (unsigned long long)callee->address);
        } else if (caller && callee->address > caller->address) {
            rec->condition = avp_sprintf(a, "definition order: retail defines %s after the caller in this unit "
                                            "(0x%08llX > 0x%08llX), so its body must not be seen before the call",
                                         C, (unsigned long long)callee->address, (unsigned long long)caller->address);
            if (c_here && f_here && c_line > f_line) {
                /* the order already holds: order alone does not decide it under this unit's flags */
                sv_note(a, rec, "our source already defines %s after the caller (line %lld > %lld) and ours still "
                                "expands it: definition order alone does not decide this under the unit's flags",
                        srcname, (long long)c_line, (long long)f_line);
                declined = true;
            } else if (c_here && f_here) {
                sv_evidence(a, rec, "our source defines %s at line %lld, before the caller (line %lld)", srcname,
                            (long long)c_line, (long long)f_line);
            }
        } else {
            declined = true;
            rec->condition = avp_sprintf(a, "the compiler declined: retail defines %s before the caller in this unit "
                                            "(0x%08llX < 0x%08llX) and still calls it; ours expands it under the "
                                            "unit's flags (the inline decision's cost is not read)",
                                         C, (unsigned long long)callee->address,
                                         (unsigned long long)(caller ? caller->address : 0));
        }
        witness_evidence(run, rec, caller, callee);
        sibling_evidence(run, rec);
        if (declined) sv_probe_flags(run, w, rec, self);
        sv_trace_inline_record(run, w, rec, res);
    } else {
        rec->ours = avp_sprintf(a, "calls %s", C);
        rec->retail = avp_sprintf(a, "expands %s here (no call)", C);
        rec->condition = avp_sprintf(a, "%s's body must be visible and inlinable before the caller", C);
        if (c_here && f_here && c_line > f_line)
            sv_evidence(a, rec, "our source defines %s at line %lld, after the caller (line %lld)", srcname,
                        (long long)c_line, (long long)f_line);
        if (callee)
            sv_evidence(a, rec, "retail also keeps a copy of %s at 0x%08llX (%s)", C, (unsigned long long)callee->address,
                        callee_unit ? callee_unit : "unit unknown");
        else
            sv_evidence(a, rec, "symbols.txt names no retail copy of %s (dead-stripped or never out of line)", C);
        witness_evidence(run, rec, caller, callee);
        sibling_evidence(run, rec);
        sv_trace_inline_record(run, w, rec, res);
    }
}
