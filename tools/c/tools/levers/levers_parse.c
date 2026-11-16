/* levers: function definitions, declarations, expressions and statements (levers.py scan_definitions,
 * find_definition, parse_decl, ExprParser, BodyParser, iter_stmts, iter_blocks, collect_typenames). */
#include "levers.h"
#include "shsm_mangle.h"

#include <stdio.h>
#include <string.h>

const char *const LV_BUILTIN[] = {"void", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
                                  "bool", "wchar_t", "_Bool", NULL};
const char *const LV_QUALS[] = {"const", "volatile", "static", "extern", "register", "inline", "struct", "union",
                                "enum", "class", "typename", "mutable", NULL};
const char *const LV_SCALAR_TYPEDEFS[] = {"s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64", "f32", "f64",
                                          "BOOL", "size_t", "vu8", "vu16", "vu32", "vs8", "vs16", "vs32", "uint",
                                          "u_int", "ptrdiff_t", NULL};
const char *const LV_INT_WORDS[] = {"char", "short", "int", "long", "signed", "unsigned", "s8", "u8", "s16", "u16",
                                    "s32", "u32", "s64", "u64", "BOOL", "size_t", "const", NULL};
const char *const LV_FLOAT_WORDS[] = {"float", "double", "f32", "f64", NULL};
const char *const LV_NARROW[] = {"char", "short", "s8", "u8", "s16", "u16", "bool", "_Bool", NULL};
const char *const LV_NOT_TYPE[] = {"return", "sizeof", "new", "delete", "throw", "this", "if", "else", "for",
                                   "while", "do", "switch", "case", "default", "break", "continue", "goto",
                                   "operator", "true", "false", "NULL", "nullptr", NULL};
const char *const LV_NOT_NAME[] = {"if", "while", "for", "switch", "return", "sizeof", "__declspec",
                                   "__attribute__", "defined", "catch", "do", "else", NULL};
const char *const LV_ASSIGN[] = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", NULL};
const char *const LV_UNARY[] = {"!", "~", "-", "+", "*", "&", "++", "--", NULL};

int lv_bin_prec(const char *op) {
    static const struct {
        const char *op;
        int p;
    } T[] = {{"||", 4}, {"&&", 5}, {"|", 6}, {"^", 7}, {"&", 8}, {"==", 9}, {"!=", 9}, {"<", 10}, {">", 10},
             {"<=", 10}, {">=", 10}, {"<<", 11}, {">>", 11}, {"+", 12}, {"-", 12}, {"*", 13}, {"/", 13}, {"%", 13}};
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (strcmp(op, T[i].op) == 0) return T[i].p;
    return 0;
}

static bool is_open(const LvTok *t) {
    return t->n == 1 && (t->t[0] == '(' || t->t[0] == '[' || t->t[0] == '{');
}
static bool is_close(const LvTok *t) {
    return t->n == 1 && (t->t[0] == ')' || t->t[0] == ']' || t->t[0] == '}');
}

/* ---- function definitions ---- */

static i32 skip_angle_back(LvToks c, i32 q, i32 lo) {
    int depth = 0;
    while (q >= lo) {
        const LvTok *t = lv_at(c, q);
        if (TEQ(t, ">")) depth++;
        else if (TEQ(t, "<")) {
            depth--;
            if (depth == 0) return q;
        } else if (TEQ(t, ">>"))
            depth += 2;
        q--;
    }
    return -1;
}

LvSpan *lv_split_top(AvpArena *a, LvToks c, i32 lo, i32 hi, const i32 *m, const char *sep, i32 *count) {
    AVP_VEC(LvSpan) out = {0};
    i32 k = lo, s = lo;
    while (k < hi) {
        const LvTok *t = lv_at(c, k);
        if (is_open(t) && t->k == LV_OP) {
            k = lv_m(m, c, k) + 1;
            continue;
        }
        if (strcmp(t->t, sep) == 0) {
            avp_vec_push(&out, ((LvSpan){s, k}));
            s = k + 1;
        }
        k++;
    }
    avp_vec_push(&out, ((LvSpan){s, hi}));
    LvSpan *r = avp_memdup(a, out.v, sizeof(LvSpan) * out.n);
    *count = (i32)out.n;
    avp_vec_free(&out);
    return r;
}

