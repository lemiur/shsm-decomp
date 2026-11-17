/* solver_cand: Phase 2's candidates (forms the rules allow) and the scorer they share with the probes.
 *   dwarf-order    a declaration moved to the sibling DWARF's order (only toward the DWARF)
 *   dwarf-scope    a declaration moved to the DWARF's scope (function scope or the block that holds its uses)
 *   dwarf-type     a local given the DWARF's type
 *   reuse-local    a local the DWARF lists, assigned from a subexpression of a statement directly before it and
 *                  used there (depth 2: two such edits, from a frontier of improving or neutral states)
 *   dwarf-local    the same for a local the DWARF lists and ours lacks: declared with the DWARF's name and type
 *   inline-local   levers' "inline single-use local" for a local neither sibling build lists
 *   const-accessor one plain spelling swapped for its const twin (solver.tsv const-accessor)
 *   order, expr, type, temp   levers' families (temp: an introduced local's name is marked invented)
 *   decl           levers' declaration swaps and the solver's declaration moves, only when the sibling has no
 *                  function of this name ("declaration order unevidenced")
 * Every candidate passes sv_candidate_rejects and keeps a RULE-EXCEPTION: R30 declaration where it is; a surviving-
 * source unit gets only the DWARF forms (dwarf-order, dwarf-scope, dwarf-type). An exact or closer candidate is
 * checked with the rule checker on its scratch text (no new [match] finding). */
#include "solver.h"
#include "match.h"
#include <stdlib.h>

/* ---- scoring --------------------------------------------------------------------------------------------------- */

typedef struct {
    SvP2 *p;
    double deadline;
    const char *base_obj;
} ScoreCtx;

static LvScore *copy_score(AvpArena *a, const LvScore *s) {
    LvScore *o = AVP_NEW(a, LvScore);
    *o = *s;
    o->error = s->error ? avp_strdup(a, s->error) : NULL;
    o->sig = s->sig ? avp_strdup(a, s->sig) : NULL;
    o->others = AVP_NEWN(a, LvOther, s->nothers + 1);
    for (size_t i = 0; i < s->nothers; i++) {
        o->others[i] = s->others[i];
        o->others[i].name = avp_strdup(a, s->others[i].name);
    }
    return o;
}

static int score_job(void *ctx, size_t index, const void *item, void *result) {
    AVP_UNUSED(index);
    AVP_UNUSED(result);
    ScoreCtx *c = ctx;
    SvTry *t = *(SvTry *const *)item;
    if (avp_time() > c->deadline) {
        t->skipped = true;
        return 0;
    }
    /* a fresh arena per compile (library code marks and resets the thread's scratch arena: never pass it down) */
    AvpArena *L = avp_arena(), *s = avp_arena_new(0);

    SvBuild *b = sv_build(&c->p->w->sc, s, t->text, NULL, NULL);
    if (!b->ok) {
        t->error = avp_strdup(L, b->error ? b->error : "compile failed");
    } else {
        LvScore *ls = sv_lvscore(s, b, c->p->symbol);
        t->score = copy_score(L, ls);
        t->compiled = ls->ok;
        if (!ls->ok) t->error = avp_strdup(L, ls->error ? ls->error : "not scored");
        t->same_object = sv_objects_equal(c->base_obj, b->obj);
    }
    if (b->dir) avp_rmtree(b->dir, 1);
    avp_arena_free(s);
    return 0;
}

/* The [match] findings of one text as the rule checker reports them, reduced to what the comparison needs: the count
 * of pending findings per function key. The rule checker's own result is large (every function of the unit with its
 * findings, about 150 MB of arena for a game unit), so it is read in an arena of its own that is freed before the
 * next call. */
typedef struct {
    AvpMap *pending; /* function key -> val.i count; keys live in the run's arena */
    bool failed;
    const char *error;
} RulesSummary;

static RulesSummary *rules_summary(SvP2 *p, LvS text, const char *tag) {
    AvpArena *a = p->run->a, *ra = avp_arena_new(0);
    RulesSummary *s = AVP_NEW(a, RulesSummary);
    s->pending = avp_map_new();
    const char *dir = avp_path_join(ra, p->w->sc.workdir, tag);
    if (avp_isdir(dir)) avp_rmtree(dir, 1);
    const char *file = avp_path_join(ra, dir, p->w->sc.basename);
    AvpJson *rr = NULL;
    AvpTry t;
    if (!avp_makedirs(dir, true) || !avp_write_file(file, text.p, text.n)) {
        s->failed = true;
        s->error = "the scratch file could not be written";
    } else if (AVP_TRY(&t)) {
        rr = match_run_rules(ra, p->run->r, p->w->u->name, file);
        avp_try_end(&t);
    } else {
        s->failed = true;
        s->error = avp_strdup(a, t.msg ? t.msg : "the rule checker raised");
    }
    if (rr && avp_json_getz(rr, "error")) {
        s->failed = true;
        s->error = avp_strdup(a, avp_cz(ra, avp_json_get_str(rr, "error", AVP_S(""))));
    } else if (rr) {
        const AvpJson *pend = avp_json_getz(rr, "pending");
        for (size_t i = 0; pend && avp_json_is_obj(pend) && i < pend->u.obj.n; i++) {
            AvpStr key = pend->u.obj.v[i].key;
            AvpVal *v = avp_map_put(s->pending, avp_str(avp_strdup(a, avp_cz(ra, key))), NULL);
            v->i = (i64)avp_json_len(pend->u.obj.v[i].val);
        }
    }
    avp_arena_free(ra);
    return s;
}

