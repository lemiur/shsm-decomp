/* rules_check_function: the per-function checks of check_rules.py: check_function (R14, R17; L2978), _loop_block,
 * _index_series, check_expressions and _stmt_ranges (R17, R35; L3063-3157), _macro_bodies and _ids_in_used_macros
 * (L3159-3195), check_locals (R2, R17, R25; L3197), check_control, check_condition, _after_condition_declarator,
 * _depth0_assign (R35; L3260-3345), check_literals, _floaty, _explained (R9, R11; L3348-3387) and
 * check_function_meta (R15, R25, R33; L3390-3425). */
#include "rules_check.h"
#include "shsm_text.h"

#define TEQ RL_TEQ
static bool in_chars(const RlTok *t, const char *set) { return t->t.n == 1 && strchr(set, t->t.p[0]) != NULL; }
static const RlTok *tk(const RlParsed *P, i64 i) { return &P->toks.v[i]; }

static bool loop_block(const RlParsed *P, i64 brace) {
    if (brace == 0 || !TEQ(tk(P, brace - 1), ")")) return false;
    i64 o = P->match[brace - 1];
    return o >= 0 && o > 0 && (TEQ(tk(P, o - 1), "for") || TEQ(tk(P, o - 1), "while"));
}

static bool index_series(const RlParsed *P, i64 lo, i64 hi, i64 k) {
    if (!(k >= 2 && k + 2 < hi && TEQ(tk(P, k - 2), "[") && TEQ(tk(P, k + 2), "]"))) return false;
    if (P->match[k - 2] != k + 2 || tk(P, k - 1)->k != RL_ID) return false;
    AvpStr x = tk(P, k - 1)->t, op = tk(P, k)->t, lit = tk(P, k + 1)->t;
    for (i64 q = lo + 1; q < hi - 3; q++)
        if (TEQ(tk(P, q), "[") && avp_str_eq(tk(P, q + 1)->t, x) && avp_str_eq(tk(P, q + 2)->t, op) &&
            tk(P, q + 3)->k == RL_NUM && !avp_str_eq(tk(P, q + 3)->t, lit) && q + 4 < hi && TEQ(tk(P, q + 4), "]"))
            return true;
    return false;
}

static void add(RlChecker *ck, RlParsed *P, i32 line, const char *rule, const char *tier, AvpStr msg) {
    rl_add(ck, P, line, rule, tier, msg, (AvpStr){0});
}

