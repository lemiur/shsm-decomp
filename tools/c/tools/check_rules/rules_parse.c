/* rules_parse: the parse model of tools/rules/check_rules.py (lines 408-1365). See rules_parse.h, rules_lex.md. */
#include "rules_parse.h"

/* ---- word lists (check_rules.py lines 149-163, 465-469) ------------------------------------------------------ */
static const char *const DECL_KEYWORDS[] = {"static", "register", "const",    "volatile", "struct",  "union",
                                            "enum",   "class",    "extern",   "auto",     "inline",  "typename",
                                            "mutable", "virtual", "unsigned", "signed",   NULL};
static const char *const BASIC_TYPE_WORDS[] = {"void",  "char", "short", "int",   "long",
                                               "float", "double", "bool", "_Bool", "wchar_t", NULL};
static const char *const AGG_KW[] = {"struct", "class", "union", "enum", NULL};
static const char *const ACCESS_WORDS[] = {"public", "private", "protected", NULL};
static const char *const ATTR_SPECIFIERS[] = {"__declspec", "__attribute__", "ATTRIBUTE_ALIGN", NULL};
static const char *const NOT_OPERATOR_NAME[] = {";", ",", ")", "=", "[", "]", NULL};

/* DECL_MACROS: -1 not one, else RL_STORAGE_NONE ("" for GLOBAL), _EXTERN or _STATIC. */
static int decl_macro(AvpStr s) {
    if (avp_str_eqz(s, "GLOBAL")) return RL_STORAGE_NONE;
    if (avp_str_eqz(s, "EXTERN")) return RL_STORAGE_EXTERN;
    if (avp_str_eqz(s, "LOCAL") || avp_str_eqz(s, "METHODDEF")) return RL_STORAGE_STATIC;
    return -1;
}

#define TEQ RL_TEQ
/* x.t in "([" / x.t in ")]" (substring tests on one-character token texts) */
static bool is_open2(const RlTok *t) { return t->t.n == 1 && (t->t.p[0] == '(' || t->t.p[0] == '['); }
static bool is_close2(const RlTok *t) { return t->t.n == 1 && (t->t.p[0] == ')' || t->t.p[0] == ']'); }

enum { CTX_FILE, CTX_NS, CTX_CLASS };

typedef struct {
    const AvpStr *v;
    size_t n;
} NsList;

typedef struct {
    AvpArena *a;
    RlParsed *P;
    const RlTok *T;
    size_t n;
    const i32 *match;
    RlParseOut *out;
} Ps;

static AvpStr join_ns(AvpArena *a, NsList ns, bool skip_empty, AvpStr cls, AvpStr name, bool with_name) {
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (size_t k = 0; k < ns.n; k++) {
        if (skip_empty && !ns.v[k].n) continue;
        if (!first) avp_buf_putz(&b, "::");
        avp_buf_put(&b, ns.v[k]);
        first = false;
    }
    if (cls.n) {
        if (!first) avp_buf_putz(&b, "::");
        avp_buf_put(&b, cls);
        first = false;
    }
    if (with_name) {
        if (!first) avp_buf_putz(&b, "::");
        avp_buf_put(&b, name);
    }
    return avp_strn(avp_buf_cz(&b), b.n);
}

static AvpStr cat3(AvpArena *a, AvpStr x, const char *mid, AvpStr y) {
    return avp_str_concat(a, avp_str_concat(a, x, avp_str(mid)), y);
}

/* ---- _Parser.function, _params -------------------------------------------------------------------------------- */
static RlParams parse_params(Ps *ps, i64 lo, i64 hi) {
    RlParams params = {0};
    if (hi <= lo) return params;
    const RlTok *T = ps->T;
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    size_t cnt = (size_t)(hi - lo);
    size_t *starts = AVP_NEWN(avp_scratch(), size_t, cnt + 1);
    size_t *ends = AVP_NEWN(avp_scratch(), size_t, cnt + 1);
    size_t np = 0, cur = (size_t)lo;
    int depth = 0;
    /* chunks are contiguous runs between depth-0 commas */
    for (size_t q = (size_t)lo; q < (size_t)hi; q++) {
        const RlTok *x = &T[q];
        if (is_open2(x)) depth++;
        else if (is_close2(x)) depth--;
        if (TEQ(x, ",") && depth == 0) {
            starts[np] = cur;
            ends[np++] = q;
            cur = q + 1;
        }
    }
    if (cur < (size_t)hi) {
        starts[np] = cur;
        ends[np++] = (size_t)hi;
    }
    for (size_t c = 0; c < np; c++) {
        const RlTok *p = &T[starts[c]];
        size_t pn = ends[c] - starts[c];
        if (!pn || (pn == 1 && (TEQ(&p[0], "void") || TEQ(&p[0], "...")))) continue;
        bool any_id = false, has_reg = false;
        for (size_t z = 0; z < pn; z++) {
            any_id |= p[z].k == RL_ID;
            has_reg |= TEQ(&p[z], "register");
        }
        RlParam *r = RL_PUSH(ps->a, &params);
        if (any_id && p[pn - 1].k == RL_ID && pn >= 2) {
            r->name = p[pn - 1].t;
            r->type = rl_ptext(ps->a, p, pn - 1);
        } else {
            r->name = avp_str("");
            r->type = rl_ptext(ps->a, p, pn);
        }
        r->reg = has_reg;
    }
    avp_arena_reset(avp_scratch(), mk);
    return params;
}