/* New [match] findings of a candidate against the unmodified scratch copy. */
static void rules_check(SvP2 *p, SvTry *t) {
    AvpArena *a = p->run->a;
    if (!p->w->rules_base) p->w->rules_base = rules_summary(p, p->text, "rules-base");
    const RulesSummary *base = p->w->rules_base, *cand = rules_summary(p, t->text, "rules-cand");
    if (cand->failed) {
        if (!base->failed) avp_vec_push(&t->rules, avp_sprintf(a, "rule checker failed: %s", cand->error ? cand->error : "?"));
        avp_map_free(cand->pending);
        return;
    }
    AvpMapIter it = {0};
    while (avp_map_next(cand->pending, &it)) {
        const AvpVal *b = avp_map_get(base->pending, it.key);
        i64 n1 = it.val->i, n0 = b ? b->i : 0;
        if (n1 > n0)
            avp_vec_push(&t->rules, avp_sprintf(a, "%.*s: %lld new [match] finding(s)", AVP_SF(it.key), (long long)(n1 - n0)));
    }
    avp_map_free(cand->pending);
}

/* The rule check costs about half a second a candidate and runs on one thread: a batch checks its best candidates
 * (exact first, then the fewest differing rows) up to this many; the others are neither listed nor counted as
 * candidates (a note says how many) so that none reaches MATCH or CLOSER unchecked. */
#define SV_RULE_CHECKS_PER_BATCH 24

static int cmp_checked(const void *x, const void *y) {
    const SvTry *a = *(SvTry *const *)x, *b = *(SvTry *const *)y;
    if (a->exact != b->exact) return a->exact ? -1 : 1;
    return a->rows_after < b->rows_after ? -1 : a->rows_after > b->rows_after;
}

void sv_p2_score(SvP2 *p, SvTry *v, size_t n) {
    AvpArena *a = p->run->a;
    if (!n) return;
    SvTry **ptrs = AVP_NEWN(a, SvTry *, n);
    for (size_t i = 0; i < n; i++) ptrs[i] = &v[i];
    ScoreCtx c = {p, p->deadline, p->w->base->obj};
    i64 jobs = p->run->jobs > 0 ? p->run->jobs : shsm_default_jobs();
    avp_pool_map(NULL, (int)AVP_MIN((i64)n, jobs), score_job, &c, ptrs, n, sizeof(SvTry *), NULL, 0);
    SvTry **due = AVP_NEWN(a, SvTry *, n);
    size_t ndue = 0;
    for (size_t i = 0; i < n; i++) {
        SvTry *t = &v[i];
        t->rows_before = p->base->rows;
        if (t->skipped || !t->score) {
            t->status = t->skipped ? "skipped" : "error";
            t->rows_after = -1;
            continue;
        }
        t->rows_after = t->score->ok ? t->score->rows : -1;
        t->exact = t->score->ok && t->score->exact;
        t->status = lv_classify(a, p->base, t->score, &t->broken);
        if (!t->probe && !t->broken.n && (t->exact || (t->rows_after >= 0 && t->rows_after < t->rows_before)))
            due[ndue++] = t;
    }
    qsort(due, ndue, sizeof(SvTry *), cmp_checked);
    size_t over = 0;
    for (size_t i = 0; i < ndue; i++) {
        if (i < SV_RULE_CHECKS_PER_BATCH && avp_time() <= p->deadline + 120) {
            rules_check(p, due[i]);
        } else {
            due[i]->unchecked = true;
            over++;
        }
    }
    if (over)
        sv_note(a, p->rec, "%s%zu improving candidate(s) of a batch were not rule-checked (the best %d of a batch are) "
                           "and are not listed",
                p->helper ? avp_sprintf(a, "helper %s: ", p->helper) : "", over, SV_RULE_CHECKS_PER_BATCH);
}

/* ---- the candidate list ---------------------------------------------------------------------------------------- */


/* The function's text in a variant (the edits are inside the function, so its span grows by the length change). */
static LvS fn_text_of(SvP2 *p, LvS text) {
    i64 d = (i64)text.n - (i64)p->text.n;
    i64 s0 = p->x->span0, s1 = p->x->span1 + d;
    if (s1 > (i64)text.n) s1 = (i64)text.n;
    if (s1 < s0) s1 = s0;
    return (LvS){text.p + s0, (size_t)(s1 - s0)};
}

/* Adds a try; a candidate passes the filter (fake forms, RULE-EXCEPTION, the R30 declarations kept). */
bool sv_gen_add_try(SvGen *g, const char *form, const char *desc, const char *evidence, i64 line, LvS text, bool probe,
                    int depth) {
    SvP2 *p = g->p;
    AvpArena *a = p->run->a;
    if (avp_time() > p->deadline) { /* generating past the budget: not built */
        g->over_budget++;
        return false;
    }
    if (!avp_set_add(g->seen, avp_str(avp_sha1_hex(a, text.p, text.n)))) return false;
    if (!probe) {
        if (!g->forms) {
            g->forms = AVP_NEW(a, LvCounter);
            AvpTry ft;
            if (AVP_TRY(&ft)) {
                *g->forms = lv_fake_forms(a, g->orig_fn, p->fn_symbol);
                avp_try_end(&ft);
            }
        }
        AvpCzVec why = sv_candidate_rejects_with(a, g->forms, g->orig_fn, fn_text_of(p, text), p->fn_symbol);
        bool bad = why.n > 0;
        avp_vec_free(&why);
        if (bad) {
            g->rejected++;
            return false;
        }
        for (size_t i = 0; i < p->protected_text.n; i++)
            if (!lv_s_contains(text, avp_str(p->protected_text.v[i]))) {
                g->kept_r30++;
                return false;
            }
    }
    SvTry t = {0};
    t.form = form;
    t.desc = p->helper ? avp_sprintf(a, "in helper %s: %s", p->helper, desc) : desc;
    t.evidence = evidence;
    t.helper = p->helper;
    t.line = line;
    t.text = text;
    t.probe = probe;
    t.depth = depth;
    avp_vec_push(g->out, t);
    return true;
}