/* ---- check_expressions ---- */
static void check_expressions(RlChecker *ck, RlParsed *P, const RlFunc *f) {
    const RlTok *toks = P->toks.v;
    i64 n = (i64)P->toks.n;
    i64 lo = f->body + 1, hi = f->body_end;
    AvpArena *a = ck->a;
    for (i64 k = lo; k < hi; k++) {
        const RlTok *t = &toks[k];
        if ((TEQ(t, "+") || TEQ(t, "*")) && t->k == RL_OP && k + 1 < hi && k > lo) {
            const RlTok *nx = &toks[k + 1], *pv = &toks[k - 1];
            bool oe = pv->k == RL_ID || pv->k == RL_NUM || TEQ(pv, ")") || TEQ(pv, "]");
            if (oe && nx->k == RL_NUM && ((TEQ(t, "+") && TEQ(nx, "0")) || (TEQ(t, "*") && TEQ(nx, "1")))) {
                bool after_bad = k + 2 < n && (TEQ(&toks[k + 2], ".") || TEQ(&toks[k + 2], "["));
                if (!after_bad && !index_series(P, lo, hi, k))
                    add(ck, P, t->line, "R17", "match",
                        rl_fmt(a, "'%.*s %.*s' is a no-op steering expression", AVP_SF(t->t), AVP_SF(nx->t)));
            }
        }
        if (TEQ(t, "!") && k + 1 < hi && TEQ(&toks[k + 1], "!"))
            add(ck, P, t->line, "R17", "advisory", AVP_S("'!!' double negation: check it is the original's spelling"));
        if (TEQ(t, "if") && t->k == RL_ID && k + 3 < hi && TEQ(&toks[k + 1], "(") && TEQ(&toks[k + 3], ")") &&
            (TEQ(&toks[k + 2], "0") || TEQ(&toks[k + 2], "1")))
            add(ck, P, t->line, "R17", "advisory", rl_fmt(a, "'if (%.*s)': constant condition", AVP_SF(toks[k + 2].t)));
    }
    /* self-assignment over _stmt_ranges */
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    RL_VEC(RlRange) rg = {0};
    {
        i64 start = lo, i = lo;
        while (i < hi) {
            const RlTok *t = &toks[i];
            if ((TEQ(t, "(") || TEQ(t, "[")) && P->match[i] >= 0) {
                i = P->match[i] + 1;
                continue;
            }
            if (TEQ(t, ";") || TEQ(t, "{") || TEQ(t, "}")) {
                if (i > start) *RL_PUSH(s, &rg) = (RlRange){(i32)start, (i32)i};
                start = i + 1;
            }
            i++;
        }
        if (hi > start) *RL_PUSH(s, &rg) = (RlRange){(i32)start, (i32)hi};
    }
    for (size_t r = 0; r < rg.n; r++) {
        i64 slo = rg.v[r].lo, shi = rg.v[r].hi;
        if (shi - slo < 3) continue;
        i64 e = -1;
        for (i64 q = slo; q < shi; q++)
            if (TEQ(&toks[q], "=")) {
                e = q;
                break;
            }
        if (e < 0) continue;
        i64 nl = e - slo, nr = shi - e - 1;
        bool same = nl > 0 && nl == nr;
        for (i64 q = 0; same && q < nl; q++) same = avp_str_eq(toks[slo + q].t, toks[e + 1 + q].t);
        for (i64 q = slo; same && q < e; q++) {
            const RlTok *x = &toks[q];
            if (!(x->k == RL_ID || TEQ(x, ".") || TEQ(x, "->") || TEQ(x, "[") || TEQ(x, "]") || TEQ(x, "*") ||
                  x->k == RL_NUM))
                same = false;
        }
        if (!same) continue;
        AvpStr base = {0};
        for (i64 q = slo; q < e; q++)
            if (toks[q].k == RL_ID) {
                base = toks[q].t;
                break;
            }
        bool bad = false;
        for (i64 q = slo; q < shi; q++)
            if (TEQ(&toks[q], "(") || TEQ(&toks[q], "++") || TEQ(&toks[q], "--")) bad = true;
        if (!bad && !(base.p && avp_set_has(ck->noted_volatile, base))) {
            AvpStr left = rl_ptext(a, toks + slo, (size_t)nl);
            AvpStr right = rl_ptext(a, toks + e + 1, (size_t)nr);
            add(ck, P, toks[slo].line, "R17", "match", rl_fmt(a, "self-assignment '%.*s = %.*s'", AVP_SF(left), AVP_SF(right)));
        }
    }
    /* R35 comma operator */
    RlRange *st;
    size_t ns = rl_statements(toks, P->match, (i32)lo, (i32)hi, &st);
    for (size_t q = 0; q < ns; q++) {
        const RlTok *x0 = &toks[st[q].lo];
        size_t len = (size_t)(st[q].hi - st[q].lo);
        if (TEQ(x0, "for") || TEQ(x0, "case") || TEQ(x0, "default")) continue;
        RlDecl *d;
        if (rl_parse_decl(s, x0, len, &d)) continue;
        i64 depth = 0;
        for (size_t m = 0; m < len; m++) {
            const RlTok *x = &x0[m];
            if (in_chars(x, "([{")) depth++;
            else if (in_chars(x, ")]}")) depth--;
            else if (TEQ(x, ",") && depth == 0) {
                add(ck, P, x->line, "R35", "checked", AVP_S("comma operator outside a for header"));
                break;
            }
        }
    }
    avp_arena_reset(s, mk);
}

