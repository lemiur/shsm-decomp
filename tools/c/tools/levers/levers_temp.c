/* levers: the temp, type and order families (levers.py lv_inline_local, lv_intro_temp, _occurrences, lv_retype,
 * _simple_assign, _mem_fields, lv_swap_stmts). */
#include "levers.h"

#include <string.h>

static LvS S(LvCtx *x, i64 a, i64 b) { return lv_slice(x, a, b); }
static const LvTok *T(LvCtx *x, i64 i) { return lv_at(x->code, i); }
static u64 P(const char *p) { return (u64)(uintptr_t)p; }

static bool member_op(const LvTok *t) { return TEQ(t, ".") || TEQ(t, "->"); }
static bool loop_kw(const char *t) { return !strcmp(t, "for") || !strcmp(t, "while") || !strcmp(t, "do"); }

/* s <= t for two id sets */
static bool ids_subset(LvIds s, LvIds t) {
    for (i32 i = 0; i < s.n; i++)
        if (!lv_ids_has(t, s.v[i])) return false;
    return true;
}

/* ---- temp ---- */

static void inline_local(LvCtx *x) {
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            if (s->kind != LS_DECL || s->decl->is_static || s->decl->ndtors != 1) continue;
            LvDtor *dt = &s->decl->dtors[0];
            i64 l0, l1;
            bool ln = lv_lines(x, s->a, s->b, &l0, &l1);
            bool narrow = false;
            for (i32 w = 0; w < s->decl->nwords; w++)
                if (lv_in(s->decl->words[w], LV_NARROW)) narrow = true;
            if (!dt->init.ok || dt->complex || !ln || narrow) continue;
            LvNode *n = NULL;
            for (size_t q = 0; q < x->exprs.n && !n; q++)
                if (x->exprs.v[q].s == s && x->exprs.v[q].n->a == dt->init.a) n = x->exprs.v[q].n;
            if (!n || lv_has_side(n) || lv_has_call(x, n->a, n->b)) continue;
            i32 nuses = 0, u = -1;
            for (i32 i = s->b + 1; i < x->rb; i++) {
                const LvTok *t = T(x, i);
                if (t->k == LV_ID && t->t == dt->name && !(i > 0 && member_op(T(x, i - 1)))) {
                    nuses++;
                    u = i;
                }
            }
            if (nuses != 1) continue;
            const LvTok *nx = T(x, u + 1), *pv = T(x, u - 1);
            if (lv_in(nx->t, LV_ASSIGN) || TEQ(nx, "++") || TEQ(nx, "--") || TEQ(nx, "[") || TEQ(pv, "++") ||
                TEQ(pv, "--") || TEQ(pv, "&"))
                continue;
            i32 j = -1;
            for (i32 q = k + 1; q < L.n; q++)
                if (L.v[q]->a <= u && u <= L.v[q]->b) {
                    j = q;
                    break;
                }
            if (j < 0 || L.v[j]->kind == LS_FOR || L.v[j]->kind == LS_WHILE || L.v[j]->kind == LS_DO) continue;
            bool loop = false;
            for (i32 i = L.v[j]->a; i < u; i++)
                if (loop_kw(T(x, i)->t)) loop = true;
            if (loop) continue;
            LvIds ids = lv_ids(x, n->a, n->b);
            bool bad = false;
            for (i32 q = k + 1; q < j && !bad; q++)
                if (lv_ids_meet(ids, lv_ids(x, L.v[q]->a, L.v[q]->b)) || lv_has_call(x, L.v[q]->a, L.v[q]->b)) bad = true;
            if (bad) continue;
            LvWrites ws = lv_writes(x, L.v[j]->a, u);
            for (size_t q = 0; q < ws.n && !bad; q++)
                if (lv_ids_meet(ws.v[q].ids, ids) || (ws.v[q].last && lv_ids_has(ids, ws.v[q].last))) bad = true;
            avp_vec_free(&ws);
            if (bad) continue;
            LvS rep = lv_prec(n) >= 16 ? lv_nt(x, n) : LV_CAT(x->a, LVS("("), lv_nt(x, n), LVS(")"));
            LV_YIELD(x, "inline single-use local", l0, {l0, l1, LVS("")}, {T(x, u)->s, T(x, u)->e, rep});
        }
    }
}

