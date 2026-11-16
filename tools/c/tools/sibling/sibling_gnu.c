/* sibling_gnu: a small demangler for gcc 2.x (gnu-v2, cfront-style) names, as the PS2 builds of the Climax lineage
 * carry them. Covered: free functions (name__F<args>), members (name__<class><args>, const members name__C<class>),
 * qualified classes (Q<n> and Q_<n>_), constructors (__<class><args>), destructors (_$_<class>, _._<class>), operators
 * (__as__, __eq__, ... and conversion operators __op<type>__), and the argument types v b c s i l x f d r w e with
 * U S C V P R, arrays A<n>_, function types F<args>_<ret>, pointers to member M<class><type>, repeats T<n> and
 * N<count><index>. Not covered: template names and arguments (t...), H template functions, static data members and
 * virtual tables (_vt$...): those give NULL. */
#include "sibling_foreign.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    const char *p;
    AvpArena *a;
    const char *types[64]; /* remembered argument types (text) */
    int ntypes;
    bool bad;
} Dm;

static int num(Dm *d) {
    if (!isdigit((unsigned char)*d->p)) {
        d->bad = true;
        return 0;
    }
    int n = 0;
    while (isdigit((unsigned char)*d->p)) n = n * 10 + (*d->p++ - '0');
    return n;
}

/* <len><name> */
static const char *ident(Dm *d) {
    int n = num(d);
    if (d->bad || n <= 0 || (int)strnlen(d->p, (size_t)n) < n) {
        d->bad = true;
        return "";
    }
    const char *s = avp_sprintf(d->a, "%.*s", n, d->p);
    d->p += n;
    return s;
}

/* a class name: <len><name> | Q<n>... | Q_<n>_... */
static const char *class_name(Dm *d) {
    if (*d->p == 'Q') {
        d->p++;
        int n;
        if (*d->p == '_') {
            d->p++;
            n = num(d);
            if (*d->p != '_') d->bad = true;
            else d->p++;
        } else if (isdigit((unsigned char)*d->p)) n = *d->p++ - '0';
        else {
            d->bad = true;
            return "";
        }
        AvpBuf b = {0};
        avp_buf_init(&b, d->a);
        for (int k = 0; k < n && !d->bad; k++) {
            if (k) avp_buf_putz(&b, "::");
            avp_buf_putz(&b, ident(d));
        }
        return avp_buf_cz(&b);
    }
    if (*d->p == 't') { /* template: unsupported */
        d->bad = true;
        return "";
    }
    return ident(d);
}

static const char *type(Dm *d);

static const char *args(Dm *d, char stop) {
    AvpBuf b = {0};
    avp_buf_init(&b, d->a);
    bool first = true;
    while (*d->p && *d->p != stop && !d->bad) {
        if (*d->p == 'T' || *d->p == 'N') {
            char k = *d->p++;
            int count = 1, idx;
            if (k == 'N') {
                if (*d->p == '_') {
                    d->p++;
                    count = num(d);
                    if (*d->p == '_') d->p++;
                } else if (isdigit((unsigned char)*d->p)) count = *d->p++ - '0';
                idx = isdigit((unsigned char)*d->p) ? *d->p++ - '0' : (d->bad = true, 0);
            } else idx = num(d);
            if (d->bad || idx < 0 || idx >= d->ntypes) {
                d->bad = true;
                break;
            }
            for (int c = 0; c < count; c++) {
                if (!first) avp_buf_putz(&b, ", ");
                avp_buf_putz(&b, d->types[idx]);
                if (d->ntypes < 64) d->types[d->ntypes++] = d->types[idx];
                first = false;
            }
            continue;
        }
        bool ell = *d->p == 'e';
        const char *t = type(d);
        if (!first) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, t);
        if (!ell && d->ntypes < 64) d->types[d->ntypes++] = t;
        first = false;
    }
    return avp_buf_cz(&b);
}

static const char *type(Dm *d) {
    const char *quals = "";
    bool uns = false, sig = false;
    for (;;) {
        char c = *d->p;
        if (c == 'C') quals = avp_sprintf(d->a, "%sconst ", quals);
        else if (c == 'V') quals = avp_sprintf(d->a, "%svolatile ", quals);
        else if (c == 'U') uns = true;
        else if (c == 'S') sig = true;
        else break;
        d->p++;
    }
    char c = *d->p;
    const char *base = NULL;
    switch (c) {
    case 'v': base = "void"; break;
    case 'b': base = "bool"; break;
    case 'c': base = "char"; break;
    case 's': base = "short"; break;
    case 'i': base = "int"; break;
    case 'l': base = "long"; break;
    case 'x': base = "long long"; break;
    case 'f': base = "float"; break;
    case 'd': base = "double"; break;
    case 'r': base = "long double"; break;
    case 'w': base = "wchar_t"; break;
    case 'e': base = "..."; break;
    default: break;
    }
    if (base) {
        d->p++;
        return avp_sprintf(d->a, "%s%s%s", quals, uns ? "unsigned " : sig ? "signed " : "", base);
    }
    if (c == 'P' || c == 'R') {
        d->p++;
        const char *t = type(d);
        return avp_sprintf(d->a, "%s %s%s", t, c == 'P' ? "*" : "&", *quals ? " const" : "");
    }
    if (c == 'A') {
        d->p++;
        int n = num(d);
        if (*d->p == '_') d->p++;
        const char *t = type(d);
        return avp_sprintf(d->a, "%s[%d]", t, n);
    }
    if (c == 'F') {
        d->p++;
        const char *ar = args(d, '_');
        if (*d->p != '_') {
            d->bad = true;
            return "";
        }
        d->p++;
        const char *r = type(d);
        return avp_sprintf(d->a, "%s (*)(%s)", r, ar);
    }
    if (c == 'M') {
        d->p++;
        const char *cl = class_name(d);
        const char *t = type(d);
        return avp_sprintf(d->a, "%s %s::*", t, cl);
    }
    if (isdigit((unsigned char)c) || c == 'Q' || c == 't') return avp_sprintf(d->a, "%s%s", quals, class_name(d));
    d->bad = true;
    return "";
}

