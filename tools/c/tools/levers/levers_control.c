/* levers: the control family (levers.py lv_ternary, lv_invert_if, lv_early_return, lv_else_return, lv_void_tail,
 * lv_for_while, lv_while_do, lv_if_merge). */
#include "levers.h"

#include <string.h>

static LvS S(LvCtx *x, i64 a, i64 b) { return lv_slice(x, a, b); }
static const LvTok *T(LvCtx *x, i64 i) { return lv_at(x->code, i); }
#define NL (x->nl)
#define UNIT (x->unit)
#define PAD (x->pad)

static LvS bare(LvCtx *x, const LvNode *n) { return n->kind == LN_PAREN ? lv_nt(x, n->kids[0]) : lv_nt(x, n); }

/* text.split() == ["else", "{"] */
static bool else_brace(LvS s) {
    const char *want[2] = {"else", "{"};
    size_t i = 0;
    int k = 0;
    while (i < s.n) {
        while (i < s.n && lv_isspace((u8)s.p[i])) i++;
        if (i >= s.n) break;
        size_t j = i;
        while (j < s.n && !lv_isspace((u8)s.p[j])) j++;
        if (k >= 2 || !lv_s_eqz((LvS){s.p + i, j - i}, want[k])) return false;
        k++;
        i = j;
    }
    return k == 2;
}

static bool any_label(LvStmts L, i32 from) {
    for (i32 i = from; i < L.n; i++)
        if (L.v[i]->kind == LS_LABEL) return true;
    return false;
}

static bool is_assign_eq(const LvNode *n) { return n->kind == LN_ASSIGN && n->op && !strcmp(n->op, "="); }

static void ternary(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            i64 l0 = 0, l1 = 0;
            bool ln = lv_lines(x, s->a, s->b, &l0, &l1);
            LvS ind = lv_indent(x, s->a);
            if ((s->kind == LS_EXPR || s->kind == LS_RETURN) && ln) {
                LvNode *n = lv_expr_of(x, s);
                if (!n) continue;
                if (s->kind == LS_EXPR && is_assign_eq(n) && n->kids[1]->kind == LN_TERN) {
                    LvNode *c = n->kids[1]->kids[0], *a = n->kids[1]->kids[1], *b = n->kids[1]->kids[2];
                    LvS lv = lv_nt(x, n->kids[0]);
                    LvS nw = LV_CAT(x->a, lv_if_head(x, ind, bare(x, c)), ind, UNIT, lv, LVS(" = "), lv_nt(x, a),
                                    LVS(";"), NL, ind, LVS("} else {"), NL, ind, UNIT, lv, LVS(" = "), lv_nt(x, b),
                                    LVS(";"), NL, ind, LVS("}"), NL);
                    LV_YIELD(x, "?: to if/else assignment", l0, {l0, l1, nw});
                }
                if (s->kind == LS_RETURN && n->kind == LN_TERN) {
                    LvNode *c = n->kids[0], *a = n->kids[1], *b = n->kids[2];
                    LvS nw = LV_CAT(x->a, lv_if_head(x, ind, bare(x, c)), ind, UNIT, LVS("return "), lv_nt(x, a),
                                    LVS(";"), NL, ind, LVS("}"), NL, ind, LVS("return "), lv_nt(x, b), LVS(";"), NL);
                    LV_YIELD(x, "?: to if and return", l0, {l0, l1, nw});
                }
            }
            if (s->kind != LS_IF || !ln) continue;
            LvNode *c = lv_cond_of(x, s);
            LvStmt *t1 = lv_sole(s->then);
            if (!c || !t1) continue;
            LvS cw = lv_wrap(x, c, 4);
            if (s->els) {
                LvStmt *t2 = lv_sole(s->els);
                if (!t2 || t1->kind != t2->kind) continue;
                LvNode *n1 = lv_expr_of(x, t1), *n2 = lv_expr_of(x, t2);
                if (!n1 || !n2) continue;
                if (t1->kind == LS_EXPR && is_assign_eq(n1) && is_assign_eq(n2) &&
                    !strcmp(lv_norm(x, n1->kids[0]->a, n1->kids[0]->b), lv_norm(x, n2->kids[0]->a, n2->kids[0]->b))) {
                    LvS nw = LV_CAT(x->a, ind, lv_nt(x, n1->kids[0]), LVS(" = "), cw, LVS(" ? "),
                                    lv_wrap(x, n1->kids[1], 3), LVS(" : "), lv_wrap(x, n2->kids[1], 3), LVS(";"), NL);
                    LV_YIELD(x, "if/else assignment to ?:", l0, {l0, l1, nw});
                } else if (t1->kind == LS_RETURN) {
                    LvS nw = LV_CAT(x->a, ind, LVS("return "), cw, LVS(" ? "), lv_wrap(x, n1, 3), LVS(" : "),
                                    lv_wrap(x, n2, 3), LVS(";"), NL);
                    LV_YIELD(x, "if/else return to ?:", l0, {l0, l1, nw});
                }
            } else if (t1->kind == LS_RETURN && k + 1 < L.n && L.v[k + 1]->kind == LS_RETURN) {
                LvNode *n1 = lv_expr_of(x, t1), *n2 = lv_expr_of(x, L.v[k + 1]);
                i64 m0, m1;
                bool l2 = lv_lines(x, L.v[k + 1]->a, L.v[k + 1]->b, &m0, &m1);
                if (!n1 || !n2 || !l2) continue;
                LvS nw = LV_CAT(x->a, ind, LVS("return "), cw, LVS(" ? "), lv_wrap(x, n1, 3), LVS(" : "),
                                lv_wrap(x, n2, 3), LVS(";"), NL);
                LV_YIELD(x, "if and return to ?:", l0, {l0, m1, nw});
            }
        }
    }
}