/* _occurrences(L, k, e, key) */
static void occurrences(LvCtx *x, LvStmts L, i32 k, const LvNode *e, const char *key, LvNodes *occ) {
    occ->n = 0;
    LvIds ids = lv_ids(x, e->a, e->b);
    AVP_VEC(const char *) fields = {0};
    for (i32 i = e->a; i <= e->b; i++)
        if (i > 0 && member_op(T(x, i - 1))) avp_vec_push(&fields, T(x, i)->t);
    LvIds fl = {fields.v, (i32)fields.n};
    size_t keyn = strlen(key);
    LvNodes w = {0};
    for (i32 j = k; j < L.n; j++) {
        LvStmt *st = L.v[j];
        if (st->kind == LS_LABEL || st->kind == LS_FOR || st->kind == LS_WHILE || st->kind == LS_DO) break;
        for (size_t q = 0; q < x->exprs.n; q++) {
            LvNode *root = x->exprs.v[q].n;
            if (!(st->a <= root->a && root->b <= st->b)) continue;
            w.n = 0;
            lv_walk(root, &w);
            for (size_t i = 0; i < w.n; i++) {
                LvNode *n = w.v[i];
                if (strcmp(lv_norm(x, n->a, n->b), key)) continue;
                bool inside = false;
                for (size_t o = 0; o < occ->n; o++)
                    if (occ->v[o]->a <= n->a && n->a <= occ->v[o]->b) inside = true;
                if (!inside) avp_vec_push(occ, n);
            }
        }
        LvWrites lv = lv_writes(x, st->a, st->b);
        bool hit = lv_has_call(x, st->a, st->b);
        for (size_t q = 0; q < lv.n && !hit; q++) {
            const LvWrite *wr = &lv.v[q];
            size_t wn = strlen(wr->key);
            if (!strcmp(wr->key, key) || (keyn > wn && !memcmp(key, wr->key, wn) && key[wn] == ' ') ||
                (ids_subset(wr->ids, ids) && wr->last && lv_ids_has(ids, wr->last)) ||
                (wr->last && lv_ids_has(fl, wr->last)))
                hit = true;
        }
        if (hit) {
            bool same = false;
            for (size_t q = 0; q < lv.n; q++)
                if (!strcmp(lv.v[q].key, key)) same = true;
            avp_vec_free(&lv);
            if (same) occ->n = 0;
            break;
        }
        avp_vec_free(&lv);
    }
    avp_vec_free(&w);
    avp_vec_free(&fields);
}