static bool fn_def(AvpArena *a, LvToks c, const i32 *m, i32 s, i32 e, const char **cls_stack, i32 ncls,
                   LvFnDef *out) {
    i32 k = s, paren = -1;
    while (k < e) {
        const LvTok *t = lv_at(c, k);
        if (TEQ(t, "(")) {
            i32 p = k - 1;
            if (p >= s && TEQ(lv_at(c, p), "operator")) {
                i32 mk = lv_m(m, c, k);
                paren = (mk + 1 < c.n && TEQ(lv_at(c, mk + 1), "(")) ? mk + 1 : k;
                break;
            }
            if (p >= s && lv_at(c, p)->k == LV_ID && !lv_in(lv_at(c, p)->t, LV_NOT_NAME)) {
                i32 nx = lv_m(m, c, k) + 1;
                if (nx >= e || TEQ(lv_at(c, nx), "const") || TEQ(lv_at(c, nx), "throw") || TEQ(lv_at(c, nx), ":")) {
                    paren = k;
                    break;
                }
            }
            k = lv_m(m, c, k) + 1;
            continue;
        }
        if (TEQ(t, "[") || TEQ(t, "{")) {
            k = lv_m(m, c, k) + 1;
            continue;
        }
        if (TEQ(t, "=")) {
            bool anyop = false;
            for (i32 x = s; x < k; x++)
                if (TEQ(lv_at(c, x), "operator")) anyop = true;
            if (!anyop) return false;
        }
        k++;
    }
    if (paren < 0 || !TEQ(lv_at(c, paren), "(")) return false;
    i32 op = -1;
    for (i32 x = s; x < paren; x++)
        if (TEQ(lv_at(c, x), "operator")) {
            op = x;
            break;
        }
    const char *name;
    i32 q;
    if (op >= 0) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "operator");
        for (i32 x = op + 1; x < paren; x++) avp_buf_putz(&b, lv_at(c, x)->t);
        name = lv_internz(avp_buf_cz(&b));
        q = op - 1;
    } else {
        name = lv_at(c, paren - 1)->t;
        q = paren - 2;
        if (q >= s && TEQ(lv_at(c, q), "~")) {
            name = lv_internz(avp_sprintf(a, "~%s", name));
            q--;
        }
    }
    AVP_VEC(const char *) quals = {0};
    while (q - 1 >= s && TEQ(lv_at(c, q), "::")) {
        i32 r = q - 1;
        if (TEQ(lv_at(c, r), ">")) {
            i32 r2 = skip_angle_back(c, r, s);
            if (r2 < 0) break;
            r = r2 - 1;
        }
        if (r < s || lv_at(c, r)->k != LV_ID) break;
        avp_vec_insert(&quals, 0, lv_at(c, r)->t);
        q = r - 1;
    }
    i32 rp = lv_m(m, c, paren);
    i32 np = 0;
    LvSpan *params = NULL;
    if (rp != paren + 1) params = lv_split_top(a, c, paren + 1, rp, m, ",", &np);
    if (np == 1 && rp == paren + 2 && TEQ(lv_at(c, paren + 1), "void")) np = 0;
    bool ret_void = false, ret_star = false;
    for (i32 x = s; x < q + 1; x++) {
        if (TEQ(lv_at(c, x), "void")) ret_void = true;
        if (TEQ(lv_at(c, x), "*")) ret_star = true;
    }
    const char *cls = quals.n ? quals.v[quals.n - 1] : (ncls ? cls_stack[ncls - 1] : NULL);
    memset(out, 0, sizeof *out);
    out->name = name;
    out->quals = quals.n ? avp_memdup(a, quals.v, sizeof(char *) * quals.n) : NULL;
    out->nquals = (i32)quals.n;
    avp_vec_free(&quals);
    out->cls = cls;
    out->nparams = np;
    out->head = s;
    out->lbrace = e;
    out->rbrace = lv_m(m, c, e);
    out->is_void = (ret_void && !ret_star) || name[0] == '~' || (cls && strcmp(name, cls) == 0);
    out->params = params;
    return true;
}

static bool hk_has(LvToks c, i32 a, i32 b, const char *w) {
    for (i32 x = a; x < b; x++)
        if (TEQ(lv_at(c, x), w)) return true;
    return false;
}

static void walk_defs(AvpArena *a, LvToks c, const i32 *m, i32 i, i32 end, const char **cls_stack, i32 ncls,
                      LvFnDefs *out) {
    i32 start = i;
    while (i < end) {
        const LvTok *t = lv_at(c, i);
        if (TEQ(t, ";")) {
            i++;
            start = i;
            continue;
        }
        if ((TEQ(t, "public") || TEQ(t, "private") || TEQ(t, "protected")) && i + 1 < end &&
            TEQ(lv_at(c, i + 1), ":")) {
            i += 2;
            start = i;
            continue;
        }
        if (TEQ(t, "(") || TEQ(t, "[")) {
            i = lv_m(m, c, i) + 1;
            continue;
        }
        if (TEQ(t, "{")) {
            i32 j = lv_m(m, c, i);
            bool has_paren = hk_has(c, start, i, "(");
            if ((start < i && TEQ(lv_at(c, start), "namespace")) ||
                (start < i && TEQ(lv_at(c, start), "extern") && i - start == 2 && lv_at(c, start + 1)->k == LV_STR)) {
                walk_defs(a, c, m, i + 1, j, cls_stack, ncls, out);
            } else if (hk_has(c, start, i, "enum") && !has_paren) {
                /* nothing */
            } else if ((hk_has(c, start, i, "class") || hk_has(c, start, i, "struct") || hk_has(c, start, i, "union")) &&
                       !has_paren) {
                i32 kw = -1;
                for (i32 x = start; x < i; x++) {
                    const LvTok *u = lv_at(c, x);
                    if (TEQ(u, "class") || TEQ(u, "struct") || TEQ(u, "union")) kw = x;
                }
                const char *name = (kw + 1 < i && lv_at(c, kw + 1)->k == LV_ID) ? lv_at(c, kw + 1)->t : "?";
                const char **st = avp_alloc(a, sizeof(char *) * (size_t)(ncls + 1));
                if (ncls) memcpy(st, cls_stack, sizeof(char *) * (size_t)ncls);
                st[ncls] = name;
                walk_defs(a, c, m, i + 1, j, st, ncls + 1, out);
            } else {
                LvFnDef d;
                if (fn_def(a, c, m, start, i, cls_stack, ncls, &d)) avp_vec_push(out, d);
            }
            i = j + 1;
            start = i;
            continue;
        }
        i++;
    }
}

