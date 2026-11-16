/* configure.exe: the parser of the configure.py subset (cfg.h). Recursive descent over Python's grammar; a form
 * outside the subset is an error naming its line. */
#include <string.h>

#include "cfg.h"

typedef struct {
    AvpArena *a;
    CfgToks t;
    size_t i;
} P;

static CfgTok *peek(P *p) { return &p->t.v[p->i]; }
static CfgTok *peek2(P *p) { return &p->t.v[p->i + 1 < p->t.n ? p->i + 1 : p->i]; }
static CfgTok *next(P *p) {
    CfgTok *t = &p->t.v[p->i];
    if (t->kind != CFG_T_EOF) p->i++;
    return t;
}
static bool is_op(const CfgTok *t, const char *op) { return t->kind == CFG_T_OP && avp_str_eqz(t->text, op); }
static bool is_kw(const CfgTok *t, const char *kw) { return t->kind == CFG_T_NAME && avp_str_eqz(t->text, kw); }
static bool accept_op(P *p, const char *op) {
    if (!is_op(peek(p), op)) return false;
    next(p);
    return true;
}
static bool accept_kw(P *p, const char *kw) {
    if (!is_kw(peek(p), kw)) return false;
    next(p);
    return true;
}

static const char *tok_desc(AvpArena *a, const CfgTok *t) {
    switch (t->kind) {
    case CFG_T_EOF: return "end of file";
    case CFG_T_NEWLINE: return "end of line";
    case CFG_T_INDENT: return "indent";
    case CFG_T_DEDENT: return "dedent";
    case CFG_T_STR: return "string";
    default: return avp_sprintf(a, "'%.*s'", AVP_SF(t->text));
    }
}

static void expect_op(P *p, const char *op) {
    if (!accept_op(p, op)) cfg_fail(peek(p)->line, "expected '%s', found %s", op, tok_desc(p->a, peek(p)));
}

static AvpStr expect_name(P *p) {
    CfgTok *t = peek(p);
    if (t->kind != CFG_T_NAME) cfg_fail(t->line, "expected a name, found %s", tok_desc(p->a, t));
    next(p);
    return t->text;
}

static const char *const k_keywords[] = {"False", "None", "True", "and", "as", "assert", "async", "await", "break",
    "class", "continue", "def", "del", "elif", "else", "except", "finally", "for", "from", "global", "if", "import",
    "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", NULL};

static bool is_keyword(AvpStr s) {
    for (const char *const *k = k_keywords; *k; k++)
        if (avp_str_eqz(s, *k)) return true;
    return false;
}

static CfgNode *node(P *p, CfgKind kind, int line) {
    CfgNode *n = AVP_NEW(p->a, CfgNode);
    n->kind = kind;
    n->line = line;
    return n;
}

static CfgNode *parse_test(P *p);
static CfgNode *parse_or(P *p);
static CfgNode *parse_arith(P *p);
static CfgNode *parse_factor(P *p);
static CfgNode *parse_testlist(P *p, bool allow_star);
static void parse_suite(P *p, CfgNodes *body);

/* ---- f-strings ---- */