static void intro_temp(LvCtx *x) {
    AvpMap *known = avp_map_new();
    for (size_t q = 0; q < x->locals.n; q++) {
        LvLocal *lc = &x->locals.v[q];
        LvDtor *dt = lc->dt;
        if (!dt->init.ok || dt->complex) continue;
        const char *key = lv_norm(x, dt->init.a, dt->init.b);
        if (avp_map_get(known, avp_str(key))) continue;
        AvpBuf b;
        avp_buf_init(&b, x->a);
        avp_buf_put(&b, lv_src(x, lc->s->decl->spec_a, lc->s->decl->spec_b));
        for (i32 i = dt->a; i < dt->name_i; i++) avp_buf_put(&b, lv_tok_s(T(x, i)));
        char *z = avp_buf_cz(&b);
        LvS *tv = avp_alloc(x->a, sizeof *tv);
        *tv = (LvS){z, b.n};
        avp_map_put(known, avp_str(key), NULL)->p = tv;
    }
    AvpISet *plain = avp_iset_new();
    for (i32 i = x->fn.head; i < x->rb; i++)
        if (T(x, i)->k == LV_ID && !member_op(T(x, i - 1))) avp_iset_add(plain, P(T(x, i)->t));
    LvNodes w = {0}, occ = {0};
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k < L.n; k++) {
            LvStmt *s = L.v[k];
            i64 l0, l1;
            bool ln = lv_lines(x, s->a, s->b, &l0, &l1);
            if (!ln || s->kind == LS_LABEL || s->kind == LS_DECL || !lv_decl_ok_at(x, L, k, NULL)) continue;
            AvpSet *seen = avp_set_new();
            for (size_t q = 0; q < x->exprs.n; q++) {
                LvNode *root = x->exprs.v[q].n;
                if (!(s->a <= root->a && root->b <= s->b)) continue;
                w.n = 0;
                lv_walk(root, &w);
                for (size_t i = 0; i < w.n; i++) {
                    LvNode *e = w.v[i];
                    if ((e->kind != LN_MEMBER && e->kind != LN_INDEX && e->kind != LN_CAST) || e->b - e->a < 2 ||
                        lv_has_side(e))
                        continue;
                    const char *key = lv_norm(x, e->a, e->b);
                    if (avp_set_has(seen, avp_str(key))) continue;
                    avp_set_add(seen, avp_str(key));
                    LvS typ = {0};
                    if (e->kind == LN_CAST)
                        typ = lv_src(x, e->ex_a, e->ex_b);
                    else {
                        AvpVal *v = avp_map_get(known, avp_str(key));
                        if (v) typ = *(LvS *)v->p;
                    }
                    if (!typ.n) continue;
                    const char *name = NULL;
                    for (i32 t = e->b; t >= e->a; t--)
                        if (T(x, t)->k == LV_ID) {
                            name = T(x, t)->t;
                            break;
                        }
                    if (!name || avp_iset_has(plain, P(name)) || avp_iset_has(x->typenames, P(name))) continue;
                    occurrences(x, L, k, e, key, &occ);
                    if (occ.n < 2) continue;
                    LvS ind = lv_indent(x, s->a);
                    LvS decl = LV_CAT(x->a, ind, lv_rstrip(typ), LVS(" "), lv_sz(name), LVS(" = "), lv_nt(x, e),
                                      LVS(";"), x->nl);
                    LvEdit *ed = avp_alloc(x->a, sizeof(LvEdit) * (occ.n + 1));
                    ed[0] = (LvEdit){l0, l0, decl};
                    for (size_t o = 0; o < occ.n; o++)
                        ed[o + 1] = (LvEdit){T(x, occ.v[o]->a)->s, T(x, occ.v[o]->b)->e, lv_sz(name)};
                    lv_yield(x, "introduce local for repeated subexpression", l0, ed, occ.n + 1);
                }
            }
            avp_set_free(seen);
        }
    }
    avp_vec_free(&w);
    avp_vec_free(&occ);
    avp_iset_free(plain);
    avp_map_free(known);
}

void lv_temp_family(LvCtx *x, int which) {
    if (which == 0) inline_local(x);
    else if (which == 1) intro_temp(x);
}

/* ---- type ---- */

static const char *const SIGN_FLIP[][2] = {
    {"int", "unsigned int"}, {"unsigned int", "int"}, {"unsigned", "int"}, {"s32", "u32"}, {"u32", "s32"},
    {"s16", "u16"}, {"u16", "s16"}, {"s8", "u8"}, {"u8", "s8"}, {"long", "unsigned long"},
    {"unsigned long", "long"}, {"short", "unsigned short"}, {"unsigned short", "short"}, {NULL, NULL}};
static const char *const SPELL[][2] = {
    {"int", "s32"}, {"s32", "int"}, {"unsigned int", "u32"}, {"u32", "unsigned int"}, {"float", "f32"},
    {"f32", "float"}, {"double", "f64"}, {"f64", "double"}, {"unsigned long", "u32"}, {"long", "s32"},
    {NULL, NULL}};

