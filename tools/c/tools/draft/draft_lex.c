/* levers.py's tokenizer, bracket matcher and definition scanner (tokenize ... scan_definitions, levers.py lines
 * 71-318) and draft.py's source scanning on top of them (scan_source, find_def, includes_of, class_header,
 * extern_of, top_level_statements, class_block). Offsets are bytes into the UTF-8 text (draft.h). */
#include "draft.h"

#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

/* ---- tokens ----------------------------------------------------------------------------------------------------- */

static const char *const KIND_NAMES[] = {"id", "num", "str", "chr", "op", "comment", "pp"};

const char *draft_tok_kind_name(DraftTokKind k) { return KIND_NAMES[k]; }

bool draft_tok_is(const DraftTok *t, const char *text) { return avp_str_eqz(t->t, text); }

/* levers._OPS sorted by length, longest first (sorted() is stable: same-length operators keep their order) */
static const char *const OPS[] = {"<<=", ">>=", "...", "->*", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                                  "&&",  "||",  "+=",  "-=",  "*=", "/=", "%=", "&=", "|=", "^=", "::", "##"};

static bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_id_start(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$'; }
static bool is_id_char(char c) { return is_id_start(c) || is_digit(c); }

/* bytes of the UTF-8 sequence starting at p[i] (the text is valid UTF-8) */
static size_t u8_width(const char *p, size_t i, size_t n) {
    unsigned char c = (unsigned char)p[i];
    size_t w = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return i + w > n ? n - i : w;
}

/* Python's \d / \w on the one non-ASCII code point text[i:i+w] */
static bool u8_class(const char *pattern, const char *p, size_t w) {
    return shsm_re_fullmatch(shsm_re(pattern, 0), avp_strn(p, w), 0, AVP_END, NULL);
}

/* str.isdigit() of the code point at text[i] as far as _NUM can start there: ASCII digits, and the non-ASCII code
 * points \d matches (Python's isdigit() also takes superscripts \d does not: the Python then fails on _NUM's None,
 * DEVIATIONS.md D-draft-2) */
static bool num_digit(AvpStr text, size_t i) {
    if (i >= text.n) return false;
    unsigned char c = (unsigned char)text.p[i];
    if (c < 0x80) return is_digit((char)c);
    return u8_class("\\d", text.p + i, u8_width(text.p, i, text.n));
}

/* levers._NUM.match(text, i): (?:\d|\.\d)(?:[eEpP][+-]|[\w.])* -> its end */
static size_t num_end(AvpStr text, size_t i) {
    size_t n = text.n;
    i += text.p[i] == '.' ? 1 + u8_width(text.p, i + 1, n) : u8_width(text.p, i, n);
    while (i < n) {
        char c = text.p[i];
        if ((c == 'e' || c == 'E' || c == 'p' || c == 'P') && i + 1 < n && (text.p[i + 1] == '+' || text.p[i + 1] == '-')) {
            i += 2;
        } else if (is_id_char(c) && c != '$') {
            i++;
        } else if (c == '.') {
            i++;
        } else if ((unsigned char)c >= 0x80) {
            size_t w = u8_width(text.p, i, n);
            if (!u8_class("\\w", text.p + i, w)) break;
            i += w;
        } else {
            break;
        }
    }
    return i;
}

static void push_tok(DraftToks *v, DraftTokKind k, AvpStr text, i64 s, i64 e) {
    avp_vec_push(v, ((DraftTok){k, avp_strn(text.p + s, (size_t)(e - s)), s, e}));
}

DraftToks draft_tokenize(AvpStr text) {
    DraftToks toks = {0};
    const char *p = text.p;
    i64 n = (i64)text.n, i = 0;
    bool bol = true;
    while (i < n) {
        char ch = p[i];
        if (is_ws(ch)) {
            if (ch == '\n') bol = true;
            i++;
            continue;
        }
        if (ch == '/' && i + 1 < n && p[i + 1] == '/') {
            i64 j = avp_str_find(text, AVP_S("\n"), i, AVP_END);
            if (j < 0) j = n;
            if (j > i + 2 && p[j - 1] == '\r') j--;
            push_tok(&toks, DRAFT_TK_COMMENT, text, i, j);
            i = j;
            continue;
        }
        if (ch == '/' && i + 1 < n && p[i + 1] == '*') {
            i64 j = avp_str_find(text, AVP_S("*/"), i + 2, AVP_END);
            j = j < 0 ? n : j + 2;
            push_tok(&toks, DRAFT_TK_COMMENT, text, i, j);
            i = j;
            continue;
        }
        if (ch == '#' && bol) {
            i64 j = i, k;
            for (;;) {
                k = avp_str_find(text, AVP_S("\n"), j, AVP_END);
                if (k < 0) {
                    k = n;
                    break;
                }
                AvpStr line = avp_str_rstrip_chars(avp_strn(p + j, (size_t)(k - j)), AVP_S("\r"));
                if (avp_str_endswith(line, AVP_S("\\"))) {
                    j = k + 1;
                    continue;
                }
                break;
            }
            i64 e = k > i && p[k - 1] == '\r' ? k - 1 : k;
            push_tok(&toks, DRAFT_TK_PP, text, i, e);
            i = e;
            continue;
        }
        bol = false;
        if (ch == '"' || ch == '\'') {
            i64 j = i + 1;
            while (j < n && p[j] != ch && p[j] != '\n') j += p[j] == '\\' ? 2 : 1;
            j = j + 1 < n ? j + 1 : n;
            push_tok(&toks, ch == '"' ? DRAFT_TK_STR : DRAFT_TK_CHR, text, i, j);
            i = j;
            continue;
        }
        if (is_id_start(ch)) {
            i64 j = i + 1;
            while (j < n && is_id_char(p[j])) j++;
            push_tok(&toks, DRAFT_TK_ID, text, i, j);
            i = j;
            continue;
        }
        if (num_digit(text, (size_t)i) || (ch == '.' && num_digit(text, (size_t)i + 1))) {
            i64 j = (i64)num_end(text, (size_t)i);
            push_tok(&toks, DRAFT_TK_NUM, text, i, j);
            i = j;
            continue;
        }
        size_t w = 0;
        for (size_t o = 0; o < sizeof OPS / sizeof *OPS; o++) {
            size_t l = strlen(OPS[o]);
            if ((size_t)(n - i) >= l && memcmp(p + i, OPS[o], l) == 0) {
                w = l;
                break;
            }
        }
        if (!w) w = u8_width(p, (size_t)i, (size_t)n);
        push_tok(&toks, DRAFT_TK_OP, text, i, i + (i64)w);
        i += (i64)w;
    }
    return toks;
}

DraftToks draft_code_tokens(const DraftToks *toks) {
    DraftToks out = {0};
    for (size_t i = 0; i < toks->n; i++)
        if (toks->v[i].k != DRAFT_TK_COMMENT && toks->v[i].k != DRAFT_TK_PP) avp_vec_push(&out, toks->v[i]);
    return out;
}

static char open_of(AvpStr t) {
    if (t.n != 1) return 0;
    return t.p[0] == ')' ? '(' : t.p[0] == ']' ? '[' : t.p[0] == '}' ? '{' : 0;
}
static bool is_open(AvpStr t) { return t.n == 1 && (t.p[0] == '(' || t.p[0] == '[' || t.p[0] == '{'); }

i64 *draft_match_brackets(AvpArena *a, const DraftToks *code) {
    i64 *m = avp_alloc(a, (code->n ? code->n : 1) * sizeof *m);
    for (size_t i = 0; i < code->n; i++) m[i] = -1;
    AvpI64Vec st = {0};
    for (size_t i = 0; i < code->n; i++) {
        const DraftTok *t = &code->v[i];
        if (t->k != DRAFT_TK_OP) continue;
        if (is_open(t->t)) {
            avp_vec_push(&st, (i64)i);
        } else if (open_of(t->t)) {
            if (!st.n || code->v[avp_vec_last(&st)].t.p[0] != open_of(t->t)) {
                avp_vec_free(&st);
                shsm_raise(DRAFT_EXC_PARSE, "unbalanced '%c' at offset %lld", t->t.p[0], (long long)t->s);
            }
            i64 j = avp_vec_pop(&st);
            m[i] = j;
            m[j] = (i64)i;
        }
    }
    if (st.n) {
        char c = code->v[avp_vec_last(&st)].t.p[0];
        avp_vec_free(&st);
        shsm_raise(DRAFT_EXC_PARSE, "unclosed '%c'", c);
    }
    avp_vec_free(&st);
    return m;
}

/* ---- function definitions -------------------------------------------------------------------------------------- */

static const char *const NOT_NAME[] = {"if",      "while",   "for",   "switch", "return", "sizeof",
                                       "__declspec", "__attribute__", "defined", "catch", "do", "else"};

static bool not_name(AvpStr t) {
    for (size_t i = 0; i < sizeof NOT_NAME / sizeof *NOT_NAME; i++)
        if (avp_str_eqz(t, NOT_NAME[i])) return true;
    return false;
}

#define T(i) (code->v[(i)].t)
#define IS(i, s) avp_str_eqz(code->v[(i)].t, (s))

i64 draft_skip_angle_back(const DraftToks *code, i64 q, i64 lo) {
    i64 depth = 0;
    while (q >= lo) {
        if (IS(q, ">")) {
            depth++;
        } else if (IS(q, "<")) {
            depth--;
            if (depth == 0) return q;
        } else if (IS(q, ">>")) {
            depth += 2;
        }
        q--;
    }
    return -1;
}

DraftSpans draft_split_top(const DraftToks *code, i64 a, i64 b, const i64 *m, const char *sep) {
    DraftSpans out = {0};
    i64 k = a, s = a;
    while (k < b) {
        if (is_open(T(k)) && code->v[k].k == DRAFT_TK_OP) {
            k = m[k] + 1;
            continue;
        }
        if (IS(k, sep)) {
            avp_vec_push(&out, ((DraftSpan){s, k}));
            s = k + 1;
        }
        k++;
    }
    avp_vec_push(&out, ((DraftSpan){s, b}));
    return out;
}

bool draft_fn_def(AvpArena *a, const DraftToks *code, const i64 *m, i64 s, i64 e, const AvpStrVec *cls_stack,
                  DraftFnDef *out) {
    i64 k = s, paren = -1, n = (i64)code->n;
    while (k < e) {
        if (IS(k, "(")) {
            i64 p = k - 1;
            if (p >= s && IS(p, "operator")) {
                paren = m[k] + 1 < n && IS(m[k] + 1, "(") ? m[k] + 1 : k;
                break;
            }
            if (p >= s && code->v[p].k == DRAFT_TK_ID && !not_name(T(p))) {
                /* the parameter list is followed by the body, `const`, `throw` or a ctor initialiser list; an
                 * earlier `MACRO(...)` (libjpeg's METHODDEF(void *)) is part of the return type */
                i64 nx = m[k] + 1;
                if (nx >= e || IS(nx, "const") || IS(nx, "throw") || IS(nx, ":")) {
                    paren = k;
                    break;
                }
            }
            k = m[k] + 1;
            continue;
        }
        if (IS(k, "[") || IS(k, "{")) {
            k = m[k] + 1;
            continue;
        }
        if (IS(k, "=")) {
            bool op = false;
            for (i64 x = s; x < k && !op; x++) op = IS(x, "operator");
            if (!op) return false;
        }
        k++;
    }
    if (paren < 0 || !IS(paren, "(")) return false;
    i64 op = -1;
    for (i64 x = s; x < paren; x++)
        if (IS(x, "operator")) {
            op = x;
            break;
        }
    const char *name;
    i64 q;
    if (op >= 0) {
        AvpBuf b;
        avp_buf_init(&b, a);
        avp_buf_putz(&b, "operator");
        for (i64 x = op + 1; x < paren; x++) avp_buf_put(&b, T(x));
        name = avp_buf_cz(&b);
        q = op - 1;
    } else {
        name = avp_cz(a, T(paren - 1));
        q = paren - 2;
        if (q >= s && IS(q, "~")) {
            name = avp_sprintf(a, "~%s", name);
            q--;
        }
    }
    AvpStrVec quals = {0};
    while (q - 1 >= s && IS(q, "::")) {
        i64 r = q - 1;
        if (IS(r, ">")) {
            i64 r2 = draft_skip_angle_back(code, r, s);
            if (r2 < 0) break;
            r = r2 - 1;
        }
        if (r < s || code->v[r].k != DRAFT_TK_ID) break;
        avp_vec_insert(&quals, 0, T(r));
        q = r - 1;
    }
    i64 rp = m[paren];
    DraftSpans params = {0};
    if (rp != paren + 1) params = draft_split_top(code, paren + 1, rp, m, ",");
    if (params.n == 1 && rp == paren + 2 && IS(paren + 1, "void")) params.n = 0;
    bool has_void = false, has_star = false;
    for (i64 x = s; x <= q; x++) {
        has_void |= IS(x, "void");
        has_star |= IS(x, "*");
    }
    const char *cls = NULL;
    if (quals.n) cls = avp_cz(a, avp_vec_last(&quals));
    else if (cls_stack && cls_stack->n) cls = avp_cz(a, avp_vec_last(cls_stack));
    out->name = name;
    out->quals = quals;
    out->cls = cls;
    out->nparams = (i64)params.n;
    out->head = s;
    out->lbrace = e;
    out->rbrace = m[e];
    out->is_void = (has_void && !has_star) || name[0] == '~' || (cls && strcmp(name, cls) == 0);
    out->params = params;
    return true;
}

static bool hk_has(const DraftToks *code, i64 a, i64 b, const char *w) {
    for (i64 x = a; x < b; x++)
        if (IS(x, w)) return true;
    return false;
}

static void walk(AvpArena *a, const DraftToks *code, const i64 *m, i64 i, i64 end, AvpStrVec *cls_stack,
                 DraftFnDefs *out) {
    i64 start = i;
    while (i < end) {
        if (IS(i, ";")) {
            i++;
            start = i;
            continue;
        }
        if ((IS(i, "public") || IS(i, "private") || IS(i, "protected")) && i + 1 < end && IS(i + 1, ":")) {
            i += 2;
            start = i;
            continue;
        }
        if (IS(i, "(") || IS(i, "[")) {
            i = m[i] + 1;
            continue;
        }
        if (IS(i, "{")) {
            i64 j = m[i];
            bool paren = hk_has(code, start, i, "(");
            if ((start < i && IS(start, "namespace")) ||
                (start < i && IS(start, "extern") && i - start > 1 && code->v[start + 1].k == DRAFT_TK_STR)) {
                walk(a, code, m, i + 1, j, cls_stack, out);
            } else if (hk_has(code, start, i, "enum") && !paren) {
                /* an enum body */
            } else if ((hk_has(code, start, i, "class") || hk_has(code, start, i, "struct") ||
                        hk_has(code, start, i, "union")) &&
                       !paren) {
                i64 kw = -1;
                for (i64 x = start; x < i; x++)
                    if (IS(x, "class") || IS(x, "struct") || IS(x, "union")) kw = x;
                AvpStr name = kw + 1 < i && code->v[kw + 1].k == DRAFT_TK_ID ? T(kw + 1) : AVP_S("?");
                avp_vec_push(cls_stack, name);
                walk(a, code, m, i + 1, j, cls_stack, out);
                cls_stack->n--;
            } else {
                DraftFnDef d;
                if (draft_fn_def(a, code, m, start, i, cls_stack, &d)) avp_vec_push(out, d);
            }
            i = j + 1;
            start = i;
            continue;
        }
        i++;
    }
}

DraftFnDefs draft_scan_definitions(AvpArena *a, const DraftToks *code, const i64 *m) {
    DraftFnDefs out = {0};
    AvpStrVec stack = {0};
    walk(a, code, m, 0, (i64)code->n, &stack, &out);
    avp_vec_free(&stack);
    return out;
}

/* ---- source scanning ---------------------------------------------------------------------------------------------- */

static const char ADDR_COMMENT[] = "//\\s*0x(8[0-9A-Fa-f]{7})\\b";

static i64 hex_of(AvpStr s) {
    i64 v = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        v = v * 16 + (c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}

/* text[i:i + count] counted in code points, as a byte end offset */
static i64 cp_advance(AvpStr text, i64 i, i64 count) {
    while (count-- > 0 && i < (i64)text.n) i += (i64)u8_width(text.p, (size_t)i, text.n);
    return i;
}

DraftDefs draft_scan_source(AvpArena *a, AvpStr text) {
    DraftToks toks = draft_tokenize(text);
    DraftToks code = draft_code_tokens(&toks);
    avp_vec_free(&toks);
    i64 *m = draft_match_brackets(a, &code);
    DraftFnDefs defs = draft_scan_definitions(a, &code, m);
    const ShsmRe *addr_re = shsm_re(ADDR_COMMENT, 0);
    DraftDefs out = {0};
    for (size_t k = 0; k < defs.n; k++) {
        const DraftFnDef *d = &defs.v[k];
        i64 start = code.v[d->head].s, end = code.v[d->rbrace].e;
        /* banner: the comments and blank lines right above the head (max of the ends of the tokens before it) */
        i64 prev_end = d->head > 0 ? code.v[d->head - 1].e : 0;
        AvpStr gap = avp_strn(text.p + prev_end, (size_t)(start - prev_end));
        i64 addr = DRAFT_NONE;
        ShsmReIter it;
        ShsmReMatch mm;
        shsm_re_iter_init(&it, addr_re, gap, 0, AVP_END);
        while (shsm_re_iter_next(&it, &mm)) addr = hex_of(shsm_re_group(&mm, 1));
        i64 nl = avp_str_find(gap, AVP_S("\n"), 0, AVP_END);
        i64 b = prev_end + (nl >= 0 ? nl + 1 : 0);
        AvpStr tail = avp_strn(text.p + end, (size_t)(cp_advance(text, end, 40) - end));
        i64 tnl = avp_str_find(tail, AVP_S("\n"), 0, AVP_END);
        if (tnl >= 0) tail.n = (size_t)tnl;
        if (addr == DRAFT_NONE && shsm_re_search(addr_re, tail, 0, AVP_END, &mm)) addr = hex_of(shsm_re_group(&mm, 1));
        avp_vec_push(&out, ((DraftDef){d->name, d->cls, start, end, b, addr}));
    }
    avp_vec_free(&code);
    return out;
}

AvpJson *draft_def_json(AvpArena *a, const DraftDef *d) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, d->name));
    avp_json_set(a, o, AVP_S("cls"), d->cls ? avp_json_strz(a, d->cls) : avp_json_null());
    avp_json_set(a, o, AVP_S("start"), avp_json_int(a, d->start));
    avp_json_set(a, o, AVP_S("end"), avp_json_int(a, d->end));
    avp_json_set(a, o, AVP_S("banner"), avp_json_int(a, d->banner));
    avp_json_set(a, o, AVP_S("address"), d->address == DRAFT_NONE ? avp_json_null() : avp_json_int(a, d->address));
    return o;
}

