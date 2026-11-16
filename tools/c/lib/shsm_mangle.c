/* CodeWarrior C++ demangling (tools/common/mangle.py: parse, demangle and the type formatting they use). The
 * recursive-descent reader follows the Python function for function; DemangleError is a throw caught exactly where
 * the Python caught it, so the errors the Python let escape (ValueError, RecursionError) escape here too. */
#include "shsm_mangle.h"

#include "avp/fmt.h"
#include "shsm_cli.h"
#include "shsm_mangle_internal.h"

#define DEMANGLE_ERROR 200 /* mangle.DemangleError (internal) */

static AVP_NORETURN void derr(void) { avp_throw(DEMANGLE_ERROR, "demangle"); }

typedef enum { K_NAME, K_PTR, K_REF, K_ARR, K_FUNC, K_MEMPTR } Kind;

typedef struct T T;
typedef struct {
    T **v;
    size_t n;
} TList;
typedef struct {
    AvpStr *v;
    size_t n;
} Scope;

struct T {
    Kind kind;
    AvpStr text;   /* K_NAME */
    T *inner;
    u64 n;         /* K_ARR */
    TList params;  /* K_FUNC */
    T *ret;        /* K_FUNC: NULL = None */
    Scope cls;     /* K_MEMPTR */
    bool cnst, vol;
};

typedef struct {
    AvpArena *a;
    AvpStr s;
    size_t i;
} P;

static T *new_t(AvpArena *a, Kind k) {
    T *t = AVP_NEW(a, T);
    t->kind = k;
    return t;
}

/* ---- formatting ---- */

static AvpStr format_type(AvpArena *a, const T *t, AvpStr decl);

static AvpStr format_scope(AvpArena *a, const AvpStr *parts, size_t n) { return avp_str_join(a, AVP_S("::"), parts, n); }

static AvpStr format_params(AvpArena *a, const TList *ps) {
    if (ps->n == 1 && ps->v[0]->kind == K_NAME && avp_str_eqz(ps->v[0]->text, "void") && !ps->v[0]->cnst)
        return AVP_S("");
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < ps->n; i++) {
        if (i) avp_buf_put(&b, AVP_S(", "));
        avp_buf_put(&b, format_type(a, ps->v[i], AVP_S("")));
    }
    return b.n ? avp_buf_str(&b) : AVP_S("");
}

static AvpStr format_type(AvpArena *a, const T *t, AvpStr decl) {
    AvpBuf b;
    avp_buf_init(&b, a);
    switch (t->kind) {
    case K_NAME: {
        if (t->cnst) avp_buf_put(&b, AVP_S("const "));
        if (t->vol) avp_buf_put(&b, AVP_S("volatile "));
        avp_buf_put(&b, t->text);
        char c0 = decl.n ? decl.p[0] : 0;
        if (decl.n && c0 != '*' && c0 != '&' && c0 != '(' && c0 != '[') avp_buf_putc(&b, ' ');
        avp_buf_put(&b, decl);
        return avp_buf_str(&b);
    }
    case K_PTR:
    case K_REF:
    case K_MEMPTR: {
        if (t->kind == K_PTR) avp_buf_putc(&b, '*');
        else if (t->kind == K_REF) avp_buf_putc(&b, '&');
        else {
            avp_buf_put(&b, format_scope(a, t->cls.v, t->cls.n));
            avp_buf_put(&b, AVP_S("::*"));
        }
        bool quals = t->cnst || t->vol;
        if (t->cnst) avp_buf_put(&b, AVP_S(" const"));
        if (t->vol) avp_buf_put(&b, AVP_S(" volatile"));
        if (quals && decl.n && decl.p[0] != '*' && decl.p[0] != '&') avp_buf_putc(&b, ' ');
        avp_buf_put(&b, decl);
        AvpStr d = avp_buf_str(&b);
        if (t->inner->kind == K_ARR || t->inner->kind == K_FUNC) {
            AvpBuf w;
            avp_buf_init(&w, a);
            avp_buf_putc(&w, '(');
            avp_buf_put(&w, d);
            avp_buf_putc(&w, ')');
            d = avp_buf_str(&w);
        }
        return format_type(a, t->inner, d);
    }
    case K_ARR: {
        avp_buf_put(&b, decl);
        avp_buf_printf(&b, "[%llu]", (unsigned long long)t->n);
        return format_type(a, t->inner, avp_buf_str(&b));
    }
    case K_FUNC: {
        AvpStr params = format_params(a, &t->params);
        AvpStr ret = t->ret ? format_type(a, t->ret, AVP_S("")) : AVP_S("void");
        avp_buf_put(&b, ret);
        avp_buf_putc(&b, ' ');
        if (decl.n) {
            avp_buf_put(&b, decl);
            avp_buf_putc(&b, '(');
        } else {
            avp_buf_putc(&b, '(');
        }
        avp_buf_put(&b, params);
        avp_buf_putc(&b, ')');
        return avp_buf_str(&b);
    }
    }
    derr();
}

