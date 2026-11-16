/* unit: the CodeWarrior demangler of tools/unit.py (lines 592-806: only what a signature stub needs). It is not
 * shsm_mangle: unit.py's own demangler spells types differently (s32, u8, `T* const`) and marks complex names. */
#include "unit.h"

#include <setjmp.h>
#include <string.h>

typedef struct {
    AvpArena *a;
    const char *s;
    size_t n;
    AvpStrVec params, used; /* outside the setjmp frame's locals */
    jmp_buf *fail;
} P;

static const struct {
    char c;
    const char *t;
} BASE_TYPES[] = {{'v', "void"}, {'i', "int"},   {'l', "s32"},     {'s', "s16"}, {'c', "char"}, {'b', "bool"},
                  {'f', "float"}, {'d', "double"}, {'w', "wchar_t"}, {'x', "s64"}, {'e', "..."}};
static const struct {
    char c;
    const char *t;
} UBASE_TYPES[] = {{'i', "unsigned int"}, {'l', "u32"}, {'s', "u16"}, {'c', "u8"}, {'x', "u64"}};
static const char *const OPERATORS[][2] = {
    {"__nw", "operator new"}, {"__dl", "operator delete"}, {"__nwa", "operator new[]"}, {"__dla", "operator delete[]"},
    {"__as", "operator="},    {"__eq", "operator=="},      {"__ne", "operator!="},      {"__ls", "operator<<"},
    {"__rs", "operator>>"},   {"__pl", "operator+"},       {"__mi", "operator-"},       {"__ml", "operator*"},
    {"__dv", "operator/"},    {"__vc", "operator[]"},      {"__rf", "operator->"},      {"__cl", "operator()"},
    {"__lt", "operator<"},    {"__gt", "operator>"},       {"__le", "operator<="},      {"__ge", "operator>="}};

static const char *operator_of(const char *base) {
    for (size_t i = 0; i < sizeof OPERATORS / sizeof *OPERATORS; i++)
        if (!strcmp(OPERATORS[i][0], base)) return OPERATORS[i][1];
    return NULL;
}

bool unit_is_base_type(const char *t) {
    for (size_t i = 0; i < sizeof BASE_TYPES / sizeof *BASE_TYPES; i++)
        if (!strcmp(BASE_TYPES[i].t, t)) return true;
    return false;
}

AVP_NORETURN static void fail(P *p) { longjmp(*p->fail, 1); }
static bool digit(char c) { return c >= '0' && c <= '9'; }
static char at(P *p, size_t i) {
    if (i >= p->n) fail(p); /* IndexError */
    return p->s[i];
}

static size_t num(P *p, size_t i, size_t *out) {
    size_t j = i;
    while (j < p->n && digit(p->s[j])) j++;
    if (j == i) fail(p);
    size_t v = 0;
    for (size_t k = i; k < j; k++) {
        if (v > (SIZE_MAX - 9) / 10) fail(p);
        v = v * 10 + (size_t)(p->s[k] - '0');
    }
    *out = v;
    return j;
}

static size_t name(P *p, size_t i, const char **out) {
    size_t n;
    i = num(p, i, &n);
    if (n > p->n - i) fail(p);
    *out = avp_cz(p->a, avp_strn(p->s + i, n));
    return i + n;
}

static size_t qual(P *p, size_t i, const char **out) {
    char c = at(p, i + 1);
    if (!digit(c)) fail(p); /* ValueError */
    int n = c - '0';
    i += 2;
    AvpBuf b;
    avp_buf_init(&b, p->a);
    for (int k = 0; k < n; k++) {
        const char *nm;
        i = name(p, i, &nm);
        if (k) avp_buf_putz(&b, "::");
        avp_buf_putz(&b, nm);
    }
    *out = avp_buf_cz(&b);
    return i;
}

typedef struct {
    bool fn;
    const char *text, *args;
} T;

typedef struct {
    AvpStrVec *used;
    bool complex;
} Flags;

static void add_used(AvpStrVec *u, const char *nm) {
    for (size_t i = 0; i < u->n; i++)
        if (avp_str_eqz(u->v[i], nm)) return;
    avp_vec_push(u, avp_str(nm));
}

static size_t type(P *p, size_t i, AvpStrVec *params, Flags *fl, T *out);