static void invert_if(LvCtx *x) {
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind != LS_IF || !s->els || s->then->kind != LS_BLOCK || s->els->kind != LS_BLOCK) continue;
        LvNode *c = lv_cond_of(x, s);
        if (!c) continue;
        i64 ta = T(x, s->then->a)->s, tb = T(x, s->then->b)->e;
        i64 ea = T(x, s->els->a)->s, eb = T(x, s->els->b)->e;
        LV_YIELD(x, "invert if/else", T(x, s->a)->s, {T(x, c->a)->s, T(x, c->b)->e, lv_negate(x, c)},
                 {ta, tb, S(x, ea, eb)}, {ea, eb, S(x, ta, tb)});
    }
}

static bool is_bare_return(const LvStmt *st) {
    const LvStmt *s = lv_sole(st);
    return s && s->kind == LS_RETURN && !s->expr.ok;
}

static void early_return(LvCtx *x) {
    LvStmts L = x->body;
    if (!x->fn.is_void) return;
    for (i32 k = 0; k < L.n; k++) {
        LvStmt *s = L.v[k];
        if (s->kind != LS_IF || s->els) continue;
        LvNode *c = lv_cond_of(x, s);
        i64 l0, l1;
        bool ln = lv_lines(x, s->a, s->b, &l0, &l1);
        if (!c || !ln) continue;
        LvS ind = lv_indent(x, s->a);
        if (is_bare_return(s->then) && k + 1 < L.n) {
            i64 f0, f1, z0, z1;
            bool first = lv_lines(x, L.v[k + 1]->a, L.v[k + 1]->b, &f0, &f1);
            bool last = lv_lines(x, L.v[L.n - 1]->a, L.v[L.n - 1]->b, &z0, &z1);
            if (!first || !last || any_label(L, k + 1)) continue;
            LvS rest;
            if (!lv_reindent(x, S(x, f0, z1), +1, false, &rest)) continue;
            LvS nw = LV_CAT(x->a, lv_if_head(x, ind, lv_negate(x, c)), rest, ind, LVS("}"), NL);
            LV_YIELD(x, "early return to wrapped rest", l0, {l0, z1, nw});
        } else if (k == L.n - 1 && s->then->kind == LS_BLOCK && !is_bare_return(s->then)) {
            i64 i0, i1;
            if (!lv_block_inner(x, s->then, &i0, &i1)) continue;
            LvS body;
            if (!lv_reindent(x, S(x, i0, i1), -1, false, &body)) continue;
            LvS nw = LV_CAT(x->a, lv_if_head(x, ind, lv_negate(x, c)), ind, UNIT, LVS("return;"), NL, ind, LVS("}"),
                            NL, body);
            LV_YIELD(x, "wrapped tail to early return", l0, {l0, l1, nw});
        }
    }
}