static CfgNode *parse_fstring_into(P *p, CfgNode *f, const CfgTok *t) {
    AvpStr s = t->text;
    AvpBuf lit;
    avp_buf_init(&lit, p->a);
    size_t i = 0;
    int line = t->line;
#define FLUSH()                                                                                   \
    do {                                                                                          \
        if (lit.n) {                                                                              \
            AvpStr v = avp_buf_str(&lit);                                                         \
            CfgNode *k = node(p, CFG_E_CONST, line);                                              \
            k->value = pv_str(p->a, t->raw ? v : cfg_unescape(p->a, v, line));                   \
            avp_vec_push(&f->items, k);                                                           \
            avp_buf_init(&lit, p->a);                                                             \
        }                                                                                         \
    } while (0)
    while (i < s.n) {
        char c = s.p[i];
        if (c == '{' && i + 1 < s.n && s.p[i + 1] == '{') {
            avp_buf_putc(&lit, '{');
            i += 2;
            continue;
        }
        if (c == '}' && i + 1 < s.n && s.p[i + 1] == '}') {
            avp_buf_putc(&lit, '}');
            i += 2;
            continue;
        }
        if (c == '}') cfg_fail(line, "f-string: single '}' is not allowed");
        if (c == '\\' && !t->raw && i + 1 < s.n) { /* an escape stays in the literal (decoded at FLUSH) */
            avp_buf_putc(&lit, c);
            avp_buf_putc(&lit, s.p[i + 1]);
            i += 2;
            continue;
        }
        if (c != '{') {
            avp_buf_putc(&lit, c);
            i++;
            continue;
        }
        FLUSH();
        /* a field: the expression ends at the top-level '}', '!' or ':' */
        size_t j = i + 1;
        int depth = 0;
        char quote = 0;
        for (; j < s.n; j++) {
            char d = s.p[j];
            if (quote) {
                if (d == quote) quote = 0;
                continue;
            }
            if (d == '\'' || d == '"') quote = d;
            else if (d == '(' || d == '[' || d == '{') depth++;
            else if ((d == ')' || d == ']' || d == '}') && depth > 0) depth--;
            else if (depth == 0 && (d == '}' || d == ':' || (d == '!' && !(j + 1 < s.n && s.p[j + 1] == '=')))) break;
        }
        if (j >= s.n) cfg_fail(line, "f-string: expecting '}'");
        AvpStr expr = avp_strn(s.p + i + 1, j - i - 1);
        int conv = 0;
        if (s.p[j] == '!') {
            if (j + 1 < s.n && (s.p[j + 1] == 'r' || s.p[j + 1] == 's')) conv = s.p[j + 1];
            else cfg_fail(line, "f-string conversions other than !r and !s are not supported");
            j += 2;
        }
        if (j < s.n && s.p[j] == ':') cfg_fail(line, "f-string format specs are not supported");
        if (j >= s.n || s.p[j] != '}') cfg_fail(line, "f-string: expecting '}'");
        AvpStr trimmed = avp_str_strip(expr);
        if (trimmed.n && trimmed.p[trimmed.n - 1] == '=') cfg_fail(line, "f-string '=' fields are not supported");
        P sub = {p->a, cfg_lex(p->a, expr, line, true), 0};
        if (sub.t.v[0].kind == CFG_T_EOF) cfg_fail(line, "f-string: valid expression required before '}'");
        CfgNode *field = node(p, CFG_E_FIELD, line);
        field->a = parse_test(&sub);
        if (accept_op(&sub, ",")) cfg_fail(line, "tuples in f-string fields are not supported");
        if (peek(&sub)->kind != CFG_T_EOF) cfg_fail(line, "f-string: unexpected %s", tok_desc(p->a, peek(&sub)));
        field->op = conv;
        avp_vec_push(&f->items, field);
        i = j + 1;
    }
    FLUSH();
#undef FLUSH
    return f;
}

/* adjacent string literals concatenate; with any f-string among them the whole is an f-string */
static CfgNode *parse_strings(P *p) {
    int line = peek(p)->line;
    size_t first = p->i;
    bool any_f = false;
    while (peek(p)->kind == CFG_T_STR) {
        any_f |= next(p)->fstr;
    }
    if (!any_f) {
        AvpBuf b;
        avp_buf_init(&b, p->a);
        for (size_t k = first; k < p->i; k++) avp_buf_put(&b, p->t.v[k].text);
        CfgNode *c = node(p, CFG_E_CONST, line);
        c->value = pv_str(p->a, avp_buf_str(&b));
        return c;
    }
    CfgNode *f = node(p, CFG_E_FSTR, line);
    for (size_t k = first; k < p->i; k++) {
        CfgTok *t = &p->t.v[k];
        if (t->fstr) {
            parse_fstring_into(p, f, t);
        } else {
            CfgNode *c = node(p, CFG_E_CONST, t->line);
            c->value = pv_str(p->a, t->text);
            avp_vec_push(&f->items, c);
        }
    }
    return f;
}

/* ---- expressions ---- */

static CfgNode *parse_star_or_test(P *p) {
    CfgTok *t = peek(p);
    if (is_op(t, "*")) {
        next(p);
        CfgNode *s = node(p, CFG_E_STARRED, t->line);
        s->a = parse_or(p);
        return s;
    }
    return parse_test(p);
}

static void no_comprehension(P *p) {
    if (is_kw(peek(p), "for") || is_kw(peek(p), "async")) cfg_fail(peek(p)->line, "comprehensions are not supported");
}