/* ---- reader ---- */

static T *p_type(P *p);
static AvpStr demangle_template_name(AvpArena *a, AvpStr raw);

static int peek(const P *p) { return p->i < p->s.n ? (unsigned char)p->s.p[p->i] : -1; }

static AvpStr take(P *p, u64 n) {
    if (n > p->s.n - p->i) derr(); /* "unexpected end" */
    AvpStr v = avp_strn(p->s.p + p->i, (size_t)n);
    p->i += (size_t)n;
    return v;
}

static bool is_digit(int c) { return c >= '0' && c <= '9'; }

static u64 number(P *p) {
    size_t j = p->i;
    u64 v = 0;
    while (p->i < p->s.n && is_digit(p->s.p[p->i])) {
        u64 d = (u64)(p->s.p[p->i] - '0');
        v = v > (UINT64_MAX - d) / 10 ? UINT64_MAX : v * 10 + d; /* Python's int is unbounded: saturate */
        p->i++;
    }
    if (j == p->i) derr();
    return v;
}

static AvpStr p_name(P *p) {
    u64 n = number(p);
    return demangle_template_name(p->a, take(p, n));
}

static Scope p_qualified(P *p) {
    Scope sc = {0};
    if (peek(p) == 'Q') {
        take(p, 1);
        AvpStr c = take(p, 1);
        if (!is_digit(c.p[0])) {
            AvpStr r = avp_repr_str(p->a, c);
            shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base 10: %.*s", AVP_SF(r));
        }
        size_t n = (size_t)(c.p[0] - '0');
        sc.v = AVP_NEWN(p->a, AvpStr, n + 1);
        for (size_t k = 0; k < n; k++) sc.v[sc.n++] = p_name(p);
        return sc;
    }
    sc.v = AVP_NEWN(p->a, AvpStr, 1);
    sc.v[sc.n++] = p_name(p);
    return sc;
}

static const char *builtin(int c) {
    switch (c) {
    case 'v': return "void";
    case 'b': return "bool";
    case 'c': return "char";
    case 's': return "short";
    case 'i': return "int";
    case 'l': return "long";
    case 'x': return "long long";
    case 'f': return "float";
    case 'd': return "double";
    case 'r': return "long double";
    case 'w': return "wchar_t";
    case 'e': return "...";
    default: return NULL;
    }
}

static TList p_params(P *p) {
    AvpPtrVec out = {0};
    while (p->i < p->s.n && peek(p) != '_' && peek(p) != '@') avp_vec_push(&out, p_type(p));
    if (!out.n) derr(); /* "empty parameter list" */
    TList l = {AVP_NEWN(p->a, T *, out.n), out.n};
    memcpy(l.v, out.v, out.n * sizeof(T *));
    avp_vec_free(&out);
    return l;
}