LvFnDefs lv_scan_definitions(AvpArena *a, LvToks c, const i32 *m) {
    LvFnDefs out = {0};
    walk_defs(a, c, m, 0, c.n, NULL, 0, &out);
    return out;
}

LvTarget lv_symbol_target(AvpArena *a, const char *symbol) {
    LvTarget r = {symbol, NULL, -1};
    ShsmDemangled *p = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        p = shsm_mangle_parse(a, symbol);
        avp_try_end(&t);
    } else {
        p = NULL;
    }
    if (!p || p->is_data) return r;
    i32 np = 0;
    for (size_t i = 0; i < p->nparams; i++)
        if (strcmp(p->params[i], "void") != 0) np++;
    r.name = p->name;
    r.cls = p->nscope ? p->scope[p->nscope - 1] : NULL;
    r.nparams = np;
    return r;
}

/* _base(cls): cls.split("<")[0] if cls else None, compared */
static bool base_eq(const char *x, const char *y) {
    bool nx = !x || !*x, ny = !y || !*y;
    if (nx || ny) return nx && ny;
    size_t lx = strcspn(x, "<"), ly = strcspn(y, "<");
    return lx == ly && memcmp(x, y, lx) == 0;
}

LvFnDef lv_find_definition(AvpArena *a, LvToks c, const i32 *m, const char *symbol) {
    LvTarget tg = lv_symbol_target(a, symbol);
    LvFnDefs all = lv_scan_definitions(a, c, m);
    LvFnDef best = {0};
    int best_r0 = 99, best_r1 = 99;
    size_t found = 0;
    for (size_t i = 0; i < all.n; i++) {
        LvFnDef *d = &all.v[i];
        if (strcmp(d->name, tg.name) != 0) continue;
        found++;
        int r0 = base_eq(d->cls, tg.cls) ? 0 : (!d->cls || !tg.cls) ? 1 : 2;
        int r1 = (tg.nparams < 0 || d->nparams == tg.nparams) ? 0 : 1;
        if (r0 < best_r0 || (r0 == best_r0 && r1 < best_r1)) {
            best = *d;
            best_r0 = r0;
            best_r1 = r1;
        }
    }
    avp_vec_free(&all);
    if (!found) lv_parse_error("no definition of %s (%s) in the source", symbol, tg.name);
    if (best_r0 == 2) lv_parse_error("no definition of %s in class %s", tg.name, tg.cls ? tg.cls : "None");
    return best;
}

/* ---- declarations ---- */

LvDecl *lv_parse_decl(AvpArena *a, LvToks c, i32 lo, i32 semi, const i32 *m) {
    i32 j = lo;
    AVP_VEC(i32) words = {0};
    LvDecl *res = NULL;
    while (j < semi) {
        const LvTok *t = lv_at(c, j);
        if (t->k == LV_ID && !lv_in(t->t, LV_NOT_TYPE)) {
            avp_vec_push(&words, j);
            j++;
            while (j + 1 < semi && TEQ(lv_at(c, j), "::") && lv_at(c, j + 1)->k == LV_ID) j += 2;
            if (j < semi && TEQ(lv_at(c, j), "<") && words.n == 1 && !lv_in(lv_at(c, words.v[0])->t, LV_BUILTIN) &&
                !lv_in(lv_at(c, words.v[0])->t, LV_QUALS)) {
                int depth = 0;
                i32 k = j;
                while (k < semi) {
                    const LvTok *u = lv_at(c, k);
                    if (TEQ(u, "<")) depth++;
                    else if (TEQ(u, ">")) {
                        depth--;
                        if (depth == 0) break;
                    } else if (TEQ(u, ";") || TEQ(u, "{") || TEQ(u, "="))
                        goto done;
                    k++;
                }
                if (k >= semi) goto done;
                j = k + 1;
            }
            continue;
        }
        break;
    }
    if (!words.n || j >= semi + 1) goto done;
    const char *nxt = j < semi ? lv_at(c, j)->t : ";";
    i32 spec_end;
    if (!strcmp(nxt, "*") || !strcmp(nxt, "&")) spec_end = j;
    else if ((!strcmp(nxt, "=") || !strcmp(nxt, ",") || !strcmp(nxt, ";") || !strcmp(nxt, "[") ||
              !strcmp(nxt, "(")) &&
             words.n >= 2)
        spec_end = words.v[words.n - 1];
    else
        goto done;
    AVP_VEC(const char *) spec = {0};
    for (size_t w = 0; w < words.n; w++)
        if (words.v[w] < spec_end) avp_vec_push(&spec, lv_at(c, words.v[w])->t);
    bool allq = true, any_builtin = false, any_static = false;
    for (size_t w = 0; w < spec.n; w++) {
        if (!lv_in(spec.v[w], LV_QUALS)) allq = false;
        if (lv_in(spec.v[w], LV_BUILTIN)) any_builtin = true;
        if (!strcmp(spec.v[w], "static") || !strcmp(spec.v[w], "extern")) any_static = true;
    }
    if (!spec.n || allq) {
        avp_vec_free(&spec);
        goto done;
    }
    if (!strcmp(nxt, "(") && !(any_builtin || lv_at(c, words.v[words.n - 2])->k == LV_ID)) {
        avp_vec_free(&spec);
        goto done;
    }
    i32 nparts;
    LvSpan *parts = lv_split_top(a, c, spec_end, semi, m, ",", &nparts);
    LvDtor *dtors = avp_alloc(a, sizeof(LvDtor) * (size_t)nparts);
    for (i32 pi = 0; pi < nparts; pi++) {
        i32 da = parts[pi].a, db = parts[pi].b;
        if (da >= db) {
            avp_vec_free(&spec);
            goto done;
        }
        i32 k = da;
        bool ptr = false, cx = false, array = false;
        while (k < db && (TEQ(lv_at(c, k), "*") || TEQ(lv_at(c, k), "&") || TEQ(lv_at(c, k), "const"))) {
            ptr = ptr || TEQ(lv_at(c, k), "*") || TEQ(lv_at(c, k), "&");
            k++;
        }
        if (k >= db || lv_at(c, k)->k != LV_ID) {
            avp_vec_free(&spec);
            goto done;
        }
        i32 name_i = k++;
        while (k < db && (TEQ(lv_at(c, k), "[") || TEQ(lv_at(c, k), "("))) {
            array = array || TEQ(lv_at(c, k), "[");
            cx = cx || TEQ(lv_at(c, k), "(");
            k = lv_m(m, c, k) + 1;
        }
        LvDtor *d = &dtors[pi];
        d->init.ok = false;
        d->eq = -1;
        if (k < db) {
            if (!TEQ(lv_at(c, k), "=")) {
                avp_vec_free(&spec);
                goto done;
            }
            d->eq = k;
            if (k + 1 >= db) {
                avp_vec_free(&spec);
                goto done;
            }
            d->init = (LvRange){k + 1, db - 1, true};
            if (TEQ(lv_at(c, k + 1), "{")) cx = true;
        }
        d->a = da;
        d->b = db - 1;
        d->name_i = name_i;
        d->name = lv_at(c, name_i)->t;
        d->ptr = ptr;
        d->array = array;
        d->complex = cx;
    }
    res = avp_alloc(a, sizeof *res);
    res->spec_a = lo;
    res->spec_b = spec_end - 1;
    res->words = avp_memdup(a, spec.v, sizeof(char *) * spec.n);
    res->nwords = (i32)spec.n;
    res->dtors = dtors;
    res->ndtors = nparts;
    res->is_static = any_static;
    avp_vec_free(&spec);
done:
    avp_vec_free(&words);
    return res;
}