static CfgNode *parse_atom(P *p) {
    CfgTok *t = peek(p);
    switch (t->kind) {
    case CFG_T_STR: return parse_strings(p);
    case CFG_T_INT: {
        next(p);
        CfgNode *c = node(p, CFG_E_CONST, t->line);
        c->value = pv_int(p->a, t->ival);
        return c;
    }
    case CFG_T_NAME: {
        next(p);
        CfgNode *c = node(p, CFG_E_CONST, t->line);
        if (avp_str_eqz(t->text, "None")) c->value = pv_none();
        else if (avp_str_eqz(t->text, "True")) c->value = pv_bool(true);
        else if (avp_str_eqz(t->text, "False")) c->value = pv_bool(false);
        else if (is_keyword(t->text)) cfg_fail(t->line, "unexpected keyword '%.*s'", AVP_SF(t->text));
        else {
            c->kind = CFG_E_NAME;
            c->name = t->text;
        }
        return c;
    }
    case CFG_T_OP:
        if (is_op(t, "(")) {
            next(p);
            CfgNode *tup = node(p, CFG_E_TUPLE, t->line);
            if (accept_op(p, ")")) return tup;
            CfgNode *first = parse_star_or_test(p);
            no_comprehension(p);
            if (accept_op(p, ")")) {
                if (first->kind == CFG_E_STARRED) cfg_fail(t->line, "cannot use starred expression here");
                return first; /* a parenthesised expression */
            }
            avp_vec_push(&tup->items, first);
            while (accept_op(p, ",")) {
                if (is_op(peek(p), ")")) break;
                avp_vec_push(&tup->items, parse_star_or_test(p));
            }
            expect_op(p, ")");
            return tup;
        }
        if (is_op(t, "[")) {
            next(p);
            CfgNode *l = node(p, CFG_E_LIST, t->line);
            if (accept_op(p, "]")) return l;
            avp_vec_push(&l->items, parse_star_or_test(p));
            no_comprehension(p);
            while (accept_op(p, ",")) {
                if (is_op(peek(p), "]")) break;
                avp_vec_push(&l->items, parse_star_or_test(p));
            }
            expect_op(p, "]");
            return l;
        }
        if (is_op(t, "{")) {
            next(p);
            CfgNode *d = node(p, CFG_E_DICT, t->line);
            if (accept_op(p, "}")) return d;
            for (;;) {
                if (is_op(peek(p), "**")) cfg_fail(peek(p)->line, "dict unpacking (**) is not supported");
                CfgNode *k = parse_test(p);
                if (!is_op(peek(p), ":")) cfg_fail(peek(p)->line, "set displays are not supported");
                next(p);
                CfgNode *v = parse_test(p);
                no_comprehension(p);
                avp_vec_push(&d->items, k);
                avp_vec_push(&d->items, v);
                if (!accept_op(p, ",")) break;
                if (is_op(peek(p), "}")) break;
            }
            expect_op(p, "}");
            return d;
        }
        break;
    default: break;
    }
    cfg_fail(t->line, "unexpected %s", tok_desc(p->a, t));
}

static CfgNode *parse_primary(P *p) {
    CfgNode *e = parse_atom(p);
    for (;;) {
        CfgTok *t = peek(p);
        if (is_op(t, ".")) {
            next(p);
            CfgNode *at = node(p, CFG_E_ATTR, t->line);
            at->a = e;
            at->name = expect_name(p);
            e = at;
        } else if (is_op(t, "(")) {
            next(p);
            CfgNode *call = node(p, CFG_E_CALL, t->line);
            call->line = e->line;
            call->a = e;
            bool seen_kw = false;
            while (!is_op(peek(p), ")")) {
                CfgTok *u = peek(p);
                CfgNode *arg;
                if (is_op(u, "**")) cfg_fail(u->line, "keyword-argument unpacking (**) is not supported");
                if (u->kind == CFG_T_NAME && is_op(peek2(p), "=")) {
                    next(p);
                    next(p);
                    arg = node(p, CFG_E_KEYWORD, u->line);
                    arg->name = u->text;
                    arg->a = parse_test(p);
                    seen_kw = true;
                } else {
                    arg = parse_star_or_test(p);
                    if (seen_kw && arg->kind != CFG_E_STARRED)
                        cfg_fail(u->line, "positional argument follows keyword argument");
                    no_comprehension(p);
                }
                avp_vec_push(&call->items, arg);
                if (!accept_op(p, ",")) break;
            }
            expect_op(p, ")");
            e = call;
        } else if (is_op(t, "[")) {
            next(p);
            CfgNode *s = node(p, CFG_E_SUBSCR, t->line);
            s->a = e;
            s->b = parse_testlist(p, false); /* a tuple subscript (Dict[str, Any]) parses; it fails if evaluated */
            if (is_op(peek(p), ":")) cfg_fail(t->line, "slices are not supported");
            expect_op(p, "]");
            e = s;
        } else {
            return e;
        }
    }
}