static T *p_type(P *p) {
    int c = peek(p);
    /* `c in "CV"` holds for the empty string at the end, and the Python then recursed without end */
    if (c < 0) shsm_raise(SHSM_EXC_RECURSION, "maximum recursion depth exceeded");
    if (c == 'C' || c == 'V') {
        bool qc = false, qv = false;
        while (peek(p) == 'C' || peek(p) == 'V') {
            if (take(p, 1).p[0] == 'C') qc = true;
            else qv = true;
        }
        T *t = p_type(p);
        t->cnst = t->cnst || qc;
        t->vol = t->vol || qv;
        return t;
    }
    if (c == 'P' || c == 'R') {
        take(p, 1);
        T *t = new_t(p->a, c == 'P' ? K_PTR : K_REF);
        t->inner = p_type(p);
        return t;
    }
    if (c == 'A') {
        take(p, 1);
        u64 n = number(p);
        if (take(p, 1).p[0] != '_') derr(); /* "array without _" */
        T *t = new_t(p->a, K_ARR);
        t->inner = p_type(p);
        t->n = n;
        return t;
    }
    if (c == 'F') {
        take(p, 1);
        T *t = new_t(p->a, K_FUNC);
        t->params = p_params(p);
        if (peek(p) == '_') {
            take(p, 1);
            t->ret = p_type(p);
        }
        return t;
    }
    if (c == 'M') {
        take(p, 1);
        Scope cls = p_qualified(p);
        T *inner = p_type(p);
        if (inner->kind == K_FUNC && inner->params.n && inner->params.v[0]->kind == K_PTR) {
            /* member function pointers carry the hidden `this` parameter first: drop it */
            if (inner->params.n > 1) {
                inner->params.v++;
                inner->params.n--;
            } else {
                T *v = new_t(p->a, K_NAME);
                v->text = AVP_S("void");
                inner->params.v = AVP_NEWN(p->a, T *, 1);
                inner->params.v[0] = v;
                inner->params.n = 1;
            }
        }
        T *t = new_t(p->a, K_MEMPTR);
        t->inner = inner;
        t->cls = cls;
        return t;
    }
    if (c == 'U' || c == 'S') {
        take(p, 1);
        const char *b = builtin(take(p, 1).p[0]);
        if (!b) derr(); /* "bad builtin" */
        T *t = new_t(p->a, K_NAME);
        t->text = avp_str(avp_sprintf(p->a, "%s%s", c == 'U' ? "unsigned " : "signed ", b));
        return t;
    }
    if (builtin(c)) {
        take(p, 1);
        T *t = new_t(p->a, K_NAME);
        t->text = avp_str(builtin(c));
        return t;
    }
    if (c == 'Q' || is_digit(c)) {
        Scope sc = p_qualified(p);
        T *t = new_t(p->a, K_NAME);
        t->text = format_scope(p->a, sc.v, sc.n);
        return t;
    }
    derr(); /* "bad type char" */
}

/* ---- template names ---- */

bool shsm_mangle_is_int_literal(AvpStr s) { /* re.fullmatch(r"-?\d+", s) */
    size_t i = (s.n && s.p[0] == '-') ? 1 : 0;
    if (i >= s.n) return false;
    for (; i < s.n; i++)
        if (!is_digit(s.p[i])) return false;
    return true;
}

static AvpStr template_arg(AvpArena *a, AvpStr arg) {
    if (shsm_mangle_is_int_literal(arg)) return arg;
    P p = {a, arg, 0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        T *ty = p_type(&p);
        AvpStr r = p.i == arg.n ? format_type(a, ty, AVP_S("")) : (AvpStr){0};
        avp_try_end(&t);
        if (r.p) return r;
        return arg;
    }
    if (t.code != DEMANGLE_ERROR) avp_throw(t.code, "%s", t.msg);
    return arg;
}