/* ---- expressions ---- */

int lv_prec(const LvNode *n) {
    switch (n->kind) {
    case LN_BIN: return lv_bin_prec(n->op);
    case LN_COMMA: return 1;
    case LN_ASSIGN: return 2;
    case LN_TERN: return 3;
    case LN_UNARY:
    case LN_CAST:
    case LN_SIZEOF: return 14;
    default: return 16;
    }
}

void lv_walk(const LvNode *n, LvNodes *out) {
    avp_vec_push(out, (LvNode *)n);
    for (i32 k = 0; k < n->nk; k++) lv_walk(n->kids[k], out);
}

static LvNode *node(AvpArena *a, u8 kind, i32 lo, i32 hi, const char *op, LvNode **kids, i32 nk, i32 opi) {
    LvNode *n = avp_alloc(a, sizeof *n);
    n->kind = kind;
    n->a = lo;
    n->b = hi;
    n->op = op;
    n->nk = nk;
    n->kids = nk ? avp_memdup(a, kids, sizeof(LvNode *) * (size_t)nk) : NULL;
    n->opi = opi;
    n->ex_a = n->ex_b = -1;
    return n;
}

void lv_expr_init(LvExprParser *p, AvpArena *a, LvToks c, const i32 *m, i32 lo, i32 hi, AvpISet *typenames) {
    p->a = a;
    p->code = c;
    p->m = m;
    p->i = lo;
    p->end = hi + 1;
    p->typenames = typenames;
    if (hi < lo) lv_parse_error("empty expression");
}

static const char *pt(LvExprParser *p, i32 i) { return i < p->end ? lv_at(p->code, i)->t : NULL; }
static bool pteq(LvExprParser *p, i32 i, const char *lit) {
    const char *t = pt(p, i);
    return t && strcmp(t, lit) == 0;
}
static bool is_typename(LvExprParser *p, const char *t) { return avp_iset_has(p->typenames, (u64)(uintptr_t)t); }

static LvNode *expr(LvExprParser *p, int minp);
static LvNode *unary(LvExprParser *p);

static LvNode *sub_parse(LvExprParser *p, i32 lo, i32 hi) {
    LvExprParser q;
    lv_expr_init(&q, p->a, p->code, p->m, lo, hi, p->typenames);
    return lv_expr_parse(&q);
}

LvNode *lv_expr_parse(LvExprParser *p) {
    LvNode *n = expr(p, 0);
    if (p->i != p->end) lv_parse_error("trailing tokens in expression");
    return n;
}