static const LvLocal *local_of(LvCtx *x, const char *name) {
    for (size_t i = 0; i < x->locals.n; i++)
        if (!strcmp(x->locals.v[i].name, name)) return &x->locals.v[i];
    return NULL;
}

static bool is_protected(SvP2 *p, const char *name) {
    for (size_t i = 0; i < p->protected_names.n; i++)
        if (!strcmp(p->protected_names.v[i], name)) return true;
    return false;
}

static i64 tok_line(LvCtx *x, i32 k) { return sv_line_of(x->text, x->code.v[k].s); }

/* A declaration statement that can move as one line set: one declarator, no initialiser or one without names. */
static bool movable_decl(LvCtx *x, const LvStmt *s, i64 *l0, i64 *l1) {
    if (s->kind != LS_DECL || s->decl->ndtors != 1 || s->decl->is_static) return false;
    const LvDtor *dt = &s->decl->dtors[0];
    if (dt->init.ok) {
        LvIds ids = lv_ids(x, dt->init.a, dt->init.b);
        if (ids.n || lv_has_call(x, dt->init.a, dt->init.b)) return false;
    }
    return lv_lines(x, s->a, s->b, l0, l1);
}

/* The decl line re-indented to `ind`. */
static LvS reindented(AvpArena *a, LvCtx *x, i64 l0, i64 l1, LvS ind) {
    LvS line = lv_slice(x, l0, l1);
    size_t k = 0;
    while (k < line.n && (line.p[k] == ' ' || line.p[k] == '\t')) k++;
    return LV_CAT(a, ind, (LvS){line.p + k, line.n - k});
}

/* ---- dwarf-order, dwarf-scope, dwarf-type ---------------------------------------------------------------------- */

static void gen_dwarf_forms(SvGen *g) {
    SvP2 *p = g->p;
    LvCtx *x = p->x;
    AvpArena *a = p->run->a;
    for (size_t i = 0; i < p->findings.n; i++) {
        const SvLcFinding *f = &p->findings.v[i];
        if (is_protected(p, f->name)) continue;
        const LvLocal *l = local_of(x, f->name);
        if (!l) continue;
        i64 l0, l1;
        if (f->kind == SV_LC_ORDER) {
            const LvLocal *o = local_of(x, f->other);
            i64 o0, o1;
            if (!o || !movable_decl(x, l->s, &l0, &l1) || !lv_lines(x, o->s->a, o->s->b, &o0, &o1)) continue;
            LvEdit ed[2] = {{l0, l1, LVS("")}, {o0, o0, reindented(a, x, l0, l1, lv_indent(x, o->s->a))}};
            LvS nw;
            if (sv_apply_edits(a, x->text, ed, 2, &nw))
                sv_gen_add_try(g, "dwarf-order", avp_sprintf(a, "declare `%s` before `%s` (the DWARF's order)", f->name, f->other),
                        f->text, tok_line(x, l->s->a), nw, false, 0);
        } else if (f->kind == SV_LC_SCOPE) {
            if (!movable_decl(x, l->s, &l0, &l1)) continue;
            if (f->dw && f->dw->depth > 0) {
                /* into the smallest block that holds every use */
                const LvBlock *best = NULL;
                i32 bs = 0, be = 0;
                for (size_t bi = 0; bi < x->blocks.n; bi++) {
                    const LvBlock *b = &x->blocks.v[bi];
                    if (!b->owner || !b->L.n) continue;
                    i32 ra = b->L.v[0]->a, rb = b->L.v[b->L.n - 1]->b;
                    bool all = true, any = false;
                    for (i32 k = x->lb + 1; k < x->rb && all; k++) {
                        if (x->code.v[k].t != l->name || (k >= l->s->a && k <= l->s->b)) continue;
                        any = true;
                        if (k < ra || k > rb) all = false;
                    }
                    if (!all || !any) continue;
                    if (!best || rb - ra < be - bs) {
                        best = b;
                        bs = ra;
                        be = rb;
                    }
                }
                if (!best) continue;
                i64 t0, t1;
                if (!lv_lines(x, best->L.v[0]->a, best->L.v[0]->b, &t0, &t1)) continue;
                LvEdit ed[2] = {{l0, l1, LVS("")}, {t0, t0, reindented(a, x, l0, l1, lv_indent(x, best->L.v[0]->a))}};
                LvS nw;
                if (sv_apply_edits(a, x->text, ed, 2, &nw))
                    sv_gen_add_try(g, "dwarf-scope", avp_sprintf(a, "declare `%s` in the block that holds its uses (the DWARF's scope)", f->name),
                            f->text, tok_line(x, l->s->a), nw, false, 0);
            } else if (f->dw) {
                /* to function scope, at the DWARF's line among the function-scope declarations */
                i64 at = -1;
                LvS ind = x->body.n ? lv_indent(x, x->body.v[0]->a) : LVS("    ");
                i64 last_end = -1;
                for (i32 k = 0; k < x->body.n; k++) {
                    LvStmt *s = x->body.v[k];
                    if (s->kind != LS_DECL) break;
                    i64 s0, s1;
                    if (!lv_lines(x, s->a, s->b, &s0, &s1)) continue;
                    last_end = s1;
                    const SvVar *dv = s->decl->ndtors ? sv_vars_find(&p->dwarf, s->decl->dtors[0].name) : NULL;
                    if (at < 0 && dv && dv->line > f->dw->line) at = s0;
                }
                if (at < 0) at = last_end >= 0 ? last_end : (x->body.n ? lv_line_start(x, x->code.v[x->body.v[0]->a].s) : -1);
                if (at < 0) continue;
                LvEdit ed[2] = {{l0, l1, LVS("")}, {at, at, reindented(a, x, l0, l1, ind)}};
                LvS nw;
                if (sv_apply_edits(a, x->text, ed, 2, &nw))
                    sv_gen_add_try(g, "dwarf-scope", avp_sprintf(a, "declare `%s` at function scope (the DWARF's scope)", f->name),
                            f->text, tok_line(x, l->s->a), nw, false, 0);
            }
        } else if (f->kind == SV_LC_TYPE) {
            if (l->s->decl->ndtors != 1 || !f->other || strpbrk(f->other, "[(")) continue;
            i64 s0 = x->code.v[l->s->a].s, s1 = x->code.v[l->dt->name_i].s;
            size_t tn = strlen(f->other);
            LvS ty = avp_str(f->other);
            if (!(tn && f->other[tn - 1] == '*')) ty = LV_CAT(a, ty, LVS(" "));
            LvEdit ed[1] = {{s0, s1, ty}};
            LvS nw;
            if (sv_apply_edits(a, x->text, ed, 1, &nw))
                sv_gen_add_try(g, "dwarf-type", avp_sprintf(a, "type `%s` as `%s` (the DWARF's type)", f->name, f->other), f->text,
                        tok_line(x, l->s->a), nw, false, 0);
        }
    }
}