static bool ends_return(const LvStmt *st) {
    return st->kind == LS_BLOCK && st->list.n && st->list.v[st->list.n - 1]->kind == LS_RETURN;
}

static void else_return(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            i64 l0, l1;
            if (s->kind != LS_IF || !ends_return(s->then) || !lv_lines(x, s->a, s->b, &l0, &l1)) continue;
            LvS ind = lv_indent(x, s->a);
            const LvTok *rb = T(x, s->then->b);
            i64 rbl = lv_line_start(x, rb->s);
            if (!lv_blank(S(x, rbl, rb->s))) continue;
            if (!s->els && k + 1 < L.n) {
                i64 f0, f1, z0, z1;
                bool first = lv_lines(x, L.v[k + 1]->a, L.v[k + 1]->b, &f0, &f1);
                bool last = lv_lines(x, L.v[L.n - 1]->a, L.v[L.n - 1]->b, &z0, &z1);
                if (!first || !last || any_label(L, k + 1)) continue;
                LvS rest;
                if (!lv_reindent(x, S(x, f0, z1), +1, false, &rest) || !lv_blank(S(x, lv_line_end(x, rb->e), f0)))
                    continue;
                LvS nw = LV_CAT(x->a, ind, LVS("} else {"), NL, rest, ind, LVS("}"), NL);
                LV_YIELD(x, "rest after return into else", rbl, {rbl, z1, nw});
            } else if (s->els && s->els->kind == LS_BLOCK) {
                i64 i0, i1;
                if (!lv_block_inner(x, s->els, &i0, &i1) || !else_brace(S(x, rb->e, T(x, s->els->a)->e))) continue;
                LvS body;
                bool okb = lv_reindent(x, S(x, i0, i1), -1, false, &body);
                i64 end = lv_line_end(x, T(x, s->els->b)->e);
                if (!okb || !lv_blank(S(x, T(x, s->els->b)->e, end))) continue;
                LvS nw = LV_CAT(x->a, ind, LVS("}"), NL, body);
                LV_YIELD(x, "else after return to rest", rbl, {rbl, end, nw});
            }
        }
    }
}

static void void_tail(LvCtx *x) {
    LvStmts L = x->body;
    if (!x->fn.is_void) return;
    for (i32 k = 0; k < L.n; k++) {
        LvStmt *s = L.v[k];
        i64 l0, l1;
        if (s->kind != LS_IF || s->then->kind != LS_BLOCK || !s->then->list.n || !lv_lines(x, s->a, s->b, &l0, &l1))
            continue;
        LvS ind = lv_indent(x, s->a);
        const LvTok *rb = T(x, s->then->b);
        i64 rbl = lv_line_start(x, rb->s);
        if (!lv_blank(S(x, rbl, rb->s))) continue;
        LvStmt *last_t = s->then->list.v[s->then->list.n - 1];
        if (!s->els && k + 1 < L.n && last_t->kind == LS_RETURN && !last_t->expr.ok && s->then->list.n > 1) {
            i64 r0, r1, f0, f1, z0, z1;
            bool lr = lv_lines(x, last_t->a, last_t->b, &r0, &r1);
            bool first = lv_lines(x, L.v[k + 1]->a, L.v[k + 1]->b, &f0, &f1);
            bool last = lv_lines(x, L.v[L.n - 1]->a, L.v[L.n - 1]->b, &z0, &z1);
            if (!lr || !first || !last || any_label(L, k + 1)) continue;
            if (!lv_blank(S(x, lv_line_end(x, rb->e), f0))) continue;
            LvS rest;
            lv_reindent(x, S(x, f0, z1), +1, false, &rest);
            LvS nw = LV_CAT(x->a, ind, LVS("} else {"), NL, rest, ind, LVS("}"), NL);
            LV_YIELD(x, "return and rest to else (void tail)", r0, {r0, r1, LVS("")}, {rbl, z1, nw});
        } else if (s->els && s->els->kind == LS_BLOCK && k == L.n - 1 && last_t->kind != LS_RETURN) {
            i64 i0, i1;
            if (!lv_block_inner(x, s->els, &i0, &i1) || !else_brace(S(x, rb->e, T(x, s->els->a)->e))) continue;
            LvS body;
            bool okb = lv_reindent(x, S(x, i0, i1), -1, false, &body);
            i64 end = lv_line_end(x, T(x, s->els->b)->e);
            if (!okb || !lv_blank(S(x, T(x, s->els->b)->e, end))) continue;
            LvS nw = LV_CAT(x->a, ind, UNIT, LVS("return;"), NL, ind, LVS("}"), NL, body);
            LV_YIELD(x, "else to return and rest (void tail)", rbl, {rbl, end, nw});
        }
    }
}