static LvNode *expr(LvExprParser *p, int minp) {
    LvNode *left = unary(p);
    while (p->i < p->end) {
        i32 opi = p->i;
        const char *t = pt(p, opi);
        if (lv_at(p->code, opi)->k != LV_OP) lv_parse_error("unexpected %s", lv_repr(p->a, lv_sz(t)));
        if (!strcmp(t, ",")) {
            if (minp > 1) break;
            p->i++;
            LvNode *right = expr(p, 2);
            left = node(p->a, LN_COMMA, left->a, right->b, ",", (LvNode *[]){left, right}, 2, opi);
        } else if (lv_in(t, LV_ASSIGN)) {
            if (minp > 2) break;
            p->i++;
            LvNode *right = expr(p, 2);
            left = node(p->a, LN_ASSIGN, left->a, right->b, t, (LvNode *[]){left, right}, 2, opi);
        } else if (!strcmp(t, "?")) {
            if (minp > 3) break;
            p->i++;
            LvNode *mid = expr(p, 1);
            if (!pteq(p, p->i, ":")) lv_parse_error("expected ':'");
            i32 ci = p->i;
            p->i++;
            LvNode *right = expr(p, 3);
            left = node(p->a, LN_TERN, left->a, right->b, "?", (LvNode *[]){left, mid, right}, 3, opi);
            left->ex_a = ci;
        } else if (lv_bin_prec(t)) {
            int pr = lv_bin_prec(t);
            if (pr < minp) break;
            p->i++;
            LvNode *right = expr(p, pr + 1);
            left = node(p->a, LN_BIN, left->a, right->b, t, (LvNode *[]){left, right}, 2, opi);
        } else
            break;
    }
    return left;
}

static bool is_type(LvExprParser *p, i32 i, i32 j) {
    i32 lo = i + 1, hi = j < p->code.n ? j : p->code.n;
    if (lo < 0) lo = 0;
    if (lo >= hi) return false;
    i32 nwords = 0;
    bool known = false, one_typename = false;
    const char *first = NULL;
    for (i32 x = lo; x < hi; x++) {
        const LvTok *t = &p->code.v[x];
        if (!(t->k == LV_ID || TEQ(t, "*") || TEQ(t, "&") || TEQ(t, "::"))) return false;
    }
    for (i32 x = lo; x < hi; x++) {
        const LvTok *t = &p->code.v[x];
        if (t->k != LV_ID) continue;
        if (lv_in(t->t, LV_NOT_TYPE)) return false;
        if (!nwords) first = t->t;
        nwords++;
        if (lv_in(t->t, LV_BUILTIN) || lv_in(t->t, LV_QUALS) || lv_in(t->t, LV_SCALAR_TYPEDEFS)) known = true;
    }
    if (!nwords) return false;
    one_typename = nwords == 1 && is_typename(p, first);
    const LvTok *last = &p->code.v[hi - 1];
    return known || TEQ(last, "*") || TEQ(last, "&") || one_typename;
}

bool lv_expr_is_cast(LvExprParser *p, i32 i) {
    i32 j = lv_m(p->m, p->code, i);
    if (!is_type(p, i, j) || j + 1 >= p->end) return false;
    const LvTok *nx = lv_at(p->code, j + 1);
    if (nx->k == LV_ID || nx->k == LV_NUM || nx->k == LV_STR || nx->k == LV_CHR || TEQ(nx, "(") || TEQ(nx, "!") ||
        TEQ(nx, "~") || TEQ(nx, "++") || TEQ(nx, "--"))
        return true;
    if (!(TEQ(nx, "-") || TEQ(nx, "+") || TEQ(nx, "*") || TEQ(nx, "&"))) return false;
    i32 hi = j < p->code.n ? j : p->code.n;
    for (i32 x = i + 1; x < hi; x++)
        if (lv_in(p->code.v[x].t, LV_BUILTIN) || lv_in(p->code.v[x].t, LV_SCALAR_TYPEDEFS)) return true;
    return false;
}

static LvNode *primary(LvExprParser *p);
static LvNode *postfix(LvExprParser *p, LvNode *n);

static LvNode *unary(LvExprParser *p) {
    i32 i = p->i;
    if (i >= p->end) lv_parse_error("missing operand");
    const LvTok *t = lv_at(p->code, i);
    if (t->k == LV_OP && lv_in(t->t, LV_UNARY)) {
        p->i++;
        LvNode *o = unary(p);
        return node(p->a, LN_UNARY, i, o->b, t->t, (LvNode *[]){o}, 1, i);
    }
    if (TEQ(t, "sizeof")) {
        p->i++;
        if (pteq(p, p->i, "(") && is_type(p, p->i, lv_m(p->m, p->code, p->i))) {
            i32 j = lv_m(p->m, p->code, p->i);
            p->i = j + 1;
            return node(p->a, LN_SIZEOF, i, j, NULL, NULL, 0, -1);
        }
        LvNode *o = unary(p);
        return node(p->a, LN_SIZEOF, i, o->b, NULL, (LvNode *[]){o}, 1, -1);
    }
    if (TEQ(t, "(") && lv_expr_is_cast(p, i)) {
        i32 j = lv_m(p->m, p->code, i);
        p->i = j + 1;
        LvNode *o = unary(p);
        LvNode *n = node(p->a, LN_CAST, i, o->b, NULL, (LvNode *[]){o}, 1, -1);
        n->ex_a = i + 1;
        n->ex_b = j - 1;
        return n;
    }
    return postfix(p, primary(p));
}

