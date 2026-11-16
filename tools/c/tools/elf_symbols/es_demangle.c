/* The GNU v2 demangler of tools/scripts/elf_symbols.py (class V2, demangle_v2, _v2_tail, detect_scheme).
 *
 * Symbols are latin-1 bytes. The Python raises DemangleError (tried again at the next "__" candidate) or ValueError /
 * IndexError (the whole symbol gives None); here both are avp_throw, told apart by the code: AVP_ERR_FORMAT is the
 * DemangleError, AVP_ERR_OTHER the other two. */
#include "elf_symbols.h"

#include <string.h>

typedef struct {
    const char *s;
    size_t len, i;
    AvpStrVec types; /* the B/T back-reference table, in order of appearance */
    AvpArena *a;
} V2;

static AVP_NORETURN void dem_err(const char *what) { avp_throw(AVP_ERR_FORMAT, "%s", what); }
static AVP_NORETURN void val_err(const char *what) { avp_throw(AVP_ERR_OTHER, "%s", what); }

static bool is_digit(int c) { return c >= '0' && c <= '9'; }
/* str.isdigit() of a latin-1 character: the ASCII digits and the superscripts 1 2 3 */
static bool py_isdigit(int c) { return is_digit(c) || c == 0xB2 || c == 0xB3 || c == 0xB9; }

static int peek(const V2 *p) { return p->i < p->len ? (unsigned char)p->s[p->i] : -1; }

static int take(V2 *p) {
    int c = peek(p);
    if (c < 0) dem_err("eof");
    p->i++;
    return c;
}

/* int(self.take()): one character that must be an ASCII digit */
static int take_int(V2 *p) {
    int c = take(p);
    if (!is_digit(c)) val_err("invalid literal for int()");
    return c - '0';
}

static i64 number(V2 *p) {
    size_t j = p->i;
    i64 v = 0;
    while (j < p->len && is_digit((unsigned char)p->s[j])) {
        if (v < (i64)1 << 50) v = v * 10 + (p->s[j] - '0');
        j++;
    }
    if (j == p->i) dem_err("number");
    p->i = j;
    return v;
}

static AvpStr name(V2 *p) {
    i64 n = number(p);
    if (n > (i64)(p->len - p->i)) dem_err("short name");
    AvpStr t = avp_strn(p->s + p->i, (size_t)n);
    p->i += (size_t)n;
    return t;
}

static AvpStr simple_component(V2 *p) {
    if (peek(p) == 't') { /* template instantiation: not supported */
        p->i++;
        (void)name(p);
        dem_err("template");
    }
    return name(p);
}

/* Parse <len><name> or Q<n><len><name>...; the components. */
static AvpStrVec qualified(V2 *p) {
    AvpStrVec out = {0};
    if (peek(p) == 'Q') {
        p->i++;
        i64 cnt;
        if (peek(p) == '_') {
            p->i++;
            cnt = number(p);
            if (take(p) != '_') dem_err("Q_");
        } else {
            cnt = take_int(p);
        }
        for (i64 k = 0; k < cnt; k++) avp_vec_push(&out, simple_component(p));
        return out;
    }
    avp_vec_push(&out, simple_component(p));
    return out;
}

static AvpStr join_colons(AvpArena *a, const AvpStrVec *v) {
    return avp_str_join(a, avp_str("::"), v->v, v->n);
}

static AvpStr cat(AvpArena *a, AvpStr x, AvpStr y) { return avp_str_concat(a, x, y); }
static AvpStr catz(AvpArena *a, AvpStr x, const char *y) { return avp_str_concat(a, x, avp_str(y)); }

static AvpStr type_(V2 *p);

static AvpStr args(V2 *p, int end) {
    AvpStrVec out = {0};
    while (p->i < p->len && peek(p) != end) {
        size_t start = p->i;
        int c = peek(p);
        AvpStr t;
        if (c == 'T') {
            p->i++;
            i64 n = take_int(p);
            t = n < (i64)p->types.n ? p->types.v[n] : avp_str("?");
        } else if (c == 'N') {
            p->i++;
            i64 cnt = take_int(p);
            i64 n = take_int(p);
            t = n < (i64)p->types.n ? p->types.v[n] : avp_str("?");
            for (i64 k = 0; k < cnt; k++) avp_vec_push(&out, t);
            continue;
        } else {
            t = type_(p);
            if (p->i - start > 1) avp_vec_push(&p->types, t); /* compound types are remembered for T back-refs */
        }
        avp_vec_push(&out, t);
    }
    AvpStr r = avp_str_join(p->a, avp_str(", "), out.v, out.n);
    avp_vec_free(&out);
    return r;
}

