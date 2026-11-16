/* A private copy of the pieces of tools/rules/check_rules.py that score_history uses: lex, pair_brackets and the
 * function list of Parsed (the _Parser scope walk, K&R definitions, aggregate bodies, the asm masking of the two-pass
 * parse), plus score_history.rough_functions. Temporary until the rule checker port can be linked
 * (DEVIATIONS.md, D-score_history-1). Only what decides Parsed.funcs (name, line, end_line) is ported: variables,
 * prototypes, typedefs and member lists are not collected. */
#include "sh_priv.h"

#include <stdlib.h>
#include <string.h>

/* ---- UTF-8 and text helpers -------------------------------------------------------------------------------------- */

static bool cont(u8 b) { return (b & 0xC0) == 0x80; }

AvpStr sh_decode_utf8(AvpArena *a, AvpStr bytes) {
    const u8 *s = (const u8 *)bytes.p;
    size_t n = bytes.n, i = 0;
    if (avp_u8_valid(bytes)) return bytes;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_reserve(&b, n + 16);
    while (i < n) {
        u8 c = s[i];
        if (c < 0x80) {
            avp_buf_putc(&b, (char)c);
            i++;
            continue;
        }
        size_t need = 0;
        u8 lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c == 0xE0) need = 2, lo = 0xA0;
        else if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF) need = 2;
        else if (c == 0xED) need = 2, hi = 0x9F;
        else if (c == 0xF0) need = 3, lo = 0x90;
        else if (c >= 0xF1 && c <= 0xF3) need = 3;
        else if (c == 0xF4) need = 3, hi = 0x8F;
        if (!need) {
            avp_buf_putcp(&b, 0xFFFD);
            i++;
            continue;
        }
        /* the maximal subpart: the lead byte and the valid continuation bytes that follow it */
        size_t k = 1;
        bool ok = true;
        while (k <= need) {
            if (i + k >= n) {
                ok = false;
                break;
            }
            u8 x = s[i + k];
            if (k == 1 ? (x < lo || x > hi) : !cont(x)) {
                ok = false;
                break;
            }
            k++;
        }
        if (ok) {
            avp_buf_putn(&b, s + i, need + 1);
            i += need + 1;
        } else {
            avp_buf_putcp(&b, 0xFFFD);
            i += k;
        }
    }
    return avp_buf_str(&b);
}

AvpStr sh_normalise(AvpArena *a, AvpStr text) {
    if (!memchr(text.p, '\r', text.n)) return text;
    char *o = avp_alloc_noz(a, text.n + 1);
    size_t m = 0;
    for (size_t i = 0; i < text.n; i++) {
        char c = text.p[i];
        if (c == '\r') {
            o[m++] = '\n';
            if (i + 1 < text.n && text.p[i + 1] == '\n') i++;
        } else {
            o[m++] = c;
        }
    }
    o[m] = 0;
    return avp_strn(o, m);
}

char *sh_sha1(AvpArena *a, AvpStr s) { return avp_sha1_hex(a, s.p, s.n); }

/* The code point at p (valid UTF-8) and its length. */
static u32 cp_at(const char *p, const char *end, size_t *len) {
    const char *q = p;
    u32 cp = avp_u8_next(&q, end);
    *len = (size_t)(q - p);
    return cp;
}

static bool cp_isdecimal(u32 cp) {
    if (cp < 0x80) return cp >= '0' && cp <= '9';
    char b[4];
    size_t n = avp_u8_put(b, cp);
    return avp_str_isdecimal(avp_strn(b, n));
}

static bool cp_isword(u32 cp) {
    if (cp < 0x80) return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_';
    char b[4];
    size_t n = avp_u8_put(b, cp);
    return avp_str_isalnum(avp_strn(b, n));
}

/* a decimal digit (\d) at p: its length, else 0 */
static size_t dec_at(const char *p, const char *end) {
    if (p >= end) return 0;
    if ((u8)*p < 0x80) return (*p >= '0' && *p <= '9') ? 1 : 0;
    size_t len;
    u32 cp = cp_at(p, end, &len);
    return cp_isdecimal(cp) ? len : 0;
}

static const char *skip_dec(const char *p, const char *end) {
    size_t l;
    while ((l = dec_at(p, end)) != 0) p += l;
    return p;
}

/* Unicode whitespace (\s) at p: its length, else 0 */
static size_t ws_at(const char *p, const char *end) {
    if (p >= end) return 0;
    u8 c = (u8)*p;
    if (c < 0x80) return ((c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x20)) ? 1 : 0;
    size_t len;
    u32 cp = cp_at(p, end, &len);
    return avp_cp_isspace(cp) ? len : 0;
}