/* ---- _macro_bodies / _ids_in_used_macros ---- */
static AvpMap *macro_bodies(RlChecker *ck, RlParsed *P) {
    if (ck->macro_bodies) return ck->macro_bodies;
    AvpMap *out = avp_map_new();
    const AvpStrVec *cl = rl_checker_closure(ck, P->path);
    for (size_t qi = 0; qi < cl->n; qi++) {
        RlParsed *Q = rl_checker_parse(ck, cl->v[qi]);
        for (size_t di = 0; di < Q->directives.n; di++) {
            const RlDirective *d = &Q->directives.v[di];
            if (!avp_str_eqz(d->name, "define") || !d->toks.n || d->toks.v[0].k != RL_ID) continue;
            const RlTok *toks = d->toks.v;
            size_t n = d->toks.n, start = 1;
            AvpSet *params = NULL;
            if (n > 1 && TEQ(&toks[1], "(") && toks[1].line == toks[0].line) {
                i32 *m = rl_pair_brackets(avp_scratch(), toks, n);
                if (m[1] >= 0) {
                    params = avp_set_new();
                    for (i32 k = 2; k < m[1]; k++)
                        if (toks[k].k == RL_ID) avp_set_add(params, toks[k].t);
                    start = (size_t)m[1] + 1;
                }
            }
            bool created;
            AvpVal *v = avp_map_put(out, toks[0].t, &created);
            if (created) v->p = avp_set_new();
            for (size_t k = start; k < n; k++)
                if (toks[k].k == RL_ID && !(params && avp_set_has(params, toks[k].t))) avp_set_add(v->p, toks[k].t);
            if (params) avp_set_free(params);
        }
    }
    ck->macro_bodies = out;
    return out;
}

static AvpSet *ids_in_used_macros(RlChecker *ck, RlParsed *P, i64 lo, i64 hi) {
    AvpMap *bodies = macro_bodies(ck, P);
    AvpSet *out = avp_set_new(), *expanded = avp_set_new();
    AvpStrVec todo = {0};
    for (i64 k = lo; k < hi; k++)
        if (tk(P, k)->k == RL_ID && avp_map_has(bodies, tk(P, k)->t)) avp_vec_push(&todo, tk(P, k)->t);
    while (todo.n) {
        AvpStr nm = todo.v[--todo.n];
        if (!avp_set_add(expanded, nm)) continue;
        AvpSet *b = avp_map_get(bodies, nm)->p;
        size_t pos = 0;
        AvpStr m;
        while (avp_set_next(b, &pos, &m)) {
            avp_set_add(out, m);
            if (avp_map_has(bodies, m)) avp_vec_push(&todo, m);
        }
    }
    avp_vec_free(&todo);
    avp_set_free(expanded);
    return out;
}

/* ---- check_locals ---- */
static bool asm_names(const RlParsed *P, i64 k, AvpStr nm) {
    for (size_t i = 0; i < P->asm_ids.n; i++)
        if (P->asm_ids.v[i].tok == k) {
            const AvpStrVec *ids = &P->asm_ids.v[i].ids;
            for (size_t q = 0; q < ids->n; q++)
                if (avp_str_eq(ids->v[q], nm)) return true;
            return false;
        }
    return false;
}