/* ---- reuse-local / dwarf-local --------------------------------------------------------------------------------- */

typedef struct {
    LvNode *n, *parent;
} NodeAt;
typedef AVP_VEC(NodeAt) NodeAts;

static void walk_parents(LvNode *n, LvNode *parent, NodeAts *out) {
    NodeAt e = {n, parent};
    avp_vec_push(out, e);
    for (i32 k = 0; k < n->nk; k++)
        if (n->kids[k]) walk_parents(n->kids[k], n, out);
}

/* The shape of a token range: identifiers as $, the rest as written. */
static const char *shape(LvCtx *x, i32 a, i32 b) {
    AvpBuf buf;
    avp_buf_init(&buf, x->a);
    for (i32 k = a; k <= b; k++) {
        const LvTok *t = &x->code.v[k];
        if (buf.n) avp_buf_putc(&buf, ' ');
        if (t->k == LV_ID) avp_buf_putc(&buf, '$');
        else avp_buf_putn(&buf, t->t, t->n);
    }
    return buf.p ? buf.p : "";
}

/* The shapes of the values the function assigns to name (initialiser, `name = value`). */
static AvpCzVec shapes_of(LvCtx *x, const char *name) {
    AvpCzVec out = {0};
    for (size_t q = 0; q < x->exprs.n; q++) {
        const LvExprEntry *e = &x->exprs.v[q];
        if (!strcmp(e->role, "init") && e->s->kind == LS_DECL) {
            for (i32 d = 0; d < e->s->decl->ndtors; d++)
                if (e->s->decl->dtors[d].name == name && e->s->decl->dtors[d].init.ok &&
                    e->s->decl->dtors[d].init.a == e->n->a)
                    avp_vec_push(&out, (char *)shape(x, e->n->a, e->n->b));
            continue;
        }
        NodeAts all = {0};
        walk_parents(e->n, NULL, &all);
        for (size_t i = 0; i < all.n; i++) {
            LvNode *n = all.v[i].n;
            if (n->kind != LN_ASSIGN || !n->op || strcmp(n->op, "=") || n->nk < 2) continue;
            LvNode *l = n->kids[0], *r = n->kids[1];
            if (l->kind == LN_PRIM && x->code.v[l->a].t == name) avp_vec_push(&out, (char *)shape(x, r->a, r->b));
        }
        avp_vec_free(&all);
    }
    return out;
}

/* Whether name's value before statement S is dead after a new assignment there: the next occurrence after the
 * expression is a plain assignment (or none), and no enclosing loop reads it before S. */
static bool reassign_safe(LvCtx *x, const LvStmt *S, i32 expr_end, const char *name) {
    i32 lo = x->lb + 1, hi = x->rb;
    for (i32 k = expr_end + 1; k < hi; k++) {
        if (x->code.v[k].t != name) continue;
        const LvTok *nx = k + 1 < hi ? &x->code.v[k + 1] : NULL;
        if (!(nx && TEQ(nx, "="))) return false;
        break;
    }
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        const LvStmt *L = x->all_stmts.v[q];
        if (L->kind != LS_WHILE && L->kind != LS_FOR && L->kind != LS_DO) continue;
        if (!(L->a <= S->a && S->b <= L->b)) continue;
        for (i32 k = L->a; k < S->a && k >= lo; k++)
            if (x->code.v[k].t == name) {
                const LvTok *nx = &x->code.v[k + 1];
                if (!TEQ(nx, "=")) return false;
            }
    }
    return true;
}

/* The declaration of a missing local at the DWARF's scope: text to insert and where. */
static bool missing_decl(SvP2 *p, LvCtx *x, const SvVar *dv, const LvStmt *S, i64 *at, LvS *text) {
    AvpArena *a = p->run->a;
    if (!dv->type || !*dv->type || strpbrk(dv->type, "[(")) return false;
    size_t tn = strlen(dv->type);
    const char *decl = avp_sprintf(a, tn && dv->type[tn - 1] == '*' ? "%s%s;" : "%s %s;", dv->type, dv->name);
    if (dv->depth > 0) {
        /* the top of the block that holds S */
        for (size_t bi = 0; bi < x->blocks.n; bi++) {
            const LvBlock *b = &x->blocks.v[bi];
            if (!b->owner) continue;
            bool holds = false;
            for (i32 k = 0; k < b->L.n; k++)
                if (b->L.v[k] == S) holds = true;
            if (!holds) continue;
            i64 t0, t1;
            if (!lv_lines(x, b->L.v[0]->a, b->L.v[0]->b, &t0, &t1)) return false;
            *at = t0;
            *text = LV_CAT(a, lv_indent(x, b->L.v[0]->a), avp_str(decl), x->nl);
            return true;
        }
    }
    /* function scope, at the DWARF's line among the declarations */
    i64 pos = -1, last_end = -1;
    for (i32 k = 0; k < x->body.n; k++) {
        LvStmt *s = x->body.v[k];
        if (s->kind != LS_DECL) break;
        i64 s0, s1;
        if (!lv_lines(x, s->a, s->b, &s0, &s1)) continue;
        last_end = s1;
        const SvVar *ov = s->decl->ndtors ? sv_vars_find(&p->dwarf, s->decl->dtors[0].name) : NULL;
        if (pos < 0 && ov && ov->line > dv->line) pos = s0;
    }
    if (pos < 0) pos = last_end >= 0 ? last_end : (x->body.n ? lv_line_start(x, x->code.v[x->body.v[0]->a].s) : -1);
    if (pos < 0) return false;
    *at = pos;
    *text = LV_CAT(a, x->body.n ? lv_indent(x, x->body.v[0]->a) : LVS("    "), avp_str(decl), x->nl);
    return true;
}

