/* levers: the expr family (levers.py lv_commute, lv_mirror, lv_compound, lv_incdec, lv_noteq, lv_reassoc). */
#include "levers.h"

#include <string.h>

static const LvTok *T(LvCtx *x, i64 i) { return lv_at(x->code, i); }
static const char *const COMMUTE[] = {"+", "*", "&", "|", "^", "==", "!=", NULL};
static const char *const ASSOC[] = {"+", "*", "&", "|", "^", NULL};
static const char *const COMPOUND[] = {"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", NULL};

static const char *mirror_of(const char *op) {
    if (!strcmp(op, "<")) return ">";
    if (!strcmp(op, ">")) return "<";
    if (!strcmp(op, "<=")) return ">=";
    if (!strcmp(op, ">=")) return "<=";
    return NULL;
}

static LvS swap_text(LvCtx *x, const LvNode *n, const char *op_text) {
    const LvNode *L = n->kids[0], *R = n->kids[1];
    int p = lv_bin_prec(n->op);
    const LvTok *o = T(x, n->opi);
    LvS mid = LV_CAT(x->a, lv_slice(x, T(x, L->b)->e, o->s), lv_sz(op_text), lv_slice(x, o->e, T(x, R->a)->s));
    return LV_CAT(x->a, lv_wrap(x, R, p), mid, lv_wrap(x, L, p + 1));
}

/* for every (stmt, role, root) and every node of root, preorder */
#define EACH_NODE(x, w, n)                                       \
    for (size_t q_ = 0; q_ < (x)->exprs.n; q_++)                 \
        for (size_t i_ = ((w).n = 0, lv_walk((x)->exprs.v[q_].n, &(w)), 0); i_ < (w).n; i_++) \
            for (LvNode *n = (w).v[i_]; n; n = NULL)

static void commute(LvCtx *x) {
    LvNodes w = {0};
    EACH_NODE(x, w, n) {
        if (n->kind == LN_BIN && lv_in(n->op, COMMUTE) && !(lv_has_side(n->kids[0]) && lv_has_side(n->kids[1]))) {
            i64 s = T(x, n->a)->s;
            LV_YIELD(x, avp_sprintf(x->a, "commute %s", n->op), s, {s, T(x, n->b)->e, swap_text(x, n, n->op)});
        }
    }
    avp_vec_free(&w);
}

static void mirror(LvCtx *x) {
    LvNodes w = {0};
    EACH_NODE(x, w, n) {
        if (n->kind == LN_BIN && mirror_of(n->op) && !(lv_has_side(n->kids[0]) && lv_has_side(n->kids[1]))) {
            i64 s = T(x, n->a)->s;
            LV_YIELD(x, avp_sprintf(x->a, "mirror %s", n->op), s, {s, T(x, n->b)->e, swap_text(x, n, mirror_of(n->op))});
        }
    }
    avp_vec_free(&w);
}

static void compound(LvCtx *x) {
    LvNodes w = {0};
    EACH_NODE(x, w, n) {
        if (n->kind != LN_ASSIGN || lv_has_side(n->kids[0])) continue;
        LvNode *L = n->kids[0], *R = n->kids[1];
        const char *lt = lv_norm(x, L->a, L->b);
        i64 s = T(x, n->a)->s, e = T(x, n->b)->e;
        size_t ol = strlen(n->op);
        char base[4] = {0};
        if (ol >= 1 && ol - 1 < sizeof base) memcpy(base, n->op, ol - 1);
        if (!strcmp(n->op, "=") && R->kind == LN_BIN && lv_in(R->op, COMPOUND) &&
            !strcmp(lv_norm(x, R->kids[0]->a, R->kids[0]->b), lt)) {
            LvS nw = LV_CAT(x->a, lv_nt(x, L), LVS(" "), lv_sz(R->op), LVS("= "), lv_nt(x, R->kids[1]));
            LV_YIELD(x, avp_sprintf(x->a, "x = x %s n to %s=", R->op, R->op), s, {s, e, nw});
        } else if (strcmp(n->op, "=") && lv_in(base, COMPOUND)) {
            const char *op = lv_internz(base);
            LvS nw = LV_CAT(x->a, lv_nt(x, L), LVS(" = "), lv_nt(x, L), LVS(" "), lv_sz(op), LVS(" "),
                            lv_wrap(x, R, lv_bin_prec(op) + 1));
            LV_YIELD(x, avp_sprintf(x->a, "%s to x = x %s n", n->op, op), s, {s, e, nw});
        }
    }
    avp_vec_free(&w);
}

static void incdec(LvCtx *x) {
    for (size_t q = 0; q < x->exprs.n; q++) {
        const char *role = x->exprs.v[q].role;
        LvNode *n = x->exprs.v[q].n;
        if (strcmp(role, "expr") && strcmp(role, "step")) continue;
        LvS nw;
        if (n->kind == LN_POST) nw = LV_CAT(x->a, lv_sz(n->op), lv_nt(x, n->kids[0]));
        else if (n->kind == LN_UNARY && (!strcmp(n->op, "++") || !strcmp(n->op, "--")))
            nw = LV_CAT(x->a, lv_nt(x, n->kids[0]), lv_sz(n->op));
        else
            continue;
        i64 s = T(x, n->a)->s;
        LV_YIELD(x, avp_sprintf(x->a, "x%s and %sx", n->op, n->op), s, {s, T(x, n->b)->e, nw});
    }
}