/* _split_targs: split at commas outside <...> */
AvpStrVec shsm_mangle_split_targs(AvpStr s) {
    AvpStrVec out = {0};
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.n; i++) {
        char ch = s.p[i];
        if (ch == '<') depth++;
        else if (ch == '>') depth--;
        if (ch == ',' && depth == 0) {
            avp_vec_push(&out, avp_str_slice(s, (i64)start, (i64)i));
            start = i + 1;
        }
    }
    avp_vec_push(&out, avp_str_slice(s, (i64)start, AVP_END));
    return out;
}

static AvpStr demangle_template_name(AvpArena *a, AvpStr raw) {
    i64 i = avp_str_findc(raw, '<');
    if (i < 0 || !avp_str_endswith(raw, AVP_S(">"))) return raw;
    AvpStrVec args = shsm_mangle_split_targs(avp_str_slice(raw, i + 1, (i64)raw.n - 1));
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_str_slice(raw, 0, i));
    avp_buf_putc(&b, '<');
    for (size_t k = 0; k < args.n; k++) {
        if (k) avp_buf_put(&b, AVP_S(", "));
        avp_buf_put(&b, template_arg(a, args.v[k]));
    }
    avp_buf_putc(&b, '>');
    avp_vec_free(&args);
    return avp_buf_str(&b);
}

/* ---- parse ---- */

typedef struct {
    bool data;
    Scope scope;
    AvpStr name, raw_name;
    AvpStr *params;
    size_t nparams;
    bool cnst, vol;
    AvpStr ret;   /* .p NULL: None */
    AvpStr thunk;
    AvpStr local; /* .p NULL: None */
} Demangled;

static AvpStr d_text(AvpArena *a, const Demangled *d) {
    AvpBuf b;
    avp_buf_init(&b, a);
    if (d->local.p) {
        avp_buf_put(&b, d->local);
        avp_buf_put(&b, AVP_S("::"));
    }
    for (size_t i = 0; i < d->scope.n; i++) {
        avp_buf_put(&b, d->scope.v[i]);
        avp_buf_put(&b, AVP_S("::"));
    }
    avp_buf_put(&b, d->thunk);
    avp_buf_put(&b, d->name);
    if (d->local.p || d->data) return b.n ? avp_buf_str(&b) : AVP_S("");
    AvpStr q = b.n ? avp_buf_str(&b) : AVP_S("");
    AvpBuf s;
    avp_buf_init(&s, a);
    if (d->ret.p) {
        avp_buf_put(&s, d->ret);
        avp_buf_putc(&s, ' ');
    }
    avp_buf_put(&s, q);
    avp_buf_putc(&s, '(');
    for (size_t i = 0; i < d->nparams; i++) {
        if (i) avp_buf_put(&s, AVP_S(", "));
        avp_buf_put(&s, d->params[i]);
    }
    avp_buf_putc(&s, ')');
    if (d->cnst) avp_buf_put(&s, AVP_S(" const"));
    if (d->vol) avp_buf_put(&s, AVP_S(" volatile"));
    return avp_buf_str(&s);
}

const char *const SHSM_MANGLE_OPS[][2] = {
    {"nw", "operator new"}, {"dl", "operator delete"}, {"nwa", "operator new[]"}, {"dla", "operator delete[]"},
    {"pl", "operator+"},    {"mi", "operator-"},       {"ml", "operator*"},       {"dv", "operator/"},
    {"md", "operator%"},    {"er", "operator^"},       {"ad", "operator&"},       {"or", "operator|"},
    {"co", "operator~"},    {"nt", "operator!"},       {"as", "operator="},       {"lt", "operator<"},
    {"gt", "operator>"},    {"apl", "operator+="},     {"ami", "operator-="},     {"amu", "operator*="},
    {"adv", "operator/="},  {"amd", "operator%="},     {"aer", "operator^="},     {"aad", "operator&="},
    {"aor", "operator|="},  {"ls", "operator<<"},      {"rs", "operator>>"},      {"ars", "operator>>="},
    {"als", "operator<<="}, {"eq", "operator=="},      {"ne", "operator!="},      {"le", "operator<="},
    {"ge", "operator>="},   {"aa", "operator&&"},      {"oo", "operator||"},      {"pp", "operator++"},
    {"mm", "operator--"},   {"cm", "operator,"},       {"rm", "operator->*"},     {"rf", "operator->"},
    {"cl", "operator()"},   {"vc", "operator[]"}};