typedef struct {
    i32 prio; /* 0: the value has the shape of one the function assigns to the local */
    const char *form, *desc;
    i64 line;
    LvS text;
} Reuse;
typedef AVP_VEC(Reuse) Reuses;

static const LvBlock *block_of(LvCtx *x, const LvStmt *s);
static bool list_encloses(const LvStmts *L, const LvStmt *s);
/* Reuse edits on one state (ctx x). */
static void gen_reuse(SvP2 *p, LvCtx *x, Reuses *out) {
    AvpArena *a = p->run->a;
    for (size_t li = 0; li < p->dwarf.n && li < 40; li++) {
        const SvVar *dv = &p->dwarf.v[li];
        if (!dv->name || !*dv->name || is_protected(p, dv->name)) continue;
        const char *name = lv_intern_find(dv->name);
        const LvLocal *l = name ? local_of(x, name) : NULL;
        bool declared = l != NULL;
        if (!declared && !name) name = lv_internz(dv->name);
        AvpCzVec shapes = declared ? shapes_of(x, name) : (AvpCzVec){0};
        for (size_t q = 0; q < x->exprs.n; q++) {
            const LvExprEntry *e = &x->exprs.v[q];
            const LvStmt *S = e->s;
            bool ok_kind = S->kind == LS_EXPR || S->kind == LS_RETURN || (S->kind == LS_IF && !strcmp(e->role, "cond")) ||
                           (S->kind == LS_SWITCH && !strcmp(e->role, "cond"));
            if (!ok_kind) continue;
            i64 ls = lv_line_start(x, x->code.v[S->a].s);
            if (!lv_blank(lv_slice(x, ls, x->code.v[S->a].s))) continue; /* `} else if (`: no line of its own */
            if (declared && !reassign_safe(x, S, e->n->b, name)) continue;
            if (declared) { /* the declaration must be in scope at S */
                const LvBlock *db = block_of(x, l->s);
                if (!db || l->s->a > S->a || !list_encloses(&db->L, S)) continue;
            }
            NodeAts all = {0};
            walk_parents(e->n, NULL, &all);
            for (size_t i = 0; i < all.n; i++) {
                LvNode *n = all.v[i].n, *par = all.v[i].parent;
                if (n == e->n && S->kind == LS_EXPR) continue; /* the whole statement */
                LvNode *val = n;
                if (n->kind == LN_PAREN) {
                    if (!n->nk || !n->kids[0]) continue;
                    val = n->kids[0];
                } else if (par && par->kind == LN_PAREN) {
                    continue; /* the parenthesised form is tried with its parentheses */
                }
                if (val->kind == LN_PRIM) {
                    if (x->code.v[val->a].k != LV_ID || x->code.v[val->a].t == name) continue;
                } else if (val->kind == LN_CALL || val->kind == LN_ASSIGN || val->kind == LN_COMMA || val->kind == LN_POST) {
                    continue;
                }
                if (lv_has_side(val) || lv_has_call(x, val->a, val->b)) continue;
                if (par && par->kind == LN_ASSIGN && par->nk && par->kids[0] == n) continue; /* an lvalue */
                if (par && par->kind == LN_UNARY && par->op &&
                    (!strcmp(par->op, "&") || !strcmp(par->op, "++") || !strcmp(par->op, "--")))
                    continue;
                if (par && par->kind == LN_POST) continue;
                const char *sh = shape(x, val->a, val->b);
                i32 prio = 1;
                for (size_t k = 0; k < shapes.n; k++) /* a lone name's shape ("$") says nothing */
                    if (strcmp(sh, "$") && !strcmp(shapes.v[k], sh)) prio = 0;
                if (!declared) prio = 1;
                /* one local per meaning (R30): a local the function already assigns values of other shapes is not
                 * given this one */
                if (declared && prio == 1 && shapes.n) continue;
                LvS vtext = lv_nt(x, val);
                LvS ind = lv_indent(x, S->a);
                LvEdit ed[3];
                size_t ne = 0;
                i64 at = ls;
                LvS assign = LV_CAT(a, ind, avp_str(dv->name), LVS(" = "), vtext, LVS(";"), x->nl);
                if (!declared) {
                    i64 dat;
                    LvS dtext;
                    if (!missing_decl(p, x, dv, S, &dat, &dtext)) continue;
                    if (dat == at) /* both at the same place: the declaration first */
                        assign = LV_CAT(a, dtext, assign);
                    else
                        ed[ne++] = (LvEdit){dat, dat, dtext};
                }
                ed[ne++] = (LvEdit){at, at, assign};
                ed[ne++] = (LvEdit){x->code.v[n->a].s, x->code.v[n->b].e, avp_str(dv->name)};
                LvS nw;
                if (!sv_apply_edits(a, x->text, ed, ne, &nw)) continue;
                Reuse r = {prio, declared ? "reuse-local" : "dwarf-local",
                           avp_sprintf(a, "%s%s = %.*s before line %lld and used there", declared ? "" : "declare ",
                                       dv->name, AVP_SF(vtext), (long long)tok_line(x, S->a)),
                           tok_line(x, S->a), nw};
                avp_vec_push(out, r);
            }
            avp_vec_free(&all);
        }
        avp_vec_free(&shapes);
    }
}