static void check_locals(RlChecker *ck, RlParsed *P, const RlFunc *f) {
    const RlTok *toks = P->toks.v;
    i64 lo = f->body + 1, hi = f->body_end;
    if (f->is_asm) return;
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    RlRange *st;
    size_t ns = rl_statements(toks, P->match, (i32)lo, (i32)hi, &st);
    RL_VEC(RlDecl) decls = {0};
    for (size_t q = 0; q < ns; q++) {
        const RlTok *x0 = &toks[st[q].lo];
        size_t len = (size_t)(st[q].hi - st[q].lo);
        if (TEQ(x0, "extern")) {
            bool paren = false;
            for (size_t m = 0; m < len; m++) paren |= TEQ(&x0[m], "(");
            if (paren)
                add(ck, P, x0->line, "R25", "checked",
                    AVP_S("local extern function declaration: put the prototype in a header"));
            else
                add(ck, P, x0->line, "R2", "match", AVP_S("local extern global declaration: declare it once, in a header"));
            continue;
        }
        RlDecl *d;
        size_t nd = rl_parse_decl(s, x0, len, &d);
        for (size_t m = 0; m < nd; m++) {
            RlDecl *o = RL_PUSH(s, &decls);
            *o = d[m];
            o->tok += st[q].lo; /* absolute token index */
        }
    }
    AvpSet *macro_ids = decls.n ? ids_in_used_macros(ck, P, lo, hi) : NULL;
    static const char *const DROP[] = {"*", "static", "const", "volatile", "register", NULL};
    for (size_t di = 0; di < decls.n; di++) {
        const RlDecl *d = &decls.v[di];
        bool is_param = false;
        for (size_t q = 0; q < f->params.n && !is_param; q++) is_param = avp_str_eq(f->params.v[q].name, d->name);
        if (is_param) continue;
        bool ptr = avp_str_contains(d->type, AVP_S("*"));
        if (P->is_cpp && !ptr) {
            AvpStrVec words = avp_str_split(d->type, AVP_S(""), -1);
            bool all = true;
            for (size_t q = 0; q < words.n; q++) {
                bool drop = false;
                for (const char *const *w = DROP; *w && !drop; w++) drop = avp_str_eqz(words.v[q], *w);
                if (!drop && !rl_sin(words.v[q], RL_SCALAR_WORDS)) all = false;
            }
            avp_vec_free(&words);
            if (!all) continue;
        }
        int reads = 0;
        for (i64 k = lo; k < hi; k++) {
            const RlTok *x = &toks[k];
            if (k == d->tok) continue;
            if (x->k == RL_ID && TEQ(x, "__ASM_BODY__") && asm_names(P, k, d->name)) {
                reads++;
                continue;
            }
            if (x->k == RL_ID && avp_str_eq(x->t, d->name)) {
                AvpStr pv = k > lo ? toks[k - 1].t : AVP_S("");
                if (avp_str_eqz(pv, ".") || avp_str_eqz(pv, "->")) continue;
                AvpStr nx = k + 1 < hi ? toks[k + 1].t : AVP_S("");
                bool stmt_start = k == lo || avp_str_eqz(pv, ";") || avp_str_eqz(pv, "{") || avp_str_eqz(pv, "}");
                if (stmt_start && avp_str_eqz(nx, "=")) continue;
                reads++;
            }
        }
        i32 decl_line = toks[d->tok].line;
        bool marked = false;
        for (i32 ln = decl_line - 2; ln <= decl_line + 1 && !marked; ln++) {
            size_t nc;
            const i32 *ix = rl_comments_on(P, ln, &nc);
            for (size_t q = 0; q < nc && !marked; q++) {
                AvpStr t = P->comments.v[ix[q]].text;
                marked = avp_str_contains(t, AVP_S("R36")) || avp_str_contains(t, AVP_S("BUG:")) ||
                         avp_str_contains(t, AVP_S("NOTE:"));
            }
        }
        if (reads == 0 && macro_ids && avp_set_has(macro_ids, d->name)) reads = 1;
        if (reads == 0 && !marked)
            add(ck, P, decl_line, "R17", "match",
                rl_fmt(ck->a, "local '%.*s' is never read: dummy/unused local", AVP_SF(d->name)));
    }
    if (macro_ids) avp_set_free(macro_ids);
    avp_arena_reset(s, mk);
    for (i64 k = lo; k < hi; k++)
        if (TEQ(&toks[k], "register") && toks[k].k == RL_ID)
            add(ck, P, toks[k].line, "R17", "advisory",
                AVP_S("'register' local: genuine only where the codebase uses it (Section F)"));
}

/* ---- check_control / check_condition ---- */
static i64 after_condition_declarator(const RlParsed *P, i64 lo, i64 hi) {
    i64 n = 0;
    for (i64 k = lo; k < hi; k++) {
        const RlTok *t = tk(P, k);
        if (TEQ(t, "=")) {
            if (n >= 2 && tk(P, k - 1)->k == RL_ID && tk(P, lo)->k == RL_ID && !TEQ(tk(P, k - 2), "::") &&
                !TEQ(tk(P, k - 2), "."))
                return k + 1;
            return lo;
        }
        if (t->k != RL_ID && !TEQ(t, "*") && !TEQ(t, "&") && !TEQ(t, "::")) return lo;
        n++;
    }
    return lo;
}