static size_t type(P *p, size_t i, AvpStrVec *params, Flags *fl, T *out) {
    AvpArena *a = p->a;
    char c = at(p, i);
    T t = {0};
    if (c == 'P' || c == 'R') {
        i = type(p, i + 1, params, fl, &t);
        const char *suffix = c == 'P' ? "*" : "&";
        out->fn = false;
        out->text = t.fn ? avp_sprintf(a, "%s (%s)(%s)", t.text, suffix, t.args) : avp_sprintf(a, "%s%s", t.text, suffix);
        return i;
    }
    if (c == 'C') {
        i = type(p, i + 1, params, fl, &t);
        if (t.fn) {
            *out = t;
            return i;
        }
        size_t n = strlen(t.text);
        out->fn = false;
        out->text = n && (t.text[n - 1] == '*' || t.text[n - 1] == '&') ? avp_sprintf(a, "%s const", t.text)
                                                                          : avp_sprintf(a, "const %s", t.text);
        return i;
    }
    if (c == 'V') {
        i = type(p, i + 1, params, fl, &t);
        out->fn = false;
        out->text = avp_sprintf(a, "volatile %s", t.text);
        return i;
    }
    if (c == 'U') {
        char u = i + 1 < p->n ? p->s[i + 1] : 0;
        for (size_t k = 0; k < sizeof UBASE_TYPES / sizeof *UBASE_TYPES; k++)
            if (UBASE_TYPES[k].c == u) {
                out->fn = false;
                out->text = UBASE_TYPES[k].t;
                return i + 2;
            }
        fail(p);
    }
    if (c == 'S' && i + 1 < p->n && p->s[i + 1] == 'c') {
        out->fn = false;
        out->text = "s8";
        return i + 2;
    }
    if (digit(c)) {
        const char *nm;
        i = name(p, i, &nm);
        if (strchr(nm, '<')) fl->complex = true;
        else add_used(fl->used, nm);
        out->fn = false;
        out->text = nm;
        return i;
    }
    if (c == 'Q') {
        const char *nm;
        i = qual(p, i, &nm);
        fl->complex = true;
        out->fn = false;
        out->text = nm;
        return i;
    }
    if (c == 'F') {
        i++;
        AvpStrVec inner = {0};
        while (i < p->n && p->s[i] != '_') {
            T x;
            i = type(p, i, &inner, fl, &x);
            avp_vec_push(&inner, avp_str(x.text));
        }
        if (i >= p->n) fail(p);
        T ret;
        i = type(p, i + 1, &inner, fl, &ret);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "");
        bool first = true;
        for (size_t k = 0; k < inner.n; k++) {
            if (avp_str_eqz(inner.v[k], "void")) continue;
            if (!first) avp_buf_putz(&b, ", ");
            avp_buf_put(&b, inner.v[k]);
            first = false;
        }
        avp_vec_free(&inner);
        out->fn = true;
        out->text = ret.text;
        out->args = avp_buf_cz(&b);
        return i;
    }
    if (c == 'T') {
        char d = at(p, i + 1);
        if (!digit(d)) fail(p);
        size_t idx = (size_t)(d - '0');
        if (idx < 1 || idx > params->n) fail(p);
        out->fn = false;
        out->text = avp_cz(a, params->v[idx - 1]);
        return i + 2;
    }
    for (size_t k = 0; k < sizeof BASE_TYPES / sizeof *BASE_TYPES; k++)
        if (BASE_TYPES[k].c == c) {
            out->fn = false;
            out->text = BASE_TYPES[k].t;
            return i + 1;
        }
    fail(p);
}

static bool attempt_body(AvpArena *a, P *p, const char *nm, size_t i, UnitDemangled *d);

/* One attempt at the "__" at name[i]: true and *d filled on success. */
static bool attempt(AvpArena *a, const char *nm, size_t i, UnitDemangled *d) {
    P *p = AVP_NEW(a, P);
    p->a = a;
    p->s = nm + i + 2;
    p->n = strlen(p->s);
    jmp_buf jb;
    p->fail = &jb;
    if (setjmp(jb)) {
        avp_vec_free(&p->params);
        avp_vec_free(&p->used);
        return false;
    }
    return attempt_body(a, p, nm, i, d);
}