static const char *lookup(const char *const (*tbl)[2], const char *k) {
    for (size_t i = 0; tbl[i][0]; i++)
        if (!strcmp(tbl[i][0], k)) return tbl[i][1];
    return NULL;
}

static bool in_file_ids(LvCtx *x, const char *z) {
    const char *p = lv_intern_find(z);
    return p && avp_iset_has(x->file_ids, P(p));
}

static void add_ids(LvCtx *x, AvpISet *set, i32 a, i32 b) {
    LvIds ids = lv_ids(x, a, b);
    for (i32 i = 0; i < ids.n; i++) avp_iset_add(set, P(ids.v[i]));
}

void lv_type_family(LvCtx *x) {
    AvpISet *used = avp_iset_new(); /* index_ids | for_ids */
    LvNodes w = {0};
    for (size_t q = 0; q < x->exprs.n; q++) {
        w.n = 0;
        lv_walk(x->exprs.v[q].n, &w);
        for (size_t i = 0; i < w.n; i++)
            if (w.v[i]->kind == LN_INDEX) add_ids(x, used, w.v[i]->kids[1]->a, w.v[i]->kids[1]->b);
    }
    avp_vec_free(&w);
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind != LS_FOR) continue;
        LvRange rs[3] = {s->init, s->cond, s->step};
        for (int r = 0; r < 3; r++)
            if (rs[r].ok) add_ids(x, used, rs[r].a, rs[r].b);
    }
    for (size_t q = 0; q < x->all_stmts.n; q++) {
        LvStmt *s = x->all_stmts.v[q];
        if (s->kind != LS_DECL || s->decl->ndtors != 1 || s->decl->is_static) continue;
        LvDtor *dt = &s->decl->dtors[0];
        if (dt->ptr || dt->array) continue;
        /* words of the normalised spec (token texts joined by spaces, split again) */
        AvpBuf core;
        avp_buf_init(&core, x->a);
        bool has_const = false, first_const = false;
        i32 nw = 0;
        LvS spec = lv_sz(lv_norm(x, s->decl->spec_a, s->decl->spec_b));
        size_t i = 0;
        while (i < spec.n) {
            while (i < spec.n && lv_isspace((u8)spec.p[i])) i++;
            if (i >= spec.n) break;
            size_t j = i;
            while (j < spec.n && !lv_isspace((u8)spec.p[j])) j++;
            LvS word = {spec.p + i, j - i};
            if (lv_s_eqz(word, "const")) {
                has_const = true;
                if (nw == 0) first_const = true;
            } else {
                if (core.n) avp_buf_putc(&core, ' ');
                avp_buf_put(&core, word);
            }
            nw++;
            i = j;
        }
        const char *cz = avp_buf_cz(&core);
        LvS pre = first_const ? LVS("const ") : LVS("");
        if (has_const && !first_const) continue;
        i64 a = T(x, s->decl->spec_a)->s, b = T(x, s->decl->spec_b)->e;
        bool idx = avp_iset_has(used, P(dt->name));
        const char *nf = lookup(SIGN_FLIP, cz);
        if (nf && idx) {
            char first[32];
            size_t fl = strcspn(nf, " ");
            memcpy(first, nf, fl);
            first[fl] = 0;
            if (!(lv_in(first, LV_SCALAR_TYPEDEFS) && !in_file_ids(x, nf)))
                LV_YIELD(x, avp_sprintf(x->a, "local %s to %s", cz, nf), a, {a, b, LV_CAT(x->a, pre, lv_sz(nf))});
        }
        const char *ns = lookup(SPELL, cz);
        if (ns && (lv_in(cz, LV_FLOAT_WORDS) || idx)) {
            if (lv_in(ns, LV_SCALAR_TYPEDEFS) && !in_file_ids(x, ns)) continue;
            LV_YIELD(x, avp_sprintf(x->a, "local %s spelled %s", cz, ns), a, {a, b, LV_CAT(x->a, pre, lv_sz(ns))});
        }
    }
    avp_iset_free(used);
}

/* ---- order ---- */

