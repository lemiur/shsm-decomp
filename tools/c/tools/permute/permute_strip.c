/* permute_strip: see permute_strip.h. Each function names the Python it ports; the comments give the regular
 * expression a scanner replaces. */
#include "permute_strip.h"

#include "shsm_text.h"

/* ---- character classes ------------------------------------------------------------------------------------------ */

static bool is_word(u8 c) { return c == '_' || (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c >= 0x80; }
static bool is_ident_start(u8 c) { return c == '_' || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z'); }
static bool is_digit(u8 c) { return c >= '0' && c <= '9'; }
static u8 at(AvpStr s, size_t i) { return i < s.n ? (u8)s.p[i] : 0; }
static bool word_at(AvpStr s, size_t i) { return i < s.n && is_word((u8)s.p[i]); }
static size_t skip_ws(AvpStr s, size_t i) { return shsm_skip_ws(s, i); }
/* end of [A-Za-z_]\w* at i (i itself when none) */
static size_t ident_end(AvpStr s, size_t i)
{
    if (i >= s.n || !is_ident_start((u8)s.p[i])) return i;
    i++;
    while (word_at(s, i)) i++;
    return i;
}
static size_t word_end(AvpStr s, size_t i)
{
    while (word_at(s, i)) i++;
    return i;
}
static bool lit_at(AvpStr s, size_t i, const char *lit)
{
    size_t n = strlen(lit);
    return i + n <= s.n && memcmp(s.p + i, lit, n) == 0;
}
/* \bword\b at i */
static bool keyword_at(AvpStr s, size_t i, const char *kw)
{
    size_t n = strlen(kw);
    return lit_at(s, i, kw) && (i == 0 || !is_word((u8)s.p[i - 1])) && !word_at(s, i + n);
}
static bool search_keyword(AvpStr s, const char *kw)
{
    for (size_t i = 0; i < s.n; i++)
        if (keyword_at(s, i, kw)) return true;
    return false;
}
/* re.sub(r"\s+", " ", s) */
static AvpStr collapse_ws(AvpArena *a, AvpStr s)
{
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < s.n;) {
        size_t w = shsm_ws_len(s, i);
        if (w) {
            avp_buf_putc(&b, ' ');
            i = skip_ws(s, i);
        } else {
            avp_buf_putc(&b, s.p[i++]);
        }
    }
    return avp_buf_str(&b);
}

/* ---- symbols ---------------------------------------------------------------------------------------------------- */

static bool parse_uint(AvpStr s, size_t *i, size_t *val)
{
    size_t j = *i, v = 0;
    if (j >= s.n || !is_digit((u8)s.p[j])) return false;
    while (j < s.n && is_digit((u8)s.p[j])) v = v * 10 + (size_t)(s.p[j++] - '0');
    *i = j;
    *val = v;
    return true;
}

/* Python slicing of enc[a:b] with a, b clamped to the string. */
static AvpStr clamp_slice(AvpStr s, size_t a, size_t b)
{
    if (a > s.n) a = s.n;
    if (b > s.n) b = s.n;
    if (b < a) b = a;
    return avp_strn(s.p + a, b - a);
}

bool permute_class_of(AvpStr enc, AvpStr *out)
{
    if (enc.n >= 2 && enc.p[0] == 'Q' && is_digit((u8)enc.p[1])) {
        size_t count = (size_t)(enc.p[1] - '0');
        AvpStr rest = clamp_slice(enc, 2, enc.n), name = {0};
        bool have = false;
        for (size_t k = 0; k < count; k++) {
            size_t i = 0, ln;
            if (!parse_uint(rest, &i, &ln)) return false;
            name = clamp_slice(rest, i, i + ln);
            rest = clamp_slice(rest, i + ln, rest.n);
            have = true;
        }
        if (!have) return false; /* Q0: name stays None */
        *out = name;
        return true;
    }
    size_t i = 0, ln;
    if (!parse_uint(enc, &i, &ln)) return false;
    *out = clamp_slice(enc, i, i + ln);
    return true;
}

static const char *const OPERATOR_SYMS[] = {
    "as", "eq", "ne", "lt", "gt", "le", "ge", "pl", "mi", "ml", "dv", "md", "vc", "rf", "nw", "dl", "nt", "co", "ls",
    "rs", "aa", "oo", "ad", "or", "er", "apl", "ami", "amu", "adv", "amd", "aad", "aor", "aer", "als", "ars", "pp",
    "mm", "cl", "vn", "vd", "cm", "rm"};

/* (?:Q\d|\d|F) at i */
static bool enc_at(AvpStr s, size_t i)
{
    u8 c = at(s, i);
    return (c == 'Q' && i + 1 < s.n && is_digit((u8)s.p[i + 1])) || is_digit(c) || c == 'F';
}

bool permute_source_name(AvpArena *a, AvpStr sym, AvpStr *out)
{
    /* A thunk "@<offset>@<symbol>" (an adjustor MWCC emits for a virtual function) is written as <symbol>'s
     * definition: its "@N@" prefixes are dropped (D-permute-2; the Python misparsed them). */
    while (sym.n > 2 && sym.p[0] == '@' && sym.p[1] >= '0' && sym.p[1] <= '9') {
        size_t i = 1;
        while (i < sym.n && sym.p[i] >= '0' && sym.p[i] <= '9') i++;
        if (i >= sym.n || sym.p[i] != '@') break;
        sym = avp_strn(sym.p + i + 1, sym.n - i - 1);
    }
    /* ^__(ct|dt)__(.*)$  (. stops at a newline: symbols have none) */
    if (lit_at(sym, 0, "__ct__") || lit_at(sym, 0, "__dt__")) {
        AvpStr cls;
        if (!permute_class_of(clamp_slice(sym, 6, sym.n), &cls)) return false;
        *out = sym.p[2] == 'c' ? cls : avp_str_concat(a, AVP_S("~"), cls);
        return true;
    }
    /* ^__([a-z]{2,3})__(ENC.*)$ with group 1 an operator code */
    if (lit_at(sym, 0, "__")) {
        size_t i = 2;
        while (i < sym.n && i < 5 && sym.p[i] >= 'a' && sym.p[i] <= 'z') i++;
        /* [a-z]{2,3} backtracks from 3 to 2 letters */
        for (size_t end = i; end >= 4; end--) {
            if (lit_at(sym, end, "__") && enc_at(sym, end + 2)) {
                AvpStr code = avp_strn(sym.p + 2, end - 2);
                for (size_t k = 0; k < sizeof OPERATOR_SYMS / sizeof *OPERATOR_SYMS; k++)
                    if (avp_str_eqz(code, OPERATOR_SYMS[k])) {
                        *out = AVP_S("operator");
                        return true;
                    }
                break; /* the match exists; only its operator code is not one */
            }
        }
    }
    /* ^(.+?)__(ENC.*)$: the first "__" after at least one character that is followed by an encoding */
    for (size_t i = 1; i + 2 <= sym.n; i++)
        if (sym.p[i] == '_' && sym.p[i + 1] == '_' && enc_at(sym, i + 2)) {
            *out = avp_strn(sym.p, i);
            return true;
        }
    return false;
}

bool permute_is_mangled(AvpStr sym)
{
    AvpArena *a = avp_scratch();
    AvpArenaMark m = avp_arena_mark(a);
    AvpStr name;
    bool r = permute_source_name(a, sym, &name);
    avp_arena_reset(a, m);
    return r;
}

AvpStr permute_dir_name(AvpArena *a, AvpStr sym)
{
    if (avp_u8_len(sym) <= 64) return sym;
    AvpStr head = avp_u8_slice(sym, 0, 48);
    const char *hex = avp_sha1_hex(a, sym.p, sym.n);
    return avp_str(avp_sprintf(a, "%.*s_%.8s", AVP_SF(head), hex));
}

/* ---- C ---------------------------------------------------------------------------------------------------------- */

static bool in_set(u8 c, const char *set) { return c && strchr(set, c) != NULL; }

/* fn_re = ^[^\n#;{}]*?\b(\w+)\s*\([^;{}]*\)\s*\{ (re.M), searched from pos: the word and the index of the "{". */
static bool c_fn_search(AvpStr s, size_t pos, AvpStr *word, size_t *brace)
{
    size_t line = pos;
    if (pos > 0 && s.p[pos - 1] != '\n') {
        i64 nl = avp_str_find(s, AVP_S("\n"), (i64)pos, (i64)s.n);
        if (nl < 0) return false;
        line = (size_t)nl + 1;
    }
    while (line <= s.n) {
        for (size_t k = line;; k++) {
            if (word_at(s, k) && (k == 0 || !is_word((u8)s.p[k - 1]))) {
                size_t we = word_end(s, k), p = skip_ws(s, we);
                if (at(s, p) == '(') {
                    size_t q = p + 1;
                    while (q < s.n && !in_set((u8)s.p[q], ";{}")) q++;
                    if (q < s.n && s.p[q] == '{') {
                        for (size_t r = q; r-- > p + 1;) {
                            if (s.p[r] == ')' && skip_ws(s, r + 1) == q) {
                                *word = avp_strn(s.p + k, we - k);
                                *brace = q;
                                return true;
                            }
                            if ((u8)s.p[r] < 0x80 && !shsm_ws_len(s, r)) {
                                if (s.p[r] != ')') break;
                            }
                        }
                    }
                }
            }
            if (k >= s.n || in_set((u8)s.p[k], "\n#;{}")) break;
        }
        i64 nl = avp_str_find(s, AVP_S("\n"), (i64)line, (i64)s.n);
        if (nl < 0) break;
        line = (size_t)nl + 1;
    }
    return false;
}

bool permute_strip_other_fns(AvpArena *a, AvpStr src, AvpStr keep, AvpStr *out)
{
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    for (;;) {
        AvpStr word;
        size_t start;
        if (!c_fn_search(src, i, &word, &start)) {
            avp_buf_put(&b, avp_strn(src.p + i, src.n - i));
            break;
        }
        int depth = 0;
        size_t j = start;
        for (;;) {
            if (j >= src.n) return false; /* IndexError */
            if (src.p[j] == '{') depth++;
            else if (src.p[j] == '}' && --depth == 0) break;
            j++;
        }
        if (avp_str_eq(word, keep)) {
            avp_buf_put(&b, avp_strn(src.p + i, j + 1 - i));
        } else {
            avp_buf_put(&b, avp_str_rstrip(avp_strn(src.p + i, start - i)));
            avp_buf_putc(&b, ';');
        }
        i = j + 1;
    }
    *out = avp_buf_str(&b);
    return true;
}

/* extern\s*"C"\s* at i: the end of the match, 0 when none. */
static size_t extern_c_at(AvpStr s, size_t i)
{
    if (!lit_at(s, i, "extern")) return 0;
    size_t j = skip_ws(s, i + 6);
    if (!lit_at(s, j, "\"C\"")) return 0;
    return skip_ws(s, j + 3);
}

bool permute_strip_extern_c(AvpArena *a, AvpStr src, AvpStr *out)
{
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t i = 0;
    for (;;) {
        /* blk = extern\s*"C"\s*\{ */
        size_t ms = 0, me = 0;
        bool found = false;
        for (size_t k = i; k < src.n && !found; k++) {
            size_t e = extern_c_at(src, k);
            if (e && at(src, e) == '{') {
                ms = k;
                me = e + 1;
                found = true;
            }
        }
        if (!found) {
            avp_buf_put(&b, avp_strn(src.p + i, src.n - i));
            break;
        }
        avp_buf_put(&b, avp_strn(src.p + i, ms - i));
        int depth = 1;
        size_t j = me;
        while (depth) {
            if (j >= src.n) return false; /* IndexError */
            depth += (src.p[j] == '{') - (src.p[j] == '}');
            j++;
        }
        AvpStr inner;
        if (!permute_strip_extern_c(a, avp_strn(src.p + me, j - 1 - me), &inner)) return false;
        avp_buf_put(&b, inner);
        i = j;
    }
    /* re.sub(r'extern\s*"C"\s*', "extern ", joined) */
    AvpStr joined = avp_buf_str(&b);
    AvpBuf r;
    avp_buf_init(&r, a);
    for (size_t k = 0; k < joined.n;) {
        size_t e = extern_c_at(joined, k);
        if (e) {
            avp_buf_putz(&r, "extern ");
            k = e;
        } else {
            avp_buf_putc(&r, joined.p[k++]);
        }
    }
    *out = avp_buf_str(&r);
    return true;
}

/* ---- C++ -------------------------------------------------------------------------------------------------------- */

static size_t skip_lit(AvpStr s, size_t i)
{
    char q = s.p[i++];
    while (i < s.n && s.p[i] != q) i += s.p[i] == '\\' ? 2 : 1;
    return i + 1;
}

static size_t skip_noncode(AvpStr s, size_t i)
{
    char c = s.p[i];
    if (c == '"' || c == '\'') return skip_lit(s, i);
    if (lit_at(s, i, "//")) {
        i64 k = avp_str_find(s, AVP_S("\n"), (i64)i, (i64)s.n);
        return k < 0 ? s.n : (size_t)k;
    }
    if (lit_at(s, i, "/*")) {
        i64 k = avp_str_find(s, AVP_S("*/"), (i64)i + 2, (i64)s.n);
        return k < 0 ? s.n : (size_t)k + 2;
    }
    return i;
}

static size_t skip_braces(AvpStr s, size_t i)
{
    int depth = 0;
    while (i < s.n) {
        size_t k = skip_noncode(s, i);
        if (k != i) {
            i = k;
            continue;
        }
        if (s.p[i] == '{') depth++;
        else if (s.p[i] == '}' && --depth == 0) return i + 1;
        i++;
    }
    return i;
}

/* One repetition of (?:[A-Za-z_]\w*\s*(?:<[^<>]*>\s*)?::\s*) at i: its end, 0 when none. */
static size_t qual_rep(AvpStr s, size_t i)
{
    size_t j = ident_end(s, i);
    if (j == i) return 0;
    j = skip_ws(s, j);
    if (at(s, j) == '<') {
        size_t k = j + 1;
        while (k < s.n && s.p[k] != '<' && s.p[k] != '>') k++;
        if (at(s, k) == '>') {
            size_t after = skip_ws(s, k + 1);
            if (lit_at(s, after, "::")) return skip_ws(s, after + 2);
        }
    }
    if (lit_at(s, j, "::")) return skip_ws(s, j + 2);
    return 0;
}

/* \s*\( at i: the index of the "(", or SIZE_MAX. */
static size_t open_paren(AvpStr s, size_t i)
{
    size_t j = skip_ws(s, i);
    return at(s, j) == '(' ? j : SIZE_MAX;
}

/* [^\s(\w]+ at i: its end (i when none) */
static size_t punct_run(AvpStr s, size_t i)
{
    while (i < s.n && s.p[i] != '(' && !is_word((u8)s.p[i]) && !shsm_ws_len(s, i)) i++;
    return i;
}

/* group 2 of _NAME_RE followed by \s*\( at q: the "(" index, or SIZE_MAX; *end the end of group 2. */
static size_t name_group(AvpStr s, size_t q, size_t *end)
{
    /* (?!operator\b)~?[A-Za-z_]\w* */
    bool op_word = lit_at(s, q, "operator") && !word_at(s, q + 8);
    if (!op_word) {
        size_t i = at(s, q) == '~' ? q + 1 : q;
        size_t e = ident_end(s, i);
        if (e > i) {
            size_t p = open_paren(s, e);
            if (p != SIZE_MAX) {
                *end = e;
                return p;
            }
        }
    }
    /* operator\s*(?:\(\s*\)|\[\s*\]|new|delete|[^\s(\w]+|[A-Za-z_]\w*(?:\s*\*)?) */
    if (!lit_at(s, q, "operator")) return SIZE_MAX;
    size_t o = skip_ws(s, q + 8), e, p;
    if (at(s, o) == '(') {
        e = skip_ws(s, o + 1);
        if (at(s, e) == ')' && (p = open_paren(s, e + 1)) != SIZE_MAX) return *end = e + 1, p;
    }
    if (at(s, o) == '[') {
        e = skip_ws(s, o + 1);
        if (at(s, e) == ']' && (p = open_paren(s, e + 1)) != SIZE_MAX) return *end = e + 1, p;
    }
    if (lit_at(s, o, "new") && (p = open_paren(s, o + 3)) != SIZE_MAX) return *end = o + 3, p;
    if (lit_at(s, o, "delete") && (p = open_paren(s, o + 6)) != SIZE_MAX) return *end = o + 6, p;
    e = punct_run(s, o);
    if (e > o && (p = open_paren(s, e)) != SIZE_MAX) return *end = e, p;
    e = ident_end(s, o);
    if (e > o) {
        size_t star = skip_ws(s, e);
        if (at(s, star) == '*' && (p = open_paren(s, star + 1)) != SIZE_MAX) return *end = star + 1, p;
        if ((p = open_paren(s, e)) != SIZE_MAX) return *end = e, p;
    }
    return SIZE_MAX;
}

bool permute_fn_name(AvpArena *a, AvpStr head, AvpStr *qual, AvpStr *name, size_t *paren)
{
    size_t reps[256];
    for (size_t s = 0; s <= head.n; s++) {
        size_t n = 0;
        reps[n++] = s;
        for (size_t e; n < 256 && (e = qual_rep(head, reps[n - 1])) != 0;) reps[n++] = e;
        for (size_t k = n; k-- > 0;) {
            size_t end, p = name_group(head, reps[k], &end);
            if (p == SIZE_MAX) continue;
            AvpStr q = avp_strn(head.p + s, reps[k] - s);
            *qual = avp_str_contains(q, AVP_S(" ")) ? avp_str_replace(a, q, AVP_S(" "), AVP_S(""), -1) : q;
            *name = avp_strn(head.p + reps[k], end - reps[k]);
            if (avp_str_startswith(*name, AVP_S("operator"))) *name = AVP_S("operator");
            *paren = p;
            return true;
        }
    }
    return false;
}

AvpStr permute_decl_of(AvpArena *a, AvpStr head, size_t paren)
{
    int depth = 0;
    size_t p = paren;
    while (p < head.n) {
        if (head.p[p] == '(') depth++;
        else if (head.p[p] == ')' && --depth == 0) break;
        p++;
    }
    AvpStr tail = p + 1 < head.n ? avp_strn(head.p + p + 1, head.n - p - 1) : AVP_S("");
    i64 colon = avp_str_findc(tail, ':');
    if (colon >= 0) tail.n = (size_t)colon;
    AvpStr first = avp_strn(head.p, p + 1 < head.n ? p + 1 : head.n);
    AvpStr decl = avp_str_rstrip(avp_str_concat(a, first, tail));
    return avp_str_concat(a, decl, AVP_S(";"));
}

/* re.search(r'\bnamespace\b|\bextern\s*"C(\+\+)?"\s*$', head) */
static bool opens_block(AvpStr head)
{
    if (search_keyword(head, "namespace")) return true;
    for (size_t i = 0; i < head.n; i++) {
        if (!lit_at(head, i, "extern") || (i > 0 && is_word((u8)head.p[i - 1]))) continue;
        size_t j = skip_ws(head, i + 6);
        if (lit_at(head, j, "\"C++\"") && skip_ws(head, j + 5) == head.n) return true;
        if (lit_at(head, j, "\"C\"") && skip_ws(head, j + 3) == head.n) return true;
    }
    return false;
}

/* "=" in re.sub(r"operator\s*(?:\(\s*\)|\[\s*\]|[^\s(\w]+)", "operator", head).split("(", 1)[0] */
static bool assigns_before_paren(AvpStr head)
{
    for (size_t i = 0; i < head.n;) {
        if (lit_at(head, i, "operator")) {
            size_t o = skip_ws(head, i + 8), e = 0;
            if (at(head, o) == '(') {
                size_t k = skip_ws(head, o + 1);
                if (at(head, k) == ')') e = k + 1;
            }
            if (!e && at(head, o) == '[') {
                size_t k = skip_ws(head, o + 1);
                if (at(head, k) == ']') e = k + 1;
            }
            if (!e) {
                size_t k = punct_run(head, o);
                if (k > o) e = k;
            }
            if (e) {
                i = e; /* replaced by "operator": no "=" and no "(" */
                continue;
            }
        }
        if (head.p[i] == '(') return false;
        if (head.p[i] == '=') return true;
        i++;
    }
    return false;
}

static bool starts_with_hash(AvpStr line) { return avp_str_startswith(avp_str_lstrip(line), AVP_S("#")); }

PermuteCxxDefs permute_scan_cxx_defs(AvpArena *a, AvpStr src)
{
    PermuteCxxDefs defs = {0};
    size_t n = src.n, i = 0, start = 0, stack = 0;
    int paren = 0;
    while (i < n) {
        size_t k = skip_noncode(src, i);
        if (k != i) {
            i = k;
            continue;
        }
        char c = src.p[i];
        if (c == '#' && (i == 0 || src.p[i - 1] == '\n')) {
            i64 nl = avp_str_find(src, AVP_S("\n"), (i64)i, (i64)n);
            i = nl < 0 ? n : (size_t)nl;
            continue;
        }
        if (c == '(') {
            paren++;
        } else if (c == ')') {
            paren--;
        } else if (c == ';' && paren == 0) {
            start = i + 1;
        } else if (c == '}' && paren == 0) {
            if (stack) stack--;
            start = i + 1;
        } else if (c == '{' && paren == 0) {
            AvpStrVec lines = avp_str_split(avp_strn(src.p + start, i - start), AVP_S("\n"), -1);
            AvpStrVec pragmas = {0}, code = {0};
            for (size_t l = 0; l < lines.n; l++) {
                if (starts_with_hash(lines.v[l])) avp_vec_push(&pragmas, lines.v[l]);
                else avp_vec_push(&code, lines.v[l]);
            }
            AvpStr head = avp_str_strip(collapse_ws(a, avp_str_join(a, AVP_S(" "), code.v, code.n)));
            avp_vec_free(&lines);
            avp_vec_free(&code);
            if (opens_block(head) && !avp_str_contains(head, AVP_S("("))) {
                avp_vec_free(&pragmas);
                stack++;
                start = i + 1;
                i++;
                continue;
            }
            AvpStr qual = {0}, name = {0};
            size_t pp = 0;
            bool nm = permute_fn_name(a, head, &qual, &name, &pp);
            size_t body = i;
            i = skip_braces(src, i);
            if (i > n) i = n; /* an unterminated literal: the Python's index past the end only ever slices */
            if (avp_str_contains(head, AVP_S("(")) && nm && !assigns_before_paren(head)) {
                PermuteCxxDef d = {head, qual, name, pp, start, body, i, pragmas,
                                   search_keyword(head, "template") || search_keyword(head, "inline")};
                avp_vec_push(&defs, d);
                start = i;
            } else {
                avp_vec_free(&pragmas);
            }
            continue;
        }
        i++;
    }
    return defs;
}

/* set(re.findall(r"~?[A-Za-z_]\w*", text)) */
static AvpSet *identifiers(AvpStr t)
{
    AvpSet *set = avp_set_new();
    for (size_t i = 0; i < t.n;) {
        size_t s = i, j = t.p[i] == '~' ? i + 1 : i;
        size_t e = ident_end(t, j);
        if (e > j) {
            avp_set_add(set, avp_strn(t.p + s, e - s));
            i = e;
        } else {
            i++;
        }
    }
    return set;
}

static bool keep_has(const AvpStr *keep, size_t nkeep, AvpStr name)
{
    for (size_t k = 0; k < nkeep; k++)
        if (avp_str_eq(keep[k], name)) return true;
    return false;
}

AvpStr permute_strip_other_fns_cxx(AvpArena *a, AvpStr src, const AvpStr *keep, size_t nkeep, bool keep_callees,
                                   size_t *kept, AvpStrVec *callees)
{
    PermuteCxxDefs defs = permute_scan_cxx_defs(a, src);
    bool *needed = avp_alloc(a, defs.n + 1);
    size_t *work = avp_alloc(a, (defs.n + 1) * sizeof *work), nwork = 0, nkept = 0;
    for (size_t d = 0; d < defs.n; d++) {
        bool w = keep_has(keep, nkeep, defs.v[d].name);
        nkept += w;
        if (w || defs.v[d].forced) {
            needed[d] = true;
            work[nwork++] = d;
        }
    }
    AvpSet *names = avp_set_new();
    while (keep_callees && nwork) {
        const PermuteCxxDef *d = &defs.v[work[--nwork]];
        AvpSet *ids = identifiers(avp_strn(src.p + d->body, d->end - d->body));
        for (size_t e = 0; e < defs.n; e++)
            if (!needed[e] && avp_set_has(ids, defs.v[e].name)) {
                needed[e] = true;
                work[nwork++] = e;
                avp_set_add(names, defs.v[e].name);
            }
        avp_set_free(ids);
    }
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t flushed = 0;
    for (size_t d = 0; d < defs.n; d++) {
        const PermuteCxxDef *x = &defs.v[d];
        if (needed[d]) continue;
        avp_buf_put(&b, avp_strn(src.p + flushed, x->start - flushed));
        if (x->pragmas.n) {
            avp_buf_put(&b, avp_str_join(a, AVP_S("\n"), x->pragmas.v, x->pragmas.n));
            avp_buf_putc(&b, '\n');
        }
        if (!x->qual.n) {
            avp_buf_putc(&b, '\n');
            avp_buf_put(&b, permute_decl_of(a, x->head, x->paren));
            avp_buf_putc(&b, '\n');
        }
        flushed = x->end;
    }
    avp_buf_put(&b, avp_strn(src.p + flushed, src.n - flushed));
    *kept = nkept;
    *callees = avp_set_sorted(names);
    for (size_t k = 0; k < callees->n; k++) callees->v[k] = avp_str_dup(a, callees->v[k]); /* the set owns its keys */
    avp_set_free(names);
    for (size_t d = 0; d < defs.n; d++) avp_vec_free(&defs.v[d].pragmas);
    avp_vec_free(&defs);
    return avp_buf_str(&b);
}
