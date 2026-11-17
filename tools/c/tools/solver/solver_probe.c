/* solver_probe: Phase 2's probes. A probe is a lever compiled only to state the condition: it goes in the record's
 * probes, can never give MATCH or CLOSER, and is never applied.
 *   decl-order  a declaration moved where the sibling's DWARF does not show it (and levers' declaration swaps): a
 *               hit is RULE-EXCEPTION territory under R30 (user's ruling 2026-11-15);
 *   const-read  one pointer-load site `X->` read through `(*(const __typeof__(X)*)&X)`: a hit with no const
 *               accessor listed for X is the ruling of 2026-11-16 (a cast to const at one site is a lever);
 *   flags       the probe-flag rows of solver.tsv (Phase 1's probe, on the target only).
 * A probe that leaves the object byte-identical to the baseline is counted "no effect on the object". */
#include "solver.h"

static const char TYPEOF_SNIPPET[] = "int solver_typeof_fn(int *p) { return (*(const __typeof__(p) *)&p)[0]; }\n";
static const char PLAIN_SNIPPET[] = "int solver_typeof_fn(int *p) { return p[0]; }\n";

int sv_typeof_accepted(SvRun *run, SvUnitWork *w, const char **why) {
    AvpArena *a = run->a;
    *why = NULL;
    AvpStr root = {0}, ext = {0};
    avp_path_splitext(w->sc.basename, &root, &ext);
    if (!run->typeof_ok) run->typeof_ok = avp_map_new();
    const char *key = avp_sprintf(a, "%s\x1f%.*s", w->u->mw_version ? w->u->mw_version : "?", AVP_SF(ext));
    bool created = false;
    AvpVal *v = avp_map_put(run->typeof_ok, avp_str(key), &created);
    if (!created) return (int)v->i;
    const char *e = avp_cz(a, ext);
    /* the control: the same snippet without __typeof__ must compile, else the snippet pipeline proves nothing */
    int plain = sv_compile_snippet(&w->sc, a, "typeof-plain", e, avp_strn(PLAIN_SNIPPET, sizeof PLAIN_SNIPPET - 1), why);
    if (plain != 1) {
        *why = avp_sprintf(a, "the control snippet does not compile (%s)", *why ? *why : "?");
        v->i = -1;
        return -1;
    }
    int ty = sv_compile_snippet(&w->sc, a, "typeof", e, avp_strn(TYPEOF_SNIPPET, sizeof TYPEOF_SNIPPET - 1), why);
    v->i = ty == 1 ? 1 : 0;
    return (int)v->i;
}

static const SvRuling *accessor_for(const SvRun *run, const char *spelling) {
    for (size_t i = 0; i < run->rulings.n; i++)
        if (run->rulings.v[i].kind == SV_RULE_CONST_ACCESSOR && !strcmp(run->rulings.v[i].arg1, spelling))
            return &run->rulings.v[i];
    return NULL;
}

/* One probe per pointer-load site `X->` (X a plain name, not `this`), at most 60. */
static void gen_const_read(SvP2 *p, SvTries *out, AvpSet *seen) {
    LvCtx *x = p->x;
    AvpArena *a = p->run->a;
    size_t n = 0;
    for (i32 k = x->lb + 1; k + 1 < x->rb && n < 60; k++) {
        const LvTok *t = &x->code.v[k];
        if (t->k != LV_ID || !TEQ(&x->code.v[k + 1], "->") || TEQ(t, "this")) continue;
        if (k > 0 && (TEQ(&x->code.v[k - 1], ".") || TEQ(&x->code.v[k - 1], "->") || TEQ(&x->code.v[k - 1], "::")))
            continue;
        LvS rep = avp_str(avp_sprintf(a, "(*(const __typeof__(%s) *)&%s)", t->t, t->t));
        LvEdit ed[1] = {{t->s, t->e, rep}};
        LvS nw;
        if (!sv_apply_edits(a, x->text, ed, 1, &nw)) continue;
        if (!avp_set_add(seen, avp_str(avp_sha1_hex(a, nw.p, nw.n)))) continue;
        SvTry tr = {0};
        tr.form = "const-read";
        i64 line = sv_line_of(x->text, t->s);
        tr.desc = avp_sprintf(a, "%sconst-qualified read of %s at line %lld", p->helper ? avp_sprintf(a, "in helper %s: ", p->helper) : "",
                              t->t, (long long)line);
        tr.site = t->t;
        tr.line = line;
        tr.text = nw;
        tr.probe = true;
        tr.helper = p->helper;
        avp_vec_push(out, tr);
        n++;
    }
}

static const char *list_text(AvpArena *a, const AvpCzVec *v) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < v->n; i++) avp_buf_printf(&b, "%s%s", i ? ", " : "", v->v[i]);
    return b.p ? b.p : "";
}