const DraftDef *draft_find_def_in(AvpArena *a, const DraftDefs *defs, const char *symbol, i64 address) {
    if (address != DRAFT_NONE)
        for (size_t i = 0; i < defs->n; i++)
            if (defs->v[i].address == address) return &defs->v[i];
    const char *cls = NULL;
    const char *name = draft_demangled_parts(a, symbol, &cls);
    AvpStr nm = avp_str(name);
    AvpStr before, after;
    while (avp_str_partition(nm, AVP_S("::"), &before, &after)) nm = after; /* name.split("::")[-1] */
    for (size_t i = 0; i < defs->n; i++) {
        const DraftDef *d = &defs->v[i];
        if (!avp_str_eqz(nm, d->name)) continue;
        if (!cls || (d->cls && strcmp(d->cls, cls) == 0) || avp_str_endswith(avp_str(d->cls), avp_str(cls))) return d;
    }
    return NULL;
}

const DraftDef *draft_find_def(AvpArena *a, AvpStr text, const char *symbol, i64 address) {
    DraftDefs defs = draft_scan_source(a, text);
    const DraftDef *d = draft_find_def_in(a, &defs, symbol, address);
    if (!d) {
        avp_vec_free(&defs);
        return NULL;
    }
    DraftDef *copy = AVP_NEW(a, DraftDef);
    *copy = *d;
    avp_vec_free(&defs);
    return copy;
}

