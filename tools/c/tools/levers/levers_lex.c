/* levers: exceptions, latin-1 string helpers, interning, the tokenizer and bracket matching (levers.py tokenize,
 * code_tokens, match_brackets). */
#include "levers.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- exceptions ---- */

const char *lv_exc_name(int code) {
    switch (code) {
    case LV_EXC_PARSE: return "ParseError";
    case LV_EXC_KEY: return "KeyError";
    case LV_EXC_INDEX: return "IndexError";
    case LV_EXC_TYPE: return "TypeError";
    case LV_EXC_ATTR: return "AttributeError";
    case LV_EXC_DEMANGLE: return "DemangleError";
    default: return NULL;
    }
}

bool lv_exc_caught(int code) { return code == LV_EXC_PARSE || code == LV_EXC_KEY || code == LV_EXC_INDEX; }

void lv_parse_error(const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    avp_throw(LV_EXC_PARSE, "%s", msg);
}

/* ---- latin-1 str helpers ---- */

bool lv_isspace(u8 c) {
    return (c >= 9 && c <= 13) || (c >= 28 && c <= 32) || c == 0x85 || c == 0xa0;
}

/* re's \w for a latin-1 character: str.isalnum() or '_'. */
bool lv_isword(u8 c) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
    if (c < 0x80) return false;
    switch (c) {
    case 170: case 178: case 179: case 181: case 185: case 186: case 188: case 189: case 190: return true;
    default: break;
    }
    return c >= 192 && c != 215 && c != 247;
}

static bool lv_isdigit_py(u8 c) { return (c >= '0' && c <= '9') || c == 178 || c == 179 || c == 185; }

LvS lv_lstrip(LvS s) {
    while (s.n && lv_isspace((u8)s.p[0])) s.p++, s.n--;
    return s;
}
LvS lv_rstrip(LvS s) {
    while (s.n && lv_isspace((u8)s.p[s.n - 1])) s.n--;
    return s;
}
LvS lv_strip(LvS s) { return lv_rstrip(lv_lstrip(s)); }
bool lv_blank(LvS s) { return lv_strip(s).n == 0; }
bool lv_s_eq(LvS a, LvS b) { return a.n == b.n && (a.n == 0 || memcmp(a.p, b.p, a.n) == 0); }
bool lv_s_eqz(LvS a, const char *z) { return lv_s_eq(a, avp_str(z)); }
bool lv_s_startswith(LvS s, LvS p) { return s.n >= p.n && (p.n == 0 || memcmp(s.p, p.p, p.n) == 0); }
bool lv_s_contains(LvS hay, LvS needle) {
    if (needle.n == 0) return true;
    if (needle.n > hay.n) return false;
    for (size_t i = 0; i + needle.n <= hay.n; i++)
        if (hay.p[i] == needle.p[0] && memcmp(hay.p + i, needle.p, needle.n) == 0) return true;
    return false;
}

LvS lv_catv(AvpArena *a, const LvS *parts, size_t n) {
    size_t total = 0;
    for (size_t i = 0; i < n; i++) total += parts[i].n;
    char *p = avp_alloc_noz(a, total + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (parts[i].n) memcpy(p + k, parts[i].p, parts[i].n);
        k += parts[i].n;
    }
    p[k] = 0;
    return (LvS){p, total};
}

LvS lv_sz(const char *z) { return avp_str(z); }
LvS lv_tok_s(const LvTok *t) { return (LvS){t->t, t->n}; }

const char *lv_repr(AvpArena *a, LvS s) {
    bool sq = false, dq = false;
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '\'') sq = true;
        if (s.p[i] == '"') dq = true;
    }
    char q = (sq && !dq) ? '"' : '\'';
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_putc(&b, q);
    for (size_t i = 0; i < s.n; i++) {
        u8 c = (u8)s.p[i];
        if (c == '\\') avp_buf_putz(&b, "\\\\");
        else if (c == (u8)q) {
            avp_buf_putc(&b, '\\');
            avp_buf_putc(&b, (char)c);
        } else if (c == '\t') avp_buf_putz(&b, "\\t");
        else if (c == '\n') avp_buf_putz(&b, "\\n");
        else if (c == '\r') avp_buf_putz(&b, "\\r");
        else if (c < 0x20 || (c >= 0x7f && c <= 0xa0) || c == 0xad) avp_buf_printf(&b, "\\x%02x", c);
        else avp_buf_putc(&b, (char)c);
    }
    avp_buf_putc(&b, q);
    return avp_buf_cz(&b);
}

/* ---- interning ---- */