static bool depth0_assign(const RlParsed *P, i64 lo, i64 hi) {
    i64 depth = 0;
    for (i64 k = lo; k < hi; k++) {
        const RlTok *x = tk(P, k);
        if (in_chars(x, "([{")) depth++;
        else if (in_chars(x, ")]}")) depth--;
        else if (TEQ(x, "=") && depth == 0) return true;
    }
    return false;
}

static void check_condition(RlChecker *ck, RlParsed *P, i64 lp, i64 rp) {
    i64 k = lp + 1;
    if (P->is_cpp) k = after_condition_declarator(P, lp + 1, rp);
    static const char *const CMP[] = {"!=", "==", "<", ">", "<=", ">=", NULL};
    while (k < rp) {
        const RlTok *t = tk(P, k);
        if ((TEQ(t, "(") || TEQ(t, "[")) && P->match[k] >= 0) {
            i64 inner_end = P->match[k];
            if (TEQ(t, "(") && depth0_assign(P, k + 1, inner_end)) {
                AvpStr prev = k - 1 > lp ? tk(P, k - 1)->t : AVP_S("");
                AvpStr nxt = inner_end + 1 < rp ? tk(P, inner_end + 1)->t : AVP_S("");
                if (avp_str_eqz(prev, "!") || rl_sin(nxt, CMP) || avp_str_eqz(prev, "==") || avp_str_eqz(prev, "!=")) {
                    k = inner_end + 1;
                    continue;
                }
                add(ck, P, t->line, "R35", "checked", AVP_S("assignment inside a condition (hidden state change)"));
                k = inner_end + 1;
                continue;
            }
            k = inner_end + 1;
            continue;
        }
        if (TEQ(t, "="))
            add(ck, P, t->line, "R35", "checked", AVP_S("assignment inside a condition (hidden state change)"));
        k++;
    }
}

static void check_control(RlChecker *ck, RlParsed *P, const RlFunc *f) {
    i64 n = (i64)P->toks.n;
    i64 lo = f->body + 1, hi = f->body_end;
    for (i64 k = lo; k < hi; k++) {
        const RlTok *t = tk(P, k);
        if (t->k != RL_ID) continue;
        bool head = TEQ(t, "if") || TEQ(t, "for") || TEQ(t, "while") || TEQ(t, "switch");
        if (head && k + 1 < hi && TEQ(tk(P, k + 1), "(") && P->match[k + 1] >= 0) {
            i64 close = P->match[k + 1];
            if (TEQ(t, "while") && k > 0 && TEQ(tk(P, k - 1), "}")) {
                i64 o = P->match[k - 1];
                if (o >= 0 && o > 0 && TEQ(tk(P, o - 1), "do")) continue;
            }
            const RlTok *nxt = close + 1 < n ? tk(P, close + 1) : NULL;
            if (nxt && !TEQ(nxt, "{") && !TEQ(nxt, ";") && !TEQ(t, "switch"))
                add(ck, P, t->line, "R35", "free", rl_fmt(ck->a, "'%.*s' body without braces", AVP_SF(t->t)));
            if (TEQ(t, "if") || TEQ(t, "while")) check_condition(ck, P, k + 1, close);
        } else if (head) {
            continue; /* Python: the if/elif chain stops at the first test on t.t, which this one is not */
        } else if (TEQ(t, "else")) {
            const RlTok *nxt = k + 1 < hi ? tk(P, k + 1) : NULL;
            if (nxt && !TEQ(nxt, "{") && !TEQ(nxt, "if"))
                add(ck, P, t->line, "R35", "free", AVP_S("'else' body without braces"));
        } else if (TEQ(t, "do")) {
            const RlTok *nxt = k + 1 < hi ? tk(P, k + 1) : NULL;
            if (nxt && !TEQ(nxt, "{")) add(ck, P, t->line, "R35", "free", AVP_S("'do' body without braces"));
        }
    }
}