static CfgNode *parse_factor(P *p) {
    CfgTok *t = peek(p);
    if (is_op(t, "-") || is_op(t, "+")) {
        next(p);
        CfgNode *u = node(p, CFG_E_UNARY, t->line);
        u->op = t->text.p[0];
        u->a = parse_factor(p);
        return u;
    }
    if (is_op(t, "~")) cfg_fail(t->line, "operator '~' is not supported");
    CfgNode *e = parse_primary(p);
    if (is_op(peek(p), "**")) cfg_fail(peek(p)->line, "operator '**' is not supported");
    return e;
}

static CfgNode *parse_term(P *p) {
    CfgNode *e = parse_factor(p);
    for (;;) {
        CfgTok *t = peek(p);
        if (is_op(t, "*") || is_op(t, "/") || is_op(t, "%")) {
            next(p);
            CfgNode *b = node(p, CFG_E_BINOP, t->line);
            b->op = t->text.p[0];
            b->a = e;
            b->b = parse_factor(p);
            e = b;
        } else if (is_op(t, "//") || is_op(t, "@")) {
            cfg_fail(t->line, "operator '%.*s' is not supported", AVP_SF(t->text));
        } else {
            return e;
        }
    }
}

static CfgNode *parse_arith(P *p) {
    CfgNode *e = parse_term(p);
    for (;;) {
        CfgTok *t = peek(p);
        if (is_op(t, "+") || is_op(t, "-")) {
            next(p);
            CfgNode *b = node(p, CFG_E_BINOP, t->line);
            b->op = t->text.p[0];
            b->a = e;
            b->b = parse_term(p);
            e = b;
        } else if (is_op(t, "<<") || is_op(t, ">>") || is_op(t, "&") || is_op(t, "|") || is_op(t, "^")) {
            cfg_fail(t->line, "operator '%.*s' is not supported", AVP_SF(t->text));
        } else {
            return e;
        }
    }
}

static int comp_op(P *p) {
    CfgTok *t = peek(p);
    static const struct { const char *s; int op; } ops[] = {{"==", CFG_CMP_EQ}, {"!=", CFG_CMP_NE}, {"<", CFG_CMP_LT},
        {"<=", CFG_CMP_LE}, {">", CFG_CMP_GT}, {">=", CFG_CMP_GE}};
    for (size_t k = 0; k < sizeof ops / sizeof ops[0]; k++)
        if (is_op(t, ops[k].s)) {
            next(p);
            return ops[k].op;
        }
    if (is_kw(t, "in")) {
        next(p);
        return CFG_CMP_IN;
    }
    if (is_kw(t, "not") && is_kw(peek2(p), "in")) {
        next(p);
        next(p);
        return CFG_CMP_NOTIN;
    }
    if (is_kw(t, "is")) {
        next(p);
        return accept_kw(p, "not") ? CFG_CMP_ISNOT : CFG_CMP_IS;
    }
    return 0;
}

static CfgNode *parse_comparison(P *p) {
    CfgNode *e = parse_arith(p);
    int op = comp_op(p);
    if (!op) return e;
    CfgNode *c = node(p, CFG_E_COMPARE, e->line);
    c->a = e;
    while (op) {
        avp_vec_push(&c->ops, op);
        avp_vec_push(&c->items, parse_arith(p));
        op = comp_op(p);
    }
    return c;
}

static CfgNode *parse_not(P *p) {
    CfgTok *t = peek(p);
    if (is_kw(t, "not")) {
        next(p);
        CfgNode *u = node(p, CFG_E_UNARY, t->line);
        u->op = 'n';
        u->a = parse_not(p);
        return u;
    }
    return parse_comparison(p);
}