/* The probes of a batch into the record; the misses counted in one note per kind. */
static void record_probes(SvP2 *p, SvTry *v, size_t n) {
    AvpArena *a = p->run->a;
    const char *kinds[] = {"decl-order", "const-read"};
    for (int kk = 0; kk < 2; kk++) {
        size_t compiled = 0, hits = 0, same = 0, failed = 0, skipped = 0, unlisted = 0;
        /* every closing probe is listed; of the improving ones the best three (fewest rows) */
        i64 best[3] = {-1, -1, -1};
        for (size_t i = 0; i < n; i++) {
            SvTry *t = &v[i];
            if (strcmp(t->form, kinds[kk]) || !t->score || !t->score->ok || t->exact || t->broken.n) continue;
            if (!(t->rows_after >= 0 && t->rows_after < t->rows_before)) continue;
            i64 r = t->rows_after;
            for (int k = 0; k < 3; k++)
                if (best[k] < 0 || r < best[k]) {
                    i64 tmp = best[k];
                    best[k] = r;
                    r = tmp;
                    if (r < 0) break;
                }
        }
        i64 limit = best[2] >= 0 ? best[2] : best[1] >= 0 ? best[1] : best[0];
        size_t listed_improving = 0;
        i64 traced_at = -1, traced_rows = -1;
        bool traced_closes = false;
        LvS traced_text = {0};
        for (size_t i = 0; i < n; i++) {
            SvTry *t = &v[i];
            if (strcmp(t->form, kinds[kk])) continue;
            if (t->skipped) {
                skipped++;
                p->rec->budget_hit = true;
                continue;
            }
            if (!t->score || !t->score->ok) {
                failed++;
                continue;
            }
            compiled++;
            if (t->same_object) same++;
            bool closes = t->exact;
            bool improves = !t->exact && t->rows_after >= 0 && t->rows_after < t->rows_before && !t->broken.n;
            if (!closes && !improves) continue;
            hits++;
            if (improves && (t->rows_after > limit || listed_improving >= 3)) {
                unlisted++;
                continue;
            }
            if (improves) listed_improving++;
            SvProbe pr = {0};
            pr.lever = avp_sprintf(a, "%s: %s", t->form, t->desc);
            pr.closes = closes;
            pr.improves = improves;
            pr.broken = t->broken;
            const char *how = closes ? "closes" : avp_sprintf(a, "improves (rows %lld -> %lld)", (long long)t->rows_before,
                                                             (long long)t->rows_after);
            if (kk == 0) {
                pr.result = avp_sprintf(a, "%s under a declaration order the sibling's DWARF does not show: a "
                                           "RULE-EXCEPTION under R30 (user's ruling 2026-11-15), not a candidate",
                                        how);
                p->rec->rule_exception = p->rec->rule_exception || closes;
            } else {
                const SvRuling *ru = accessor_for(p->run, t->site);
                pr.result = ru ? avp_sprintf(a, "%s under a const-qualified read of %s at line %lld; solver.tsv lists the "
                                                "const accessor %s for it (see the const-accessor candidates)",
                                             how, t->site, (long long)t->line, ru->arg2)
                               : avp_sprintf(a, "%s under a const-qualified read of %s at line %lld; no const accessor "
                                                "is listed for it: a cast to const at one site is a lever (user's "
                                                "ruling 2026-11-16)",
                                             how, t->site, (long long)t->line);
            }
            if (t->broken.n)
                pr.result = closes ? avp_sprintf(a, "%s; closes %s, opens %s", pr.result, p->symbol, list_text(a, &t->broken))
                                   : avp_sprintf(a, "%s; breaks %s", pr.result, list_text(a, &t->broken));
            avp_vec_push(&p->rec->probes, pr);
            /* the declaration probe traced against ours: the first closing one, else the one with the fewest rows */
            if (kk == 0 && !p->helper && (traced_at < 0 || (closes && !traced_closes) ||
                                          (closes == traced_closes && t->rows_after < traced_rows))) {
                traced_at = (i64)p->rec->probes.n - 1;
                traced_text = t->text;
                traced_closes = closes;
                traced_rows = t->rows_after;
            }
        }
        if (traced_at >= 0) {
            /* what the declaration move does in the allocator's numbers */
            const char *ag = sv_trace_against(p, traced_text, "probe");
            SvProbe *pr = &p->rec->probes.v[traced_at];
            if (ag) pr->result = avp_sprintf(a, "%s; %s", pr->result, ag);
        }
        size_t total = compiled + failed + skipped;
        if (!total) continue;
        sv_note(a, p->rec, "%s%s probes: %zu compiled, %zu hit (%zu improving ones not listed), %zu byte-identical to the baseline (no effect on the "
                           "object), %zu failed to compile, %zu skipped by the budget",
                p->helper ? avp_sprintf(a, "helper %s: ", p->helper) : "", kinds[kk], compiled, hits, unlisted, same, failed,
                skipped);
    }
}

void sv_p2_probes(SvP2 *p) {
    AvpArena *a = p->run->a;
    SvTries v = {0};
    AvpSet *seen = avp_set_new();
    avp_set_add(seen, avp_str(avp_sha1_hex(a, p->text.p, p->text.n)));
    if (p->sib.ok || p->dbg.ok) sv_p2_decl_moves(p, &v, seen, false);
    const char *why = NULL;
    int ty = sv_typeof_accepted(p->run, p->w, &why);
    if (ty == 1) {
        if (!p->helper) sv_evidence(a, p->rec, "this compiler accepts __typeof__ (a generated snippet compiles; its "
                                               "control without __typeof__ compiles too)");
        gen_const_read(p, &v, seen);
    } else if (ty == 0) {
        sv_note(a, p->rec, "this compiler rejects __typeof__ (%s): no const-read probe", why ? why : "?");
    } else {
        sv_note(a, p->rec, "the __typeof__ control failed (%s): no const-read probe", why ? why : "?");
    }
    sv_p2_score(p, v.v, v.n);
    record_probes(p, v.v, v.n);
    if (!p->helper) sv_probe_flags(p->run, p->w, p->rec, false);
    avp_set_free(seen);
}