static bool ishex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static int isdig_hex(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static bool in_set(char c, const char *set) { return c && strchr(set, c) != NULL; }

/* _NUM.match(text, i): the end, or NULL */
static const char *num_match(const char *p, const char *end) {
    if (p + 2 < end && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && ishex(p[2])) {
        const char *q = p + 2;
        while (q < end && ishex(*q)) q++;
        while (q < end && in_set(*q, "uUlL")) q++;
        return q;
    }
    const char *k;
    if (dec_at(p, end)) {
        k = skip_dec(p, end);
        if (k < end && *k == '.') k = skip_dec(k + 1, end);
    } else if (p < end && *p == '.' && dec_at(p + 1, end)) {
        k = skip_dec(p + 1, end);
    } else {
        return NULL;
    }
    if (k < end && (*k == 'e' || *k == 'E')) {
        const char *m = k + 1;
        if (m < end && (*m == '+' || *m == '-')) m++;
        if (dec_at(m, end)) k = skip_dec(m, end);
    }
    while (k < end && in_set(*k, "fFuUlL")) k++;
    return k;
}

static bool id_start(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$'; }
static bool id_char(char c) { return id_start(c) || (c >= '0' && c <= '9'); }

static const char *const OPS[] = {"<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                                  "&&",  "||",  "+=",  "-=", "*=", "/=", "%=", "&=", "|=", "^=", "::", "##"};

static size_t skip_quoted(const char *s, size_t n, size_t i, char q) {
    size_t j = i + 1;
    while (j < n) {
        char c = s[j];
        if (c == '\\') {
            j += 2;
            continue;
        }
        if (c == q || c == '\n') return j + 1;
        j++;
    }
    return n;
}

static size_t count_nl(const char *p, size_t n) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++) c += p[i] == '\n';
    return c;
}

static bool if_zero(AvpStr r) {
    const char *p = r.p, *e = r.p + r.n;
    size_t l;
    if (p < e && *p == '(') p++;
    while ((l = ws_at(p, e)) != 0) p += l;
    if (p >= e || *p != '0') return false;
    p++;
    while (p < e && in_set(*p, "lLuU")) p++;
    while ((l = ws_at(p, e)) != 0) p += l;
    if (p < e && *p == ')') p++;
    while ((l = ws_at(p, e)) != 0) p += l;
    return p == e;
}

static AvpStr rstrip_cr(AvpStr s) {
    while (s.n && s.p[s.n - 1] == '\r') s.n--;
    return s;
}

static void lex_impl(AvpArena *a, AvpStr text, int line, bool top, ShLex *out) {
    const char *s = text.p;
    size_t n = text.n, i = 0;
    bool bol = true, dead = false;
    int dead_nest = 0;
    size_t mark_t = 0, mark_c = 0;
    ShTokVec *toks = &out->toks;
    ShCommentVec *comments = &out->comments;
    while (i < n) {
        char c = s[i];
        if (c == '\n') {
            line++;
            i++;
            bol = true;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            i++;
            continue;
        }
        if (c == '\\' && i + 1 < n && (s[i + 1] == '\n' || s[i + 1] == '\r')) {
            i++;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            const char *nl = memchr(s + i, '\n', n - i);
            size_t j = nl ? (size_t)(nl - s) : n;
            bool trailing = toks->n && avp_vec_last(toks).line == line;
            avp_vec_push(comments, ((ShComment){line, line, rstrip_cr(avp_strn(s + i + 2, j - i - 2)), trailing}));
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i64 jf = avp_str_find(text, AVP_S("*/"), (i64)i + 2, AVP_END);
            size_t j = jf < 0 ? n : (size_t)jf;
            AvpStr body = avp_strn(s + i + 2, j - i - 2);
            bool trailing = toks->n && avp_vec_last(toks).line == line;
            int nl = (int)count_nl(body.p, body.n);
            avp_vec_push(comments, ((ShComment){line, line + nl, body, trailing}));
            line += nl;
            i = j + 2 < n ? j + 2 : n;
            continue;
        }
        if (c == '#' && bol && top) {
            int start_line = line;
            size_t c_before = comments->n;
            size_t j = i + 1;
            AvpBuf buf;
            avp_buf_init(&buf, a);
            while (j < n) {
                char ch = s[j];
                if (ch == '\\' && ((j + 1 < n && s[j + 1] == '\n') || (j + 2 < n && s[j + 1] == '\r' && s[j + 2] == '\n'))) {
                    j += (s[j + 1] == '\r') ? 3 : 2;
                    line++;
                    avp_buf_putc(&buf, ' ');
                    continue;
                }
                if (ch == '\n') break;
                if (ch == '/' && j + 1 < n && s[j + 1] == '/') {
                    const char *nl = memchr(s + j, '\n', n - j);
                    size_t k = nl ? (size_t)(nl - s) : n;
                    avp_vec_push(comments, ((ShComment){line, line, rstrip_cr(avp_strn(s + j + 2, k - j - 2)), true}));
                    j = k;
                    break;
                }
                if (ch == '/' && j + 1 < n && s[j + 1] == '*') {
                    i64 kf = avp_str_find(text, AVP_S("*/"), (i64)j + 2, AVP_END);
                    size_t k = kf < 0 ? n : (size_t)kf;
                    AvpStr body = avp_strn(s + j + 2, k - j - 2);
                    int nl = (int)count_nl(body.p, body.n);
                    avp_vec_push(comments, ((ShComment){line, line + nl, body, true}));
                    line += nl;
                    j = k + 2 < n ? k + 2 : n;
                    avp_buf_putc(&buf, ' ');
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    size_t k = skip_quoted(s, n, j, ch);
                    avp_buf_putn(&buf, s + j, k - j);
                    j = k;
                    continue;
                }
                avp_buf_putc(&buf, ch);
                j++;
            }
            AvpStr raw = avp_str_strip(avp_buf_str(&buf));
            /* re.match(r"(\w+)\s*(.*)$", raw, re.S) */
            const char *p = raw.p, *e = raw.p + raw.n;
            while (p < e) {
                size_t l;
                u32 cp = cp_at(p, e, &l);
                if (!cp_isword(cp)) break;
                p += l;
            }
            AvpStr name, rest;
            if (p == raw.p) {
                name = avp_strn("", 0);
                rest = raw;
            } else {
                name = avp_strn(raw.p, (size_t)(p - raw.p));
                size_t l;
                while ((l = ws_at(p, e)) != 0) p += l;
                rest = avp_strn(p, (size_t)(e - p));
            }
            i = j;
            if (dead) {
                if (avp_str_eqz(name, "if") || avp_str_eqz(name, "ifdef") || avp_str_eqz(name, "ifndef")) {
                    dead_nest++;
                } else if (dead_nest) {
                    if (avp_str_eqz(name, "endif")) dead_nest--;
                } else if (avp_str_eqz(name, "else") || avp_str_eqz(name, "elif") || avp_str_eqz(name, "endif")) {
                    /* kept = comments[c_before:]; del toks[mark_t:]; del comments[mark_c:]; comments.extend(kept) */
                    size_t nkept = comments->n - c_before;
                    ShComment *kept = avp_alloc(a, (nkept ? nkept : 1) * sizeof(ShComment));
                    if (nkept) memcpy(kept, comments->v + c_before, nkept * sizeof(ShComment));
                    toks->n = mark_t;
                    comments->n = mark_c;
                    for (size_t q = 0; q < nkept; q++) avp_vec_push(comments, kept[q]);
                    dead = false;
                    ShLex sub = {0};
                    lex_impl(a, rest, start_line, false, &sub);
                    sh_lex_free(&sub);
                    avp_vec_push(&out->directives, ((ShDirective){start_line, line, name, rest}));
                }
                continue;
            }
            ShLex sub = {0};
            lex_impl(a, rest, start_line, false, &sub);
            sh_lex_free(&sub);
            avp_vec_push(&out->directives, ((ShDirective){start_line, line, name, rest}));
            if (avp_str_eqz(name, "if") && if_zero(rest)) {
                dead = true;
                dead_nest = 0;
                mark_t = toks->n;
                mark_c = comments->n;
            }
            continue;
        }
        bol = false;
        if (c == '"' || c == '\'') {
            size_t j = skip_quoted(s, n, i, c);
            avp_vec_push(toks, ((ShTok){avp_strn(s + i, j - i), line, c == '"' ? SH_STR : SH_CHR}));
            i = j;
            continue;
        }
        const char *m = num_match(s + i, s + n);
        if (m) {
            avp_vec_push(toks, ((ShTok){avp_strn(s + i, (size_t)(m - (s + i))), line, SH_NUM}));
            i = (size_t)(m - s);
            continue;
        }
        if (id_start(c)) {
            size_t j = i + 1;
            while (j < n && id_char(s[j])) j++;
            avp_vec_push(toks, ((ShTok){avp_strn(s + i, j - i), line, SH_ID}));
            i = j;
            continue;
        }
        size_t oplen = 0;
        for (size_t q = 0; q < sizeof OPS / sizeof *OPS; q++) {
            size_t l = strlen(OPS[q]);
            if (i + l <= n && memcmp(s + i, OPS[q], l) == 0) {
                oplen = l;
                break;
            }
        }
        if (!oplen) {
            if (ws_at(s + i, s + n)) avp_throw(SH_EXC_CRASH, "AttributeError: 'NoneType' object has no attribute 'group'");
            cp_at(s + i, s + n, &oplen);
        }
        avp_vec_push(toks, ((ShTok){avp_strn(s + i, oplen), line, SH_OP}));
        i += oplen;
    }
    if (dead) {
        toks->n = mark_t;
        comments->n = mark_c;
    }
}

void sh_lex(AvpArena *a, AvpStr text, ShLex *out) {
    memset(out, 0, sizeof *out);
    lex_impl(a, text, 1, true, out);
}

void sh_lex_free(ShLex *lx) {
    avp_vec_free(&lx->toks);
    avp_vec_free(&lx->comments);
    avp_vec_free(&lx->directives);
}

static bool teq(const ShTok *t, const char *cz) {
    size_t l = strlen(cz);
    return t->t.n == l && memcmp(t->t.p, cz, l) == 0;
}

int *sh_pair_brackets(AvpArena *a, const ShTok *toks, size_t n) {
    int *match = avp_alloc_noz(a, (n ? n : 1) * sizeof(int));
    for (size_t i = 0; i < n; i++) match[i] = -1;
    AVP_VEC(int) stack = {0};
    for (size_t i = 0; i < n; i++) {
        const ShTok *t = &toks[i];
        if (t->k != SH_OP || t->t.n != 1) continue;
        char c = t->t.p[0];
        if (c == '(' || c == '[' || c == '{') {
            avp_vec_push(&stack, (int)i);
        } else if (c == ')' || c == ']' || c == '}') {
            char want = c == ')' ? '(' : c == ']' ? '[' : '{';
            for (size_t s = stack.n; s-- > 0;) {
                if (toks[stack.v[s]].t.p[0] == want) {
                    int o = stack.v[s];
                    stack.n = s;
                    match[o] = (int)i;
                    match[i] = o;
                    break;
                }
            }
        }
    }
    avp_vec_free(&stack);
    return match;
}

/* ---- the parser (_Parser.scope and the pieces it calls for function definitions) -------------------------------- */

typedef struct {
    AvpStr name;
    int line, end_line;
    i64 head, body, body_end;
    bool is_asm;
} PFunc;
typedef AVP_VEC(PFunc) PFuncVec;

typedef struct {
    AvpArena *a;
    const ShTok *toks;
    i64 n;
    const int *match;
    PFuncVec funcs;
} Parser;

enum { CTX_FILE, CTX_CLASS, CTX_NS };

static bool in_list(const ShTok *t, const char *const *list) {
    for (; *list; list++)
        if (teq(t, *list)) return true;
    return false;
}
static bool str_in(AvpStr s, const char *const *list) {
    for (; *list; list++)
        if (avp_str_eqz(s, *list)) return true;
    return false;
}

static const char *const DECL_MACROS[] = {"GLOBAL", "EXTERN", "LOCAL", "METHODDEF", NULL};
static const char *const DECL_KEYWORDS[] = {"static",   "register", "const",   "volatile", "struct",  "union",
                                            "enum",     "class",    "extern",  "auto",     "inline",  "typename",
                                            "mutable",  "virtual",  "unsigned", "signed",  NULL};
static const char *const BASIC_TYPE_WORDS[] = {"void",   "char", "short", "int",   "long",
                                               "float",  "double", "bool", "_Bool", "wchar_t", NULL};
static const char *const AGG_KW[] = {"struct", "class", "union", "enum", NULL};

static i64 mget(const Parser *p, i64 i) { return (i >= 0 && i < p->n) ? p->match[i] : -1; }

static i64 after_decl_macro(const ShTok *toks, i64 paren, i64 end) {
    int depth = 0;
    i64 q = paren;
    while (q < end) {
        if (teq(&toks[q], "(")) {
            depth++;
        } else if (teq(&toks[q], ")")) {
            depth--;
            if (depth == 0) break;
        }
        q++;
    }
    q++;
    while (q < end) {
        if (teq(&toks[q], "(")) return q;
        if (toks[q].k != SH_ID) break;
        q++;
    }
    return paren;
}

static bool is_open_bracket(const ShTok *t) { return teq(t, "(") || teq(t, "["); }
static bool is_close_bracket(const ShTok *t) { return teq(t, ")") || teq(t, "]"); }

/* _knr_definition: true with the '{' and '}' of the body */
static bool knr_definition(Parser *P, i64 paren_first, i64 semi, i64 end, i64 *brace, i64 *closing) {
    const ShTok *toks = P->toks;
    if (toks[paren_first - 1].k != SH_ID || in_list(&toks[paren_first - 1], DECL_MACROS)) return false;
    i64 rp = mget(P, paren_first);
    if (rp < 0 || rp >= semi) return false;
    AvpStrVec names = {0};
    bool ok = false;
    for (i64 x = paren_first + 1; x < rp; x++) {
        if (teq(&toks[x], ",")) continue;
        if (toks[x].k != SH_ID || in_list(&toks[x], DECL_KEYWORDS) || in_list(&toks[x], BASIC_TYPE_WORDS)) goto done;
        avp_vec_push(&names, toks[x].t);
    }
    if (rp + 1 >= semi) goto done;
    i64 k = rp + 1;
    for (;;) {
        i64 lo = k;
        while (k < end && !teq(&toks[k], ";") && !teq(&toks[k], "{") && !teq(&toks[k], "}") && !teq(&toks[k], "=")) {
            if (toks[k].k != SH_ID && toks[k].k != SH_NUM && !teq(&toks[k], "*") && !teq(&toks[k], ",") &&
                !teq(&toks[k], "[") && !teq(&toks[k], "]"))
                goto done;
            k++;
        }
        if (k >= end || !teq(&toks[k], ";") || k == lo) goto done;
        /* parts split at depth-0 commas (depth over [ ]); each part's name is its last depth-0 identifier */
        i64 part_lo = lo;
        int bdepth = 0;
        for (i64 x = lo; x <= k; x++) {
            bool at_end = x == k;
            if (!at_end) {
                if (teq(&toks[x], "[")) bdepth++;
                else if (teq(&toks[x], "]")) bdepth--;
            }
            if (at_end || (teq(&toks[x], ",") && bdepth == 0)) {
                int depth = 0;
                i64 name_at = -1;
                for (i64 z = part_lo; z < x; z++) {
                    if (teq(&toks[z], "[")) depth++;
                    else if (teq(&toks[z], "]")) depth--;
                    else if (depth == 0 && toks[z].k == SH_ID) name_at = z;
                }
                if (name_at < 0) goto done;
                bool known = false;
                for (size_t q = 0; q < names.n; q++)
                    if (avp_str_eq(names.v[q], toks[name_at].t)) known = true;
                if (!known) goto done;
                part_lo = x + 1;
            }
        }
        k++;
        if (k < end && teq(&toks[k], "{")) break;
    }
    i64 cl = mget(P, k);
    if (cl < 0) goto done;
    *brace = k;
    *closing = cl;
    ok = true;
done:
    avp_vec_free(&names);
    return ok;
}

/* function(head, i, paren_first, brace, closing, ctx, cls) */
static void function(Parser *P, i64 hs, i64 he, i64 i, i64 paren_first, i64 brace, i64 closing, AvpStr cls) {
    const ShTok *toks = P->toks;
    PFunc f = {0};
    f.head = i;
    f.line = he > hs ? toks[hs].line : toks[brace].line;
    f.body = brace;
    f.body_end = closing;
    f.end_line = toks[closing].line;
    int d = 0;
    for (i64 x = hs; x < he; x++) {
        if (is_open_bracket(&toks[x])) d++;
        else if (is_close_bracket(&toks[x])) d--;
        else if (d == 0 && toks[x].k == SH_ID && (teq(&toks[x], "asm") || teq(&toks[x], "__asm"))) f.is_asm = true;
    }
    /* the name from pre = toks[i:paren_first] */
    const ShTok *pre = toks + i;
    i64 np = paren_first - i;
    i64 k = np - 1, op_idx = -1;
    AvpBuf nb;
    avp_buf_init(&nb, P->a);
    for (i64 q = 0; q < np; q++)
        if (teq(&pre[q], "operator")) op_idx = q;
    if (op_idx >= 0) {
        AvpBuf ob;
        avp_buf_init(&ob, P->a);
        avp_buf_putz(&ob, "operator");
        for (i64 q = op_idx + 1; q < np; q++) avp_buf_put(&ob, pre[q].t);
        k = op_idx - 1;
        if (k >= 1 && teq(&pre[k], "::")) {
            avp_buf_put(&nb, pre[k - 1].t);
            avp_buf_putz(&nb, "::");
        }
        avp_buf_put(&nb, avp_buf_str(&ob));
    } else if (k >= 0 && pre[k].k == SH_ID) {
        AvpStr parts[512];
        size_t nparts = 0;
        AvpStr first = pre[k].t;
        if (k >= 1 && teq(&pre[k - 1], "~")) {
            first = avp_str_concat(P->a, AVP_S("~"), first);
            k--;
        }
        k--;
        /* collected in reverse */
        parts[nparts++] = first;
        while (k >= 1 && teq(&pre[k], "::") && pre[k - 1].k == SH_ID) {
            if (nparts < 512) parts[nparts++] = pre[k - 1].t;
            k -= 2;
        }
        for (size_t q = nparts; q-- > 0;) {
            avp_buf_put(&nb, parts[q]);
            if (q) avp_buf_putz(&nb, "::");
        }
    } else {
        avp_buf_putz(&nb, "<anon>");
    }
    AvpStr name = avp_buf_str(&nb);
    if (name.n == 0) name = avp_strn("", 0);
    if (cls.n && !avp_str_contains(name, AVP_S("::"))) {
        AvpBuf cb;
        avp_buf_init(&cb, P->a);
        avp_buf_put(&cb, cls);
        avp_buf_putz(&cb, "::");
        avp_buf_put(&cb, name);
        name = avp_buf_str(&cb);
    }
    f.name = name;
    avp_vec_push(&P->funcs, f);
}

static void scope(Parser *P, i64 i, i64 end, int ctx, AvpStr cls) {
    const ShTok *toks = P->toks;
    while (i < end) {
        const ShTok *t = &toks[i];
        if (teq(t, ";")) {
            i++;
            continue;
        }
        if (ctx == CTX_CLASS && (teq(t, "public") || teq(t, "private") || teq(t, "protected")) && i + 1 < end &&
            teq(&toks[i + 1], ":")) {
            i += 2;
            continue;
        }
        i64 j = i, paren_first = -1;
        bool assign = false;
        char stop = 0;
        while (j < end) {
            const ShTok *x = &toks[j];
            if (x->k == SH_OP) {
                if (teq(x, "(") || teq(x, "[")) {
                    if (teq(x, "(") && paren_first < 0 && !assign &&
                        !(j > i && (teq(&toks[j - 1], "__declspec") || teq(&toks[j - 1], "__attribute__") ||
                                    teq(&toks[j - 1], "__attribute") || teq(&toks[j - 1], "alignas"))))
                        paren_first = j;
                    i64 m = mget(P, j);
                    j = m >= 0 ? m + 1 : j + 1;
                    continue;
                }
                if (teq(x, "=")) {
                    assign = true;
                } else if (teq(x, ";") || teq(x, "}")) {
                    stop = x->t.p[0];
                    break;
                } else if (teq(x, "{")) {
                    if (assign) {
                        i64 m = mget(P, j);
                        j = m >= 0 ? m + 1 : j + 1;
                        continue;
                    }
                    stop = '{';
                    break;
                }
            }
            j++;
        }
        if (j >= end) return; /* decl(toks[i:end]): declarations only */
        i64 hs = i, he = j;   /* head = toks[i:j] */
        if (stop == ';' && paren_first > i && ctx != CTX_CLASS) {
            i64 brace, closing;
            if (knr_definition(P, paren_first, j, end, &brace, &closing)) {
                i64 rp = mget(P, paren_first);
                function(P, i, rp + 1, i, paren_first, brace, closing, cls);
                i = closing + 1;
                continue;
            }
        }
        if (stop == ';' || stop == '}') {
            i = j + 1;
            continue;
        }
        i64 closing = mget(P, j);
        if (closing < 0) return;
        /* `class T` / `typename T` in a template parameter list declares a parameter, not a type */
        i64 hn = he - hs;
        u8 *in_tparams = avp_alloc(P->a, (size_t)(hn ? hn : 1));
        int depth = 0;
        for (i64 k = 0; k < hn; k++) {
            const ShTok *h = &toks[hs + k];
            if (teq(h, "<") && k > 0 && (teq(&toks[hs + k - 1], "template") || depth > 0)) depth++;
            else if (teq(h, ">") && depth > 0) depth--;
            else if (depth > 0) in_tparams[k] = 1;
        }
        i64 kw_idx = -1;
        for (i64 k = 0; k < hn; k++) {
            if (in_tparams[k]) continue;
            const ShTok *h = &toks[hs + k];
            if (in_list(h, AGG_KW) || teq(h, "namespace")) {
                if (paren_first < 0 || i + k < paren_first) {
                    kw_idx = k;
                    break;
                }
            }
        }
        if (hn == 2 && teq(&toks[hs], "extern") && toks[hs + 1].k == SH_STR) {
            scope(P, j + 1, closing, ctx, cls);
            i = closing + 1;
            continue;
        }
        if (kw_idx >= 0 && teq(&toks[hs + kw_idx], "namespace")) {
            scope(P, j + 1, closing, CTX_NS, avp_strn("", 0));
            i = closing + 1;
            continue;
        }
        if (kw_idx >= 0) {
            AvpStr kind = toks[hs + kw_idx].t;
            i64 k = kw_idx + 1;
            while (k < hn && toks[hs + k].k == SH_ID &&
                   (teq(&toks[hs + k], "__attribute__") || teq(&toks[hs + k], "__declspec")))
                k += 2;
            AvpStr name = {"", 0};
            if (k < hn && toks[hs + k].k == SH_ID && !teq(&toks[hs + k], "final")) name = toks[hs + k].t;
            /* trailing declarators up to ';' */
            i64 e = closing + 1;
            AvpStr last_id = {NULL, 0};
            bool tail = false;
            while (e < end && !teq(&toks[e], ";")) {
                if ((teq(&toks[e], "(") || teq(&toks[e], "[") || teq(&toks[e], "{")) && mget(P, e) >= 0) e = mget(P, e);
                tail = true;
                if (toks[e].k == SH_ID) last_id = toks[e].t;
                e++;
            }
            bool is_typedef = false;
            for (i64 q = hs; q < he; q++)
                if (teq(&toks[q], "typedef")) is_typedef = true;
            if (is_typedef && tail && last_id.p && name.n == 0) name = last_id;
            if (avp_str_eqz(kind, "struct") || avp_str_eqz(kind, "class") || avp_str_eqz(kind, "union")) {
                AvpStr sub = cls;
                if (name.n) {
                    if (cls.n == 0) {
                        sub = name;
                    } else {
                        AvpBuf cb;
                        avp_buf_init(&cb, P->a);
                        avp_buf_put(&cb, cls);
                        avp_buf_putz(&cb, "::");
                        avp_buf_put(&cb, name);
                        sub = avp_buf_str(&cb);
                    }
                }
                scope(P, j + 1, closing, CTX_CLASS, sub);
            }
            i = e + 1;
            continue;
        }
        if (paren_first > i && in_list(&toks[paren_first - 1], DECL_MACROS))
            paren_first = after_decl_macro(toks, paren_first, j);
        if (paren_first >= 0) {
            function(P, hs, he, i, paren_first, j, closing, cls);
            i = closing + 1;
            if (i < end && teq(&toks[i], ";")) i++;
            continue;
        }
        i = closing + 1;
    }
}

static void parse(Parser *P) { scope(P, 0, P->n, CTX_FILE, avp_strn("", 0)); }

typedef struct {
    i64 lo, hi;
} Mask;

static int mask_cmp(const void *x, const void *y) {
    const Mask *a = x, *b = y;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    return 0;
}

static bool is_asm_word(const ShTok *t) { return teq(t, "asm") || teq(t, "__asm") || teq(t, "__asm__"); }

void sh_parsed_funcs(AvpArena *a, const ShLex *lx, ShFuncVec *out) {
    const ShTok *toks = lx->toks.v;
    i64 n = (i64)lx->toks.n;
    int *raw_match = sh_pair_brackets(a, toks, (size_t)n);
    Parser probe = {a, toks, n, raw_match, {0}};
    parse(&probe);
    /* _find_asm_sites: the masks */
    AVP_VEC(Mask) masks = {0};
    for (size_t q = 0; q < probe.funcs.n; q++)
        if (probe.funcs.v[q].is_asm) avp_vec_push(&masks, ((Mask){probe.funcs.v[q].body + 1, probe.funcs.v[q].body_end}));
    for (i64 i = 0; i < n; i++) {
        const ShTok *t = &toks[i];
        if (t->k != SH_ID || !is_asm_word(t)) continue;
        i64 j = i + 1;
        while (j < n && (teq(&toks[j], "volatile") || teq(&toks[j], "__volatile__"))) j++;
        if (j < n && teq(&toks[j], "(") && raw_match[j] >= 0) continue; /* GCC-style asm(...): no mask */
        if (j < n && teq(&toks[j], "{") && raw_match[j] >= 0) {
            const PFunc *enclosing = NULL;
            bool is_body = false;
            for (size_t q = 0; q < probe.funcs.n; q++) {
                const PFunc *f = &probe.funcs.v[q];
                if (f->body < i && i < f->body_end) enclosing = f;
                if (f->body == j) is_body = true;
            }
            if (enclosing && enclosing->is_asm) continue;
            if (is_body) continue;
            avp_vec_push(&masks, ((Mask){j + 1, raw_match[j]}));
        }
    }
    const ShTok *ftoks = toks;
    i64 fn = n;
    ShTokVec masked = {0};
    if (masks.n) {
        qsort(masks.v, masks.n, sizeof(Mask), mask_cmp);
        i64 i = 0;
        size_t mi = 0;
        while (i < n) {
            if (mi < masks.n && i == masks.v[mi].lo) {
                int line = toks[i].line;
                avp_vec_push(&masked, ((ShTok){AVP_S("__ASM_BODY__"), line, SH_ID}));
                avp_vec_push(&masked, ((ShTok){AVP_S(";"), line, SH_OP}));
                i = masks.v[mi].hi > i ? masks.v[mi].hi : i;
                mi++;
                while (mi < masks.n && masks.v[mi].lo < i) mi++;
                continue;
            }
            avp_vec_push(&masked, toks[i]);
            i++;
        }
        ftoks = masked.v;
        fn = (i64)masked.n;
    }
    avp_vec_free(&masks);
    int *match = sh_pair_brackets(a, ftoks, (size_t)fn);
    Parser p = {a, ftoks, fn, match, {0}};
    parse(&p);
    for (size_t q = 0; q < p.funcs.n; q++)
        avp_vec_push(out, ((ShFunc){p.funcs.v[q].name, p.funcs.v[q].line, p.funcs.v[q].end_line}));
    avp_vec_free(&p.funcs);
    avp_vec_free(&probe.funcs);
    avp_vec_free(&masked);
}

/* ---- rough_functions --------------------------------------------------------------------------------------------- */

static const char *const KEYWORDS[] = {"if",       "else",       "while",         "for",          "do",
                                       "switch",   "return",     "sizeof",        "case",         "catch",
                                       "new",      "delete",     "defined",       "typeof",       "__typeof__",
                                       "__declspec", "__attribute__", "asm",      "__asm",        NULL};

bool sh_is_keyword(AvpStr s) { return str_in(s, KEYWORDS); }

void sh_rough_functions(AvpArena *a, const ShTok *toks, size_t nt, ShFuncVec *out) {
    int *match = sh_pair_brackets(a, toks, nt);
    i64 n = (i64)nt, i = 0, stmt = 0;
    while (i < n) {
        const ShTok *t = &toks[i];
        if (teq(t, "{")) {
            i64 j = match[i];
            i = j >= 0 ? j + 1 : i + 1;
            stmt = i;
            continue;
        }
        if (teq(t, ";")) {
            i++;
            stmt = i;
            continue;
        }
        if (teq(t, "(") && i > stmt && toks[i - 1].k == SH_ID && !str_in(toks[i - 1].t, KEYWORDS)) {
            i64 j = match[i];
            if (j < 0) {
                i++;
                continue;
            }
            i64 k = j + 1;
            while (k < n && (teq(&toks[k], "const") || teq(&toks[k], "volatile"))) k++;
            if (k < n && teq(&toks[k], ":")) {
                i64 m = k;
                while (m < n && !teq(&toks[m], "{") && !teq(&toks[m], ";"))
                    m = (teq(&toks[m], "(") && match[m] >= 0) ? match[m] + 1 : m + 1;
                k = m;
            }
            if (k < n && teq(&toks[k], "{") && match[k] >= 0) {
                AvpStr parts[512];
                size_t np = 0;
                AvpStr first = toks[i - 1].t;
                i64 q = i - 2;
                if (q >= stmt && teq(&toks[q], "~")) {
                    first = avp_str_concat(a, AVP_S("~"), first);
                    q--;
                }
                parts[np++] = first;
                while (q - 1 >= stmt && teq(&toks[q], "::") && toks[q - 1].k == SH_ID) {
                    if (np < 512) parts[np++] = toks[q - 1].t;
                    q -= 2;
                }
                AvpBuf nb;
                avp_buf_init(&nb, a);
                for (size_t z = np; z-- > 0;) {
                    avp_buf_put(&nb, parts[z]);
                    if (z) avp_buf_putz(&nb, "::");
                }
                avp_vec_push(out, ((ShFunc){avp_buf_str(&nb), toks[stmt].line, toks[match[k]].line}));
                i = match[k] + 1;
                stmt = i;
                continue;
            }
            i = j + 1;
            continue;
        }
        i++;
    }
}

/* (?m)^\s*(0x8[0-9A-Fa-f]{7})\s*$ searched in text: the leftmost match's group is the first such number with only
   whitespace before it on its line and only whitespace after it up to the end of its line (or of the text). */
bool sh_addr_in_comment(AvpStr text, i64 *addr) {
    const char *s = text.p, *e = text.p + text.n;
    for (const char *p = s; p + 10 <= e; p++) {
        if (p[0] != '0' || p[1] != 'x' || p[2] != '8') continue;
        bool hex = true;
        for (int k = 3; k < 10; k++)
            if (!ishex(p[k])) hex = false;
        if (!hex) continue;
        /* before: back to the line start over whitespace other than newlines */
        const char *r = p;
        while (r > s) {
            const char *q = r - 1;
            while (q > s && ((u8)*q & 0xC0) == 0x80) q--;
            if (*q == '\n' || !ws_at(q, e)) break;
            r = q;
        }
        if (r != s && r[-1] != '\n') continue;
        const char *q = p + 10;
        size_t l;
        while (q < e && *q != '\n' && (l = ws_at(q, e)) != 0) q += l;
        if (q != e && *q != '\n') continue;
        i64 v = 0;
        for (int k = 2; k < 10; k++) v = v * 16 + (isdig_hex(p[k]));
        *addr = v;
        return true;
    }
    return false;
}