static void noteq(LvCtx *x) {
    LvNodes w = {0};
    EACH_NODE(x, w, n) {
        if (n->kind == LN_UNARY && !strcmp(n->op, "!") && n->kids[0]->kind == LN_PAREN) {
            LvNode *inner = n->kids[0]->kids[0];
            if (inner->kind == LN_BIN && (!strcmp(inner->op, "==") || !strcmp(inner->op, "!="))) {
                const LvTok *o = T(x, inner->opi);
                bool eq = !strcmp(inner->op, "==");
                LvS nw = LV_CAT(x->a, lv_slice(x, T(x, inner->a)->s, o->s), eq ? LVS("!=") : LVS("=="),
                                lv_slice(x, o->e, T(x, inner->b)->e));
                i64 s = T(x, n->a)->s;
                LV_YIELD(x, avp_sprintf(x->a, "!(a %s b) to a %s b", inner->op, eq ? "!=" : "=="), s,
                         {s, T(x, n->b)->e, nw});
            }
        } else if (n->kind == LN_BIN && !strcmp(n->op, "!=")) {
            const LvTok *o = T(x, n->opi);
            LvS nw = LV_CAT(x->a, LVS("!("), lv_slice(x, T(x, n->a)->s, o->s), LVS("=="),
                            lv_slice(x, o->e, T(x, n->b)->e), LVS(")"));
            i64 s = T(x, n->a)->s;
            LV_YIELD(x, "a != b to !(a == b)", s, {s, T(x, n->b)->e, nw});
        }
    }
    avp_vec_free(&w);
}

static bool int_only(LvCtx *x, const LvNode *n) {
    LvNodes w = {0};
    bool ok = true;
    lv_walk(n, &w);
    for (size_t i = 0; ok && i < w.n; i++) {
        const LvNode *e = w.v[i];
        if (e->kind == LN_CALL || e->kind == LN_MEMBER || e->kind == LN_INDEX || e->kind == LN_CAST ||
            e->kind == LN_ASSIGN || (e->kind == LN_UNARY && !strcmp(e->op, "*"))) {
            ok = false;
            break;
        }
        if (e->kind == LN_PRIM) {
            const LvTok *t = T(x, e->a);
            if (t->k == LV_ID && !avp_iset_has(x->int_names, (u64)(uintptr_t)t->t)) ok = false;
            else if (t->k == LV_NUM) {
                bool hex = t->n >= 2 && t->t[0] == '0' && (t->t[1] == 'x' || t->t[1] == 'X');
                if (!hex && strpbrk(t->t, ".eEfF")) ok = false;
            }
            if (ok && t->k != LV_ID && t->k != LV_NUM && t->k != LV_CHR) ok = false;
        }
    }
    avp_vec_free(&w);
    return ok;
}

static void reassoc(LvCtx *x) {
    LvNodes w = {0};
    EACH_NODE(x, w, n) {
        if (n->kind != LN_BIN || !lv_in(n->op, ASSOC)) continue;
        if ((!strcmp(n->op, "+") || !strcmp(n->op, "*")) && !int_only(x, n)) continue;
        LvNode *L = n->kids[0], *R = n->kids[1];
        const char *o = n->op;
        if (L->kind == LN_BIN && !strcmp(L->op, o)) {
            LvNode *B = L->kids[1];
            i64 s0 = T(x, n->a)->s, sb = T(x, B->a)->s, e0 = T(x, n->b)->e;
            LV_YIELD(x, avp_sprintf(x->a, "a %s b %s c to a %s (b %s c)", o, o, o, o), s0,
                     {s0, e0, LV_CAT(x->a, lv_slice(x, s0, sb), LVS("("), lv_slice(x, sb, e0), LVS(")"))});
        }
        if (R->kind == LN_PAREN && R->kids[0]->kind == LN_BIN && !strcmp(R->kids[0]->op, o)) {
            LvNode *inner = R->kids[0];
            i64 s0 = T(x, n->a)->s, e0 = T(x, n->b)->e;
            LV_YIELD(x, avp_sprintf(x->a, "a %s (b %s c) to a %s b %s c", o, o, o, o), s0,
                     {s0, e0, LV_CAT(x->a, lv_slice(x, s0, T(x, R->a)->s), lv_nt(x, inner))});
        }
    }
    avp_vec_free(&w);
}

void lv_expr_family(LvCtx *x, int which) {
    switch (which) {
    case 0: commute(x); break;
    case 1: mirror(x); break;
    case 2: compound(x); break;
    case 3: incdec(x); break;
    case 4: noteq(x); break;
    case 5: reassoc(x); break;
    default: break;
    }
}