static LvNode *primary(LvExprParser *p) {
    i32 i = p->i;
    const LvTok *t = lv_at(p->code, i);
    if (TEQ(t, "(")) {
        i32 j = lv_m(p->m, p->code, i);
        if (j >= p->end) lv_parse_error("paren out of range");
        LvNode *inner = sub_parse(p, i + 1, j - 1);
        p->i = j + 1;
        return node(p->a, LN_PAREN, i, j, NULL, (LvNode *[]){inner}, 1, -1);
    }
    if (t->k == LV_NUM || t->k == LV_CHR) {
        p->i++;
        return node(p->a, LN_PRIM, i, i, NULL, NULL, 0, -1);
    }
    if (t->k == LV_STR) {
        while (p->i < p->end && lv_at(p->code, p->i)->k == LV_STR) p->i++;
        return node(p->a, LN_PRIM, i, p->i - 1, NULL, NULL, 0, -1);
    }
    if (t->k == LV_ID || TEQ(t, "::")) {
        static const char *const unsup[] = {"new", "delete", "throw", "operator", "static_cast", "reinterpret_cast",
                                            "const_cast", "dynamic_cast", "template", "typename", NULL};
        static const char *const keep[] = {"this", "true", "false", "NULL", "nullptr", NULL};
        if (lv_in(t->t, unsup) || (t->k == LV_ID && lv_in(t->t, LV_NOT_TYPE) && !lv_in(t->t, keep)))
            lv_parse_error("unsupported %s", lv_repr(p->a, lv_tok_s(t)));
        if (TEQ(t, "::")) p->i++;
        if (p->i >= p->end || lv_at(p->code, p->i)->k != LV_ID) lv_parse_error("expected identifier");
        p->i++;
        while (p->i + 1 < p->end && pteq(p, p->i, "::") && lv_at(p->code, p->i + 1)->k == LV_ID) p->i += 2;
        if (pteq(p, p->i, "<") && !is_typename(p, lv_at(p->code, i)->t)) {
            i32 k = p->i;
            int depth = 0;
            while (k < p->end) {
                const LvTok *u = lv_at(p->code, k);
                if (TEQ(u, "<")) depth++;
                else if (TEQ(u, ">") || TEQ(u, ">>")) {
                    depth -= TEQ(u, ">") ? 1 : 2;
                    if (depth <= 0) {
                        if (k + 1 < p->end && (TEQ(lv_at(p->code, k + 1), "(") || TEQ(lv_at(p->code, k + 1), "::")))
                            lv_parse_error("template-id");
                        break;
                    }
                }
                k++;
            }
        }
        return node(p->a, LN_PRIM, i, p->i - 1, NULL, NULL, 0, -1);
    }
    lv_parse_error("unexpected %s", lv_repr(p->a, lv_tok_s(t)));
}

static LvNode *postfix(LvExprParser *p, LvNode *n) {
    while (p->i < p->end) {
        const char *t = pt(p, p->i);
        if (!strcmp(t, "(")) {
            i32 j = lv_m(p->m, p->code, p->i);
            AVP_VEC(LvNode *) kids = {0};
            avp_vec_push(&kids, n);
            if (j > p->i + 1) {
                i32 np;
                LvSpan *sp = lv_split_top(p->a, p->code, p->i + 1, j, p->m, ",", &np);
                for (i32 k = 0; k < np; k++) avp_vec_push(&kids, sub_parse(p, sp[k].a, sp[k].b - 1));
            }
            p->i = j + 1;
            LvNode *c = node(p->a, LN_CALL, n->a, j, NULL, kids.v, (i32)kids.n, -1);
            avp_vec_free(&kids);
            n = c;
        } else if (!strcmp(t, "[")) {
            i32 j = lv_m(p->m, p->code, p->i);
            LvNode *idx = sub_parse(p, p->i + 1, j - 1);
            p->i = j + 1;
            n = node(p->a, LN_INDEX, n->a, j, NULL, (LvNode *[]){n, idx}, 2, -1);
        } else if (!strcmp(t, ".") || !strcmp(t, "->")) {
            if (p->i + 1 >= p->end || lv_at(p->code, p->i + 1)->k != LV_ID) lv_parse_error("expected member name");
            n = node(p->a, LN_MEMBER, n->a, p->i + 1, t, (LvNode *[]){n}, 1, p->i);
            p->i += 2;
        } else if (!strcmp(t, "++") || !strcmp(t, "--")) {
            n = node(p->a, LN_POST, n->a, p->i, t, (LvNode *[]){n}, 1, p->i);
            p->i++;
        } else
            break;
    }
    return n;
}

/* ---- statements ---- */

typedef struct {
    AvpArena *a;
    LvToks c;
    const i32 *m;
} BP;

static LvStmt *stmt(BP *bp, i32 i, i32 end);

static i32 expect(BP *bp, i32 i, const char *t) {
    if (i >= bp->c.n || strcmp(lv_at(bp->c, i)->t, t) != 0) lv_parse_error("expected %s", lv_repr(bp->a, lv_sz(t)));
    return i;
}

static i32 find_semi(BP *bp, i32 i, i32 end) {
    i32 k = i;
    while (k < end) {
        const LvTok *t = lv_at(bp->c, k);
        if (TEQ(t, ";")) return k;
        if (t->k == LV_OP && is_open(t)) {
            k = lv_m(bp->m, bp->c, k) + 1;
            continue;
        }
        k++;
    }
    lv_parse_error("missing ';'");
}

static LvStmts stmts(BP *bp, i32 i, i32 end) {
    AVP_VEC(LvStmt *) out = {0};
    while (i < end) {
        LvStmt *s = stmt(bp, i, end);
        avp_vec_push(&out, s);
        i = s->b + 1;
    }
    LvStmts r = {out.n ? avp_memdup(bp->a, out.v, sizeof(LvStmt *) * out.n) : NULL, (i32)out.n};
    avp_vec_free(&out);
    return r;
}