/* ---- const-accessor -------------------------------------------------------------------------------------------- */

static void gen_const_accessor(SvGen *g) {
    SvP2 *p = g->p;
    LvCtx *x = p->x;
    AvpArena *a = p->run->a;
    for (size_t i = 0; i < p->run->rulings.n; i++) {
        const SvRuling *ru = &p->run->rulings.v[i];
        if (ru->kind != SV_RULE_CONST_ACCESSOR || !ru->arg2) continue;
        const char *plain = lv_intern_find(ru->arg1);
        if (!plain) continue;
        for (i32 k = x->lb + 1; k < x->rb; k++) {
            if (x->code.v[k].t != plain) continue;
            LvEdit ed[1] = {{x->code.v[k].s, x->code.v[k].e, avp_str(ru->arg2)}};
            LvS nw;
            if (sv_apply_edits(a, x->text, ed, 1, &nw))
                sv_gen_add_try(g, "const-accessor",
                        avp_sprintf(a, "`%s` -> `%s` at line %lld (solver.tsv const-accessor)", ru->arg1, ru->arg2,
                                    (long long)tok_line(x, k)),
                        *ru->note ? ru->note : NULL, tok_line(x, k), nw, false, 0);
        }
    }
}

/* ---- levers' families ------------------------------------------------------------------------------------------ */

static size_t id_count(LvS fn, const char *name) {
    size_t n = strlen(name), c = 0;
    for (size_t i = 0; i + n <= fn.n; i++)
        if (!memcmp(fn.p + i, name, n) && (i == 0 || !lv_isword((u8)fn.p[i - 1])) &&
            (i + n == fn.n || !lv_isword((u8)fn.p[i + n])))
            c++;
    return c;
}

static void gen_levers(SvGen *g, bool decl_family) {
    SvP2 *p = g->p;
    AvpArena *a = p->run->a;
    bool has_sib = p->sib.ok || p->dbg.ok;
    static const char *const FAMS[] = {"order", "expr", "type", "temp"};
    static const char *const DECL[] = {"decl"};
    LvCandidates gen = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        LvCtx *cx = lv_ctx_new(a, p->text, p->fn_symbol);
        gen = decl_family ? lv_generate(cx, DECL, 1) : lv_generate(cx, FAMS, 4);
        avp_try_end(&t);
    } else {
        return;
    }
    for (size_t i = 0; i < gen.n; i++) {
        LvCandidate *c = &gen.v[i];
        const char *form = c->family;
        const char *desc = avp_sprintf(a, "%s: %s", c->lever, lv_utf8(a, c->desc));
        const char *evidence = NULL;
        if (decl_family) {
            desc = avp_sprintf(a, "declaration order unevidenced (no sibling DWARF): %s", desc);
        } else if (!strcmp(c->lever, "inline single-use local")) {
            const char *gone = NULL;
            LvS fn1 = fn_text_of(p, c->text);
            for (size_t k = 0; k < p->ours.n && !gone; k++)
                if (id_count(fn1, p->ours.v[k].name) == 0) gone = p->ours.v[k].name;
            if (gone && sv_vars_find(&p->dwarf, gone)) continue; /* the DWARF lists it: away from the DWARF */
            if (gone && has_sib) {
                form = "inline-local";
                evidence = avp_sprintf(a, "`%s` is listed by neither sibling build", gone);
            }
        } else if (!strcmp(c->lever, "introduce local for repeated subexpression")) {
            desc = avp_sprintf(a, "%s (name invented)", desc);
        }
        sv_gen_add_try(g, form, desc, evidence, c->line, c->text, false, 0);
    }
    avp_vec_free(&gen);
}

/* ---- records ---------------------------------------------------------------------------------------------------- */

void sv_p2_record(SvP2 *p, SvTry *v, size_t n, size_t *counts, AvpMap *tally) {
    AvpArena *a = p->run->a;
    for (size_t i = 0; i < n; i++) {
        SvTry *t = &v[i];
        if (t->skipped) counts[0]++;
        else if (!t->score) counts[1]++;
        else counts[2]++;
        if (!t->skipped) {
            AvpVal *tv = avp_map_put(tally, avp_str(t->form), NULL);
            tv->i += t->score ? 1 : (1LL << 32);
        }
        bool keep = t->score && !t->unchecked && (t->exact || (t->rows_after >= 0 && t->rows_after < t->rows_before));
        if (!keep) continue;
        /* an unevidenced declaration swap is kept only when the trace shows the swapped locals in one sweep */
        if (!strcmp(t->form, "decl") && !sv_trace_keep_decl(p, t->text)) continue;
        SvCandidate c = {0};
        c.form = t->form;
        c.desc = t->desc;
        c.evidence = t->evidence;
        c.line = t->line;
        c.compiled = true;
        c.exact = t->exact;
        c.rows_before = t->rows_before;
        c.rows_after = t->rows_after;
        c.text = t->text;
        c.broken = t->broken;
        c.rules = t->rules;
        if (t->exact && t->broken.n) {
            AvpBuf b;
            avp_buf_init(&b, a);
            for (size_t k = 0; k < t->broken.n; k++) avp_buf_printf(&b, "%s%s", k ? ", " : "", t->broken.v[k]);
            c.desc = avp_sprintf(a, "%s (closes %s, opens %s)", t->desc, p->symbol, b.p);
        }
        avp_vec_push(&p->rec->candidates, c);
    }
}

