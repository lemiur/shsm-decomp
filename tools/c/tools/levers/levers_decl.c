/* levers: the decl family (levers.py lv_swap_decls, lv_move_decl, lv_split_init, lv_merge_init, lv_split_multi,
 * lv_merge_multi). */
#include "levers.h"

#include <string.h>

static LvS S(LvCtx *x, i64 a, i64 b) { return lv_slice(x, a, b); }
static const LvTok *T(LvCtx *x, i64 i) { return lv_at(x->code, i); }

static void swap_decls(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k + 1 < L.n; k++) {
            LvStmt *a = L.v[k], *b = L.v[k + 1];
            if (a->kind != LS_DECL || b->kind != LS_DECL || a->decl->is_static || b->decl->is_static) continue;
            i64 lx0, lx1, ly0, ly1;
            bool okx = lv_lines(x, a->a, a->b, &lx0, &lx1), oky = lv_lines(x, b->a, b->b, &ly0, &ly1);
            if (!okx || !oky) continue;
            LvIds yi = lv_ids(x, b->a, b->b);
            bool meet = false;
            for (i32 d = 0; d < a->decl->ndtors; d++)
                if (lv_ids_has(yi, a->decl->dtors[d].name)) meet = true;
            if (meet) continue;
            if (lv_has_call(x, a->a, a->b) && lv_has_call(x, b->a, b->b)) continue;
            LvS tx = S(x, lx0, lx1), ty = S(x, ly0, ly1), mid = S(x, lx1, ly0);
            LV_YIELD(x, "swap declarations", lx0, {lx0, ly1, LV_CAT(x->a, ty, mid, tx)});
        }
    }
}

static bool init_movable(LvCtx *x, const LvDtor *dt, LvStmt **between, i32 nb) {
    if (!dt->init.ok) return true;
    if (dt->complex || lv_has_call(x, dt->init.a, dt->init.b)) return false;
    for (i32 k = dt->init.a; k <= dt->init.b; k++) {
        const LvTok *t = T(x, k);
        if (lv_in(t->t, LV_ASSIGN) || TEQ(t, "++") || TEQ(t, "--")) return false;
    }
    LvIds ids = lv_ids(x, dt->init.a, dt->init.b);
    for (i32 i = 0; i < nb; i++) {
        if (lv_ids_meet(ids, lv_ids(x, between[i]->a, between[i]->b))) return false;
        if (ids.n && lv_has_call(x, between[i]->a, between[i]->b)) return false;
    }
    return true;
}

static void move_decl(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            if (s->kind != LS_DECL || s->decl->is_static || s->decl->ndtors != 1) continue;
            LvDtor *dt = &s->decl->dtors[0];
            i64 l0, l1;
            if (!lv_lines(x, s->a, s->b, &l0, &l1)) continue;
            LvS dtext = S(x, l0, l1);
            i32 u = -1;
            for (i32 j = k + 1; j < L.n; j++)
                if (lv_ids_has(lv_ids(x, L.v[j]->a, L.v[j]->b), dt->name)) {
                    u = j;
                    break;
                }
            if (u >= 0 && u > k + 1 && L.v[u]->kind != LS_LABEL && init_movable(x, dt, L.v + k + 1, u - k - 1) &&
                lv_decl_ok_at(x, L, u, s)) {
                i64 u0, u1;
                if (lv_lines(x, L.v[u]->a, L.v[u]->b, &u0, &u1) && lv_s_eq(lv_indent(x, L.v[u]->a), lv_indent(x, s->a)))
                    LV_YIELD(x, "move declaration to first use", l0, {l0, l1, LVS("")}, {u0, u0, dtext});
            }
            i32 t = L.n;
            for (i32 j = 0; j < L.n; j++)
                if (L.v[j]->kind != LS_DECL) {
                    t = j;
                    break;
                }
            bool nolabel = true;
            for (i32 j = t; j < k; j++)
                if (L.v[j]->kind == LS_LABEL) nolabel = false;
            if (t < k && nolabel) {
                bool used = false;
                for (i32 j = t; j < k; j++)
                    if (lv_ids_has(lv_ids(x, L.v[j]->a, L.v[j]->b), dt->name)) used = true;
                if (used) continue;
                if (!init_movable(x, dt, L.v + t, k - t)) continue;
                i64 t0, t1;
                if (lv_lines(x, L.v[t]->a, L.v[t]->b, &t0, &t1))
                    LV_YIELD(x, "move declaration to top of block", l0, {t0, t0, dtext}, {l0, l1, LVS("")});
            }
        }
    }
}

static bool scalar(LvCtx *x, const LvDecl *d, const LvDtor *dt) {
    bool sub = true, cnst = false;
    for (i32 i = 0; i < d->nwords; i++) {
        const char *w = d->words[i];
        if (!strcmp(w, "const")) cnst = true;
        if (lv_in(w, LV_QUALS)) continue;
        if (!lv_in(w, LV_BUILTIN) && !lv_in(w, LV_SCALAR_TYPEDEFS)) sub = false;
    }
    if (!(dt->ptr || sub) || cnst || dt->array || dt->complex) return false;
    return strchr(lv_norm(x, dt->a, dt->name_i), '&') == NULL;
}