const size_t SHSM_MANGLE_NOPS = AVP_ARRAY_LEN(SHSM_MANGLE_OPS);

static const char *op_name(AvpStr code) {
    for (size_t i = 0; i < SHSM_MANGLE_NOPS; i++)
        if (avp_str_eqz(code, SHSM_MANGLE_OPS[i][0])) return SHSM_MANGLE_OPS[i][1];
    return NULL;
}

/* _base_name: .p NULL for None */
static AvpStr base_name(AvpArena *a, AvpStr base, const Scope *scope) {
    if (avp_str_eqz(base, "__ct") || avp_str_eqz(base, "__dt")) {
        if (!scope->n) return (AvpStr){0};
        AvpStr last = scope->v[scope->n - 1], head, rest;
        avp_str_partition(last, AVP_S("<"), &head, &rest);
        if (avp_str_eqz(base, "__ct")) return head;
        return avp_str_concat(a, AVP_S("~"), head);
    }
    if (avp_str_startswith(base, AVP_S("__op")) && base.n > 4 && !op_name(avp_str_slice(base, 2, 4)) &&
        !op_name(avp_str_slice(base, 2, 5))) {
        P p = {a, avp_str_slice(base, 4, AVP_END), 0};
        T *t = p_type(&p);
        if (p.i != base.n - 4) derr(); /* "bad conversion operator" */
        return avp_str_concat(a, AVP_S("operator "), format_type(a, t, AVP_S("")));
    }
    if (avp_str_startswith(base, AVP_S("__"))) {
        const char *op = op_name(avp_str_slice(base, 2, AVP_END));
        if (op) return avp_str(op);
    }
    return demangle_template_name(a, base);
}

/* _parse_rest: false for None */
static bool parse_rest(AvpArena *a, AvpStr base, AvpStr rest, Demangled *d) {
    memset(d, 0, sizeof *d);
    AvpStr raw_base = base;
    AvpTry t;
    if (AVP_TRY(&t)) {
        P p = {a, rest, 0};
        bool ok = true;
        if (peek(&p) == 'Q' || is_digit(peek(&p))) d->scope = p_qualified(&p);
        bool qc = false, qv = false, quals = false;
        while (peek(&p) == 'C' || peek(&p) == 'V') {
            quals = true;
            if (take(&p, 1).p[0] == 'C') qc = true;
            else qv = true;
        }
        if (peek(&p) == 'F') {
            take(&p, 1);
            TList params = p_params(&p);
            if (peek(&p) == '_') {
                take(&p, 1);
                d->ret = format_type(a, p_type(&p), AVP_S(""));
            }
            if (format_params(a, &params).n == 0) {
                d->nparams = 0;
            } else {
                d->params = AVP_NEWN(a, AvpStr, params.n);
                for (size_t k = 0; k < params.n; k++) d->params[k] = format_type(a, params.v[k], AVP_S(""));
                d->nparams = params.n;
            }
            d->cnst = qc;
            d->vol = qv;
        } else if (quals || !d->scope.n) {
            ok = false;
        } else {
            d->data = true;
            if (avp_str_eqz(base, "__vt")) base = AVP_S("__vtable"); /* cwdemangle's spelling of the vtable object */
        }
        if (ok && p.i != rest.n) ok = false;
        if (ok) {
            AvpStr name = base_name(a, base, &d->scope);
            if (!name.p) ok = false;
            else {
                d->name = name;
                d->raw_name = raw_base;
            }
        }
        avp_try_end(&t);
        return ok;
    }
    if (t.code != DEMANGLE_ERROR) avp_throw(t.code, "%s", t.msg);
    return false;
}