/* ---- check_literals ---- */
static bool floaty(const RlParsed *P, i64 k, i64 lo) {
    i64 s = k;
    while (s > lo && !TEQ(tk(P, s - 1), ";") && !TEQ(tk(P, s - 1), "{") && !TEQ(tk(P, s - 1), "}")) s--;
    for (i64 q = s; q < k; q++)
        if (TEQ(tk(P, q), "float") || TEQ(tk(P, q), "f32") || TEQ(tk(P, q), "double") || TEQ(tk(P, q), "f64"))
            return true;
    return false;
}

static bool explained(const RlParsed *P, i32 line) {
    size_t n;
    rl_comments_on(P, line, &n);
    if (n) return true;
    const i32 *ix = rl_comments_on(P, line - 1, &n);
    for (size_t q = 0; q < n; q++)
        if (!P->comments.v[ix[q]].trailing) return true;
    return false;
}

static void check_literals(RlChecker *ck, RlParsed *P, const RlFunc *f) {
    i64 lo = f->body + 1, hi = f->body_end;
    static const char *const BITS[] = {"&", "|", "^", "&=", "|=", "^=", NULL};
    static const char *const BITS_AFTER[] = {"&", "|", "^", NULL};
    static const char *const CMP[] = {"<", "<=", ">", ">=", NULL};
    for (i64 k = lo; k < hi; k++) {
        const RlTok *t = tk(P, k);
        if (t->k != RL_NUM) continue;
        AvpStr txt = avp_str_rstrip_chars(avp_str_lower(ck->a, t->t), AVP_S("ul"));
        bool is_hex = avp_str_startswith(txt, AVP_S("0x"));
        u64 val = 0;
        bool over = false;
        if (is_hex) {
            if (txt.n < 3) continue;
            for (size_t i = 2; i < txt.n; i++) {
                char c = txt.p[i];
                unsigned dg = c >= '0' && c <= '9' ? (unsigned)(c - '0') : (c >= 'a' && c <= 'f') ? (unsigned)(c - 'a' + 10) : 99;
                if (dg == 99) goto next;
                if (val >> 60) over = true;
                val = val * 16 + dg;
            }
        } else {
            if (!txt.n) continue;
            for (size_t i = 0; i < txt.n; i++) {
                char c = txt.p[i];
                if (c < '0' || c > '9') goto next;
                if (val > (UINT64_MAX - 9) / 10) over = true;
                val = val * 10 + (unsigned)(c - '0');
            }
        }
        {
            AvpStr prev = k > lo ? tk(P, k - 1)->t : AVP_S("");
            AvpStr nxt = k + 1 < hi ? tk(P, k + 1)->t : AVP_S("");
            bool bitctx = rl_sin(prev, BITS) || rl_sin(nxt, BITS_AFTER);
            /* a literal of 64 bits or more: D-check_rules-1 */
            bool ge256 = over || val >= 256, ge1000 = over || val >= 0x1000, lt100 = !over && val < 0x100;
            if (!is_hex && bitctx && ge256 && !over && ((val & (val - 1)) == 0 || ((val + 1) & val) == 0))
                add(ck, P, t->line, "R9", "free",
                    rl_fmt(ck->a, "decimal mask/flag %.*s: write masks and flags in hex", AVP_SF(t->t)));
            if (is_hex && txt.n == 10 && strchr("3456789abcd", txt.p[2]) && floaty(P, k, lo))
                add(ck, P, t->line, "R9", "free", rl_fmt(ck->a, "raw float bits %.*s: write the float literal", AVP_SF(t->t)));
            if (is_hex && rl_sin(prev, CMP) && lt100 && !bitctx)
                add(ck, P, t->line, "R9", "free",
                    rl_fmt(ck->a, "hex loop/compare bound %.*s: counts are decimal", AVP_SF(t->t)));
            if (ge1000 && !explained(P, t->line))
                add(ck, P, t->line, "R11", "free",
                    rl_fmt(ck->a, "unexplained constant %.*s: name it or comment it", AVP_SF(t->t)));
        }
    next:;
    }
}