static CfgNode *parse_boolop(P *p, const char *kw, int op, CfgNode *(*sub)(P *)) {
    CfgNode *e = sub(p);
    if (!is_kw(peek(p), kw)) return e;
    CfgNode *b = node(p, CFG_E_BOOLOP, e->line);
    b->op = op;
    avp_vec_push(&b->items, e);
    while (accept_kw(p, kw)) avp_vec_push(&b->items, sub(p));
    return b;
}

static CfgNode *parse_and(P *p) { return parse_boolop(p, "and", 'a', parse_not); }
static CfgNode *parse_or(P *p) { return parse_boolop(p, "or", 'o', parse_and); }

static CfgNode *parse_test(P *p) {
    CfgTok *t = peek(p);
    if (is_kw(t, "lambda")) cfg_fail(t->line, "lambda is not supported");
    CfgNode *e = parse_or(p);
    if (is_kw(peek(p), "if")) {
        next(p);
        CfgNode *c = node(p, CFG_E_IFEXP, e->line);
        c->a = e;
        c->b = parse_or(p);
        if (!accept_kw(p, "else")) cfg_fail(peek(p)->line, "expected 'else' after 'if' expression");
        c->c = parse_test(p);
        return c;
    }
    if (is_op(peek(p), ":=")) cfg_fail(peek(p)->line, "assignment expressions (:=) are not supported");
    return e;
}

/* testlist_star_expr: a tuple when there is a comma */
static CfgNode *parse_testlist(P *p, bool allow_star) {
    int line = peek(p)->line;
    CfgNode *first = allow_star ? parse_star_or_test(p) : parse_test(p);
    if (!is_op(peek(p), ",")) return first;
    CfgNode *tup = node(p, CFG_E_TUPLE, line);
    avp_vec_push(&tup->items, first);
    while (accept_op(p, ",")) {
        CfgTok *t = peek(p);
        if (t->kind == CFG_T_NEWLINE || is_op(t, "=") || is_op(t, ":") || is_op(t, ";") || t->kind == CFG_T_EOF) break;
        avp_vec_push(&tup->items, allow_star ? parse_star_or_test(p) : parse_test(p));
    }
    return tup;
}

/* ---- statements ---- */

static void check_target(CfgNode *t) {
    switch (t->kind) {
    case CFG_E_NAME:
    case CFG_E_ATTR:
    case CFG_E_SUBSCR: return;
    case CFG_E_TUPLE:
    case CFG_E_LIST:
        for (size_t i = 0; i < t->items.n; i++) {
            if (t->items.v[i]->kind == CFG_E_STARRED) cfg_fail(t->line, "starred assignment targets are not supported");
            check_target(t->items.v[i]);
        }
        return;
    default: cfg_fail(t->line, "cannot assign to this expression");
    }
}

static AvpStr parse_dotted(P *p) {
    CfgTok *t = peek(p);
    AvpStr first = expect_name(p);
    const char *start = first.p;
    const char *end = first.p + first.n;
    while (is_op(peek(p), ".")) {
        next(p);
        AvpStr part = expect_name(p);
        end = part.p + part.n;
    }
    (void)t;
    /* the dotted name as written without spaces */
    AvpBuf b;
    avp_buf_init(&b, p->a);
    for (const char *c = start; c < end; c++)
        if (*c != ' ' && *c != '\t') avp_buf_putc(&b, *c);
    return avp_buf_str(&b);
}