static LvStmt *mk(BP *bp, u8 kind, i32 a, i32 b) {
    LvStmt *s = avp_alloc(bp->a, sizeof *s);
    s->kind = kind;
    s->a = a;
    s->b = b;
    s->lp = s->rp = s->wh = -1;
    return s;
}

static LvStmt *stmt(BP *bp, i32 i, i32 end) {
    LvToks c = bp->c;
    const char *t = lv_at(c, i)->t;
    if (!strcmp(t, "{")) {
        i32 j = lv_m(bp->m, c, i);
        LvStmts body = stmts(bp, i + 1, j);
        LvStmt *s = mk(bp, LS_BLOCK, i, j);
        s->list = body;
        return s;
    }
    if (!strcmp(t, "if")) {
        i32 lp = expect(bp, i + 1, "(");
        i32 rp = lv_m(bp->m, c, lp);
        LvStmt *then = stmt(bp, rp + 1, end);
        i32 k = then->b + 1;
        LvStmt *els = NULL;
        if (k < end && TEQ(lv_at(c, k), "else")) els = stmt(bp, k + 1, end);
        LvStmt *s = mk(bp, LS_IF, i, (els ? els : then)->b);
        s->lp = lp;
        s->rp = rp;
        s->then = then;
        s->els = els;
        return s;
    }
    if (!strcmp(t, "while") || !strcmp(t, "switch")) {
        i32 lp = expect(bp, i + 1, "(");
        i32 rp = lv_m(bp->m, c, lp);
        LvStmt *body = stmt(bp, rp + 1, end);
        LvStmt *s = mk(bp, !strcmp(t, "while") ? LS_WHILE : LS_SWITCH, i, body->b);
        s->lp = lp;
        s->rp = rp;
        s->body = body;
        return s;
    }
    if (!strcmp(t, "for")) {
        i32 lp = expect(bp, i + 1, "(");
        i32 rp = lv_m(bp->m, c, lp);
        i32 np;
        LvSpan *parts = lv_split_top(bp->a, c, lp + 1, rp, bp->m, ";", &np);
        if (np != 3) lv_parse_error("for header");
        LvRange rng[3];
        for (int q = 0; q < 3; q++)
            rng[q] = parts[q].b > parts[q].a ? (LvRange){parts[q].a, parts[q].b - 1, true} : (LvRange){0, 0, false};
        LvStmt *body = stmt(bp, rp + 1, end);
        LvStmt *s = mk(bp, LS_FOR, i, body->b);
        s->lp = lp;
        s->rp = rp;
        s->init = rng[0];
        s->cond = rng[1];
        s->step = rng[2];
        s->body = body;
        s->semi0 = parts[0].b;
        s->semi1 = parts[1].b;
        return s;
    }
    if (!strcmp(t, "do")) {
        LvStmt *body = stmt(bp, i + 1, end);
        i32 w = expect(bp, body->b + 1, "while");
        i32 lp = expect(bp, w + 1, "(");
        i32 rp = lv_m(bp->m, c, lp);
        i32 semi = expect(bp, rp + 1, ";");
        LvStmt *s = mk(bp, LS_DO, i, semi);
        s->body = body;
        s->wh = w;
        s->lp = lp;
        s->rp = rp;
        return s;
    }
    if (!strcmp(t, "case") || !strcmp(t, "default")) {
        i32 k = i;
        while (k < end && !TEQ(lv_at(c, k), ":")) k++;
        if (k >= end) lv_parse_error("label");
        return mk(bp, LS_LABEL, i, k);
    }
    if (lv_at(c, i)->k == LV_ID && i + 1 < end && TEQ(lv_at(c, i + 1), ":") && !lv_in(t, LV_NOT_TYPE))
        return mk(bp, LS_LABEL, i, i + 1);
    if (!strcmp(t, ";")) return mk(bp, LS_EMPTY, i, i);
    i32 semi = find_semi(bp, i, end);
    if (!strcmp(t, "return")) {
        LvStmt *s = mk(bp, LS_RETURN, i, semi);
        if (semi > i + 1) s->expr = (LvRange){i + 1, semi - 1, true};
        return s;
    }
    if (!strcmp(t, "break") || !strcmp(t, "continue") || !strcmp(t, "goto")) return mk(bp, LS_JUMP, i, semi);
    LvDecl *d = lv_parse_decl(bp->a, c, i, semi, bp->m);
    if (d) {
        LvStmt *s = mk(bp, LS_DECL, i, semi);
        s->decl = d;
        return s;
    }
    LvStmt *s = mk(bp, LS_EXPR, i, semi);
    if (semi > i) s->expr = (LvRange){i, semi - 1, true};
    return s;
}

LvStmts lv_body_parse(AvpArena *a, LvToks c, const i32 *m, i32 lo, i32 end) {
    BP bp = {a, c, m};
    return stmts(&bp, lo, end);
}

static void children(const LvStmt *s, AvpPtrVec *out) {
    if (s->then) avp_vec_push(out, s->then);
    if (s->els) avp_vec_push(out, s->els);
    if (s->body) avp_vec_push(out, s->body);
    if (s->kind == LS_BLOCK)
        for (i32 i = 0; i < s->list.n; i++) avp_vec_push(out, s->list.v[i]);
}

static void iter_one(const LvStmt *s, AvpPtrVec *out) {
    avp_vec_push(out, (void *)s);
    AvpPtrVec ch = {0};
    children(s, &ch);
    for (size_t i = 0; i < ch.n; i++) iter_one(ch.v[i], out);
    avp_vec_free(&ch);
}