AvpStrVec draft_includes_of(AvpStr text) {
    const ShsmRe *re = shsm_re("^\\s*#\\s*include\\s*[<\"]([^>\"]+)[>\"]", SHSM_RE_M);
    ShsmReFindall f = shsm_re_findall(re, text, 0, AVP_END);
    AvpStrVec out = {0};
    for (size_t i = 0; i < f.count; i++) avp_vec_push(&out, shsm_re_findall_at(&f, i, 0));
    shsm_re_findall_free(&f);
    return out;
}

/* os.path.splitext(os.path.basename(path))[0] */
static AvpStr stem_of(AvpArena *a, const char *path) {
    AvpStr root, ext;
    avp_path_splitext(avp_cz(a, avp_path_basename(path)), &root, &ext);
    return root;
}

const char *draft_class_header(AvpArena *a, const char *include_dir, AvpStr source_text, const char *cls,
                               const char *source_path) {
    AvpStr stem = stem_of(a, source_path);
    AvpStrVec incs = draft_includes_of(source_text);
    for (size_t i = 0; i < incs.n; i++) {
        if (avp_str_eq(stem_of(a, avp_cz(a, incs.v[i])), stem)) {
            const char *r = avp_cz(a, incs.v[i]);
            avp_vec_free(&incs);
            return r;
        }
    }
    /* re.escape(cls or "\0"): NUL is not escaped (3.7+), so the pattern holds a NUL byte */
    AvpStr esc = cls ? shsm_re_escape(a, avp_str(cls)) : avp_strn("\0", 1);
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "\\b(?:class|struct)\\s+");
    avp_buf_put(&pat, esc);
    avp_buf_putz(&pat, "\\b[^;]*\\{");
    const ShsmRe *re = shsm_re_try_n(avp_buf_str(&pat), 0);
    const char *found = NULL;
    for (size_t i = 0; i < incs.n && !found; i++) {
        const char *p = avp_path_join(a, include_dir, avp_cz(a, incs.v[i]));
        AvpStr text;
        if (!avp_read_text(a, p, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_REPLACE, AVP_NL_DEFAULT), &text)) continue;
        if (shsm_re_search(re, text, 0, AVP_END, NULL)) found = avp_cz(a, incs.v[i]);
    }
    avp_vec_free(&incs);
    return found;
}