static CfgNode *parse_small(P *p) {
    CfgTok *t = peek(p);
    int line = t->line;
    if (t->kind == CFG_T_NAME) {
        if (accept_kw(p, "pass")) return node(p, CFG_S_PASS, line);
        if (accept_kw(p, "break")) return node(p, CFG_S_BREAK, line);
        if (accept_kw(p, "continue")) return node(p, CFG_S_CONTINUE, line);
        if (accept_kw(p, "return")) {
            CfgNode *r = node(p, CFG_S_RETURN, line);
            CfgTok *u = peek(p);
            if (u->kind != CFG_T_NEWLINE && !is_op(u, ";") && u->kind != CFG_T_EOF) r->a = parse_testlist(p, true);
            return r;
        }
        if (accept_kw(p, "import")) {
            CfgNode *s = node(p, CFG_S_IMPORT, line);
            do {
                AvpStr mod = parse_dotted(p);
                AvpStr as = {NULL, 0};
                if (accept_kw(p, "as")) as = expect_name(p);
                avp_vec_push(&s->names, mod);
                avp_vec_push(&s->asnames, as);
            } while (accept_op(p, ","));
            return s;
        }
        if (accept_kw(p, "from")) {
            CfgNode *s = node(p, CFG_S_FROMIMPORT, line);
            if (is_op(peek(p), ".")) cfg_fail(line, "relative imports are not supported");
            s->name = parse_dotted(p);
            if (!accept_kw(p, "import")) cfg_fail(peek(p)->line, "expected 'import'");
            if (is_op(peek(p), "*")) cfg_fail(line, "'import *' is not supported");
            bool paren = accept_op(p, "(");
            do {
                if (paren && is_op(peek(p), ")")) break;
                AvpStr nm = expect_name(p);
                AvpStr as = {NULL, 0};
                if (accept_kw(p, "as")) as = expect_name(p);
                avp_vec_push(&s->names, nm);
                avp_vec_push(&s->asnames, as);
            } while (accept_op(p, ","));
            if (paren) expect_op(p, ")");
            return s;
        }
        static const char *const unsupported[] = {"del", "global", "nonlocal", "assert", "raise", "yield", "await",
                                                  "class", "while", "try", "with", "async", "except", "finally", NULL};
        for (const char *const *k = unsupported; *k; k++)
            if (is_kw(t, *k)) cfg_fail(line, "'%s' statements are not supported", *k);
    }
    CfgNode *e = parse_testlist(p, true);
    CfgTok *u = peek(p);
    if (is_op(u, ":")) { /* annotated assignment */
        next(p);
        check_target(e);
        if (e->kind != CFG_E_NAME && e->kind != CFG_E_ATTR && e->kind != CFG_E_SUBSCR)
            cfg_fail(line, "only single targets can be annotated");
        (void)parse_test(p);
        CfgNode *s = node(p, CFG_S_ANNASSIGN, line);
        avp_vec_push(&s->items, e);
        if (accept_op(p, "=")) s->a = parse_testlist(p, true);
        return s;
    }
    if (is_op(u, "=")) {
        CfgNode *s = node(p, CFG_S_ASSIGN, line);
        CfgNode *value = e;
        while (accept_op(p, "=")) {
            check_target(value);
            avp_vec_push(&s->items, value);
            value = parse_testlist(p, true);
        }
        s->a = value;
        return s;
    }
    if (u->kind == CFG_T_OP && u->text.n >= 2 && u->text.p[u->text.n - 1] == '=' && !is_op(u, "==") &&
        !is_op(u, "!=") && !is_op(u, "<=") && !is_op(u, ">="))
        cfg_fail(line, "augmented assignment (%.*s) is not supported", AVP_SF(u->text));
    CfgNode *s = node(p, CFG_S_EXPR, line);
    s->a = e;
    return s;
}

static void parse_simple(P *p, CfgNodes *out) {
    for (;;) {
        avp_vec_push(out, parse_small(p));
        if (!accept_op(p, ";")) break;
        if (peek(p)->kind == CFG_T_NEWLINE) break;
    }
    CfgTok *t = peek(p);
    if (t->kind == CFG_T_EOF) return;
    if (t->kind != CFG_T_NEWLINE) cfg_fail(t->line, "invalid syntax: unexpected %s", tok_desc(p->a, t));
    next(p);
}

static void parse_stmt(P *p, CfgNodes *out);

static CfgNode *parse_if_rest(P *p, int line) {
    CfgNode *s = node(p, CFG_S_IF, line);
    s->a = parse_test(p);
    parse_suite(p, &s->body);
    CfgTok *t = peek(p);
    if (is_kw(t, "elif")) {
        next(p);
        avp_vec_push(&s->orelse, parse_if_rest(p, t->line));
    } else if (is_kw(t, "else")) {
        next(p);
        parse_suite(p, &s->orelse);
    }
    return s;
}