static RlFunc build_function(Ps *ps, size_t i, size_t head_end, i64 paren_first, size_t brace, size_t closing,
                             AvpStr cls) {
    const RlTok *T = ps->T;
    AvpArena *a = ps->a;
    RlFunc f = {0};
    const RlTok *head = &T[i];
    size_t hn = head_end > i ? head_end - i : 0;
    f.head = (i32)i;
    f.line = hn ? head[0].line : T[brace].line;
    f.lparen = (i32)paren_first;
    f.rparen = ps->match[paren_first] >= 0 ? ps->match[paren_first] : (i32)paren_first;
    f.body = (i32)brace;
    f.body_end = (i32)closing;
    f.end_line = T[closing].line;
    int d = 0;
    for (size_t q = 0; q < hn; q++) {
        const RlTok *x = &head[q];
        if (is_open2(x)) d++;
        else if (is_close2(x)) d--;
        else if (d == 0 && x->k == RL_ID) {
            if (TEQ(x, "asm") || TEQ(x, "__asm")) f.is_asm = true;
            if (TEQ(x, "static") || decl_macro(x->t) == RL_STORAGE_STATIC ||
                rl_storage_get(ps->P, x->t) == RL_STORAGE_STATIC)
                f.is_static = true;
            if (TEQ(x, "virtual")) f.virtual_ = true;
        }
    }
    f.template_ = hn >= 3 && TEQ(&head[0], "template") && TEQ(&head[1], "<") && !TEQ(&head[2], ">");
    const RlTok *pre = &T[i];
    i64 pn = paren_first - (i64)i;
    if (pn < 0) pn = 0;
    AvpStr name;
    i64 k = pn - 1, op_idx = -1;
    for (i64 q = 0; q < pn; q++)
        if (TEQ(&pre[q], "operator")) op_idx = q;
    if (op_idx >= 0) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "operator");
        for (i64 q = op_idx + 1; q < pn; q++) avp_buf_put(&b, pre[q].t);
        name = avp_strn(avp_buf_cz(&b), b.n);
        k = op_idx - 1;
        if (k >= 1 && TEQ(&pre[k], "::")) name = cat3(a, pre[k - 1].t, "::", name);
    } else if (k >= 0 && pre[k].k == RL_ID) {
        AvpStrVec parts = {0};
        AvpStr first = pre[k].t;
        if (k >= 1 && TEQ(&pre[k - 1], "~")) {
            first = avp_str_concat(a, avp_str("~"), first);
            k--;
        }
        avp_vec_push(&parts, first);
        k--;
        while (k >= 1 && TEQ(&pre[k], "::") && pre[k - 1].k == RL_ID) {
            avp_vec_insert(&parts, 0, pre[k - 1].t);
            k -= 2;
        }
        name = avp_str_join(a, avp_str("::"), parts.v, parts.n);
        avp_vec_free(&parts);
    } else {
        name = avp_str("<anon>");
    }
    if (cls.n && avp_str_find(name, avp_str("::"), 0, AVP_END) < 0) name = cat3(a, cls, "::", name);
    f.name = name;
    f.in_class = cls;
    f.params = parse_params(ps, (i64)f.lparen + 1, f.rparen);
    if (f.virtual_) {
        RlNameLine *v = RL_PUSH(a, &ps->out->virtuals);
        v->name = name;
        v->line = f.line;
    }
    return f;
}

/* ---- _knr_definition ------------------------------------------------------------------------------------------- */
static bool knr_definition(Ps *ps, size_t i, size_t paren_first, size_t semi, size_t end, size_t *head_end,
                           size_t *brace, size_t *closing_out, RlParams *out) {
    (void)i;
    const RlTok *T = ps->T;
    static const char *const STOPS[] = {";", "{", "}", "=", NULL};
    static const char *const OKOPS[] = {"*", ",", "[", "]", NULL};
    if (T[paren_first - 1].k != RL_ID || decl_macro(T[paren_first - 1].t) >= 0) return false;
    i32 rp = ps->match[paren_first];
    if (rp < 0 || (size_t)rp >= semi) return false;
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpArena *sc = avp_scratch();
    bool ok = false;
    size_t nn = 0;
    AvpStr *names = AVP_NEWN(sc, AvpStr, (size_t)rp - paren_first + 1);
    for (size_t q = paren_first + 1; q < (size_t)rp; q++) {
        const RlTok *x = &T[q];
        if (TEQ(x, ",")) continue;
        if (x->k != RL_ID || rl_tin(x, DECL_KEYWORDS) || rl_tin(x, BASIC_TYPE_WORDS)) goto done;
        names[nn++] = x->t;
    }
    if ((size_t)rp + 1 >= semi) goto done;
    /* params dict: the last assignment of a name wins */
    RL_VEC(RlParam) pd = {0};
    size_t k = (size_t)rp + 1;
    for (;;) {
        size_t lo = k;
        while (k < end && !rl_tin(&T[k], STOPS)) {
            if (T[k].k != RL_ID && T[k].k != RL_NUM && !rl_tin(&T[k], OKOPS)) goto done;
            k++;
        }
        if (k >= end || !TEQ(&T[k], ";") || k == lo) goto done;
        /* split at depth-0 commas */
        size_t cnt = k - lo;
        size_t *ps_ = AVP_NEWN(sc, size_t, cnt + 1), *pe = AVP_NEWN(sc, size_t, cnt + 1);
        size_t np = 0, cur = lo;
        int depth = 0;
        for (size_t q = lo; q < k; q++) {
            const RlTok *x = &T[q];
            if (TEQ(x, "[")) depth++;
            else if (TEQ(x, "]")) depth--;
            if (TEQ(x, ",") && depth == 0) {
                ps_[np] = cur;
                pe[np++] = q;
                cur = q + 1;
            }
        }
        ps_[np] = cur;
        pe[np++] = k;
        const RlTok **base = NULL;
        size_t bn = 0;
        for (size_t pi = 0; pi < np; pi++) {
            const RlTok *part = &T[ps_[pi]];
            size_t pn = pe[pi] - ps_[pi];
            int dd = 0;
            i64 name_at = -1;
            for (size_t z = 0; z < pn; z++) {
                const RlTok *x = &part[z];
                if (TEQ(x, "[")) dd++;
                else if (TEQ(x, "]")) dd--;
                else if (dd == 0 && x->k == RL_ID) name_at = (i64)z;
            }
            bool listed = false;
            if (name_at >= 0)
                for (size_t q = 0; q < nn; q++) listed |= avp_str_eq(names[q], part[name_at].t);
            if (!listed) goto done;
            const RlTok **ty = AVP_NEWN(sc, const RlTok *, bn + (size_t)name_at + 1);
            size_t tn = 0;
            if (pi == 0) {
                for (i64 z = 0; z < name_at; z++) ty[tn++] = &part[z];
                base = AVP_NEWN(sc, const RlTok *, tn + 1);
                bn = tn;
                memcpy(base, ty, tn * sizeof *ty);
                while (bn && TEQ(base[bn - 1], "*")) bn--;
            } else {
                for (size_t z = 0; z < bn; z++) ty[tn++] = base[z];
                for (i64 z = 0; z < name_at; z++) ty[tn++] = &part[z];
            }
            bool reg = false;
            for (size_t z = 0; z < tn; z++) reg |= TEQ(ty[z], "register");
            RlParam *r = RL_PUSH(sc, &pd);
            r->name = part[name_at].t;
            r->type = rl_ptext_ptrs(ps->a, ty, tn);
            r->reg = reg;
        }
        k++;
        if (k < end && TEQ(&T[k], "{")) break;
    }
    i32 closing = ps->match[k];
    if (closing < 0) goto done;
    *head_end = (size_t)rp + 1;
    *brace = k;
    *closing_out = (size_t)closing;
    RlParams res = {0};
    for (size_t q = 0; q < nn; q++) {
        RlParam got = {names[q], avp_str(""), false};
        for (size_t z = pd.n; z-- > 0;)
            if (avp_str_eq(pd.v[z].name, names[q])) {
                got = pd.v[z];
                break;
            }
        *RL_PUSH(ps->a, &res) = got;
    }
    *out = res;
    ok = true;
done:
    avp_arena_reset(sc, mk);
    return ok;
}