static void gen_reuse_tries(SvGen *g, LvCtx *x, i32 prio, const char *prefix, const char *prefix_form, int depth) {
    SvP2 *p = g->p;
    AvpArena *a = p->run->a;
    Reuses rs = {0};
    gen_reuse(p, x, &rs);
    size_t taken = 0;
    for (size_t i = 0; i < rs.n; i++) {
        Reuse *r = &rs.v[i];
        if (prio >= 0 && r->prio != prio) continue;
        if (r->prio == 1 && taken >= 400) continue; /* an unshaped value: capped */
        const char *form = prefix_form && !strcmp(prefix_form, "dwarf-local") ? prefix_form : r->form;
        const char *desc = prefix ? avp_sprintf(a, "%s; %s", prefix, r->desc) : r->desc;
        if (sv_gen_add_try(g, form, desc, "a local the sibling's DWARF lists", r->line, r->text, false, depth) && r->prio)
            taken++;
    }
    avp_vec_free(&rs);
}

/* Depth 2: the reuse states of a batch that improve or are neutral with changed code, best first (at most 6). */
static void deepen(SvGen *g, SvTry *v, size_t n) {
    SvP2 *p = g->p;
    AvpArena *a = p->run->a;
    AVP_VEC(SvTry *) fr = {0};
    for (size_t i = 0; i < n; i++) {
        SvTry *t = &v[i];
        if (t->depth != 1 || !t->score || !t->score->ok || t->broken.n || t->exact) continue;
        if (strcmp(t->form, "reuse-local") && strcmp(t->form, "dwarf-local")) continue;
        bool changed = !t->score->sig || !p->base->sig || strcmp(t->score->sig, p->base->sig);
        if (!(!strcmp(t->status, "improved") || (!strcmp(t->status, "neutral") && changed))) continue;
        avp_vec_push(&fr, t);
    }
    for (size_t i = 1; i < fr.n; i++)
        for (size_t j = i; j > 0 && fr.v[j - 1]->rows_after > fr.v[j]->rows_after; j--) {
            SvTry *s = fr.v[j];
            fr.v[j] = fr.v[j - 1];
            fr.v[j - 1] = s;
        }
    for (size_t i = 0; i < fr.n && i < 6; i++) {
        AvpTry tt;
        LvCtx *cx = NULL;
        if (AVP_TRY(&tt)) {
            cx = lv_ctx_new(a, fr.v[i]->text, p->fn_symbol);
            avp_try_end(&tt);
        } else {
            cx = NULL;
        }
        if (!cx) continue;
        const char *d = fr.v[i]->desc;
        if (p->helper) d = d + strlen("in helper ") + strlen(p->helper) + 2;
        gen_reuse_tries(g, cx, -1, d, fr.v[i]->form, 2);
    }
    avp_vec_free(&fr);
}

/* ---- declaration moves (probes against the DWARF; candidates when the sibling has no such function) ------------ */

/* The statement list a statement sits in. */
static const LvBlock *block_of(LvCtx *x, const LvStmt *s) {
    for (size_t bi = 0; bi < x->blocks.n; bi++)
        for (i32 k = 0; k < x->blocks.v[bi].L.n; k++)
            if (x->blocks.v[bi].L.v[k] == s) return &x->blocks.v[bi];
    return NULL;
}

/* Whether list L holds s, directly or inside one of its statements. */
static bool list_encloses(const LvStmts *L, const LvStmt *s) {
    for (i32 k = 0; k < L->n; k++)
        if (L->v[k]->a <= s->a && s->b <= L->v[k]->b) return true;
    return false;
}

/* How many of (the order of A and B, B's scope) agree with the DWARF; -1 when it shows neither. */
static int dwarf_agreement(SvP2 *p, const char *A, const char *B, bool b_first, int b_depth) {
    const SvVar *da = sv_vars_find(&p->dwarf, A), *db = sv_vars_find(&p->dwarf, B);
    int n = 0, known = 0;
    if (da && db && da->line > 0 && db->line > 0 && da->line != db->line && (da->depth == 0) == (db->depth == 0)) {
        known++;
        if ((db->line < da->line) == b_first) n++;
    }
    if (db) {
        known++;
        if ((db->depth == 0) == (b_depth == 0)) n++;
    }
    return known ? n : -1;
}

void sv_p2_decl_moves(SvP2 *p, SvTries *out, AvpSet *seen, bool as_candidates) {
    LvCtx *x = p->x;
    AvpArena *a = p->run->a;
    SvGen g = {p, out, seen, lv_slice(x, x->span0, x->span1), 0, 0, NULL, 0};
    size_t made = 0;
    for (size_t bi = 0; bi < x->locals.n && made < 400; bi++) {
        const LvLocal *B = &x->locals.v[bi];
        i64 b0, b1;
        if (is_protected(p, B->name) || !movable_decl(x, B->s, &b0, &b1)) continue;
        const LvBlock *bb = block_of(x, B->s);
        if (!bb) continue;
        int b_depth = bb->owner ? 1 : 0;
        bool b_before_now = false;
        for (size_t ai = 0; ai < x->locals.n && made < 400; ai++) {
            const LvLocal *A = &x->locals.v[ai];
            if (A->s == B->s) continue;
            const LvBlock *ab = block_of(x, A->s);
            if (!ab) continue;
            bool same = ab == bb;
            if (!same && !list_encloses(&ab->L, B->s)) continue;
            i64 a0, a1;
            if (!lv_lines(x, A->s->a, A->s->b, &a0, &a1)) continue;
            b_before_now = B->s->a < A->s->a;
            int now = dwarf_agreement(p, A->name, B->name, b_before_now, b_depth);
            for (int after = 0; after < 2; after++) {
                if (after && (!same || A->s->a < B->s->a)) continue; /* "after A": A later in the same list */
                if (!after && b_before_now && same) continue;          /* already before A */
                int new_depth = ab->owner ? 1 : 0;
                int then = dwarf_agreement(p, A->name, B->name, !after, new_depth);
                if (!as_candidates && now >= 0 && then > now) continue; /* toward the DWARF: a DWARF candidate */
                i64 at = after ? a1 : a0;
                LvEdit ed[2] = {{b0, b1, LVS("")}, {at, at, reindented(a, x, b0, b1, lv_indent(x, A->s->a))}};
                LvS nw;
                if (!sv_apply_edits(a, x->text, ed, 2, &nw)) continue;
                const char *what = avp_sprintf(a, "declare `%s` %s `%s`", B->name, after ? "after" : "before", A->name);
                if (as_candidates)
                    sv_gen_add_try(&g, "decl", avp_sprintf(a, "declaration order unevidenced (no sibling DWARF): %s", what), NULL,
                            tok_line(x, B->s->a), nw, false, 0);
                else
                    sv_gen_add_try(&g, "decl-order", what, NULL, tok_line(x, B->s->a), nw, true, 0);
                made++;
            }
        }
    }
    if (!as_candidates) {
        /* levers' declaration swaps, as probes */
        static const char *const DECL[] = {"decl"};
        AvpTry t;
        if (AVP_TRY(&t)) {
            LvCtx *cx = lv_ctx_new(a, p->text, p->fn_symbol);
            LvCandidates gen = lv_generate(cx, DECL, 1);
            for (size_t i = 0; i < gen.n; i++)
                sv_gen_add_try(&g, "decl-order", avp_sprintf(a, "%s: %s", gen.v[i].lever, lv_utf8(a, gen.v[i].desc)), NULL,
                        gen.v[i].line, gen.v[i].text, true, 0);
            avp_vec_free(&gen);
            avp_try_end(&t);
        }
    }
}