static CfgParams *parse_params(P *p) {
    CfgParams *ps = AVP_NEW(p->a, CfgParams);
    expect_op(p, "(");
    while (!is_op(peek(p), ")")) {
        CfgTok *t = peek(p);
        if (is_op(t, "*")) {
            next(p);
            if (is_op(peek(p), ",") || is_op(peek(p), ")")) cfg_fail(t->line, "keyword-only parameters are not supported");
            ps->star = expect_name(p);
            if (accept_op(p, ":")) (void)parse_test(p);
            if (!is_op(peek(p), ")")) {
                if (accept_op(p, ",") && is_op(peek(p), ")")) break;
                cfg_fail(t->line, "parameters after *args are not supported");
            }
            break;
        }
        if (is_op(t, "**") || is_op(t, "/")) cfg_fail(t->line, "'%.*s' parameters are not supported", AVP_SF(t->text));
        AvpStr name = expect_name(p);
        if (accept_op(p, ":")) (void)parse_test(p); /* the annotation is not evaluated */
        avp_vec_push(&ps->names, name);
        if (accept_op(p, "=")) avp_vec_push(&ps->defaults, parse_test(p));
        else if (ps->defaults.n) cfg_fail(t->line, "parameter without a default follows parameter with a default");
        if (!accept_op(p, ",")) break;
    }
    expect_op(p, ")");
    return ps;
}

static void parse_stmt(P *p, CfgNodes *out) {
    CfgTok *t = peek(p);
    int line = t->line;
    if (t->kind == CFG_T_INDENT) cfg_fail(line, "unexpected indent");
    if (is_kw(t, "if")) {
        next(p);
        avp_vec_push(out, parse_if_rest(p, line));
        return;
    }
    if (is_kw(t, "for")) {
        next(p);
        CfgNode *s = node(p, CFG_S_FOR, line);
        /* the target: names, tuples and lists of them (parsed below 'in') */
        int tline = peek(p)->line;
        CfgNode *first = parse_arith(p);
        if (is_op(peek(p), ",")) {
            CfgNode *tup = node(p, CFG_E_TUPLE, tline);
            avp_vec_push(&tup->items, first);
            while (accept_op(p, ",")) {
                if (is_kw(peek(p), "in")) break;
                avp_vec_push(&tup->items, parse_arith(p));
            }
            first = tup;
        }
        check_target(first);
        s->b = first;
        if (!accept_kw(p, "in")) cfg_fail(peek(p)->line, "expected 'in'");
        s->a = parse_testlist(p, true);
        parse_suite(p, &s->body);
        if (is_kw(peek(p), "else")) cfg_fail(peek(p)->line, "'for ... else' is not supported");
        avp_vec_push(out, s);
        return;
    }
    if (is_kw(t, "def")) {
        next(p);
        CfgNode *s = node(p, CFG_S_DEF, line);
        s->name = expect_name(p);
        s->params = parse_params(p);
        if (accept_op(p, "->")) (void)parse_test(p);
        parse_suite(p, &s->body);
        avp_vec_push(out, s);
        return;
    }
    if (is_op(t, "@")) cfg_fail(line, "decorators are not supported");
    if (is_kw(t, "else") || is_kw(t, "elif")) cfg_fail(line, "invalid syntax: unexpected '%.*s'", AVP_SF(t->text));
    parse_simple(p, out);
}

static void parse_suite(P *p, CfgNodes *body) {
    expect_op(p, ":");
    if (peek(p)->kind != CFG_T_NEWLINE) {
        parse_simple(p, body);
        return;
    }
    next(p);
    if (peek(p)->kind != CFG_T_INDENT) cfg_fail(peek(p)->line, "expected an indented block");
    next(p);
    while (peek(p)->kind != CFG_T_DEDENT && peek(p)->kind != CFG_T_EOF) parse_stmt(p, body);
    if (peek(p)->kind == CFG_T_DEDENT) next(p);
}

CfgNodes cfg_parse_file(AvpArena *a, AvpStr src) {
    P p = {a, cfg_lex(a, src, 1, false), 0};
    CfgNodes out = {0};
    while (peek(&p)->kind != CFG_T_EOF) {
        if (peek(&p)->kind == CFG_T_NEWLINE) {
            next(&p);
            continue;
        }
        if (peek(&p)->kind == CFG_T_DEDENT) cfg_fail(peek(&p)->line, "unexpected dedent");
        parse_stmt(&p, &out);
    }
    return out;
}