static const struct {
    const char *code, *op;
} OPS[] = {{"as", "="},    {"eq", "=="},  {"ne", "!="},  {"nw", " new"}, {"dl", " delete"}, {"vn", " new[]"},
           {"vd", " delete[]"}, {"pl", "+"}, {"mi", "-"}, {"ml", "*"}, {"dv", "/"}, {"md", "%"}, {"apl", "+="},
           {"ami", "-="}, {"aml", "*="}, {"adv", "/="}, {"amd", "%="}, {"lt", "<"}, {"gt", ">"}, {"le", "<="},
           {"ge", ">="}, {"aa", "&&"}, {"oo", "||"}, {"nt", "!"}, {"co", "~"}, {"vc", "[]"}, {"cl", "()"},
           {"rf", "->"}, {"ad", "&"}, {"er", "^"}, {"or", "|"}, {"aad", "&="}, {"aor", "|="}, {"aer", "^="},
           {"ls", "<<"}, {"rs", ">>"}, {"als", "<<="}, {"ars", ">>="}, {"pp", "++"}, {"mm", "--"}, {NULL, NULL}};

/* Parse the signature after "__": [C]<class>args | F args. qualified/args out. */
static bool signature(Dm *d, const char **cls, const char **ar, bool *is_const) {
    *cls = NULL;
    *is_const = false;
    if (*d->p == 'F') {
        d->p++;
        *ar = args(d, 0);
        return !d->bad;
    }
    if (*d->p == 'C') {
        *is_const = true;
        d->p++;
    }
    if (!(isdigit((unsigned char)*d->p) || *d->p == 'Q')) return false;
    *cls = class_name(d);
    if (d->bad) return false;
    d->types[d->ntypes++] = *cls;
    *ar = args(d, 0);
    return !d->bad;
}

static const char *finish(AvpArena *a, const char *cls, const char *name, const char *ar, bool is_const, bool qual) {
    const char *q = cls ? avp_sprintf(a, "%s::%s", cls, name) : name;
    if (qual) return q;
    if (!*ar || strcmp(ar, "void") == 0) ar = "";
    return avp_sprintf(a, "%s(%s)%s", q, ar, is_const ? " const" : "");
}

static const char *demangle(AvpArena *a, const char *m, bool qual) {
    if (!m || !*m) return NULL;
    Dm d = {.a = a};
    /* destructors */
    if ((m[0] == '_' && (m[1] == '$' || m[1] == '.') && m[2] == '_')) {
        d.p = m + 3;
        const char *cls = class_name(&d);
        if (d.bad || *d.p) return NULL;
        const char *last = strrchr(cls, ':');
        return finish(a, cls, avp_sprintf(a, "~%s", last ? last + 1 : cls), "", false, qual);
    }
    /* constructors: __<class>args */
    if (m[0] == '_' && m[1] == '_' && (isdigit((unsigned char)m[2]) || m[2] == 'Q')) {
        d.p = m + 2;
        const char *cls, *ar;
        bool c;
        if (signature(&d, &cls, &ar, &c) && cls) {
            const char *last = strrchr(cls, ':');
            return finish(a, cls, last ? last + 1 : cls, ar, c, qual);
        }
        d = (Dm){.a = a};
    }
    /* operators: __<code>__<sig> */
    if (m[0] == '_' && m[1] == '_' && islower((unsigned char)m[2])) {
        const char *sep = strstr(m + 2, "__");
        if (sep) {
            size_t n = (size_t)(sep - (m + 2));
            const char *op = NULL;
            for (int k = 0; OPS[k].code; k++)
                if (strlen(OPS[k].code) == n && strncmp(OPS[k].code, m + 2, n) == 0) op = OPS[k].op;
            Dm t = {.a = a};
            if (!op && n > 2 && strncmp(m + 2, "op", 2) == 0) { /* conversion: __op<type>__ */
                t.p = m + 4;
                const char *ty = type(&t);
                if (!t.bad && t.p == sep) op = avp_sprintf(a, " %s", ty);
            }
            if (op) {
                d.p = sep + 2;
                const char *cls, *ar;
                bool c;
                if (signature(&d, &cls, &ar, &c)) return finish(a, cls, avp_sprintf(a, "operator%s", op), ar, c, qual);
                return NULL;
            }
        }
    }
    /* name__sig: the first "__" (not at the start) whose tail parses */
    for (const char *s = strstr(m + 1, "__"); s; s = strstr(s + 1, "__")) {
        d = (Dm){.a = a, .p = s + 2};
        const char *cls, *ar;
        bool c;
        if (!*d.p) continue;
        if (signature(&d, &cls, &ar, &c) && !*d.p)
            return finish(a, cls, avp_sprintf(a, "%.*s", (int)(s - m), m), ar, c, qual);
    }
    return NULL;
}

const char *sib_gnu_demangle(AvpArena *a, const char *mangled) { return demangle(a, mangled, false); }
const char *sib_gnu_qualified(AvpArena *a, const char *mangled) { return demangle(a, mangled, true); }