void sv_p2_candidates(SvP2 *p) {
    AvpArena *a = p->run->a;
    SvRecord *rec = p->rec;
    double t0 = avp_time();
    double full = p->deadline;
    /* candidates take at most 60% of the stop's budget: the probes keep the rest */
    p->deadline = t0 + (full - t0) * 0.6;
    bool has_sib = p->sib.ok || p->dbg.ok;
    size_t counts[3] = {0}, ncand = 0;
    AvpMap *tally = avp_map_new(); /* form -> compiled + (failed << 32) */
    SvGen g = {p, NULL, avp_set_new(), lv_slice(p->x, p->x->span0, p->x->span1), 0, 0, NULL, 0};
    /* batch 1: the DWARF forms, const accessors, shaped reuse-local */
    SvTries b1 = {0};
    g.out = &b1;
    gen_dwarf_forms(&g);
    if (!p->surviving) {
        gen_const_accessor(&g);
        gen_reuse_tries(&g, p->x, 0, NULL, NULL, 1);
    }
    sv_p2_score(p, b1.v, b1.n);
    sv_p2_record(p, b1.v, b1.n, counts, tally);
    ncand += b1.n;
    /* batch 2: depth 2 from batch 1's reuse frontier */
    SvTries b2 = {0};
    g.out = &b2;
    if (!p->surviving) deepen(&g, b1.v, b1.n);
    sv_p2_score(p, b2.v, b2.n);
    sv_p2_record(p, b2.v, b2.n, counts, tally);
    ncand += b2.n;
    /* batch 3: levers' families, unshaped reuse-local, and declaration moves when no sibling DWARF exists */
    SvTries b3 = {0};
    g.out = &b3;
    if (!p->surviving) {
        gen_levers(&g, false);
        gen_reuse_tries(&g, p->x, 1, NULL, NULL, 1);
        if (!has_sib) {
            gen_levers(&g, true);
            sv_p2_decl_moves(p, &b3, g.seen, true);
        }
    }
    sv_p2_score(p, b3.v, b3.n);
    sv_p2_record(p, b3.v, b3.n, counts, tally);
    ncand += b3.n;
    /* batch 4: depth 2 from batch 3's reuse frontier */
    SvTries b4 = {0};
    g.out = &b4;
    if (!p->surviving) deepen(&g, b3.v, b3.n);
    sv_p2_score(p, b4.v, b4.n);
    sv_p2_record(p, b4.v, b4.n, counts, tally);
    ncand += b4.n;
    const char *who = p->helper ? avp_sprintf(a, "helper %s: ", p->helper) : "";
    AvpBuf tb;
    avp_buf_init(&tb, a);
    AvpMapIter it = {0};
    while (avp_map_next(tally, &it))
        avp_buf_printf(&tb, "%s%.*s %lld/%lld", tb.n ? ", " : "", AVP_SF(it.key), (long long)(it.val->i & 0xFFFFFFFF),
                       (long long)(it.val->i >> 32));
    avp_map_free(tally);
    sv_note(a, rec, "%scandidates: %zu generated (%zu scored, %zu failed to compile, %zu skipped by the budget), %zu "
                    "rejected by the fake-form filter, %zu kept off a RULE-EXCEPTION: R30 declaration%s%s%s",
            who, ncand, counts[2], counts[1], counts[0], g.rejected, g.kept_r30, tb.n ? "; by form (scored/failed): " : "",
            tb.n ? tb.p : "",
            g.over_budget ? avp_sprintf(a, "; %zu more not built (budget reached while generating)", g.over_budget) : "");
    rec->ran_candidates = rec->ran_candidates || ncand > 0;
    if (counts[0] || g.over_budget) rec->budget_hit = true;
    if (!has_sib && !p->surviving)
        sv_note(a, rec, "%sno sibling DWARF: declaration swaps are candidates (declaration order unevidenced)%s", who,
                p->decl_dropped ? avp_sprintf(a, "; %zu improving swap(s) dropped: the regalloc trace pushes the swapped "
                                                 "locals in different sweeps, so the swap cannot trade their registers",
                                              p->decl_dropped)
                                : "");
    p->deadline = full;
    avp_set_free(g.seen);
}