static void for_while(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            i64 l0 = 0, l1 = 0;
            bool ln = (s->kind == LS_FOR || s->kind == LS_WHILE) && lv_lines(x, s->a, s->b, &l0, &l1);
            if (!ln || s->body->kind != LS_BLOCK || lv_ids_hasz(lv_ids(x, s->body->a, s->body->b), "continue")) continue;
            LvS ind = lv_indent(x, s->a);
            const LvTok *rb = T(x, s->body->b);
            i64 rbl = lv_line_start(x, rb->s);
            if (!lv_blank(S(x, rbl, rb->s))) continue;
            if (s->kind == LS_FOR && s->cond.ok && s->step.ok) {
                if (s->init.ok && lv_parse_decl(x->a, x->code, s->init.a, s->init.b + 1, x->m)) continue;
                LvS head = LV_CAT(x->a, LVS("while ("), PAD, lv_src(x, s->cond.a, s->cond.b), PAD, LVS(")"));
                LvEdit ed[3];
                size_t ne = 0;
                ed[ne++] = (LvEdit){T(x, s->a)->s, T(x, s->rp)->e, head};
                ed[ne++] = (LvEdit){rbl, rbl, LV_CAT(x->a, ind, UNIT, lv_src(x, s->step.a, s->step.b), LVS(";"), NL)};
                if (s->init.ok)
                    ed[ne++] = (LvEdit){T(x, s->a)->s, T(x, s->a)->s,
                                        LV_CAT(x->a, lv_src(x, s->init.a, s->init.b), LVS(";"), NL, ind)};
                lv_yield(x, "for to while", l0, ed, ne);
            } else if (s->kind == LS_WHILE && s->body->list.n) {
                LvStmt *last = s->body->list.v[s->body->list.n - 1];
                LvNode *n = last->kind == LS_EXPR ? lv_expr_of(x, last) : NULL;
                i64 c0, c1;
                bool lc = lv_lines(x, last->a, last->b, &c0, &c1);
                LvIds cond_ids = lv_ids(x, s->lp + 1, s->rp - 1);
                if (!n || !lc || (n->kind != LN_ASSIGN && n->kind != LN_POST && n->kind != LN_UNARY)) continue;
                if (n->kind == LN_UNARY && strcmp(n->op, "++") && strcmp(n->op, "--")) continue;
                LvNode *tgt = n->kids[0];
                if (tgt->kind != LN_PRIM || !lv_ids_has(cond_ids, T(x, tgt->a)->t)) continue;
                const char *var = T(x, tgt->a)->t;
                LvS init = LVS("");
                LvEdit ed[3];
                size_t ne = 0;
                LvStmt *prev = k > 0 ? L.v[k - 1] : NULL;
                LvNode *pn = prev && prev->kind == LS_EXPR ? lv_expr_of(x, prev) : NULL;
                i64 p0 = 0, p1 = 0;
                bool lp = pn && lv_lines(x, prev->a, prev->b, &p0, &p1);
                if (pn && lp && pn->kind == LN_ASSIGN && lv_s_eq(lv_nt(x, pn->kids[0]), lv_sz(var))) {
                    init = lv_nt(x, pn);
                    ed[ne++] = (LvEdit){p0, p1, LVS("")};
                }
                LvS head = LV_CAT(x->a, LVS("for ("), PAD, init, LVS("; "), lv_src(x, s->lp + 1, s->rp - 1), LVS("; "),
                                  lv_nt(x, n), PAD, LVS(")"));
                ed[ne++] = (LvEdit){T(x, s->a)->s, T(x, s->rp)->e, head};
                ed[ne++] = (LvEdit){c0, c1, LVS("")};
                lv_yield(x, "while to for", l0, ed, ne);
            }
        }
    }
}