static AvpStr type_(V2 *p) {
    AvpArena *a = p->a;
    int c = peek(p);
    AvpBuf qb;
    avp_buf_init(&qb, a);
    bool unsigned_ = false;
    while (c == 'C' || c == 'V' || c == 'U' || c == 'S') {
        p->i++;
        if (c == 'C') avp_buf_putz(&qb, " const");
        else if (c == 'V') avp_buf_putz(&qb, " volatile");
        else if (c == 'U') unsigned_ = true; /* 'unsigned ' / 'signed ' are stripped from quals right away */
        c = peek(p);
    }
    AvpStr quals = avp_buf_str(&qb);
    c = peek(p);
    if (c == 'P') {
        p->i++;
        return cat(a, catz(a, type_(p), " *"), quals);
    }
    if (c == 'R') {
        p->i++;
        return cat(a, catz(a, type_(p), " &"), quals);
    }
    if (c == 'A') {
        p->i++;
        i64 n = number(p);
        if (take(p) != '_') dem_err("A_");
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, type_(p));
        avp_buf_printf(&b, " [%lld]", (long long)n);
        return avp_buf_str(&b);
    }
    if (c == 'F') {
        p->i++;
        AvpStr as = args(p, '_');
        take(p);
        AvpStr ret = type_(p);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, ret);
        avp_buf_putz(&b, " (");
        avp_buf_put(&b, as);
        avp_buf_putc(&b, ')');
        return avp_buf_str(&b);
    }
    if (c == 'T') {
        p->i++;
        i64 n = take_int(p);
        if (n < (i64)p->types.n) return cat(a, p->types.v[n], quals);
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_printf(&b, "?T%lld", (long long)n);
        return avp_buf_str(&b);
    }
    if (c == 'N') {
        p->i++;
        (void)take_int(p); /* the repeat count */
        i64 n = take_int(p);
        return n < (i64)p->types.n ? cat(a, p->types.v[n], quals) : avp_str("?N");
    }
    static const struct {
        char c;
        const char *name;
    } builtin[] = {{'v', "void"}, {'i', "int"},   {'l', "long"},   {'s', "short"}, {'c', "char"},      {'f', "float"},
                   {'d', "double"}, {'r', "long double"}, {'b', "bool"}, {'w', "wchar_t"}, {'x', "long long"}};
    if (c >= 0) {
        for (size_t k = 0; k < sizeof builtin / sizeof *builtin; k++) {
            if (builtin[k].c == c) {
                p->i++;
                AvpStr base = avp_str(builtin[k].name);
                if (unsigned_) base = cat(a, avp_str("unsigned "), base);
                return cat(a, base, quals);
            }
        }
    }
    if (c >= 0 && (py_isdigit(c) || c == 'Q')) {
        AvpStrVec q = qualified(p);
        return cat(a, join_colons(a, &q), quals);
    }
    dem_err("type");
}

static bool has_prefix(AvpStr s, const char *pre) { return avp_str_startswith(s, avp_str(pre)); }

static const struct {
    const char *code, *op;
} OPS[] = {{"nw", "new"},    {"dl", "delete"}, {"vn", "new []"}, {"vd", "delete []"}, {"pl", "+"},   {"mi", "-"},
           {"ml", "*"},      {"dv", "/"},      {"md", "%"},      {"er", "^"},         {"ad", "&"},   {"or", "|"},
           {"co", "~"},      {"nt", "!"},      {"as", "="},      {"lt", "<"},         {"gt", ">"},   {"apl", "+="},
           {"ami", "-="},    {"aml", "*="},    {"adv", "/="},    {"amd", "%="},       {"aer", "^="}, {"aad", "&="},
           {"aor", "|="},    {"ls", "<<"},     {"rs", ">>"},     {"als", "<<="},      {"ars", ">>="}, {"eq", "=="},
           {"ne", "!="},     {"le", "<="},     {"ge", ">="},     {"aa", "&&"},        {"oo", "||"},  {"pp", "++"},
           {"mm", "--"},     {"cm", ","},      {"rm", "->*"},    {"rf", "->"},        {"cl", "()"},  {"vc", "[]"}};

static AvpStr v2_tail(AvpArena *a, AvpStr head, AvpStr tail) {
    V2 p = {tail.p, tail.n, 0, {0}, a};
    AvpStrVec cls = {0};
    const char *konst = "";
    if (peek(&p) == 'C') {
        p.i++;
        konst = " const";
    }
    if (peek(&p) != 'F') {
        cls = qualified(&p);
        avp_vec_push(&p.types, join_colons(a, &cls));
        if (peek(&p) == 'C') {
            p.i++;
            konst = " const";
        }
    }
    if (peek(&p) == 'F') p.i++;
    else if (cls.n == 0) dem_err("no F");
    AvpStr as = p.i < p.len ? args(&p, -1) : avp_str("void");
    if (p.i < p.len) dem_err("trailing");
    bool is_op = false;
    if (has_prefix(head, "__")) {
        AvpStr code = avp_str_slice(head, 2, (i64)head.n);
        for (size_t k = 0; k < sizeof OPS / sizeof *OPS; k++) {
            if (avp_str_eqz(code, OPS[k].code)) {
                head = catz(a, avp_str("operator"), OPS[k].op);
                is_op = true;
                break;
            }
        }
    }
    if (!is_op && has_prefix(head, "__op")) head = cat(a, avp_str("operator "), avp_str_slice(head, 4, (i64)head.n));
    avp_vec_push(&cls, head);
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, join_colons(a, &cls));
    avp_buf_putc(&b, '(');
    avp_buf_put(&b, as);
    avp_buf_putc(&b, ')');
    avp_buf_putz(&b, konst);
    return avp_buf_str(&b);
}