static LvNode *simple_assign(LvCtx *x, const LvStmt *st) {
    if (st->kind != LS_EXPR) return NULL;
    LvNode *n = lv_expr_of(x, st);
    if (!n || n->kind != LN_ASSIGN || lv_has_side(n->kids[1]) || lv_has_call(x, st->a, st->b)) return NULL;
    LvNode *L = n->kids[0];
    if (lv_has_side(L) || lv_has_call(x, L->a, L->b)) return NULL;
    return n;
}

/* _mem_fields(n): the member names in n, plus "*" when n has an index or a dereference */
static LvIds mem_fields(LvCtx *x, const LvNode *n) {
    AVP_VEC(const char *) v = {0};
    for (i32 i = n->a; i <= n->b; i++)
        if (i > 0 && member_op(T(x, i - 1))) {
            const char *t = T(x, i)->t;
            bool have = false;
            for (size_t q = 0; q < v.n; q++)
                if (v.v[q] == t) have = true;
            if (!have) avp_vec_push(&v, t);
        }
    LvNodes w = {0};
    lv_walk(n, &w);
    bool star = false;
    for (size_t i = 0; i < w.n; i++)
        if (w.v[i]->kind == LN_INDEX || (w.v[i]->kind == LN_UNARY && !strcmp(w.v[i]->op, "*"))) star = true;
    avp_vec_free(&w);
    if (star) {
        const char *st = lv_internz("*");
        bool have = false;
        for (size_t q = 0; q < v.n; q++)
            if (v.v[q] == st) have = true;
        if (!have) avp_vec_push(&v, st);
    }
    LvIds r = {v.n ? avp_memdup(x->a, v.v, sizeof(char *) * v.n) : NULL, (i32)v.n};
    avp_vec_free(&v);
    return r;
}

void lv_order_family(LvCtx *x) {
    const char *star = lv_internz("*");
    for (size_t bi = 0; bi < x->blocks.n; bi++) {
        LvStmts L = x->blocks.v[bi].L;
        for (i32 k = 0; k + 1 < L.n; k++) {
            LvStmt *X = L.v[k], *Y = L.v[k + 1];
            LvNode *nx = simple_assign(x, X), *ny = simple_assign(x, Y);
            if (!nx || !ny) continue;
            i64 x0, x1, y0, y1;
            bool lx = lv_lines(x, X->a, X->b, &x0, &x1), ly = lv_lines(x, Y->a, Y->b, &y0, &y1);
            if (!lx || !ly) continue;
            LvNode *tx = nx->kids[0], *ty = ny->kids[0];
            const char *kx = lv_norm(x, tx->a, tx->b), *ky = lv_norm(x, ty->a, ty->b);
            if (!strcmp(kx, ky)) continue;
            if (strstr(lv_norm(x, Y->a, Y->b), kx) || strstr(lv_norm(x, X->a, X->b), ky)) continue;
            if (tx->kind == LN_PRIM && lv_ids_has(lv_ids(x, Y->a, Y->b), T(x, tx->a)->t)) continue;
            if (ty->kind == LN_PRIM && lv_ids_has(lv_ids(x, X->a, X->b), T(x, ty->a)->t)) continue;
            LvIds fx = mem_fields(x, tx), fy = mem_fields(x, ty);
            LvIds rx = mem_fields(x, nx->kids[1]), ry = mem_fields(x, ny->kids[1]);
            if (fx.n && (lv_ids_meet(fx, fy) || lv_ids_meet(fx, ry) || (lv_ids_has(fx, star) && (fy.n || ry.n))))
                continue;
            if (fy.n && (lv_ids_meet(fy, rx) || (lv_ids_has(fy, star) && rx.n))) continue;
            LvS t1 = S(x, x0, x1), t2 = S(x, y0, y1), mid = S(x, x1, y0);
            LV_YIELD(x, "swap independent statements", x0, {x0, y1, LV_CAT(x->a, t2, mid, t1)});
        }
    }
}