static void while_do(LvCtx *x) {
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        i64 l0, l1;
        if (!lv_lines(x, s->a, s->b, &l0, &l1)) continue;
        LvS ind = lv_indent(x, s->a);
        if (s->kind == LS_WHILE && x->file_has_do && s->body->kind == LS_BLOCK) {
            LvNode *cond = lv_cond_of(x, s);
            i64 i0 = 0, i1 = 0;
            bool inner = lv_block_inner(x, s->body, &i0, &i1);
            if (!cond || lv_has_side(cond) || !inner) continue;
            LvS body;
            lv_reindent(x, S(x, i0, i1), +1, false, &body);
            LvS ct = lv_nt(x, cond);
            LvS nw = LV_CAT(x->a, lv_if_head(x, ind, ct), ind, UNIT, LVS("do {"), NL, body, ind, UNIT, LVS("} while ("),
                            PAD, ct, PAD, LVS(");"), NL, ind, LVS("}"), NL);
            LV_YIELD(x, "while to if/do-while", l0, {l0, l1, nw});
        } else if (s->kind == LS_IF && !s->els) {
            LvStmt *d = lv_sole(s->then);
            if (!d || d->kind != LS_DO || d->body->kind != LS_BLOCK) continue;
            if (strcmp(lv_norm(x, s->lp + 1, s->rp - 1), lv_norm(x, d->lp + 1, d->rp - 1))) continue;
            i64 i0, i1;
            if (!lv_block_inner(x, d->body, &i0, &i1)) continue;
            LvS body = S(x, i0, i1);
            if (s->then->kind == LS_BLOCK && !lv_reindent(x, body, -1, false, &body)) continue;
            LvS nw = LV_CAT(x->a, ind, LVS("while ("), PAD, lv_src(x, s->lp + 1, s->rp - 1), PAD, LVS(") {"), NL, body,
                            ind, LVS("}"), NL);
            LV_YIELD(x, "if/do-while to while", l0, {l0, l1, nw});
        }
    }
}

static void if_merge(LvCtx *x) {
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind != LS_IF || s->els) continue;
        i64 l0, l1;
        bool ln = lv_lines(x, s->a, s->b, &l0, &l1);
        LvNode *c = lv_cond_of(x, s);
        if (!ln || !c) continue;
        LvS ind = lv_indent(x, s->a);
        LvStmt *inner_if = lv_sole(s->then);
        if (inner_if && inner_if->kind == LS_IF && !inner_if->els) {
            LvNode *c2 = lv_cond_of(x, inner_if);
            if (!c2) continue;
            LvS then_text = S(x, T(x, inner_if->then->a)->s, T(x, inner_if->then->b)->e);
            if (s->then->kind == LS_BLOCK) {
                if (!lv_reindent(x, then_text, -1, true, &then_text) ||
                    !lv_blank(S(x, T(x, s->then->a)->e, T(x, inner_if->a)->s)))
                    continue;
                if (inner_if->then->kind != LS_BLOCK) continue;
            }
            LvS cond = LV_CAT(x->a, lv_wrap(x, c, 5), LVS(" && "), lv_wrap(x, c2, 6));
            LvS nw = LV_CAT(x->a, ind, LVS("if ("), PAD, cond, PAD, LVS(") "), lv_lstrip(then_text), NL);
            LV_YIELD(x, "merge nested ifs into &&", l0, {l0, l1, nw});
        }
        if (c->kind == LN_BIN && !strcmp(c->op, "&&") && s->then->kind == LS_BLOCK) {
            LvNode *a = c->kids[0], *b = c->kids[1];
            LvS then_text = S(x, T(x, s->then->a)->s, T(x, s->then->b)->e);
            lv_reindent(x, then_text, +1, true, &then_text);
            LvS nw = LV_CAT(x->a, lv_if_head(x, ind, lv_nt(x, a)), ind, UNIT, LVS("if ("), PAD, lv_nt(x, b), PAD,
                            LVS(") "), lv_lstrip(then_text), NL, ind, LVS("}"), NL);
            LV_YIELD(x, "split && into nested ifs", l0, {l0, l1, nw});
        }
    }
}

void lv_control_family(LvCtx *x, int which) {
    switch (which) {
    case 0: ternary(x); break;
    case 1: invert_if(x); break;
    case 2: early_return(x); break;
    case 3: else_return(x); break;
    case 4: void_tail(x); break;
    case 5: for_while(x); break;
    case 6: while_do(x); break;
    case 7: if_merge(x); break;
    default: break;
    }
}
