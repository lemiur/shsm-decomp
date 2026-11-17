/* solver_layout: Phase 3, control-flow decisions. For each `control-flow` stop the record carries wall's control
 * finding as its condition (retail's and our block, edge and return counts, the first wrong branch or the missing or
 * extra blocks), and the candidates are source edits of the branch structure, each compiled and scored like Phase 2's:
 *   control / expr    levers' `control` and `expr` families (?: and if/else, inverted if/else, early return, for and
 *                     while, merged and split nested ifs; the operand and comparison respellings), as levers judges them
 *   else-after-return `if ( c ) { ...; return x; } { rest }` written as `... } else { rest }`, and the reverse (an
 *                     else arm after a returning if written as a bare block)
 *   merge-ifs         two consecutive ifs with the same returning body as one `||` condition, and the reverse
 * Levers already offers `if (c) {..return..} rest` against `else { rest }` (all the statements after the if); the
 * two forms above are the ones it does not produce (a bare block that follows the returning if, and two ifs). Every
 * candidate passes the Phase 2 filter (sv_gen_add_try: no fake form) and the rule check; depth 2 composes two edits
 * from the improving states of depth 1. A surviving-source unit gets none. Same-file helpers the target expands get
 * the same treatment. */
#include "solver.h"

static const LvTok *T(LvCtx *x, i64 i) { return lv_at(x->code, i); }
static LvS S(LvCtx *x, i64 a, i64 b) { return lv_slice(x, a, b); }

/* ---- the condition --------------------------------------------------------------------------------------------- */

static i64 geti(const AvpJson *o, const char *k) { return o ? avp_json_get_i64(o, k, -1) : -1; }

/* A short key for what one of wall's control findings says. */
static const char *finding_key(const char *f) {
    static const struct {
        const char *prefix, *key;
    } K[] = {{"extra block(s)", "extra blocks in ours"},
             {"missing block(s)", "missing blocks in ours"},
             {"loop rotation", "loop rotation"},
             {"different loop count", "loop count"},
             {"early return versus shared epilogue", "returns"},
             {"inverted condition", "inverted condition"},
             {"different block order", "block order"},
             {"same block, edge and loop counts", "same shape, other wiring"},
             {"graph search gave up", "undecided"}};
    for (size_t i = 0; i < AVP_ARRAY_LEN(K); i++)
        if (!strncmp(f, K[i].prefix, strlen(K[i].prefix))) return K[i].key;
    if (strstr(f, "aligned branch(es) reach different blocks")) return "wrong branch target";
    if (strstr(f, "switch")) return "switch";
    return "other";
}

static const char *condition_of(SvRun *run, SvRecord *rec, const AvpJson *d, AvpCzVec *keys) {
    AvpArena *a = run->a;
    const AvpJson *cf = avp_json_getz(d, "control");
    const AvpJson *sr = cf ? avp_json_getz(cf, "retail") : NULL, *sc = cf ? avp_json_getz(cf, "compiled") : NULL;
    const AvpJson *iso = cf ? avp_json_getz(cf, "isomorphic") : NULL;
    const AvpJson *fi = cf ? avp_json_getz(cf, "findings") : NULL;
    if (!cf) {
        sv_note(a, rec, "wall gave no control-flow finding for this function");
        return rec->wall_verdict ? avp_sprintf(a, "wall: %s", rec->wall_verdict) : NULL;
    }
    const char *shape = "undecided";
    if (iso && iso->kind == AVP_JSON_BOOL) shape = iso->b ? "isomorphic" : "not isomorphic";
    sv_evidence(a, rec, "wall control flow (%s): retail %lld blocks / %lld edges / %lld returns, ours %lld / %lld / %lld",
                shape, (long long)geti(sr, "blocks"), (long long)geti(sr, "edges"), (long long)geti(sr, "returns"),
                (long long)geti(sc, "blocks"), (long long)geti(sc, "edges"), (long long)geti(sc, "returns"));
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *first = NULL;
    for (size_t i = 0; fi && i < avp_json_len(fi); i++) {
        const char *f = avp_cz(a, avp_json_str_or(avp_json_at(fi, i), AVP_S("")));
        if (!*f) continue;
        if (!first) first = f;
        if (i < 4) sv_evidence(a, rec, "wall control finding: %s", f);
        const char *k = finding_key(f);
        bool seen = false;
        for (size_t j = 0; j < keys->n; j++) seen = seen || !strcmp(keys->v[j], k);
        if (!seen) avp_vec_push(keys, (char *)k);
    }
    avp_buf_printf(&b, "retail's control flow has %lld blocks, %lld edges and %lld returns, ours %lld, %lld and %lld",
                   (long long)geti(sr, "blocks"), (long long)geti(sr, "edges"), (long long)geti(sr, "returns"),
                   (long long)geti(sc, "blocks"), (long long)geti(sc, "edges"), (long long)geti(sc, "returns"));
    if (first) avp_buf_printf(&b, "; wall: %s", first);
    if (rec->site_addr >= 0 && rec->ours && rec->retail)
        avp_buf_printf(&b, "; the first differing branch is at retail 0x%08llX (ours `%s`, retail `%s`)",
                       (unsigned long long)rec->site_addr, rec->ours, rec->retail);
    avp_buf_putz(&b, ": the source's branch structure has to produce retail's blocks");
    return b.p;
}