static bool attempt_body(AvpArena *a, P *p, const char *nm, size_t i, UnitDemangled *d) {
    Flags fl = {&p->used, false};
    size_t j = 0;
    const char *cls = "";
    if (p->s[0] == 'Q') j = qual(p, 0, &cls);
    else if (digit(p->s[0])) j = name(p, 0, &cls);
    bool cnst = false;
    if (j < p->n && p->s[j] == 'C' && j + 1 < p->n && p->s[j + 1] == 'F') {
        cnst = true;
        j++;
    }
    if (j >= p->n || p->s[j] != 'F') fail(p); /* data (vtable and the like), not a function */
    j++;
    while (j < p->n) {
        if (p->s[j] == 'N') { /* N<count><index>: repeat a parameter */
            char cc = at(p, j + 1), ci = at(p, j + 2);
            if (!digit(cc) || !digit(ci)) fail(p);
            int cnt = cc - '0', idx = ci - '0';
            for (int k = 0; k < cnt; k++) {
                i64 x = (i64)idx - 1;
                if (x < 0) x += (i64)p->params.n; /* Python's params[-1] */
                if (x < 0 || x >= (i64)p->params.n) fail(p);
                avp_vec_push(&p->params, p->params.v[x]);
            }
            j += 3;
            continue;
        }
        T t;
        j = type(p, j, &p->params, &fl, &t);
        avp_vec_push(&p->params, avp_str(t.text));
    }
    const char *base = avp_cz(a, avp_strn(nm, i));
    d->base = base;
    d->cls = cls;
    d->is_const = cnst;
    d->mangled = true;
    d->is_ctor = !strcmp(base, "__ct");
    d->is_dtor = !strcmp(base, "__dt");
    d->has_args = true;
    for (size_t k = 0; k < p->params.n; k++)
        if (!avp_str_eqz(p->params.v[k], "void")) avp_vec_push(&d->args, avp_str_dup(a, p->params.v[k]));
    for (size_t k = 0; k < p->used.n; k++) avp_vec_push(&d->used, p->used.v[k]);
    d->complex = fl.complex || strstr(cls, "::") || strchr(cls, '<');
    if (!strncmp(base, "__", 2) && !(d->is_ctor || d->is_dtor || operator_of(base))) d->has_args = false;
    avp_vec_free(&p->params);
    avp_vec_free(&p->used);
    return true;
}

UnitDemangled unit_demangle(AvpArena *a, const char *nm) {
    UnitDemangled d = {0};
    d.raw = nm;
    d.cls = "";
    d.thunk = "";
    /* ^(@-?\d+@)(.*)$ */
    if (nm[0] == '@') {
        size_t k = 1;
        if (nm[k] == '-') k++;
        size_t ds = k;
        while (digit(nm[k])) k++;
        if (k > ds && nm[k] == '@') {
            d = unit_demangle(a, nm + k + 1);
            d.thunk = avp_cz(a, avp_strn(nm, k + 1));
            d.raw = nm;
            return d;
        }
    }
    if (!strncmp(nm, "__sinit_", 8)) {
        d.sinit = true;
        d.base = nm;
        return d;
    }
    d.base = nm;
    for (const char *q = strstr(nm, "__"); q; q = strstr(q + 2, "__")) {
        size_t i = (size_t)(q - nm);
        char r = q[2];
        if (!r || !(digit(r) || r == 'Q' || r == 'F' || r == 'C')) continue;
        if (i == 0) continue;
        UnitDemangled x = d;
        x.args = (AvpStrVec){0};
        x.used = (AvpStrVec){0};
        if (attempt(a, nm, i, &x)) return x;
    }
    return d;
}

const char *unit_dm_short_cls(AvpArena *a, const UnitDemangled *d) {
    const char *k = d->cls, *q;
    while ((q = strstr(k, "::"))) k = q + 2;
    return avp_strdup(a, k);
}

const char *unit_dm_fname(AvpArena *a, const UnitDemangled *d) {
    if (d->is_ctor) return unit_dm_short_cls(a, d);
    if (d->is_dtor) return avp_sprintf(a, "~%s", unit_dm_short_cls(a, d));
    const char *op = operator_of(d->base);
    return op ? op : d->base;
}

const char *unit_dm_qualified(AvpArena *a, const UnitDemangled *d) {
    return *d->cls ? avp_sprintf(a, "%s::%s", d->cls, unit_dm_fname(a, d)) : unit_dm_fname(a, d);
}

bool unit_dm_generated(const UnitDemangled *d) { return d->sinit || *d->thunk; }

const char *unit_sig_text(AvpArena *a, const UnitDemangled *d, bool defining) {
    const char *ret = d->is_ctor || d->is_dtor ? "" : "void ";
    AvpStr args = d->has_args ? avp_str_join(a, AVP_S(", "), d->args.v, d->args.n) : AVP_S("");
    const char *inner = args.n ? avp_sprintf(a, "( %.*s )", (int)args.n, args.p) : "()";
    const char *nm = defining ? unit_dm_qualified(a, d) : unit_dm_fname(a, d);
    return avp_sprintf(a, "%s%s%s%s", ret, nm, inner, d->is_const ? " const" : "");
}
