/* rules_lex: lexer of tools/rules/check_rules.py (lines 196-400). See rules_lex.h and rules_lex.md. */
#include "rules_lex.h"
#include "shsm_re.h"

const char *const rl_kind_names[5] = {"id", "num", "str", "chr", "op"};

void rl_vec_grow(AvpArena *a, void **v, size_t *cap, size_t need, size_t elem) {
    if (need <= *cap) return;
    size_t nc = *cap ? *cap * 2 : 16;
    while (nc < need) nc *= 2;
    *v = avp_arena_realloc(a, *v, *cap * elem, nc * elem);
    *cap = nc;
}

bool rl_sin(AvpStr s, const char *const *words) {
    for (; *words; words++) {
        size_t n = strlen(*words);
        if (s.n == n && memcmp(s.p, *words, n) == 0) return true;
    }
    return false;
}

bool rl_tin(const RlTok *t, const char *const *words) { return rl_sin(t->t, words); }

bool rl_read_source(AvpArena *a, const char *path, AvpStr *out) {
    return avp_read_text(a, path, AVP_TM_UTF8_REPLACE, out);
}

/* Byte length of the UTF-8 sequence led by byte b (the text is valid UTF-8). */
static size_t cp_len(unsigned char b) {
    if (b < 0x80) return 1;
    if (b >= 0xF0) return 4;
    if (b >= 0xE0) return 3;
    if (b >= 0xC0) return 2;
    return 1;
}

size_t rl_skip_quoted(AvpStr text, size_t i, char q) {
    size_t n = text.n;
    size_t j = i + 1;
    while (j < n) {
        char c = text.p[j];
        if (c == '\\') {
            /* j += 2 in code points: the backslash and the character after it */
            j += 1 + (j + 1 < n ? cp_len((unsigned char)text.p[j + 1]) : 1);
            continue;
        }
        if (c == q || c == '\n') return j + 1;
        j++;
    }
    return n;
}

/* Byte length of the code point at s[i] when it is a Unicode decimal digit (re's \d for str), else 0. */
static size_t dec_at(const char *s, size_t n, size_t i) {
    if (i >= n) return 0;
    unsigned char b = (unsigned char)s[i];
    if (b < 0x80) return (b >= '0' && b <= '9') ? 1 : 0;
    size_t l = cp_len(b);
    if (i + l > n) return 0;
    return avp_str_isdecimal(avp_strn(s + i, l)) ? l : 0;
}

static size_t dec_run(const char *s, size_t n, size_t i) {
    size_t j = i, l;
    while ((l = dec_at(s, n, j)) != 0) j += l;
    return j - i;
}

static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

/* _NUM.match(text, i): the match length, or 0 when there is none.
 * 0[xX][0-9a-fA-F]+[uUlL]*|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuUlL]* */
static size_t num_match(const char *s, size_t n, size_t i) {
    if (i + 2 < n && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X') && is_hex(s[i + 2])) {
        size_t j = i + 2;
        while (j < n && is_hex(s[j])) j++;
        while (j < n && s[j] && strchr("uUlL", s[j])) j++;
        return j - i;
    }
    size_t j = i;
    size_t d = dec_run(s, n, i);
    if (d > 0) {
        j = i + d;
        if (j < n && s[j] == '.') {
            j++;
            j += dec_run(s, n, j);
        }
    } else if (i < n && s[i] == '.' && dec_at(s, n, i + 1)) {
        j = i + 1;
        j += dec_run(s, n, j);
    } else {
        return 0;
    }
    if (j < n && (s[j] == 'e' || s[j] == 'E')) {
        size_t k = j + 1;
        if (k < n && (s[k] == '+' || s[k] == '-')) k++;
        size_t e = dec_run(s, n, k);
        if (e > 0) j = k + e;
    }
    while (j < n && s[j] && strchr("fFuUlL", s[j])) j++;
    return j - i;
}