/* ---- the solver's own forms ------------------------------------------------------------------------------------- */

static bool ends_in_return(const LvStmt *st) {
    if (st->kind == LS_RETURN) return true;
    return st->kind == LS_BLOCK && st->list.n && st->list.v[st->list.n - 1]->kind == LS_RETURN;
}

static bool span_has_pp(LvCtx *x, i64 s, i64 e) {
    for (i32 q = 0; q < x->nppl; q++)
        if (x->ppl[q].ls < e && x->ppl[q].le > s) return true;
    return false;
}

static LvS bare(LvCtx *x, const LvNode *n) { return n->kind == LN_PAREN ? lv_nt(x, n->kids[0]) : lv_nt(x, n); }

static i64 line_of_tok(LvCtx *x, i32 k) { return sv_line_of(x->text, T(x, k)->s); }

/* `if (c) { ..return..} {block}` and `if (c) { ..return..} else {block}`, each written as the other. */
static void gen_else_after_return(SvGen *g, LvCtx *x, const char *prefix, int depth) {
    AvpArena *a = g->p->run->a;
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            if (s->kind != LS_IF || !ends_in_return(s->then)) continue;
            i64 end = T(x, s->then->b)->e;
            LvS ind = lv_indent(x, s->a);
            LvEdit ed[1];
            LvS nw;
            if (!s->els && k + 1 < L.n && L.v[k + 1]->kind == LS_BLOCK) {
                const LvTok *lb = T(x, L.v[k + 1]->a);
                if (!lv_blank(S(x, end, lb->s)) || span_has_pp(x, end, lb->s)) continue;
                LvS rep = s->then->kind == LS_BLOCK ? LVS(" else ") : LV_CAT(x->a, x->nl, ind, LVS("else "));
                ed[0] = (LvEdit){end, lb->s, rep};
                if (sv_apply_edits(a, x->text, ed, 1, &nw))
                    sv_gen_add_try(g, "else-after-return",
                                   avp_sprintf(a, "%sthe block after the returning `if` becomes its else arm", prefix),
                                   "the block follows an `if` whose body returns: an else arm and a bare block compile "
                                   "to the same statements but may place their blocks differently",
                                   line_of_tok(x, s->a), nw, false, depth);
            } else if (s->els && s->els->kind == LS_BLOCK && s->els->a > 0) {
                const LvTok *et = T(x, s->els->a - 1), *lb = T(x, s->els->a);
                if (!lv_s_eqz(lv_tok_s(et), "else") || et->s < end || !lv_blank(S(x, end, et->s)) ||
                    !lv_blank(S(x, et->e, lb->s)) || span_has_pp(x, end, lb->s))
                    continue;
                ed[0] = (LvEdit){end, lb->s, LV_CAT(x->a, x->nl, ind)};
                if (sv_apply_edits(a, x->text, ed, 1, &nw))
                    sv_gen_add_try(g, "else-after-return",
                                   avp_sprintf(a, "%sthe else arm of the returning `if` becomes a bare block", prefix),
                                   "the else arm follows an `if` whose body returns", line_of_tok(x, s->a), nw, false,
                                   depth);
            }
        }
    }
}