void lv_iter_stmts(AvpArena *a, LvStmts L, AvpPtrVec *out) {
    (void)a;
    for (i32 i = 0; i < L.n; i++) iter_one(L.v[i], out);
}

static void blocks_in(LvStmt *s, LvBlocks *out) {
    if (s->kind == LS_BLOCK) {
        lv_iter_blocks(s->list, s, out);
        return;
    }
    AvpPtrVec ch = {0};
    children(s, &ch);
    for (size_t i = 0; i < ch.n; i++) blocks_in(ch.v[i], out);
    avp_vec_free(&ch);
}

void lv_iter_blocks(LvStmts L, LvStmt *owner, LvBlocks *out) {
    avp_vec_push(out, ((LvBlock){owner, L}));
    for (i32 i = 0; i < L.n; i++) blocks_in(L.v[i], out);
}

AvpISet *lv_collect_typenames(LvToks c) {
    AvpISet *names = avp_iset_new();
    for (int i = 0; LV_SCALAR_TYPEDEFS[i]; i++) avp_iset_add(names, (u64)(uintptr_t)lv_internz(LV_SCALAR_TYPEDEFS[i]));
    for (i32 i = 0; i < c.n; i++) {
        const LvTok *t = &c.v[i];
        if ((TEQ(t, "struct") || TEQ(t, "class") || TEQ(t, "union") || TEQ(t, "enum")) && i + 1 < c.n &&
            c.v[i + 1].k == LV_ID)
            avp_iset_add(names, (u64)(uintptr_t)c.v[i + 1].t);
        if (TEQ(t, "typedef")) {
            i32 k = i + 1, depth = 0;
            while (k < c.n && !(TEQ(&c.v[k], ";") && depth == 0)) {
                if (is_open(&c.v[k])) depth++;
                else if (is_close(&c.v[k])) depth--;
                k++;
            }
            i32 j = k - 1;
            while ((j > i && (TEQ(&c.v[j], ")") || TEQ(&c.v[j], "]"))) || (j > i && c.v[j].k == LV_NUM)) j--;
            if (j > i && c.v[j].k == LV_ID) avp_iset_add(names, (u64)(uintptr_t)c.v[j].t);
        }
    }
    return names;
}

/* ---- definitions by address ---- */

/* `fn_<8 hex digits>` (with or without a mangled tail): the address in the name. */
bool lv_symbol_address(const char *symbol, u64 *addr) {
    if (strncmp(symbol, "fn_", 3) != 0) return false;
    u64 v = 0;
    for (int i = 0; i < 8; i++) {
        char ch = symbol[3 + i];
        int d = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
        if (d < 0) return false;
        v = v * 16 + (u64)d;
    }
    char tail = symbol[11];
    if (tail != 0 && !(tail == '_' && symbol[12] == '_')) return false;
    *addr = v;
    return true;
}

/* A `// ... 0x<ADDR>` line comment: the text, trailing blanks aside, ends with the address. */
static bool comment_ends_with(const LvTok *t, u64 addr) {
    if (t->k != LV_COMMENT || t->n < 4 || t->t[0] != '/' || t->t[1] != '/') return false;
    u32 n = t->n;
    while (n && (t->t[n - 1] == ' ' || t->t[n - 1] == '\t' || t->t[n - 1] == '\r')) n--;
    char want[16];
    int wl = snprintf(want, sizeof want, "0x%08llX", (unsigned long long)addr);
    if (n < (u32)wl) return false;
    for (int i = 0; i < wl; i++) {
        char ch = t->t[n - (u32)wl + (u32)i];
        if (ch >= 'a' && ch <= 'f') ch = (char)(ch - 32);
        if (ch == 'X') ch = 'x';
        if (ch != want[i]) return false;
    }
    return n == (u32)wl || t->t[n - (u32)wl - 1] == ' ' || t->t[n - (u32)wl - 1] == '\t';
}

/* The definition marked with the address (the project's annotation rule): the one whose header comment ends with
 * `// 0x<ADDR>` (the next definition after the comment), or whose closing brace carries it on the same line. */
bool lv_find_definition_at(AvpArena *a, LvS text, LvToks toks, LvToks c, const i32 *m, u64 addr, LvFnDef *out) {
    LvFnDefs all = lv_scan_definitions(a, c, m);
    bool found = false;
    for (i32 i = 0; i < toks.n && !found; i++) {
        const LvTok *cm = &toks.v[i];
        if (!comment_ends_with(cm, addr)) continue;
        for (size_t d = 0; d < all.n && !found; d++) { /* trailing: `} // 0x...` */
            i64 re = c.v[all.v[d].rbrace].e;
            if (re <= cm->s) {
                bool same_line = true;
                for (i64 k = re; k < cm->s && k < (i64)text.n; k++)
                    if (text.p[k] == '\n') same_line = false;
                if (same_line) {
                    *out = all.v[d];
                    found = true;
                }
            }
        }
        i64 best = -1;
        for (size_t d = 0; d < all.n && !found; d++) { /* leading: the next definition below the comment */
            i64 hs = c.v[all.v[d].head].s;
            if (hs >= cm->e && (best < 0 || hs < c.v[all.v[best].head].s)) best = (i64)d;
        }
        if (!found && best >= 0) {
            *out = all.v[best];
            found = true;
        }
    }
    avp_vec_free(&all);
    return found;
}