static void split_init(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            if (s->kind != LS_DECL || s->decl->is_static || s->decl->ndtors != 1) continue;
            LvDtor *dt = &s->decl->dtors[0];
            i64 l0, l1;
            if (!dt->init.ok || !scalar(x, s->decl, dt) || !lv_lines(x, s->a, s->b, &l0, &l1)) continue;
            if (!lv_mixed_ok(x)) {
                bool later = false;
                for (i32 j = k + 1; j < L.n; j++)
                    if (L.v[j]->kind == LS_DECL) later = true;
                if (later) continue;
            }
            LvS ind = lv_indent(x, s->a);
            i64 sa = T(x, s->a)->s;
            LvS nw = LV_CAT(x->a, S(x, sa, T(x, dt->name_i)->e), LVS(";"), x->nl, ind, lv_sz(dt->name), LVS(" = "),
                            lv_src(x, dt->init.a, dt->init.b), LVS(";"));
            LV_YIELD(x, "split initialisation", sa, {sa, T(x, s->b)->e, nw});
        }
    }
}

static void merge_init(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k + 1 < L.n; k++) {
            LvStmt *a = L.v[k], *b = L.v[k + 1];
            if (a->kind != LS_DECL || a->decl->is_static || a->decl->ndtors != 1 || b->kind != LS_EXPR) continue;
            LvDtor *dt = &a->decl->dtors[0];
            if (dt->init.ok || !scalar(x, a->decl, dt)) continue;
            LvNode *n = NULL;
            for (size_t q = 0; q < x->exprs.n; q++)
                if (x->exprs.v[q].s == b) {
                    n = x->exprs.v[q].n;
                    break;
                }
            if (!n || n->kind != LN_ASSIGN || strcmp(n->op, "=") || !lv_s_eqz(lv_nt(x, n->kids[0]), dt->name)) continue;
            if (lv_ids_has(lv_ids(x, n->kids[1]->a, n->kids[1]->b), dt->name)) continue;
            i64 d0, d1;
            if (!lv_lines(x, a->a, a->b, &d0, &d1) || !lv_lines(x, b->a, b->b, &d0, &d1)) continue;
            i64 sa = T(x, a->a)->s;
            LvS nw = LV_CAT(x->a, S(x, sa, T(x, dt->name_i)->e), LVS(" = "), lv_nt(x, n->kids[1]), LVS(";"));
            LV_YIELD(x, "merge initialisation", sa, {sa, T(x, a->b)->e, nw},
                     {lv_line_start(x, T(x, b->a)->s), lv_line_end(x, T(x, b->b)->e), LVS("")});
        }
    }
}

static void split_multi(LvCtx *x) {
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        i64 l0, l1;
        if (s->kind != LS_DECL || s->decl->ndtors < 2 || !lv_lines(x, s->a, s->b, &l0, &l1)) continue;
        LvS spec = lv_src(x, s->decl->spec_a, s->decl->spec_b);
        LvS ind = lv_indent(x, s->a);
        LvS sep = LV_CAT(x->a, x->nl, ind);
        AvpBuf b;
        avp_buf_init(&b, x->a);
        for (i32 d = 0; d < s->decl->ndtors; d++) {
            if (d) avp_buf_put(&b, sep);
            avp_buf_put(&b, spec);
            avp_buf_putc(&b, ' ');
            avp_buf_put(&b, lv_src(x, s->decl->dtors[d].a, s->decl->dtors[d].b));
            avp_buf_putc(&b, ';');
        }
        char *z = avp_buf_cz(&b);
        LV_YIELD(x, "split declarators", T(x, s->a)->s, {T(x, s->a)->s, T(x, s->b)->e, ((LvS){z, b.n})});
    }
}

static void merge_multi(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k + 1 < L.n; k++) {
            LvStmt *a = L.v[k], *b = L.v[k + 1];
            if (a->kind != LS_DECL || b->kind != LS_DECL) continue;
            if (strcmp(lv_norm(x, a->decl->spec_a, a->decl->spec_b), lv_norm(x, b->decl->spec_a, b->decl->spec_b)))
                continue;
            i64 d0, d1;
            if (!lv_lines(x, a->a, a->b, &d0, &d1) || !lv_lines(x, b->a, b->b, &d0, &d1)) continue;
            AvpBuf buf;
            avp_buf_init(&buf, x->a);
            avp_buf_put(&buf, lv_src(x, a->decl->spec_a, a->decl->spec_b));
            avp_buf_putc(&buf, ' ');
            int cnt = 0;
            const LvStmt *both[2] = {a, b};
            for (int w = 0; w < 2; w++)
                for (i32 d = 0; d < both[w]->decl->ndtors; d++) {
                    if (cnt++) avp_buf_putz(&buf, ", ");
                    avp_buf_put(&buf, lv_src(x, both[w]->decl->dtors[d].a, both[w]->decl->dtors[d].b));
                }
            avp_buf_putc(&buf, ';');
            char *z = avp_buf_cz(&buf);
            i64 sa = T(x, a->a)->s;
            LV_YIELD(x, "merge declarators", sa, {sa, T(x, a->b)->e, ((LvS){z, buf.n})},
                     {lv_line_start(x, T(x, b->a)->s), lv_line_end(x, T(x, b->b)->e), LVS("")});
        }
    }
}

void lv_decl_family(LvCtx *x, int which) {
    switch (which) {
    case 0: swap_decls(x); break;
    case 1: move_decl(x); break;
    case 2: split_init(x); break;
    case 3: merge_init(x); break;
    case 4: split_multi(x); break;
    case 5: merge_multi(x); break;
    default: break;
    }
}