/* ---- check_function ---- */
static const ShsmRe *generic_labels(void) {
    return shsm_re("(?:L|LAB|LABEL|label|loc|lbl|Label)_?[0-9A-Fa-f]*|joined_\\w+", 0);
}
static const ShsmRe *meaningful_labels(void) {
    return shsm_re("(?i)(fail|failed|failure|done|retry|again|out|error|err|exit|cleanup|clean|end|finish|bail|abort|"
                   "next|skip|restart|top|loop|ret|return|leave)\\w*",
                   0);
}

void rl_check_function(RlChecker *ck, RlParsed *P, RlFunc *f) {
    const RlTok *toks = P->toks.v;
    const i32 *match = P->match;
    i64 n = (i64)P->toks.n;
    i64 lo = f->body + 1, hi = f->body_end;
    AvpArena *s = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(s);
    bool *init_braces = avp_alloc(s, (size_t)n + 1);
    for (i64 k = lo; k < hi; k++)
        if (TEQ(&toks[k], "{") && TEQ(&toks[k - 1], "=") && match[k] >= 0)
            for (i64 j = k; j <= match[k]; j++)
                if (TEQ(&toks[j], "{") || TEQ(&toks[j], "}")) init_braces[j] = true;
    i64 depth = 0;
    for (i64 k = lo; k < hi; k++) {
        const RlTok *t = &toks[k];
        if (init_braces[k]) continue;
        if (TEQ(t, "(")) depth++;
        else if (TEQ(t, ")")) depth--;
        else if (TEQ(t, ";") && depth == 0) {
            if (k - 1 < 0) continue;
            const RlTok *prev = &toks[k - 1];
            if ((TEQ(prev, ";") || TEQ(prev, "{")) && k - 1 >= lo - 1) {
                if (!(TEQ(prev, "{") && loop_block(P, k - 1))) add(ck, P, t->line, "R17", "match", AVP_S("bare ';' statement"));
            } else if (TEQ(prev, ")") && match[k - 1] >= 0) {
                i64 o = match[k - 1];
                if (o > 0 && TEQ(&toks[o - 1], "if")) add(ck, P, t->line, "R17", "match", AVP_S("empty 'if' body"));
            } else if (TEQ(prev, "else")) {
                add(ck, P, t->line, "R17", "match", AVP_S("empty 'else' body"));
            }
        } else if (TEQ(t, "{") && k + 1 < hi && TEQ(&toks[k + 1], "}")) {
            if (k - 1 < 0) continue;
            const RlTok *prev = &toks[k - 1];
            if (TEQ(prev, ")") && match[k - 1] >= 0) {
                i64 o = match[k - 1];
                if (o > 0 && TEQ(&toks[o - 1], "if")) {
                    const RlTok *after = k + 2 < n ? &toks[k + 2] : NULL;
                    if (!after || !TEQ(after, "else")) add(ck, P, t->line, "R17", "match", AVP_S("empty 'if' block"));
                }
            } else if (TEQ(prev, "else")) {
                add(ck, P, t->line, "R17", "match", AVP_S("empty 'else' block"));
            } else if (TEQ(prev, ";") || TEQ(prev, "{") || TEQ(prev, "}")) {
                add(ck, P, t->line, "R17", "match", AVP_S("empty '{ }' block"));
            }
        } else if (TEQ(t, "{") && k + 1 < hi) {
            const RlTok *prev = k - 1 >= 0 ? &toks[k - 1] : NULL;
            if (prev && (TEQ(prev, ";") || TEQ(prev, "{") || TEQ(prev, "}")) && k - 1 >= lo - 1 && k - 1 != f->body)
                add(ck, P, t->line, "R17", "advisory",
                    AVP_S("arbitrary '{ }' block: scope a local only in an existing block"));
        } else if (TEQ(t, "do") && t->k == RL_ID) {
            if (k + 1 < hi && TEQ(&toks[k + 1], "{") && match[k + 1] >= 0) {
                i64 ce = match[k + 1];
                if (ce + 4 < n && TEQ(&toks[ce + 1], "while") && TEQ(&toks[ce + 2], "(") && TEQ(&toks[ce + 3], "0") &&
                    TEQ(&toks[ce + 4], ")"))
                    add(ck, P, t->line, "R17", "match", AVP_S("do { } while ( 0 ) in a function body: use the macro"));
            }
        } else if (TEQ(t, "goto") && t->k == RL_ID) {
            AvpStr lab = k + 1 < hi ? toks[k + 1].t : AVP_S("?");
            AvpStr why;
            if (shsm_re_fullmatch(generic_labels(), lab, 0, AVP_END, NULL) ||
                !shsm_re_fullmatch(meaningful_labels(), lab, 0, AVP_END, NULL))
                why = rl_fmt(ck->a, "label '%.*s' is not a meaningful cleanup/retry name", AVP_SF(lab));
            else
                why = rl_fmt(ck->a, "label '%.*s'", AVP_SF(lab));
            add(ck, P, t->line, "R14", "checked", rl_fmt(ck->a, "goto: %.*s", AVP_SF(why)));
        }
    }
    avp_arena_reset(s, mk);
    check_expressions(ck, P, f);
    check_locals(ck, P, f);
    check_control(ck, P, f);
    check_literals(ck, P, f);
}