/* Two consecutive ifs with the same returning body as one `||` condition, and the reverse. */
static void gen_merge_ifs(SvGen *g, LvCtx *x, const char *prefix, int depth) {
    AvpArena *a = g->p->run->a;
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s1 = L.v[k];
            if (s1->kind != LS_IF || s1->els || !ends_in_return(s1->then)) continue;
            LvNode *c1 = lv_cond_of(x, s1);
            i64 l0, l1;
            if (!c1 || !lv_lines(x, s1->a, s1->b, &l0, &l1)) continue;
            LvS ind = lv_indent(x, s1->a);
            LvS then1 = S(x, T(x, s1->then->a)->s, T(x, s1->then->b)->e);
            LvEdit ed[1];
            LvS nw;
            /* merge with the next if */
            if (k + 1 < L.n) {
                LvStmt *s2 = L.v[k + 1];
                LvNode *c2 = s2->kind == LS_IF && !s2->els && ends_in_return(s2->then) ? lv_cond_of(x, s2) : NULL;
                i64 m0, m1;
                if (c2 && lv_lines(x, s2->a, s2->b, &m0, &m1) &&
                    !strcmp(lv_norm(x, s1->then->a, s1->then->b), lv_norm(x, s2->then->a, s2->then->b)) &&
                    lv_blank(S(x, T(x, s1->b)->e, T(x, s2->a)->s)) && !span_has_pp(x, l0, m1)) {
                    LvS cond = LV_CAT(x->a, lv_wrap(x, c1, 4), LVS(" || "), lv_wrap(x, c2, 5));
                    LvS text = LV_CAT(x->a, ind, LVS("if ("), x->pad, cond, x->pad, LVS(") "), then1, x->nl);
                    ed[0] = (LvEdit){l0, m1, text};
                    if (sv_apply_edits(a, x->text, ed, 1, &nw))
                        sv_gen_add_try(g, "merge-ifs",
                                       avp_sprintf(a, "%smerge the two ifs with the same returning body into one `||` "
                                                      "condition",
                                                   prefix),
                                       "two consecutive ifs have the same returning body", line_of_tok(x, s1->a), nw,
                                       false, depth);
                }
            }
            /* split a top-level || */
            if (c1->kind == LN_BIN && c1->op && !strcmp(c1->op, "||") && !span_has_pp(x, l0, l1)) {
                LvS text = LV_CAT(x->a, ind, LVS("if ("), x->pad, bare(x, c1->kids[0]), x->pad, LVS(") "), then1, x->nl,
                                  x->nl, ind, LVS("if ("), x->pad, bare(x, c1->kids[1]), x->pad, LVS(") "), then1,
                                  x->nl);
                ed[0] = (LvEdit){l0, l1, text};
                if (sv_apply_edits(a, x->text, ed, 1, &nw))
                    sv_gen_add_try(g, "merge-ifs",
                                   avp_sprintf(a, "%ssplit the `||` condition of the returning `if` into two ifs with "
                                                  "the same body",
                                               prefix),
                                   "the returning body is the same for both operands", line_of_tok(x, s1->a), nw, false,
                                   depth);
            }
        }
    }
}

/* levers' control and expr families on one text (a context made on it). */
static void gen_levers_control(SvGen *g, LvCtx *cx, const char *prefix, int depth) {
    AvpArena *a = g->p->run->a;
    static const char *const FAMS[] = {"control", "expr"};
    LvCandidates gen = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        gen = lv_generate(cx, FAMS, 2);
        avp_try_end(&t);
    } else {
        return;
    }
    for (size_t i = 0; i < gen.n; i++) {
        LvCandidate *c = &gen.v[i];
        sv_gen_add_try(g, c->family, avp_sprintf(a, "%s%s: %s", prefix, c->lever, lv_utf8(a, c->desc)), NULL, c->line,
                       c->text, false, depth);
    }
    avp_vec_free(&gen);
}

static void gen_all(SvGen *g, LvCtx *cx, const char *prefix, int depth) {
    gen_else_after_return(g, cx, prefix, depth);
    gen_merge_ifs(g, cx, prefix, depth);
    gen_levers_control(g, cx, prefix, depth);
}

/* Depth 2: the states of a batch that improve or are neutral with changed code, best first (at most 6). */
static void deepen(SvGen *g, SvTry *v, size_t n) {
    SvP2 *p = g->p;
    AvpArena *a = p->run->a;
    AVP_VEC(SvTry *) fr = {0};
    for (size_t i = 0; i < n; i++) {
        SvTry *t = &v[i];
        if (!t->score || !t->score->ok || t->broken.n || t->exact || !t->status) continue;
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
        gen_all(g, cx, avp_sprintf(a, "after `%s`, then ", d), 2);
    }
    avp_vec_free(&fr);
}