static AvpStr qualified_name(AvpArena *a, V2 *p) {
    AvpStrVec q = qualified(p);
    return join_colons(a, &q);
}

/* the body of demangle_v2 up to the "except": false is a plain `return None`; a throw is the except clause */
static bool demangle_inner(AvpArena *a, AvpStr s, AvpStr *out) {
    if (has_prefix(s, "_$_") || has_prefix(s, "_._")) {
        V2 p = {s.p + 3, s.n - 3, 0, {0}, a};
        AvpStrVec cls = qualified(&p);
        if (cls.n == 0) val_err("list index out of range");
        *out = cat(a, catz(a, join_colons(a, &cls), "::~"), catz(a, cls.v[cls.n - 1], "()"));
        return true;
    }
    if (has_prefix(s, "__") && s.n > 2 && (py_isdigit((unsigned char)s.p[2]) || s.p[2] == 'Q')) {
        V2 p = {s.p + 2, s.n - 2, 0, {0}, a};
        AvpStrVec cls = qualified(&p);
        AvpStr joined = join_colons(a, &cls);
        avp_vec_push(&p.types, joined);
        AvpStr as = p.i < p.len ? args(&p, -1) : avp_str("void");
        if (cls.n == 0) val_err("list index out of range");
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_put(&b, joined);
        avp_buf_putz(&b, "::");
        avp_buf_put(&b, cls.v[cls.n - 1]);
        avp_buf_putc(&b, '(');
        avp_buf_put(&b, as);
        avp_buf_putc(&b, ')');
        *out = avp_buf_str(&b);
        return true;
    }
    if (has_prefix(s, "__vt_")) {
        V2 p = {s.p + 5, s.n - 5, 0, {0}, a};
        *out = cat(a, avp_str("vtable for "), qualified_name(a, &p));
        return true;
    }
    if (has_prefix(s, "__tf")) {
        V2 p = {s.p + 4, s.n - 4, 0, {0}, a};
        *out = cat(a, avp_str("typeinfo function for "), qualified_name(a, &p));
        return true;
    }
    if (has_prefix(s, "__ti")) {
        V2 p = {s.p + 4, s.n - 4, 0, {0}, a};
        *out = cat(a, avp_str("typeinfo for "), qualified_name(a, &p));
        return true;
    }
    /* re.finditer(r'__(?=[CFQ0-9])', s): non-overlapping matches */
    AVP_VEC(size_t) starts = {0};
    for (size_t i = 0; i + 2 < s.n;) {
        int c = (unsigned char)s.p[i + 2];
        if (s.p[i] == '_' && s.p[i + 1] == '_' && (c == 'C' || c == 'F' || c == 'Q' || is_digit(c))) {
            avp_vec_push(&starts, i);
            i += 2;
        } else {
            i++;
        }
    }
    for (size_t k = 0; k < starts.n; k++) {
        size_t at = starts.v[k];
        AvpStr head = avp_strn(s.p, at), tail = avp_strn(s.p + at + 2, s.n - at - 2);
        if (head.n == 0) continue;
        AvpTry t;
        if (AVP_TRY(&t)) {
            AvpStr r = v2_tail(a, head, tail);
            avp_try_end(&t);
            if (r.n) {
                *out = r;
                return true;
            }
        } else if (t.code != AVP_ERR_FORMAT) {
            avp_throw(t.code, "%s", t.msg); /* ValueError / IndexError: not caught by the loop */
        }
    }
    return false;
}

bool elf_demangle_v2(AvpArena *a, AvpStr sym, AvpStr *out) {
    AvpTry t;
    if (AVP_TRY(&t)) {
        bool ok = demangle_inner(a, sym, out);
        avp_try_end(&t);
        return ok;
    }
    return false;
}

const char *elf_detect_scheme(const AvpStr *names, size_t n) {
    size_t itan = 0, v2 = 0;
    for (size_t k = 0; k < n; k++) {
        AvpStr s = names[k];
        if (has_prefix(s, "_Z")) itan++;
        bool hit = has_prefix(s, "_$_") || has_prefix(s, "__vt_") || has_prefix(s, "__ti") || has_prefix(s, "__tf");
        for (size_t i = 0; !hit && i + 2 < s.n; i++) {
            int c = (unsigned char)s.p[i + 2];
            if (s.p[i] == '_' && s.p[i + 1] == '_' && (c == 'C' || c == 'F' || c == 'Q' || is_digit(c))) hit = true;
        }
        if (hit) v2++;
    }
    if (itan > v2 && itan > 10) return "itanium";
    if (v2 > 10) return "gnu-v2";
    return "none";
}