static bool parse(AvpArena *a, AvpStr s, Demangled *out);

/* ^(@\d+@(?:\d+@)?)(.+)$ : the length of group 1, or 0 */
static size_t thunk_len(AvpStr s) {
    if (!s.n || s.p[0] != '@') return 0;
    size_t i = 1;
    while (i < s.n && is_digit(s.p[i])) i++;
    if (i == 1 || i >= s.n || s.p[i] != '@') return 0;
    size_t g = ++i;
    size_t k = i;
    while (k < s.n && is_digit(s.p[k])) k++;
    if (k > i && k < s.n && s.p[k] == '@' && k + 1 < s.n) g = k + 1;
    return g < s.n ? g : 0;
}

static bool parse(AvpArena *a, AvpStr s, Demangled *out) {
    AvpStr thunk = AVP_S("");
    size_t tl = thunk_len(s);
    if (tl && !avp_str_startswith(s, AVP_S("@LOCAL@")) && !avp_str_startswith(s, AVP_S("@unnamed@"))) {
        thunk = avp_str_slice(s, 0, (i64)tl);
        s = avp_str_slice(s, (i64)tl, AVP_END);
    }
    if (avp_str_startswith(s, AVP_S("@LOCAL@"))) {
        AvpStr rest = avp_str_slice(s, 7, AVP_END);
        i64 j = avp_str_rfind(rest, AVP_S("@"), 0, AVP_END);
        if (j <= 0) return false;
        Demangled outer;
        if (!parse(a, avp_str_slice(rest, 0, j), &outer)) return false;
        memset(out, 0, sizeof *out);
        out->data = true;
        out->name = avp_str_slice(rest, j + 1, AVP_END);
        out->thunk = AVP_S("");
        out->local = d_text(a, &outer);
        if (!out->local.p) out->local = AVP_S("");
        return true;
    }
    size_t lead = 0;
    while (lead < s.n && s.p[lead] == '_') lead++;
    size_t start = lead > 1 ? lead : 1;
    int depth = 0;
    for (size_t i = start; i + 1 < s.n; i++) {
        char ch = s.p[i];
        if (ch == '<') depth++;
        else if (ch == '>') depth--;
        else if (depth == 0 && ch == '_' && s.p[i + 1] == '_') {
            AvpStr base = avp_str_slice(s, 0, (i64)i), rest = avp_str_slice(s, (i64)i + 2, AVP_END);
            if (!base.n || !rest.n) continue;
            if (parse_rest(a, base, rest, out)) {
                out->thunk = thunk;
                return true;
            }
        }
    }
    return false;
}

const char *shsm_demangle(AvpArena *a, const char *mangled) {
    Demangled d;
    if (!parse(a, avp_str(mangled), &d)) return NULL;
    return avp_cz(a, d_text(a, &d));
}

ShsmDemangled *shsm_mangle_parse(AvpArena *a, const char *mangled) {
    Demangled d;
    if (!parse(a, avp_str(mangled), &d)) return NULL;
    ShsmDemangled *r = AVP_NEW(a, ShsmDemangled);
    r->is_data = d.data;
    r->kind = d.data ? "data" : "function";
    r->nscope = d.scope.n;
    r->scope = AVP_NEWN(a, const char *, d.scope.n + 1);
    for (size_t i = 0; i < d.scope.n; i++) r->scope[i] = avp_cz(a, d.scope.v[i]);
    r->name = avp_cz(a, d.name);
    r->raw_name = avp_cz(a, d.raw_name);
    r->nparams = d.nparams;
    r->params = AVP_NEWN(a, const char *, d.nparams + 1);
    for (size_t i = 0; i < d.nparams; i++) r->params[i] = avp_cz(a, d.params[i]);
    r->is_const = d.cnst;
    r->is_volatile = d.vol;
    r->ret = d.ret.p ? avp_cz(a, d.ret) : NULL;
    r->thunk = avp_cz(a, d.thunk);
    r->local = d.local.p ? avp_cz(a, d.local) : NULL;
    return r;
}