/* ---- _members, _hides_data_member, _decl_name ----------------------------------------------------------------- */
static const RlTok *decl_name(const RlTok *const *stmt, size_t n) {
    const RlTok *last = NULL;
    int depth = 0;
    for (size_t q = 0; q < n; q++) {
        const RlTok *x = stmt[q];
        if (is_open2(x)) depth++;
        else if (is_close2(x)) depth--;
        else if (depth == 0 && (TEQ(x, "=") || TEQ(x, ":") || TEQ(x, ","))) break;
        else if (depth == 0 && x->k == RL_ID) last = x;
    }
    return last;
}

static bool all_caps_word(AvpStr s) {
    if (!s.n || !(s.p[0] >= 'A' && s.p[0] <= 'Z')) return false;
    for (size_t q = 1; q < s.n; q++) {
        char c = s.p[q];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

static bool hides_data_member(const RlTok *const *stmt, size_t n) {
    if (!n) return false;
    for (size_t q = 0; q < n; q++)
        if (TEQ(stmt[q], "{}")) return false;
    for (size_t q = 0; q + 1 < n; q++)
        if (TEQ(stmt[q], "(") && TEQ(stmt[q + 1], "*")) return true;
    return stmt[0]->k == RL_ID && all_caps_word(stmt[0]->t) && n > 1 && TEQ(stmt[1], "(");
}

static void members(Ps *ps, RlAgg *agg, size_t lo, size_t hi) {
    const RlTok *T = ps->T;
    AvpArena *a = ps->a;
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpArena *sc = avp_scratch();
    RlTokPtrs stmt = {0};
    size_t i = lo;
    while (i < hi) {
        const RlTok *t = &T[i];
        if (TEQ(t, "{") && ps->match[i] >= 0) {
            RlTok *ph = AVP_NEW(sc, RlTok);
            ph->k = RL_OP;
            ph->t = avp_str("{}");
            ph->line = t->line;
            *RL_PUSH(sc, &stmt) = ph;
            i = (size_t)ps->match[i] + 1;
            continue;
        }
        if ((TEQ(t, "(") || TEQ(t, "[")) && ps->match[i] >= 0) {
            for (size_t q = i; q <= (size_t)ps->match[i]; q++) *RL_PUSH(sc, &stmt) = &T[q];
            i = (size_t)ps->match[i] + 1;
            continue;
        }
        if (TEQ(t, ";")) {
            bool has_paren = false, has_brace = false;
            for (size_t q = 0; q < stmt.n; q++) {
                has_paren |= TEQ(stmt.v[q], "(");
                has_brace |= TEQ(stmt.v[q], "{}");
            }
            if (stmt.n && !has_paren && !has_brace) {
                *RL_PUSH(a, &agg->members) = rl_ptext_ptrs(a, stmt.v, stmt.n);
                const RlTok *nm = decl_name(stmt.v, stmt.n);
                if (nm) {
                    size_t at = 0;
                    while (at < stmt.n && stmt.v[at] != nm) at++;
                    RlMemberDecl *md = RL_PUSH(a, &agg->member_decls);
                    md->type = rl_ptext_ptrs(a, stmt.v, at);
                    md->name = nm->t;
                    md->line = nm->line;
                }
            } else if (agg->first_hidden < 0 && hides_data_member(stmt.v, stmt.n)) {
                agg->first_hidden = (i64)agg->members.n;
            }
            stmt.n = 0;
            i++;
            continue;
        }
        *RL_PUSH(sc, &stmt) = t;
        i++;
    }
    avp_arena_reset(sc, mk);
}

/* ---- _is_object_definition, _after_decl_macro ----------------------------------------------------------------- */
static bool is_object_definition(const RlTok *const *body, size_t n, size_t par) {
    static const char *const UNARY[] = {"&", "*", "-", "!", "~", NULL};
    if (par >= 2 && TEQ(body[par - 2], "::")) return true;
    if (par + 1 >= n) return false;
    const RlTok *nxt = body[par + 1];
    return nxt->k == RL_NUM || nxt->k == RL_STR || nxt->k == RL_CHR || rl_tin(nxt, UNARY);
}

static size_t after_decl_macro_ptrs(const RlTok *const *toks, size_t paren, size_t end) {
    int depth = 0;
    size_t q = paren;
    while (q < end) {
        if (TEQ(toks[q], "(")) depth++;
        else if (TEQ(toks[q], ")")) {
            depth--;
            if (depth == 0) break;
        }
        q++;
    }
    q++;
    while (q < end) {
        if (TEQ(toks[q], "(")) return q;
        if (toks[q]->k != RL_ID) break;
        q++;
    }
    return paren;
}

static size_t after_decl_macro(const RlTok *toks, size_t paren, size_t end) {
    int depth = 0;
    size_t q = paren;
    while (q < end) {
        if (TEQ(&toks[q], "(")) depth++;
        else if (TEQ(&toks[q], ")")) {
            depth--;
            if (depth == 0) break;
        }
        q++;
    }
    q++;
    while (q < end) {
        if (TEQ(&toks[q], "(")) return q;
        if (toks[q].k != RL_ID) break;
        q++;
    }
    return paren;
}

/* ---- decl ------------------------------------------------------------------------------------------------------- */
static void decl(Ps *ps, const RlTok *h, size_t hn, int ctx, NsList ns, AvpStr cls) {
    static const char *const SKIP_FIRST[] = {"using", "friend", "template", "static_assert", NULL};
    AvpArena *a = ps->a;
    if (!hn) return;
    if (rl_tin(&h[0], SKIP_FIRST)) return;
    if (TEQ(&h[0], "typedef")) {
        AvpStr name = {NULL, 0};
        int depth = 0;
        for (size_t q = 0; q < hn; q++) {
            const RlTok *x = &h[q];
            if (TEQ(x, "(")) depth++;
            else if (TEQ(x, ")")) depth--;
            else if (x->k == RL_ID && depth <= 1 && !rl_tin(x, DECL_KEYWORDS)) name = x->t;
        }
        if (name.n) {
            RlNameLine *td = RL_PUSH(a, &ps->out->typedefs);
            td->name = name;
            td->line = h[0].line;
        }
        return;
    }
    if (rl_tin(&h[0], AGG_KW) && hn == 2 && h[1].k == RL_ID) {
        RlFwd *fw = RL_PUSH(a, &ps->out->fwds);
        fw->kind = h[0].t;
        fw->name = h[1].t;
        fw->line = h[0].line;
        return;
    }
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpArena *sc = avp_scratch();
    bool is_extern = false, is_static = false, is_const = false;
    const RlTok **body = AVP_NEWN(sc, const RlTok *, hn);
    size_t bn = 0, q = 0;
    while (q < hn) {
        const RlTok *x = &h[q];
        if (rl_tin(x, ATTR_SPECIFIERS) && q + 1 < hn && TEQ(&h[q + 1], "(")) {
            int depth = 0;
            while (q < hn) {
                if (TEQ(&h[q], "(")) depth++;
                else if (TEQ(&h[q], ")")) {
                    depth--;
                    if (depth == 0) {
                        q++;
                        break;
                    }
                }
                q++;
            }
            continue;
        }
        if (TEQ(x, "extern")) {
            is_extern = true;
            q++;
            if (q < hn && h[q].k == RL_STR) q++;
            continue;
        }
        if (x->k == RL_ID) {
            int sm = rl_storage_get(ps->P, x->t);
            if (sm != RL_STORAGE_NONE) {
                if (sm == RL_STORAGE_STATIC) is_static = true;
                else is_extern = true;
                q++;
                continue;
            }
        }
        if (TEQ(x, "static")) is_static = true;
        if (TEQ(x, "const")) is_const = true;
        body[bn++] = x;
        q++;
    }
    if (!bn) goto out;
    size_t eq = bn;
    for (size_t z = 0; z < bn; z++)
        if (TEQ(body[z], "=")) {
            eq = z;
            break;
        }
    i64 par = -1;
    int bracket = 0;
    for (size_t z = 0; z < bn; z++) {
        if (TEQ(body[z], "[")) bracket++;
        else if (TEQ(body[z], "]")) bracket--;
        else if (TEQ(body[z], "(") && bracket == 0) {
            par = (i64)z;
            break;
        }
    }
    if (par >= 1) {
        int dm = decl_macro(body[par - 1]->t);
        if (dm >= 0) {
            is_extern = is_extern || dm == RL_STORAGE_EXTERN;
            is_static = is_static || dm == RL_STORAGE_STATIC;
            par = (i64)after_decl_macro_ptrs(body, (size_t)par, bn);
        }
    }
    if (par >= 0 && (size_t)par < eq) {
        if ((size_t)par + 1 < bn && TEQ(body[par + 1], "*")) {
            const RlTok *nm = NULL;
            for (size_t z = (size_t)par + 2; z < bn; z++)
                if (body[z]->k == RL_ID) {
                    nm = body[z];
                    break;
                }
            if (nm && ctx != CTX_CLASS) {
                RlVar *v = RL_PUSH(a, &ps->out->vars);
                v->name = nm->t;
                v->type_text = avp_str("fnptr");
                v->line = body[0]->line;
                v->is_extern = is_extern;
                v->is_static = is_static;
                v->is_const = is_const;
                v->ns = join_ns(a, ns, false, avp_str(""), avp_str(""), false);
            }
            goto out;
        }
        if (par >= 1 && body[par - 1]->k == RL_ID) {
            if (ctx == CTX_CLASS) {
                bool virt = false;
                for (size_t z = 0; z < bn; z++) virt |= TEQ(body[z], "virtual");
                if (virt) {
                    RlNameLine *vl = RL_PUSH(a, &ps->out->virtuals);
                    vl->name = cat3(a, cls, "::", body[par - 1]->t);
                    vl->line = body[0]->line;
                }
                goto out;
            }
            if (is_object_definition(body, bn, (size_t)par)) goto out;
            RlProto *pr = RL_PUSH(a, &ps->out->protos);
            pr->name = body[par - 1]->t;
            pr->line = body[par - 1]->line;
            pr->is_extern = is_extern;
            int depth = 0;
            i64 end_i = -1;
            for (size_t z = (size_t)par; z < bn; z++) {
                if (TEQ(body[z], "(")) depth++;
                else if (TEQ(body[z], ")")) {
                    depth--;
                    if (depth == 0) {
                        end_i = (i64)z;
                        break;
                    }
                }
            }
            pr->empty_params = end_i >= 0 && end_i == par + 1;
        }
        goto out;
    }
    if (ctx == CTX_CLASS) goto out;
    {
        i64 name_at = -1;
        int depth = 0;
        bool addr_bound = false;
        for (size_t z = 0; z < bn; z++) {
            const RlTok *x = body[z];
            if (is_open2(x)) depth++;
            else if (is_close2(x)) depth--;
            if (depth == 0 && (TEQ(x, "=") || TEQ(x, ","))) break;
            if (depth == 0 && TEQ(x, ":")) {
                addr_bound = true;
                break;
            }
            if (depth == 0 && x->k == RL_ID && !rl_tin(x, DECL_KEYWORDS)) name_at = (i64)z;
        }
        if (name_at < 0) goto out;
        RlVar *v = RL_PUSH(a, &ps->out->vars);
        v->name = body[name_at]->t;
        bool arr = false;
        for (size_t z = (size_t)name_at; z < bn; z++) arr |= TEQ(body[z], "[");
        v->type_text = rl_ptext_ptrs(a, body, (size_t)name_at);
        if (arr) v->type_text = avp_str_concat(a, v->type_text, avp_str(" []"));
        v->line = body[name_at]->line;
        v->is_extern = is_extern;
        v->is_static = is_static;
        v->is_const = is_const;
        v->has_init = eq < bn;
        v->addr_bound = addr_bound;
        v->ns = join_ns(a, ns, false, avp_str(""), avp_str(""), false);
    }
out:
    avp_arena_reset(sc, mk);
}

/* ---- scope ------------------------------------------------------------------------------------------------------ */
static void scope(Ps *ps, size_t i, size_t end, int ctx, NsList ns, AvpStr cls) {
    static const char *const ATTR_PREV[] = {"__declspec", "__attribute__", "__attribute", "alignas", NULL};
    const RlTok *T = ps->T;
    const i32 *match = ps->match;
    AvpArena *a = ps->a;
    while (i < end) {
        const RlTok *t = &T[i];
        if (TEQ(t, ";")) {
            i++;
            continue;
        }
        if (ctx == CTX_CLASS && rl_tin(t, ACCESS_WORDS) && i + 1 < end && TEQ(&T[i + 1], ":")) {
            i += 2;
            continue;
        }
        size_t j = i;
        i64 paren_first = -1;
        bool assign = false;
        char stop = 0;
        while (j < end) {
            const RlTok *x = &T[j];
            if (x->k == RL_OP) {
                if (TEQ(x, "(") || TEQ(x, "[")) {
                    if (TEQ(x, "(") && paren_first < 0 && !assign && !(j > i && rl_tin(&T[j - 1], ATTR_PREV)))
                        paren_first = (i64)j;
                    i32 m = match[j];
                    j = m >= 0 ? (size_t)m + 1 : j + 1;
                    continue;
                }
                if (TEQ(x, "=")) {
                    assign = true;
                } else if (TEQ(x, ";") || TEQ(x, "}")) {
                    stop = x->t.p[0];
                    break;
                } else if (TEQ(x, "{")) {
                    if (assign) {
                        i32 m = match[j];
                        j = m >= 0 ? (size_t)m + 1 : j + 1;
                        continue;
                    }
                    stop = '{';
                    break;
                }
            }
            j++;
        }
        if (j >= end) {
            decl(ps, &T[i], end - i, ctx, ns, cls);
            return;
        }
        const RlTok *head = &T[i];
        size_t hn = j - i;
        if (stop == ';' && paren_first > (i64)i && ctx != CTX_CLASS) {
            size_t head_end, brace, closing;
            RlParams params;
            if (knr_definition(ps, i, (size_t)paren_first, j, end, &head_end, &brace, &closing, &params)) {
                RlFunc f = build_function(ps, i, head_end, paren_first, brace, closing, cls);
                f.params = params;
                *RL_PUSH(a, &ps->out->funcs) = f;
                i = closing + 1;
                continue;
            }
        }
        if (stop == ';') {
            decl(ps, head, hn, ctx, ns, cls);
            i = j + 1;
            continue;
        }
        if (stop == '}') {
            i = j + 1;
            continue;
        }
        if (match[j] < 0) return;
        size_t closing = (size_t)match[j];
        /* `class T` / `typename T` inside a `template < ... >` parameter list declares a parameter, not a type */
        i64 kw_idx = -1;
        {
            AvpArenaMark mk = avp_arena_mark(avp_scratch());
            bool *in_tparams = AVP_NEWN(avp_scratch(), bool, hn + 1);
            int depth = 0;
            for (size_t k = 0; k < hn; k++) {
                const RlTok *hk = &head[k];
                if (TEQ(hk, "<") && k > 0 && (TEQ(&head[k - 1], "template") || depth > 0)) depth++;
                else if (TEQ(hk, ">") && depth > 0) depth--;
                else if (depth > 0) in_tparams[k] = true;
            }
            for (size_t k = 0; k < hn; k++) {
                if (in_tparams[k]) continue;
                if (rl_tin(&head[k], AGG_KW) || TEQ(&head[k], "namespace")) {
                    if (paren_first < 0 || (i64)(i + k) < paren_first) {
                        kw_idx = (i64)k;
                        break;
                    }
                }
            }
            avp_arena_reset(avp_scratch(), mk);
        }
        if (hn == 2 && TEQ(&head[0], "extern") && head[1].k == RL_STR) {
            scope(ps, j + 1, closing, ctx, ns, cls);
            i = closing + 1;
            continue;
        }
        if (kw_idx >= 0 && TEQ(&head[kw_idx], "namespace")) {
            AvpStr name = (size_t)kw_idx + 1 < hn ? head[kw_idx + 1].t : avp_str("");
            AvpStr *nv = AVP_NEWN(a, AvpStr, ns.n + 1);
            if (ns.n) memcpy(nv, ns.v, ns.n * sizeof *nv);
            nv[ns.n] = name;
            scope(ps, j + 1, closing, CTX_NS, (NsList){nv, ns.n + 1}, avp_str(""));
            i = closing + 1;
            continue;
        }
        if (kw_idx >= 0) {
            AvpStr kind = head[kw_idx].t;
            size_t k = (size_t)kw_idx + 1;
            while (k < hn && head[k].k == RL_ID && (TEQ(&head[k], "__attribute__") || TEQ(&head[k], "__declspec")))
                k += 2;
            AvpStr name = avp_str("");
            if (k < hn && head[k].k == RL_ID && !TEQ(&head[k], "final")) name = head[k].t;
            /* trailing declarators up to ';' (a bracket group contributes its closer) */
            size_t e = closing + 1;
            RlTokPtrs tail = {0};
            AvpArenaMark mk = avp_arena_mark(avp_scratch());
            AvpArena *sc = avp_scratch();
            while (e < end && !TEQ(&T[e], ";")) {
                if ((TEQ(&T[e], "(") || TEQ(&T[e], "[") || TEQ(&T[e], "{")) && match[e] >= 0) e = (size_t)match[e];
                *RL_PUSH(sc, &tail) = &T[e];
                e++;
            }
            bool is_typedef = false;
            for (size_t z = 0; z < hn; z++) is_typedef |= TEQ(&head[z], "typedef");
            if (is_typedef && tail.n) {
                const RlTok *last_id = NULL;
                for (size_t z = 0; z < tail.n; z++)
                    if (tail.v[z]->k == RL_ID) last_id = tail.v[z];
                if (last_id) {
                    RlNameLine *td = RL_PUSH(a, &ps->out->typedefs);
                    td->name = last_id->t;
                    td->line = head[0].line;
                    if (!name.n) name = last_id->t;
                }
            }
            /* an unnamed aggregate has no qualified name */
            AvpStr qual = name.n ? join_ns(a, ns, true, cls, name, true) : avp_str("");
            RlAgg *agg = RL_PUSH(a, &ps->out->aggs);
            size_t agg_index = ps->out->aggs.n - 1;
            agg->kind = kind;
            agg->name = qual;
            agg->line = head[0].line;
            agg->keyword_line = head[0].line;
            agg->first_hidden = -1;
            agg->body_text = rl_ptext(a, &T[j + 1], closing - (j + 1));
            if (avp_str_eqz(kind, "struct") || avp_str_eqz(kind, "class") || avp_str_eqz(kind, "union")) {
                AvpStr sub_cls = name.n ? (cls.n ? cat3(a, cls, "::", name) : name) : cls;
                scope(ps, j + 1, closing, CTX_CLASS, ns, sub_cls);
                /* the aggs vector may have moved: re-fetch the record */
                members(ps, &ps->out->aggs.v[agg_index], j + 1, closing);
            }
            if (!is_typedef && tail.n && ctx != CTX_CLASS) {
                /* `struct T { ... } name;` also defines a variable */
                int depth = 0;
                const RlTok *vname = NULL;
                for (size_t z = 0; z < tail.n; z++) {
                    const RlTok *x = tail.v[z];
                    if (is_open2(x)) depth++;
                    else if (is_close2(x)) depth--;
                    else if (depth == 0 && TEQ(x, "=")) break;
                    else if (depth == 0 && x->k == RL_ID && !rl_tin(x, ATTR_SPECIFIERS)) vname = x;
                }
                if (vname) {
                    RlVar *v = RL_PUSH(a, &ps->out->vars);
                    v->name = vname->t;
                    v->type_text = avp_str_concat(a, avp_str_concat(a, kind, avp_str(" ")), qual);
                    v->line = vname->line;
                    for (size_t z = 0; z < hn; z++) {
                        v->is_extern |= TEQ(&head[z], "extern");
                        v->is_static |= TEQ(&head[z], "static");
                        v->is_const |= TEQ(&head[z], "const");
                    }
                    for (size_t z = 0; z < tail.n; z++) v->has_init |= TEQ(tail.v[z], "=");
                    v->ns = join_ns(a, ns, false, avp_str(""), avp_str(""), false);
                }
            }
            avp_arena_reset(sc, mk);
            i = e + 1;
            continue;
        }
        if (paren_first > (i64)i && decl_macro(T[paren_first - 1].t) >= 0)
            paren_first = (i64)after_decl_macro(T, (size_t)paren_first, j);
        if (paren_first >= 0) {
            RlFunc f = build_function(ps, i, j, paren_first, j, closing, cls);
            for (size_t z = 0; z < ns.n; z++) f.anon_ns |= ns.v[z].n == 0;
            *RL_PUSH(a, &ps->out->funcs) = f;
            i = closing + 1;
            if (i < end && TEQ(&T[i], ";")) i++;
            continue;
        }
        i = closing + 1;
    }
}

static void run_parser(AvpArena *a, RlParsed *P, const RlTok *T, size_t n, const i32 *match, RlParseOut *out) {
    Ps ps = {a, P, T, n, match, out};
    scope(&ps, 0, n, CTX_FILE, (NsList){NULL, 0}, avp_str(""));
}

/* ---- uses_cpp_syntax, storage macros --------------------------------------------------------------------------- */
bool rl_uses_cpp_syntax(const RlTok *toks, size_t n) {
    static const char *const W5[] = {"namespace", "virtual", "typename", "friend", "explicit", NULL};
    for (size_t i = 0; i < n; i++) {
        const RlTok *t = &toks[i];
        if (t->k == RL_OP) {
            if (TEQ(t, "::")) return true;
            continue;
        }
        if (t->k != RL_ID) continue;
        if (i && (TEQ(&toks[i - 1], ".") || TEQ(&toks[i - 1], "->"))) continue;
        const RlTok *nxt = i + 1 < n ? &toks[i + 1] : NULL;
        if (rl_tin(t, W5)) {
            if (nxt && !rl_tin(nxt, NOT_OPERATOR_NAME)) return true;
        } else if (TEQ(t, "template")) {
            if (nxt && TEQ(nxt, "<")) return true;
        } else if (TEQ(t, "class")) {
            if (nxt && nxt->k == RL_ID) return true;
        } else if (rl_tin(t, ACCESS_WORDS)) {
            if (nxt && TEQ(nxt, ":")) return true;
        } else if (TEQ(t, "operator")) {
            if (nxt && !rl_tin(nxt, NOT_OPERATOR_NAME)) return true;
        }
    }
    return false;
}

void rl_scan_storage_macros(AvpMap *m, AvpStr text) {
    const ShsmRe *re = shsm_re("^[ \\t]*#[ \\t]*define[ \\t]+([A-Za-z_]\\w*)[ \\t]+(static|extern)[ \\t]*(?:/[*/].*)?$",
                               SHSM_RE_M);
    ShsmReIter it;
    ShsmReMatch mm;
    shsm_re_iter_init(&it, re, text, 0, AVP_END);
    while (shsm_re_iter_next(&it, &mm)) {
        AvpStr word = shsm_re_group(&mm, 2);
        avp_map_put(m, shsm_re_group(&mm, 1), NULL)->i = avp_str_eqz(word, "static") ? RL_STORAGE_STATIC
                                                                                      : RL_STORAGE_EXTERN;
    }
}

int rl_storage_get(const RlParsed *P, AvpStr name) {
    AvpVal *v = P->storage_own ? avp_map_get(P->storage_own, name) : NULL;
    if (!v && P->storage_global) v = avp_map_get(P->storage_global, name);
    return v ? (int)v->i : RL_STORAGE_NONE;
}

/* ---- Parsed ------------------------------------------------------------------------------------------------------ */
static bool ends_with_ci(AvpStr s, const char *const *exts) {
    for (; *exts; exts++) {
        size_t l = strlen(*exts);
        if (s.n < l) continue;
        bool eq = true;
        for (size_t q = 0; q < l && eq; q++) {
            char c = s.p[s.n - l + q];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            eq = c == (*exts)[q];
        }
        if (eq) return true;
    }
    return false;
}

static AvpStr site_kind_token(AvpArena *a, AvpStr word) { return avp_str_concat(a, avp_str("token:"), word); }

static const RlFunc *enclosing_func(const RlParseOut *probe, size_t i) {
    const RlFunc *enc = NULL;
    for (size_t q = 0; q < probe->funcs.n; q++) {
        const RlFunc *f = &probe->funcs.v[q];
        if ((i64)f->body < (i64)i && (i64)i < (i64)f->body_end) enc = f;
    }
    return enc;
}

static void add_site(RlParsed *P, i32 line, AvpStr name, AvpStr kind) {
    RlAsmSite *s = RL_PUSH(P->a, &P->asm_sites);
    s->line = line;
    s->name = name;
    s->kind = kind;
}

static void add_mask(RlParsed *P, i64 lo, i64 hi) {
    RlMask *m = RL_PUSH(P->a, &P->masks);
    m->lo = (i32)lo;
    m->hi = (i32)hi;
}

static void find_asm_sites(RlParsed *P, const RlParseOut *probe, const RlTok *T, size_t n, const i32 *match) {
    static const char *const ASM_WORDS[] = {"asm", "__asm", "__asm__", NULL};
    static const char *const VOL_WORDS[] = {"volatile", "__volatile__", NULL};
    static const char *const EMIT_WORDS[] = {"INCLUDE_ASM", "__emit", "__emit__", NULL};
    for (size_t q = 0; q < probe->funcs.n; q++) {
        const RlFunc *f = &probe->funcs.v[q];
        if (!f->is_asm) continue;
        i32 line = f->line;
        for (i32 k = f->head; k < f->body; k++)
            if (rl_tin(&T[k], ASM_WORDS)) {
                line = T[k].line;
                break;
            }
        add_site(P, line, f->name, avp_str("function"));
        add_mask(P, (i64)f->body + 1, f->body_end);
    }
    for (size_t i = 0; i < n; i++) {
        const RlTok *t = &T[i];
        if (t->k == RL_ID && rl_tin(t, ASM_WORDS)) {
            size_t j = i + 1;
            while (j < n && rl_tin(&T[j], VOL_WORDS)) j++;
            if (j < n && TEQ(&T[j], "(") && match[j] >= 0) {
                const RlFunc *enc = enclosing_func(probe, i);
                if (enc && enc->is_asm) continue;
                add_site(P, t->line, enc ? enc->name : avp_str(""), avp_str("block"));
                continue;
            }
            if (j < n && TEQ(&T[j], "{") && match[j] >= 0) {
                const RlFunc *enc = enclosing_func(probe, i);
                if (enc && enc->is_asm) continue;
                bool is_body = false;
                for (size_t q = 0; q < probe->funcs.n; q++) is_body |= probe->funcs.v[q].body == (i32)j;
                if (is_body) continue; /* the body of an asm function, already recorded */
                add_site(P, t->line, enc ? enc->name : avp_str(""), avp_str("block"));
                add_mask(P, (i64)j + 1, match[j]);
            }
        } else if (t->k == RL_ID && rl_tin(t, EMIT_WORDS)) {
            const RlFunc *enc = enclosing_func(probe, i);
            add_site(P, t->line, enc ? enc->name : avp_str(""), site_kind_token(P->a, t->t));
        }
    }
}

static int cmp_mask(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const RlMask *a = x, *b = y;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    return 0;
}

static RlToks mask_asm(RlParsed *P, RlToks toks) {
    if (!P->masks.n) return toks;
    AvpArena *a = P->a;
    RlMask *masks = AVP_NEWN(a, RlMask, P->masks.n);
    memcpy(masks, P->masks.v, P->masks.n * sizeof *masks);
    avp_sort(masks, P->masks.n, sizeof *masks, cmp_mask, NULL, false);
    size_t nm = P->masks.n;
    RlToks out = {0};
    size_t i = 0, mi = 0;
    while (i < toks.n) {
        if (mi < nm && (i64)i == masks[mi].lo) {
            i32 line = toks.v[i].line;
            size_t hi = (size_t)masks[mi].hi > i ? (size_t)masks[mi].hi : i;
            RlAsmIds *ai = RL_PUSH(a, &P->asm_ids);
            ai->tok = (i32)out.n;
            AvpSet *ids = avp_set_new();
            for (size_t q = i; q < hi; q++)
                if (toks.v[q].k == RL_ID) avp_set_add(ids, toks.v[q].t);
            ai->ids = avp_set_sorted(ids);
            /* the sorted views point into the set's own keys, which die with it */
            for (size_t z = 0; z < ai->ids.n; z++) ai->ids.v[z] = avp_str_dup(a, ai->ids.v[z]);
            avp_set_free(ids);
            RlTok *b = RL_PUSH(a, &out);
            b->k = RL_ID;
            b->t = avp_str("__ASM_BODY__");
            b->line = line;
            RlTok *s = RL_PUSH(a, &out);
            s->k = RL_OP;
            s->t = avp_str(";");
            s->line = line;
            i = hi;
            mi++;
            while (mi < nm && (size_t)masks[mi].lo < i) mi++;
            continue;
        }
        *RL_PUSH(a, &out) = toks.v[i];
        i++;
    }
    return out;
}

static AvpStr last_scope_part(AvpStr name) {
    /* name.split("::")[-1]: the text after the last separator of a left-to-right split */
    size_t start = 0, q = 0;
    while (q + 1 < name.n) {
        if (name.p[q] == ':' && name.p[q + 1] == ':') {
            start = q + 2;
            q += 2;
        } else {
            q++;
        }
    }
    return avp_strn(name.p + start, name.n - start);
}

RlParsed *rl_parse(AvpArena *a, AvpStr path, AvpStr text, const AvpMap *storage_macros) {
    static const char *const HDR_EXTS[] = {".h", ".hh", ".hpp", ".hxx", ".inc", NULL};
    static const char *const CPP_EXTS[] = {".cc", ".cp", ".cpp", ".cxx", ".hh", ".hpp", ".hxx", NULL};
    RlParsed *P = AVP_NEW(a, RlParsed);
    P->a = a;
    P->path = path;
    P->text = text;
    P->storage_global = storage_macros;
    P->storage_own = avp_map_new();
    shsm_re_assume_valid(text);
    rl_scan_storage_macros(P->storage_own, text);
    P->is_header = ends_with_ci(path, HDR_EXTS);
    P->is_cpp = ends_with_ci(path, CPP_EXTS);
    P->lines = avp_str_split(text, avp_str("\n"), -1);
    P->cpp_syntax_memo = -1;
    RlLexed lx = rl_lex(a, text, 1, true);
    P->ok = lx.ok;
    P->error_line = lx.error_line;
    if (!lx.ok) return P;
    P->comments = lx.comments;
    P->directives = lx.directives;
    /* first pass finds asm bodies; second pass parses with them masked */
    i32 *raw_match = rl_pair_brackets(a, lx.toks.v, lx.toks.n);
    RlParseOut probe = {0};
    run_parser(a, P, lx.toks.v, lx.toks.n, raw_match, &probe);
    find_asm_sites(P, &probe, lx.toks.v, lx.toks.n, raw_match);
    P->toks = mask_asm(P, lx.toks);
    P->match = rl_pair_brackets(a, P->toks.v, P->toks.n);
    run_parser(a, P, P->toks.v, P->toks.n, P->match, &P->p);
    /* _comments_by_line */
    i32 maxl = 0;
    for (size_t q = 0; q < P->comments.n; q++)
        if (P->comments.v[q].end_line > maxl) maxl = P->comments.v[q].end_line;
    P->cbl_lines = maxl + 1;
    P->cbl_start = AVP_NEWN(a, i32, (size_t)P->cbl_lines + 1);
    for (size_t q = 0; q < P->comments.n; q++)
        for (i32 ln = P->comments.v[q].line; ln <= P->comments.v[q].end_line; ln++)
            if (ln >= 0) P->cbl_start[ln + 1]++;
    for (i32 ln = 0; ln < P->cbl_lines; ln++) P->cbl_start[ln + 1] += P->cbl_start[ln];
    P->cbl_idx = AVP_NEWN(a, i32, (size_t)P->cbl_start[P->cbl_lines] + 1);
    {
        AvpArenaMark mk = avp_arena_mark(avp_scratch());
        i32 *fill = AVP_NEWN(avp_scratch(), i32, (size_t)P->cbl_lines + 1);
        memcpy(fill, P->cbl_start, ((size_t)P->cbl_lines + 1) * sizeof(i32));
        for (size_t q = 0; q < P->comments.n; q++)
            for (i32 ln = P->comments.v[q].line; ln <= P->comments.v[q].end_line; ln++)
                if (ln >= 0) P->cbl_idx[fill[ln]++] = (i32)q;
        avp_arena_reset(avp_scratch(), mk);
    }
    P->func_names = avp_set_new();
    for (size_t q = 0; q < P->p.funcs.n; q++) avp_set_add(P->func_names, last_scope_part(P->p.funcs.v[q].name));
    const ShsmRe *inc = shsm_re("\\s*[<\"]([^>\"]+)[>\"]", 0);
    for (size_t q = 0; q < P->directives.n; q++) {
        const RlDirective *d = &P->directives.v[q];
        ShsmReMatch m;
        if (avp_str_eqz(d->name, "include") && shsm_re_match(inc, d->rest, 0, AVP_END, &m))
            avp_vec_push(&P->includes, shsm_re_group(&m, 1));
    }
    return P;
}

void rl_parsed_free(RlParsed *P) {
    if (!P) return;
    avp_map_free(P->storage_own);
    if (P->func_names) avp_set_free(P->func_names);
    if (P->offset_macros_memo) avp_set_free(P->offset_macros_memo);
    if (P->volatile_noted_memo) avp_set_free(P->volatile_noted_memo);
    for (size_t q = 0; q < P->asm_ids.n; q++) avp_vec_free(&P->asm_ids.v[q].ids);
    avp_vec_free(&P->lines);
    avp_vec_free(&P->includes);
    memset(P, 0, sizeof *P);
}

/* ---- derived facts and helpers ------------------------------------------------------------------------------------ */
bool rl_cpp_syntax(RlParsed *P) {
    if (P->cpp_syntax_memo < 0) P->cpp_syntax_memo = rl_uses_cpp_syntax(P->toks.v, P->toks.n);
    return P->cpp_syntax_memo != 0;
}

AvpSet *rl_offset_macros(RlParsed *P) {
    if (P->offset_macros_memo) return P->offset_macros_memo;
    AvpSet *v = avp_set_new();
    const ShsmRe *off = shsm_re("(?i)(?:^|_)(?:offset|ofs)(?:_|$)", 0);
    const ShsmRe *hdr = shsm_re("(?i)header|hdr", 0);
    for (size_t q = 0; q < P->directives.n; q++) {
        const RlDirective *d = &P->directives.v[q];
        if (!avp_str_eqz(d->name, "define") || !d->toks.n || d->toks.v[0].k != RL_ID) continue;
        const RlTok *body = d->toks.v + 1;
        size_t bn = d->toks.n - 1;
        if (bn >= 3 && TEQ(&body[0], "(") && TEQ(&body[bn - 1], ")")) {
            body++;
            bn -= 2;
        }
        AvpStr nm = d->toks.v[0].t;
        if (bn == 1 && body[0].k == RL_NUM && !TEQ(&d->toks.v[1], "(") && shsm_re_search(off, nm, 0, AVP_END, NULL) &&
            !shsm_re_search(hdr, nm, 0, AVP_END, NULL))
            avp_set_add(v, nm);
    }
    P->offset_macros_memo = v;
    return v;
}

static bool volatile_type(AvpStr s) {
    static const char *const W[] = {"vu8", "vu16", "vu32", "vu64", "vs8", "vs16", "vs32", "vs64", "vf32", "vf64", NULL};
    return rl_sin(s, W);
}

AvpStr rl_volatile_decl_name(const RlTok *toks, size_t n, size_t i) {
    static const char *const STOP[] = {";", "=", "{", "[", ":", ",", ")", "}", NULL};
    const RlTok *last = NULL;
    for (size_t j = i; j < n; j++) {
        const RlTok *x = &toks[j];
        if (rl_tin(x, STOP)) break;
        if (x->k == RL_ID) last = x;
        else if (TEQ(x, "*")) last = NULL;
    }
    if (last && !TEQ(last, "volatile") && !TEQ(last, "const") && !volatile_type(last->t)) return last->t;
    return (AvpStr){NULL, 0};
}

const ShsmRe *rl_re_volatile_note(void) { return shsm_re("volatile:\\s*\\S", 0); }
const ShsmRe *rl_re_delay_note(void) { return shsm_re("volatile:\\s*delay loop,\\s*\\S", 0); }
const ShsmRe *rl_re_assert_note(void) {
    return shsm_re("volatile:\\s*read by a release-disabled assertion,\\s*\\S", 0);
}
const ShsmRe *rl_re_byteptr_note(void) { return shsm_re("byteptr:\\s*\\S", 0); }
const ShsmRe *rl_re_pad_note(void) { return shsm_re("pad(?:ding)?:\\s*\\S", 0); }
const ShsmRe *rl_re_exception(void) { return shsm_re("RULE-EXCEPTION", 0); }

AvpSet *rl_volatile_noted(RlParsed *P) {
    if (P->volatile_noted_memo) return P->volatile_noted_memo;
    AvpSet *v = avp_set_new();
    for (size_t i = 0; i < P->toks.n; i++) {
        const RlTok *t = &P->toks.v[i];
        if (t->k == RL_ID && (TEQ(t, "volatile") || volatile_type(t->t))) {
            AvpStr nm = rl_volatile_decl_name(P->toks.v, P->toks.n, i);
            if (nm.p && nm.n && rl_has_note(P, t->line, rl_re_volatile_note(), true)) avp_set_add(v, nm);
        }
    }
    P->volatile_noted_memo = v;
    return v;
}

const i32 *rl_comments_on(const RlParsed *P, i32 line, size_t *count) {
    if (line < 0 || line >= P->cbl_lines || !P->cbl_start) {
        *count = 0;
        return NULL;
    }
    *count = (size_t)(P->cbl_start[line + 1] - P->cbl_start[line]);
    return P->cbl_idx + P->cbl_start[line];
}

bool rl_has_note(const RlParsed *P, i32 line, const ShsmRe *re, bool above) {
    size_t n;
    const i32 *ix = rl_comments_on(P, line, &n);
    for (size_t q = 0; q < n; q++)
        if (shsm_re_search(re, P->comments.v[ix[q]].text, 0, AVP_END, NULL)) return true;
    if (above) {
        ix = rl_comments_on(P, line - 1, &n);
        for (size_t q = 0; q < n; q++) {
            const RlComment *c = &P->comments.v[ix[q]];
            if (!c->trailing && shsm_re_search(re, c->text, 0, AVP_END, NULL)) return true;
        }
    }
    return false;
}

i64 rl_func_at(const RlParsed *P, i32 line) {
    i64 best = -1;
    for (size_t q = 0; q < P->p.funcs.n; q++) {
        const RlFunc *f = &P->p.funcs.v[q];
        if (f->line <= line && line <= f->end_line)
            if (best < 0 || f->line >= P->p.funcs.v[best].line) best = (i64)q;
    }
    return best;
}

bool rl_no_copy_in_retail(const RlParsed *P, const RlFunc *fn) {
    const ShsmRe *re = shsm_re("no copy in retail(?:\\s*\\(dead-stripped(?: or inlined)?\\))?\\s*$", 0);
    size_t n;
    const i32 *ix = rl_comments_on(P, fn->end_line, &n);
    for (size_t q = 0; q < n; q++) {
        const RlComment *c = &P->comments.v[ix[q]];
        if (c->trailing && shsm_re_search(re, avp_str_strip(c->text), 0, AVP_END, NULL)) return true;
    }
    return false;
}