static AvpMap *g_intern;
static AvpArena *g_intern_arena;

const char *lv_intern(const char *p, size_t n) {
    if (!g_intern) {
        g_intern = avp_map_new();
        g_intern_arena = avp_arena_new(0);
    }
    bool created;
    AvpVal *v = avp_map_put(g_intern, avp_strn(p, n), &created);
    if (created) {
        char *c = avp_alloc_noz(g_intern_arena, n + 1);
        if (n) memcpy(c, p, n);
        c[n] = 0;
        v->p = c;
    }
    return v->p;
}
const char *lv_internz(const char *z) { return lv_intern(z, strlen(z)); }
const char *lv_intern_find(const char *z) {
    if (!g_intern) return NULL;
    AvpVal *v = avp_map_get(g_intern, avp_str(z));
    return v ? v->p : NULL;
}

const char *lv_kind_name(u8 k) {
    static const char *const names[] = {"id", "num", "str", "chr", "op", "comment", "pp"};
    return k < 7 ? names[k] : "?";
}

/* ---- tokens ---- */

static const char *const OPS[] = {"<<=", ">>=", "...", "->*", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                                  "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "::", "##", NULL};

static bool starts_at(LvS t, size_t i, const char *lit) {
    size_t n = strlen(lit);
    return i + n <= t.n && memcmp(t.p + i, lit, n) == 0;
}

static i64 find_from(LvS t, size_t i, const char *lit) {
    size_t n = strlen(lit);
    for (size_t k = i; k + n <= t.n; k++)
        if (memcmp(t.p + k, lit, n) == 0) return (i64)k;
    return -1;
}

static bool id_start(u8 c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$'; }
static bool id_cont(u8 c) { return id_start(c) || (c >= '0' && c <= '9'); }

static void push_tok(AvpArena *a, LvToks *out, size_t *cap, u8 k, LvS text, size_t s, size_t e) {
    if ((size_t)out->n == *cap) {
        size_t nc = *cap ? *cap * 2 : 256;
        out->v = avp_arena_realloc(a, out->v, *cap * sizeof(LvTok), nc * sizeof(LvTok));
        *cap = nc;
    }
    LvTok *t = &out->v[out->n++];
    t->k = k;
    t->t = lv_intern(text.p + s, e - s);
    t->n = (u32)(e - s);
    t->s = (i32)s;
    t->e = (i32)e;
}

LvToks lv_tokenize(AvpArena *a, LvS text) {
    LvToks toks = {0};
    size_t cap = 0, i = 0, n = text.n;
    bool bol = true;
    const u8 *p = (const u8 *)text.p;
    while (i < n) {
        u8 ch = p[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v') {
            if (ch == '\n') bol = true;
            i++;
            continue;
        }
        if (starts_at(text, i, "//")) {
            i64 j = find_from(text, i, "\n");
            j = j < 0 ? (i64)n : j;
            if (j > (i64)i + 2 && p[j - 1] == '\r') j--;
            push_tok(a, &toks, &cap, LV_COMMENT, text, i, (size_t)j);
            i = (size_t)j;
            continue;
        }
        if (starts_at(text, i, "/*")) {
            i64 j = find_from(text, i + 2, "*/");
            j = j < 0 ? (i64)n : j + 2;
            push_tok(a, &toks, &cap, LV_COMMENT, text, i, (size_t)j);
            i = (size_t)j;
            continue;
        }
        if (ch == '#' && bol) {
            size_t j = i, k;
            for (;;) {
                i64 kk = find_from(text, j, "\n");
                if (kk < 0) {
                    k = n;
                    break;
                }
                k = (size_t)kk;
                size_t q = k; /* text[j:k].rstrip("\r").endswith("\\") */
                while (q > j && p[q - 1] == '\r') q--;
                if (q > j && p[q - 1] == '\\') {
                    j = k + 1;
                    continue;
                }
                break;
            }
            size_t e = (k > i && p[k - 1] == '\r') ? k - 1 : k;
            push_tok(a, &toks, &cap, LV_PP, text, i, e);
            i = e;
            continue;
        }
        bol = false;
        if (ch == '"' || ch == '\'') {
            size_t j = i + 1;
            while (j < n && p[j] != ch && p[j] != '\n') j += p[j] == '\\' ? 2 : 1;
            j = j + 1 < n ? j + 1 : n;
            push_tok(a, &toks, &cap, ch == '"' ? LV_STR : LV_CHR, text, i, j);
            i = j;
            continue;
        }
        if (id_start(ch)) {
            size_t j = i + 1;
            while (j < n && id_cont(p[j])) j++;
            push_tok(a, &toks, &cap, LV_ID, text, i, j);
            i = j;
            continue;
        }
        if (lv_isdigit_py(ch) || (ch == '.' && i + 1 < n && lv_isdigit_py(p[i + 1]))) {
            /* _NUM = (?:\d|\.\d)(?:[eEpP][+-]|[\w.])*, \d being 0-9 for latin-1 */
            size_t j;
            if (ch >= '0' && ch <= '9') j = i + 1;
            else if (ch == '.' && i + 1 < n && p[i + 1] >= '0' && p[i + 1] <= '9') j = i + 2;
            else avp_throw(LV_EXC_ATTR, "'NoneType' object has no attribute 'group'");
            for (;;) {
                if (j + 1 < n && (p[j] == 'e' || p[j] == 'E' || p[j] == 'p' || p[j] == 'P') &&
                    (p[j + 1] == '+' || p[j + 1] == '-'))
                    j += 2;
                else if (j < n && (lv_isword(p[j]) || p[j] == '.'))
                    j += 1;
                else
                    break;
            }
            push_tok(a, &toks, &cap, LV_NUM, text, i, j);
            i = j;
            continue;
        }
        size_t len = 1;
        for (int o = 0; OPS[o]; o++)
            if (starts_at(text, i, OPS[o])) {
                len = strlen(OPS[o]);
                break;
            }
        push_tok(a, &toks, &cap, LV_OP, text, i, i + len);
        i += len;
    }
    return toks;
}

LvToks lv_code_tokens(AvpArena *a, LvToks toks) {
    LvToks out = {avp_alloc_noz(a, sizeof(LvTok) * (size_t)(toks.n ? toks.n : 1)), 0};
    for (i32 i = 0; i < toks.n; i++)
        if (toks.v[i].k != LV_COMMENT && toks.v[i].k != LV_PP) out.v[out.n++] = toks.v[i];
    return out;
}

const LvTok *lv_at(LvToks c, i64 i) {
    if (i < 0) i += c.n;
    if (i < 0 || i >= c.n) avp_throw(LV_EXC_INDEX, "list index out of range");
    return &c.v[i];
}

i32 lv_m(const i32 *m, LvToks c, i64 i) {
    if (i < 0 || i >= c.n || m[i] < 0) avp_throw(LV_EXC_KEY, "%lld", (long long)i);
    return m[i];
}

bool lv_in(const char *s, const char *const *tbl) {
    for (; *tbl; tbl++)
        if (strcmp(s, *tbl) == 0) return true;
    return false;
}

static char close_of(const char *t) {
    if (!t[0] || t[1]) return 0;
    return t[0] == '(' ? ')' : t[0] == '[' ? ']' : t[0] == '{' ? '}' : 0;
}
static char open_of(const char *t) {
    if (!t[0] || t[1]) return 0;
    return t[0] == ')' ? '(' : t[0] == ']' ? '[' : t[0] == '}' ? '{' : 0;
}

i32 *lv_match_brackets(AvpArena *a, LvToks code) {
    i32 *m = avp_alloc_noz(a, sizeof(i32) * (size_t)(code.n ? code.n : 1));
    for (i32 i = 0; i < code.n; i++) m[i] = -1;
    i32 *st = avp_alloc_noz(a, sizeof(i32) * (size_t)(code.n ? code.n : 1));
    i32 sp = 0;
    for (i32 i = 0; i < code.n; i++) {
        const LvTok *t = &code.v[i];
        if (t->k != LV_OP) continue;
        if (close_of(t->t)) st[sp++] = i;
        else if (open_of(t->t)) {
            if (!sp || code.v[st[sp - 1]].t[0] != open_of(t->t))
                lv_parse_error("unbalanced %s at offset %d", lv_repr(a, lv_tok_s(t)), t->s);
            i32 j = st[--sp];
            m[i] = j;
            m[j] = i;
        }
    }
    if (sp) lv_parse_error("unclosed %s", lv_repr(a, lv_tok_s(&code.v[st[sp - 1]])));
    return m;
}

LvS lv_norm_text(AvpArena *a, LvS s) {
    LvToks c = lv_code_tokens(a, lv_tokenize(a, s));
    AvpBuf b;
    avp_buf_init(&b, a);
    for (i32 i = 0; i < c.n; i++) {
        if (i) avp_buf_putc(&b, ' ');
        avp_buf_putn(&b, c.v[i].t, c.v[i].n);
    }
    char *z = avp_buf_cz(&b);
    return (LvS){z, strlen(z)};
}