const char *shsm_demangled_text(AvpArena *a, const ShsmDemangled *d) {
    AvpBuf b;
    avp_buf_init(&b, a);
    if (d->local) {
        avp_buf_putz(&b, d->local);
        avp_buf_put(&b, AVP_S("::"));
    }
    for (size_t i = 0; i < d->nscope; i++) {
        avp_buf_putz(&b, d->scope[i]);
        avp_buf_put(&b, AVP_S("::"));
    }
    avp_buf_putz(&b, d->thunk);
    avp_buf_putz(&b, d->name);
    if (d->local || d->is_data) return avp_buf_cz(&b);
    AvpBuf s;
    avp_buf_init(&s, a);
    if (d->ret) {
        avp_buf_putz(&s, d->ret);
        avp_buf_putc(&s, ' ');
    }
    avp_buf_putz(&s, avp_buf_cz(&b));
    avp_buf_putc(&s, '(');
    for (size_t i = 0; i < d->nparams; i++) {
        if (i) avp_buf_put(&s, AVP_S(", "));
        avp_buf_putz(&s, d->params[i]);
    }
    avp_buf_putc(&s, ')');
    if (d->is_const) avp_buf_put(&s, AVP_S(" const"));
    if (d->is_volatile) avp_buf_put(&s, AVP_S(" volatile"));
    return avp_buf_cz(&s);
}

static AvpJson *json_strs(AvpArena *a, const char *const *v, size_t n) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < n; i++) avp_json_arr_push(a, arr, avp_json_strz(a, v[i]));
    return arr;
}

AvpJson *shsm_demangled_to_json(AvpArena *a, const ShsmDemangled *d) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("kind"), avp_json_strz(a, d->kind));
    avp_json_set(a, o, AVP_S("scope"), json_strs(a, d->scope, d->nscope));
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, d->name));
    avp_json_set(a, o, AVP_S("raw_name"), avp_json_strz(a, d->raw_name));
    avp_json_set(a, o, AVP_S("params"), json_strs(a, d->params, d->nparams));
    avp_json_set(a, o, AVP_S("const"), avp_json_bool(d->is_const));
    avp_json_set(a, o, AVP_S("volatile"), avp_json_bool(d->is_volatile));
    avp_json_set(a, o, AVP_S("ret"), d->ret ? avp_json_strz(a, d->ret) : avp_json_null());
    avp_json_set(a, o, AVP_S("thunk"), avp_json_strz(a, d->thunk));
    avp_json_set(a, o, AVP_S("local"), d->local ? avp_json_strz(a, d->local) : avp_json_null());
    avp_json_set(a, o, AVP_S("text"), avp_json_strz(a, shsm_demangled_text(a, d)));
    return o;
}

const char *shsm_demangle_template_name(AvpArena *a, const char *raw) {
    return avp_cz(a, demangle_template_name(a, avp_str(raw)));
}

const char *shsm_demangle_or_name(AvpArena *a, const char *name) {
    const char *d = shsm_demangle(a, name);
    return (d && *d) ? d : name;
}

const char *shsm_qualified(AvpArena *a, const char *name) {
    const char *d = shsm_demangle(a, name);
    if (!d || !*d) return name;
    int depth = 0;
    size_t n = strlen(d), cut = n;
    for (size_t i = 0; i < n; i++) {
        if (d[i] == '<') depth++;
        else if (d[i] == '>') depth--;
        else if (d[i] == '(' && depth == 0) {
            cut = i;
            break;
        }
    }
    return avp_cz(a, avp_strn(d, cut));
}