static bool id_start(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$'; }
static bool id_char(char c) { return id_start(c) || (c >= '0' && c <= '9'); }

static const char *const MULTI_OPS[] = {"<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                                        "&&",  "||",  "+=",  "-=", "*=", "/=", "%=", "&=", "|=", "^=", "::", "##"};

static bool starts(const char *s, size_t n, size_t i, const char *lit) {
    size_t l = strlen(lit);
    return i + l <= n && memcmp(s + i, lit, l) == 0;
}

static i64 find_from(const char *s, size_t n, size_t i, const char *lit) {
    return avp_str_find(avp_strn(s, n), avp_str(lit), (i64)i, AVP_END);
}

static void push_tok(AvpArena *a, RlToks *v, u8 k, const char *p, size_t len, i32 line) {
    RlTok *t = RL_PUSH(a, v);
    t->k = k;
    t->t = avp_strn(p, len);
    t->line = line;
}

static void push_comment(AvpArena *a, RlComments *v, i32 line, i32 end_line, AvpStr text, bool trailing) {
    RlComment *c = RL_PUSH(a, v);
    c->line = line;
    c->end_line = end_line;
    c->text = text;
    c->trailing = trailing;
}

static size_t count_nl(const char *p, size_t n) {
    size_t c = 0;
    for (size_t i = 0; i < n; i++) c += p[i] == '\n';
    return c;
}

static AvpStr rstrip_cr(AvpStr s) {
    while (s.n && s.p[s.n - 1] == '\r') s.n--;
    return s;
}

RlLexed rl_lex(AvpArena *a, AvpStr text, i32 line, bool top) {
    RlLexed o = {0};
    o.ok = true;
    const char *s = text.p;
    size_t n = text.n, i = 0;
    bool bol = true;
    /* `#if 0` arms are compiled out: their tokens and comments are dropped when the arm closes */
    bool dead = false;
    int dead_nest = 0;
    size_t mark_t = 0, mark_c = 0;
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
        if (starts(s, n, i, "//")) {
            i64 j = find_from(s, n, i, "\n");
            size_t e = j < 0 ? n : (size_t)j;
            bool trailing = o.toks.n && o.toks.v[o.toks.n - 1].line == line;
            push_comment(a, &o.comments, line, line, rstrip_cr(avp_strn(s + i + 2, e - i - 2)), trailing);
            i = e;
            continue;
        }
        if (starts(s, n, i, "/*")) {
            i64 j = find_from(s, n, i + 2, "*/");
            size_t e = j < 0 ? n : (size_t)j;
            AvpStr body = avp_strn(s + i + 2, e - i - 2);
            bool trailing = o.toks.n && o.toks.v[o.toks.n - 1].line == line;
            i32 nl = (i32)count_nl(body.p, body.n);
            push_comment(a, &o.comments, line, line + nl, body, trailing);
            line += nl;
            i = e + 2 < n ? e + 2 : n;
            continue;
        }
        if (c == '#' && bol && top) {
            i32 start_line = line;
            size_t c_before = o.comments.n;
            size_t j = i + 1;
            AvpBuf buf;
            avp_buf_init(&buf, a);
            while (j < n) {
                char ch = s[j];
                if (ch == '\\' && (starts(s, n, j + 1, "\n") || starts(s, n, j + 1, "\r\n"))) {
                    j += starts(s, n, j + 1, "\r\n") ? 3 : 2;
                    line++;
                    avp_buf_putc(&buf, ' ');
                    continue;
                }
                if (ch == '\n') break;
                if (starts(s, n, j, "//")) {
                    i64 k = find_from(s, n, j, "\n");
                    size_t e = k < 0 ? n : (size_t)k;
                    push_comment(a, &o.comments, line, line, rstrip_cr(avp_strn(s + j + 2, e - j - 2)), true);
                    j = e;
                    break;
                }
                if (starts(s, n, j, "/*")) {
                    i64 k = find_from(s, n, j + 2, "*/");
                    size_t e = k < 0 ? n : (size_t)k;
                    AvpStr body = avp_strn(s + j + 2, e - j - 2);
                    i32 nl = (i32)count_nl(body.p, body.n);
                    push_comment(a, &o.comments, line, line + nl, body, true);
                    line += nl;
                    j = e + 2 < n ? e + 2 : n;
                    avp_buf_putc(&buf, ' ');
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    size_t k = rl_skip_quoted(text, j, ch);
                    avp_buf_putn(&buf, s + j, k - j);
                    j = k;
                    continue;
                }
                avp_buf_putc(&buf, ch);
                j++;
            }
            AvpStr raw = avp_str_strip(avp_buf_str(&buf));
            AvpStr name = {"", 0}, rest = raw;
            ShsmReMatch m;
            shsm_re_assume_valid(raw);
            if (shsm_re_match(shsm_re("(\\w+)\\s*(.*)$", SHSM_RE_S), raw, 0, AVP_END, &m)) {
                name = shsm_re_group(&m, 1);
                rest = shsm_re_group(&m, 2);
            }
            i = j;
            if (dead) {
                static const char *const OPENERS[] = {"if", "ifdef", "ifndef", NULL};
                static const char *const CLOSERS[] = {"else", "elif", "endif", NULL};
                if (rl_sin(name, OPENERS)) {
                    dead_nest++;
                } else if (dead_nest) {
                    if (avp_str_eqz(name, "endif")) dead_nest--;
                } else if (rl_sin(name, CLOSERS)) {
                    /* kept = comments[c_before:]; del toks[mark_t:]; del comments[mark_c:]; comments.extend(kept) */
                    size_t nk = o.comments.n - c_before;
                    if (nk) memmove(&o.comments.v[mark_c], &o.comments.v[c_before], nk * sizeof(RlComment));
                    o.comments.n = mark_c + nk;
                    o.toks.n = mark_t;
                    dead = false;
                    RlLexed sub = rl_lex(a, rest, start_line, false);
                    if (!sub.ok) return sub;
                    RlDirective *d = RL_PUSH(a, &o.directives);
                    d->line = start_line;
                    d->end_line = line;
                    d->name = name;
                    d->rest = rest;
                    d->toks = sub.toks;
                }
                continue;
            }
            RlLexed sub = rl_lex(a, rest, start_line, false);
            if (!sub.ok) return sub;
            RlDirective *d = RL_PUSH(a, &o.directives);
            d->line = start_line;
            d->end_line = line;
            d->name = name;
            d->rest = rest;
            d->toks = sub.toks;
            if (avp_str_eqz(name, "if") && shsm_re_match(shsm_re("\\(?\\s*0[lLuU]*\\s*\\)?\\s*$", 0), rest, 0, AVP_END, NULL)) {
                dead = true;
                dead_nest = 0;
                mark_t = o.toks.n;
                mark_c = o.comments.n;
            }
            continue;
        }
        bol = false;
        if (c == '"' || c == '\'') {
            size_t j = rl_skip_quoted(text, i, c);
            push_tok(a, &o.toks, c == '"' ? RL_STR : RL_CHR, s + i, j - i, line);
            i = j;
            continue;
        }
        size_t l = num_match(s, n, i);
        if (l) {
            push_tok(a, &o.toks, RL_NUM, s + i, l, line);
            i += l;
            continue;
        }
        if (id_start(c)) {
            size_t j = i + 1;
            while (j < n && id_char(s[j])) j++;
            push_tok(a, &o.toks, RL_ID, s + i, j - i, line);
            i = j;
            continue;
        }
        size_t ol = 0;
        for (size_t q = 0; q < sizeof MULTI_OPS / sizeof *MULTI_OPS; q++) {
            if (starts(s, n, i, MULTI_OPS[q])) {
                ol = strlen(MULTI_OPS[q]);
                break;
            }
        }
        if (!ol) {
            /* \S: one code point; a whitespace one is _OP.match None (AttributeError in the Python) */
            ol = cp_len((unsigned char)c);
            if (i + ol > n) ol = n - i;
            const char *p = s + i;
            u32 cp = avp_u8_next(&p, s + i + ol);
            if (avp_cp_isspace(cp)) {
                o.ok = false;
                o.error_line = line;
                return o;
            }
        }
        push_tok(a, &o.toks, RL_OP, s + i, ol, line);
        i += ol;
    }
    if (dead) {
        o.toks.n = mark_t;
        o.comments.n = mark_c;
    }
    return o;
}

i32 *rl_pair_brackets(AvpArena *a, const RlTok *toks, size_t n) {
    i32 *match = AVP_NEWN(a, i32, n ? n : 1);
    for (size_t i = 0; i < n; i++) match[i] = -1;
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    i32 *stack = AVP_NEWN(avp_scratch(), i32, n ? n : 1);
    size_t sp = 0;
    for (size_t i = 0; i < n; i++) {
        const RlTok *t = &toks[i];
        if (t->k != RL_OP || t->t.n != 1) continue;
        char ch = t->t.p[0];
        if (ch == '(' || ch == '[' || ch == '{') {
            stack[sp++] = (i32)i;
            continue;
        }
        char want = ch == ')' ? '(' : ch == ']' ? '[' : ch == '}' ? '{' : 0;
        if (!want) continue;
        /* tolerate imbalance: pop to the matching opener */
        for (size_t s = sp; s-- > 0;) {
            if (toks[stack[s]].t.p[0] == want) {
                i32 o = stack[s];
                sp = s;
                match[o] = (i32)i;
                match[i] = o;
                break;
            }
        }
    }
    avp_arena_reset(avp_scratch(), mk);
    return match;
}

AvpStr rl_ptext(AvpArena *a, const RlTok *toks, size_t n) {
    size_t len = 0;
    for (size_t i = 0; i < n; i++) len += toks[i].t.n + (i ? 1 : 0);
    char *p = avp_alloc_noz(a, len + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) p[o++] = ' ';
        memcpy(p + o, toks[i].t.p, toks[i].t.n);
        o += toks[i].t.n;
    }
    p[o] = 0;
    return avp_strn(p, o);
}

AvpStr rl_ptext_ptrs(AvpArena *a, const RlTok *const *toks, size_t n) {
    size_t len = 0;
    for (size_t i = 0; i < n; i++) len += toks[i]->t.n + (i ? 1 : 0);
    char *p = avp_alloc_noz(a, len + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) p[o++] = ' ';
        memcpy(p + o, toks[i]->t.p, toks[i]->t.n);
        o += toks[i]->t.n;
    }
    p[o] = 0;
    return avp_strn(p, o);
}