const char *draft_extern_of(AvpArena *a, AvpStr stmt, const char *name) {
    stmt = avp_str_strip(avp_str_rstrip_chars(avp_str_strip(stmt), AVP_S(";")));
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "^(.*?\\b");
    avp_buf_put(&pat, shsm_re_escape(a, avp_str(name)));
    avp_buf_putz(&pat, "\\b(?:\\s*\\[[^\\]]*\\])?)\\s*(?:=.*|\\(.*\\))?$");
    ShsmReMatch mm;
    if (!shsm_re_match(shsm_re_try_n(avp_buf_str(&pat), SHSM_RE_S), stmt, 0, AVP_END, &mm)) return NULL;
    AvpStr decl = avp_str_strip(shsm_re_group(&mm, 1));
    if (avp_str_startswith(decl, AVP_S("static "))) return avp_sprintf(a, "%.*s;", AVP_SF(decl));
    if (avp_str_startswith(decl, AVP_S("extern "))) return avp_cz(a, decl);
    return avp_sprintf(a, "extern %.*s;", AVP_SF(decl));
}

/* The comment and preprocessor blanking of top_level_statements: the Python puts one space per code point; here
 * each code point's first byte becomes a space and its continuation bytes 0xFF (a byte UTF-8 never holds), so byte
 * offsets stay those of the text and dropping the 0xFF bytes gives the Python's spaces. */