/* ---- check_function_meta ---- */
void rl_check_function_meta(RlChecker *ck, RlParsed *P) {
    bool have_last = false;
    u32 last_addr = 0;
    AvpStr last_name = {0};
    const ShsmRe *annot = shsm_re("^\\s*(0x8[0-9A-Fa-f]{7})\\s*$", 0);
    for (size_t i = 0; i < P->p.funcs.n; i++) {
        RlFunc *f = &P->p.funcs.v[i];
        bool small = f->body_end - f->body <= 7;
        bool has_header = false;
        for (int d = 1; d >= 0 && !has_header; d--) {
            size_t nc;
            const i32 *ix = rl_comments_on(P, f->line - d, &nc);
            for (size_t q = 0; q < nc; q++)
                if (!P->comments.v[ix[q]].trailing) has_header = true;
        }
        if (!has_header && !small && !f->in_class.n)
            rl_add(ck, P, f->line, "R33", "free", AVP_S("function without a header comment"), f->name);
        size_t nc;
        const i32 *ix = rl_comments_on(P, f->end_line, &nc);
        for (size_t q = 0; q < nc; q++) {
            ShsmReMatch m;
            if (shsm_re_match(annot, P->comments.v[ix[q]].text, 0, AVP_END, &m)) {
                u64 v = 0;
                avp_parse_uint(shsm_re_group(&m, 1), 16, &v);
                f->has_addr = true;
                f->addr = (u32)v;
            }
        }
        if (f->has_addr && !f->in_class.n && !f->is_static && !f->template_ && !f->anon_ns) {
            if (have_last && f->addr < last_addr)
                rl_add(ck, P, f->line, "R15", "checked",
                       rl_fmt(ck->a, "function order differs from the binary: 0x%08X is placed after 0x%08X (%.*s)",
                              f->addr, last_addr, AVP_SF(last_name)),
                       f->name);
            have_last = true;
            last_addr = f->addr;
            last_name = f->name;
        }
    }
    if (!P->is_cpp) {
        for (size_t i = 0; i < P->p.funcs.n; i++) {
            const RlFunc *f = &P->p.funcs.v[i];
            if (f->lparen + 1 == f->rparen && !f->is_asm)
                rl_add(ck, P, f->line, "R25", "checked", AVP_S("'()' parameter list in C: write '(void)'"), f->name);
        }
        for (size_t i = 0; i < P->p.protos.n; i++)
            if (P->p.protos.v[i].empty_params)
                add(ck, P, P->p.protos.v[i].line, "R25", "checked", AVP_S("'()' prototype in C: write '(void)'"));
    }
    if (!P->is_header) {
        for (size_t i = 0; i < P->p.protos.n; i++) {
            const RlProto *pr = &P->p.protos.v[i];
            if (!avp_set_has(P->func_names, pr->name))
                add(ck, P, pr->line, "R25", "checked",
                    rl_fmt(ck->a, "prototype of '%.*s' in a source file: it belongs in a header", AVP_SF(pr->name)));
        }
    }
}