/* The candidates of one function (the target or a helper): depth 1, then depth 2 from its improving states. */
static void control_candidates(SvP2 *p) {
    AvpArena *a = p->run->a;
    SvRecord *rec = p->rec;
    size_t counts[3] = {0}, ncand = 0;
    AvpMap *tally = avp_map_new();
    SvGen g = {p, NULL, avp_set_new(), lv_slice(p->x, p->x->span0, p->x->span1), 0, 0, NULL, 0};
    avp_set_add(g.seen, avp_str(avp_sha1_hex(a, p->text.p, p->text.n)));
    SvTries b1 = {0}, b2 = {0};
    g.out = &b1;
    gen_all(&g, p->x, "", 1);
    sv_p2_score(p, b1.v, b1.n);
    sv_p2_record(p, b1.v, b1.n, counts, tally);
    ncand += b1.n;
    g.out = &b2;
    deepen(&g, b1.v, b1.n);
    sv_p2_score(p, b2.v, b2.n);
    sv_p2_record(p, b2.v, b2.n, counts, tally);
    ncand += b2.n;
    const char *who = p->helper ? avp_sprintf(a, "helper %s: ", p->helper) : "";
    AvpBuf tb;
    avp_buf_init(&tb, a);
    AvpMapIter it = {0};
    while (avp_map_next(tally, &it))
        avp_buf_printf(&tb, "%s%.*s %lld/%lld", tb.n ? ", " : "", AVP_SF(it.key), (long long)(it.val->i & 0xFFFFFFFF),
                       (long long)(it.val->i >> 32));
    avp_map_free(tally);
    sv_note(a, rec, "%scandidates: %zu generated (%zu scored, %zu failed to compile, %zu skipped by the budget), %zu "
                    "rejected by the fake-form filter%s%s%s",
            who, ncand, counts[2], counts[1], counts[0], g.rejected, tb.n ? "; by form (scored/failed): " : "",
            tb.n ? tb.p : "",
            g.over_budget ? avp_sprintf(a, "; %zu more not built (budget reached while generating)", g.over_budget) : "");
    rec->ran_candidates = rec->ran_candidates || ncand > 0;
    if (counts[0] || g.over_budget) rec->budget_hit = true;
    avp_set_free(g.seen);
}

/* ---- the stop -------------------------------------------------------------------------------------------------- */

void sv_phase3_control(SvRun *run, SvUnitWork *w, SvRecord *rec, const AvpJson *d, const CcCompareResult *res) {
    AvpArena *a = run->a;
    rec->phase_built = true;
    AvpCzVec keys = {0};
    rec->condition = condition_of(run, rec, d, &keys);
    SvP2 *p = AVP_NEW(a, SvP2);
    p->run = run;
    p->w = w;
    p->rec = rec;
    p->res = res;
    p->symbol = rec->function;
    p->fn_symbol = rec->function;
    p->text = w->text0;
    p->surviving = rec->surviving_source;
    p->deadline = avp_time() + (run->stop_seconds > 0 ? run->stop_seconds : 120);
    p->base = sv_lvscore(a, w->base, rec->function);
    if (p->surviving) {
        const SvRuling *ru = sv_rulings_surviving(&run->rulings, w->u->name, w->u->path);
        sv_note(a, rec, "surviving source (solver.tsv: surviving-source %s%s%s): no respelling candidates", ru->arg1,
                *ru->note ? ", " : "", ru->note);
    } else if (run->diagnose_only) {
        sv_note(a, rec, "--diagnose-only: no candidate compiled");
    } else if (!p->base->ok) {
        sv_note(a, rec, "the unmodified scratch copy does not score the function: %s", p->base->error);
    } else if (!sv_p2_open_ctx(p, rec->function)) {
        sv_note(a, rec, "the function's text does not parse with levers' parser: no candidates");
        avp_vec_push(&keys, (char *)"unparsable");
    } else {
        control_candidates(p);
        AvpCzVec helpers = sv_p2_expanded_helpers(p);
        for (size_t i = 0; i < helpers.n; i++) {
            SvP2 *h = AVP_NEW(a, SvP2);
            *h = *p;
            h->helper = helpers.v[i];
            h->fn_symbol = helpers.v[i];
            h->x = NULL;
            sv_evidence(a, rec, "same-file helper %s: ours and retail both expand it here (neither calls it)", h->helper);
            if (!sv_p2_open_ctx(h, h->helper)) {
                sv_note(a, rec, "helper %s does not parse: skipped", h->helper);
                continue;
            }
            control_candidates(h);
        }
    }
    AvpBuf cb;
    avp_buf_init(&cb, a);
    avp_buf_putz(&cb, "control-flow:");
    if (!keys.n) avp_buf_putz(&cb, " wall named no difference");
    for (size_t i = 0; i < keys.n; i++) avp_buf_printf(&cb, "%s %s", i ? "," : "", keys.v[i]);
    rec->cause = cb.p;
    avp_vec_free(&keys);
}

void sv_p3_own_forms(SvGen *g, LvCtx *x) {
    gen_else_after_return(g, x, "", 1);
    gen_merge_ifs(g, x, "", 1);
}