static void blank(void *ctx, const ShsmReMatch *m, AvpBuf *out) {
    (void)ctx;
    AvpStr g = shsm_re_group(m, 0);
    for (size_t i = 0; i < g.n; i++) {
        unsigned char c = (unsigned char)g.p[i];
        avp_buf_putc(out, c >= 0x80 && c < 0xC0 ? (char)-1 : ' ');
    }
}

static int span_cmp(const void *x, const void *y) {
    const DraftSpan *a = x, *b = y;
    if (a->s != b->s) return a->s < b->s ? -1 : 1;
    return a->e < b->e ? -1 : a->e > b->e;
}

DraftStmts draft_top_level_statements(AvpArena *a, AvpStr text, const DraftDefs *defs) {
    DraftSpans spans = {0};
    for (size_t i = 0; i < defs->n; i++) avp_vec_push(&spans, ((DraftSpan){defs->v[i].start, defs->v[i].end}));
    if (spans.n) qsort(spans.v, spans.n, sizeof *spans.v, span_cmp);
    i64 n = (i64)text.n;
    avp_vec_push(&spans, ((DraftSpan){n, n}));
    const ShsmRe *re = shsm_re("//[^\\n]*|/\\*.*?\\*/|^[ \\t]*#[^\\n]*", SHSM_RE_S | SHSM_RE_M);
    AvpStr clean = shsm_re_sub_fn(a, re, blank, NULL, text, 0, NULL);
    DraftStmts out = {0};
    i64 pos = 0;
    for (size_t k = 0; k < spans.n; k++) {
        i64 s = spans.v[k].s;
        /* re.finditer(r"[^;{}#]*;", clean[pos:s]): each run of other characters that ends at a ';' */
        i64 at = pos;
        while (at < s) {
            i64 j = at;
            while (j < s && !strchr(";{}#", clean.p[j])) j++;
            if (j >= s) break;
            if (clean.p[j] == ';') {
                AvpBuf b;
                avp_buf_init(&b, a);
                for (i64 x = at; x <= j; x++)
                    if ((unsigned char)clean.p[x] != 0xFF) avp_buf_putc(&b, clean.p[x]);
                AvpStr st = avp_str_strip(avp_buf_str(&b));
                if (st.n && st.p[0] != '#') avp_vec_push(&out, ((DraftStmt){at, j + 1, st}));
            }
            at = j + 1;
        }
        pos = spans.v[k].e;
    }
    avp_vec_free(&spans);
    return out;
}

bool draft_class_block(AvpArena *a, AvpStr text, const char *name, i64 *start, i64 *end) {
    DraftToks toks = draft_tokenize(text);
    DraftToks cv = draft_code_tokens(&toks);
    avp_vec_free(&toks);
    const DraftToks *code = &cv;
    i64 *m = draft_match_brackets(a, code);
    i64 n = (i64)code->n;
    bool found = false;
    for (i64 i = 0; i < n - 2 && !found; i++) {
        if (!(IS(i, "class") || IS(i, "struct")) || !IS(i + 1, name)) continue;
        i64 j = i + 2;
        while (j < n && !IS(j, "{") && !IS(j, ";")) j++;
        if (j >= n || !IS(j, "{")) continue;
        i64 k = m[j];
        *end = k + 1 < n && IS(k + 1, ";") ? code->v[k + 1].e : code->v[k].e;
        *start = code->v[i].s;
        i64 prev = i ? code->v[i - 1].e : 0;
        i64 c = avp_str_find(avp_strn(text.p + prev, (size_t)(*start - prev)), AVP_S("/*"), 0, AVP_END);
        if (c >= 0) *start = prev + c;
        found = true;
    }
    avp_vec_free(&cv);
    return found;
}
