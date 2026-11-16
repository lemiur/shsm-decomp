/* classes_asm_parse.c: the parsers of tools/classes.py (parse_data_regions, unquote, split_operands, parse_int,
 * parse_asm_text, _unescape, _add_bytes, parse_symbols_text) and the CodeWarrior class-name helpers (_parse_qual,
 * mangled_class, vt_symbol_class, method_text).
 *
 * The Python regular expressions are matched by hand, with Python's str semantics: \s is Unicode whitespace
 * (str.isspace), \d a Unicode decimal digit, \w a Unicode alphanumeric or '_', `.` anything but '\n'; the input is
 * valid UTF-8 (the asm is decoded with errors="replace"). Lines are str.splitlines() lines. Each matcher's comment
 * names the pattern and why a greedy scan gives the same match as the backtracking engine. */
#include "classes_prog.h"

#include "shsm_cli.h"

/* ---- character classes ---------------------------------------------------------------------------------------- */

static inline size_t cp_len(u8 c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }

/* Byte length of the whitespace code point at s[i], 0 when none (re's \s on str). */
static inline size_t ws_at(AvpStr s, size_t i) {
    if (i >= s.n) return 0;
    u8 c = (u8)s.p[i];
    if (c < 0x80) return (c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) ? 1 : 0;
    const char *p = s.p + i;
    u32 cp = avp_u8_next(&p, s.p + s.n);
    return avp_cp_isspace(cp) ? (size_t)(p - (s.p + i)) : 0;
}
static inline size_t skip_ws(AvpStr s, size_t i) {
    size_t k;
    while ((k = ws_at(s, i)) != 0) i += k;
    return i;
}
/* End of the run of non-whitespace code points from i (\S*). */
static inline size_t skip_nonws(AvpStr s, size_t i) {
    while (i < s.n && ws_at(s, i) == 0) i += cp_len((u8)s.p[i]);
    return i;
}
static inline bool is_hex(char c) { return (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'); }
static inline size_t skip_hex(AvpStr s, size_t i) {
    while (i < s.n && is_hex(s.p[i])) i++;
    return i;
}
static bool one_cp(AvpStr s, size_t i, bool (*pred)(AvpStr), size_t *len) {
    size_t k = cp_len((u8)s.p[i]);
    if (i + k > s.n) k = s.n - i;
    *len = k;
    return pred(avp_strn(s.p + i, k));
}
/* Byte length of the decimal digit (\d) at s[i], 0 when none. */
static inline size_t dec_at(AvpStr s, size_t i) {
    if (i >= s.n) return 0;
    u8 c = (u8)s.p[i];
    if (c < 0x80) return (c >= '0' && c <= '9') ? 1 : 0;
    size_t k;
    return one_cp(s, i, avp_str_isdecimal, &k) ? k : 0;
}
static inline size_t skip_dec(AvpStr s, size_t i) {
    size_t k;
    while ((k = dec_at(s, i)) != 0) i += k;
    return i;
}
/* A word character (\w) at s[i]. */
static bool word_at(AvpStr s, size_t i, size_t *len) {
    u8 c = (u8)s.p[i];
    if (c < 0x80) {
        *len = 1;
        return c == '_' || (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z');
    }
    return one_cp(s, i, avp_str_isalnum, len);
}
static bool starts(AvpStr s, size_t i, const char *lit) {
    size_t n = strlen(lit);
    return i + n <= s.n && memcmp(s.p + i, lit, n) == 0;
}
static AvpStr sub(AvpStr s, size_t i, size_t j) { return avp_strn(s.p + i, j - i); }

/* ---- lines ----------------------------------------------------------------------------------------------------- */
/* str.splitlines() iteration: \n \r \r\n \v \f \x1c \x1d \x1e U+0085 U+2028 U+2029. */
typedef struct {
    const char *p, *e;
} Lines;
static bool next_line(Lines *it, AvpStr *line) {
    if (it->p >= it->e) return false;
    const char *p = it->p, *e = it->e;
    const char *q = p;
    while (q < e) {
        u8 c = (u8)*q;
        if (c == '\n' || c == '\r' || c == 0x0B || c == 0x0C || (c >= 0x1C && c <= 0x1E)) {
            *line = avp_strn(p, (size_t)(q - p));
            q++;
            if (c == '\r' && q < e && *q == '\n') q++;
            it->p = q;
            return true;
        }
        if (c == 0xC2 && q + 1 < e && (u8)q[1] == 0x85) {
            *line = avp_strn(p, (size_t)(q - p));
            it->p = q + 2;
            return true;
        }
        if (c == 0xE2 && q + 2 < e && (u8)q[1] == 0x80 && ((u8)q[2] == 0xA8 || (u8)q[2] == 0xA9)) {
            *line = avp_strn(p, (size_t)(q - p));
            it->p = q + 3;
            return true;
        }
        q++;
    }
    *line = avp_strn(p, (size_t)(e - p));
    it->p = e;
    return true;
}

/* ---- splits.txt ----------------------------------------------------------------------------------------------- */

static int region_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ClassesRegion *a = x, *b = y;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    if (a->end != b->end) return a->end < b->end ? -1 : 1;
    return avp_str_cmp(a->library, b->library);
}

static bool hex_value(AvpStr h, i64 *out) { return avp_parse_int(h, 16, out, NULL); }

ClassesRegions classes_parse_data_regions(AvpArena *a, AvpStr text) {
    (void)a;
    ClassesRegions out = {0};
    AvpStr unit = {0};
    Lines it = {text.p, text.p + text.n};
    AvpStr line;
    while (next_line(&it, &line)) {
        if (line.n && ws_at(line, 0) == 0) {
            AvpStr r = avp_str_rstrip(line);
            if (r.n && r.p[r.n - 1] == ':') {
                unit = avp_strn(r.p, r.n - 1);
                continue;
            }
        }
        /* ^\s+\.data\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+) */
        size_t i = skip_ws(line, 0);
        if (i == 0 || !starts(line, i, ".data")) continue;
        size_t j = skip_ws(line, i + 5);
        if (j == i + 5 || !starts(line, j, "start:0x")) continue;
        size_t h1 = j + 8, h1e = skip_hex(line, h1);
        if (h1e == h1) continue;
        size_t k = skip_ws(line, h1e);
        if (k == h1e || !starts(line, k, "end:0x")) continue;
        size_t h2 = k + 6, h2e = skip_hex(line, h2);
        if (h2e == h2) continue;
        if (CLASSES_NONE(unit) || unit.n == 0 || avp_str_eqz(unit, "Sections")) continue;
        ClassesRegion r = {0};
        hex_value(sub(line, h1, h1e), &r.start);
        hex_value(sub(line, h2, h2e), &r.end);
        AvpStr before, after;
        avp_str_partition(unit, AVP_S("/"), &before, &after);
        r.library = before;
        avp_vec_push(&out, r);
    }
    avp_sort(out.v, out.n, sizeof(ClassesRegion), region_cmp, NULL, false);
    return out;
}

AvpStr classes_region_of_library(AvpStr library) {
    if (avp_str_eqz(library, "game")) return AVP_S("game");
    if (avp_str_eqz(library, "climax")) return AVP_S("climax-core");
    return (AvpStr){0};
}

/* ---- small helpers -------------------------------------------------------------------------------------------- */

AvpStr classes_unquote(AvpStr s) {
    if (s.n >= 2 && s.p[0] == '"' && s.p[s.n - 1] == '"') return avp_strn(s.p + 1, s.n - 2);
    return s;
}

AvpStrVec classes_split_operands(AvpArena *a, AvpStr text) {
    /* count first, so the vector lives in the arena */
    size_t n = 1;
    bool quoted = false;
    for (size_t i = 0; i < text.n; i++) {
        if (text.p[i] == '"') quoted = !quoted;
        if (text.p[i] == ',' && !quoted) n++;
    }
    AvpStrVec out = {0};
    out.v = AVP_NEWN(a, AvpStr, n);
    out.cap = n;
    size_t start = 0;
    quoted = false;
    for (size_t i = 0; i < text.n; i++) {
        if (text.p[i] == '"') quoted = !quoted;
        if (text.p[i] == ',' && !quoted) {
            out.v[out.n++] = avp_str_strip(sub(text, start, i));
            start = i + 1;
        }
    }
    out.v[out.n++] = avp_str_strip(sub(text, start, text.n));
    return out;
}

/* int(s) / int(s, 16) with a value that wraps modulo 2**64 where Python's would not fit (ASCII digits only). */
static bool py_int(AvpStr s, int base, i64 *out) {
    bool ovf = false;
    if (avp_parse_int(s, base, out, &ovf)) return true;
    if (!ovf) return false;
    s = avp_str_strip(s);
    size_t i = 0;
    bool neg = false;
    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) neg = s.p[i++] == '-';
    if (base == 16 && i + 1 < s.n && s.p[i] == '0' && (s.p[i + 1] | 0x20) == 'x') i += 2;
    u64 acc = 0;
    for (; i < s.n; i++) {
        char c = s.p[i];
        if (c == '_') continue;
        int d = (c >= '0' && c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10;
        acc = acc * (u64)base + (u64)d;
    }
    *out = (i64)(neg ? 0 - acc : acc);
    return true;
}

bool classes_parse_int(AvpStr s, i64 *out) {
    s = avp_str_strip(s);
    bool neg = s.n && s.p[0] == '-';
    if (neg) s = avp_strn(s.p + 1, s.n - 1);
    i64 v;
    bool hex = s.n >= 2 && s.p[0] == '0' && (s.p[1] | 0x20) == 'x';
    if (!py_int(s, hex ? 16 : 10, &v)) return false;
    *out = neg ? (i64)(0 - (u64)v) : v;
    return true;
}

/* ^-?(0x[0-9A-Fa-f]+|\d+)$ */
static bool is_num(AvpStr a) {
    size_t i = 0;
    if (i < a.n && a.p[i] == '-') i++;
    if (starts(a, i, "0x")) {
        size_t e = skip_hex(a, i + 2);
        if (e > i + 2 && e == a.n) return true;
    }
    size_t e = skip_dec(a, i);
    return e > i && e == a.n;
}

/* ---- _unescape / _add_bytes ----------------------------------------------------------------------------------- */

static inline u8 latin1(u32 cp) { return cp < 256 ? (u8)cp : (u8)'?'; }

static void unescape(AvpBytes *out, AvpStr s) {
    const char *p = s.p, *e = s.p + s.n;
    while (p < e) {
        const char *q = p;
        u32 ch = avp_u8_next(&q, e);
        if (ch == '\\' && q < e) {
            const char *r = q;
            u32 nx = avp_u8_next(&r, e);
            if (nx == 'x') {
                size_t k = 0;
                u32 v = 0;
                while (k < 2 && r + k < e && is_hex(r[k])) {
                    char c = r[k];
                    v = v * 16 + (u32)((c >= '0' && c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10);
                    k++;
                }
                if (k) {
                    avp_vec_push(out, (u8)v);
                    p = r + k;
                    continue;
                }
            }
            if (nx >= '0' && nx <= '7') {
                const char *t = q;
                u32 v = 0;
                size_t k = 0;
                while (k < 3 && t < e && *t >= '0' && *t <= '7') {
                    v = v * 8 + (u32)(*t - '0');
                    t++;
                    k++;
                }
                avp_vec_push(out, (u8)(v & 0xFF));
                p = t;
                continue;
            }
            u8 b = nx == 'n' ? '\n' : nx == 't' ? '\t' : nx == 'r' ? '\r' : latin1(nx);
            avp_vec_push(out, b);
            p = r;
            continue;
        }
        avp_vec_push(out, latin1(ch));
        p = q;
    }
}

/* ^"(.*)"$ : the text between the quotes when s starts and ends with one (two distinct quotes). */
static bool quoted_all(AvpStr s, AvpStr *inner) {
    if (s.n >= 2 && s.p[0] == '"' && s.p[s.n - 1] == '"') {
        *inner = avp_strn(s.p + 1, s.n - 2);
        return true;
    }
    return false;
}

typedef enum { K_4BYTE, K_2BYTE, K_BYTE, K_STRING, K_SKIP, K_FLOAT, K_DOUBLE, K_ASCIZ, K_ASCII } DataKind;

static void add_bytes(ClassesObj *o, DataKind kind, AvpArena *a, AvpStr arg) {
    if (!o->has_data) return;
    if (kind == K_STRING || kind == K_ASCIZ || kind == K_ASCII) {
        AvpStr inner;
        if (!quoted_all(arg, &inner)) {
            o->has_data = false; /* AttributeError on None.group */
            return;
        }
        unescape(&o->data, inner);
        if (kind != K_ASCII) avp_vec_push(&o->data, (u8)0);
    } else if (kind == K_SKIP) {
        i64 n;
        if (!classes_parse_int(arg, &n)) {
            o->has_data = false;
            return;
        }
        if (n > 0) {
            avp_vec_reserve(&o->data, o->data.n + (size_t)n);
            memset(o->data.v + o->data.n, 0, (size_t)n);
            o->data.n += (size_t)n;
        }
    } else if (kind == K_BYTE || kind == K_2BYTE || kind == K_4BYTE) {
        int n = kind == K_BYTE ? 1 : kind == K_2BYTE ? 2 : 4;
        AvpStrVec ops = classes_split_operands(a, arg);
        for (size_t i = 0; i < ops.n; i++) {
            u64 v = 0;
            i64 x;
            if (is_num(ops.v[i]) && classes_parse_int(ops.v[i], &x)) v = (u64)x;
            for (int b = n - 1; b >= 0; b--) avp_vec_push(&o->data, (u8)(v >> (8 * b)));
        }
    } else {
        o->has_data = false;
    }
}

/* ---- parse_asm_text ------------------------------------------------------------------------------------------- */

/* RE_INSN ^/\* ([0-9A-Fa-f]{8}) [0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]{2} ?)+\s*\*\/\s+(\S+)\s*(.*?)\s*$
 * Each greedy run is followed by something its class cannot hold (a hex run by \s, \s by hex, the byte pairs by
 * \s* then '*'), so backtracking never finds another split; group 3 is the rest stripped of \s on both sides. */
static bool match_insn(AvpStr L, i64 *addr, AvpStr *mn, AvpStr *rest) {
    if (L.n < 12 || L.p[0] != '/' || L.p[1] != '*' || L.p[2] != ' ') return false;
    for (size_t k = 3; k < 11; k++)
        if (!is_hex(L.p[k])) return false;
    if (L.p[11] != ' ') return false;
    size_t i = skip_hex(L, 12);
    if (i == 12) return false;
    size_t j = skip_ws(L, i);
    if (j == i) return false;
    i = j;
    size_t pairs = 0;
    while (i + 1 < L.n && is_hex(L.p[i]) && is_hex(L.p[i + 1])) {
        i += 2;
        pairs++;
        if (i < L.n && L.p[i] == ' ') i++;
    }
    if (!pairs) return false;
    i = skip_ws(L, i);
    if (!starts(L, i, "*/")) return false;
    i += 2;
    j = skip_ws(L, i);
    if (j == i) return false;
    size_t m = skip_nonws(L, j);
    if (m == j) return false;
    *mn = sub(L, j, m);
    *rest = avp_str_rstrip(sub(L, skip_ws(L, m), L.n));
    i64 v = 0;
    for (size_t k = 3; k < 11; k++) {
        char c = L.p[k];
        v = v * 16 + ((c >= '0' && c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    *addr = v;
    return true;
}

/* ^\.endfn\b / ^\.endobj\b */
static bool match_end(AvpStr L, const char *kw) {
    if (!starts(L, 0, kw)) return false;
    size_t i = strlen(kw), k;
    return i >= L.n || !word_at(L, i, &k);
}

/* ^\s*\.(4byte|2byte|byte|string|skip|float|double|asciz|ascii)\s*(.*?)\s*$ : the first alternative that is a
 * prefix wins (the tail always matches); the argument is the rest stripped. */
static bool match_data(AvpStr L, DataKind *kind, AvpStr *arg) {
    static const char *const kws[] = {"4byte", "2byte", "byte", "string", "skip", "float", "double", "asciz", "ascii"};
    size_t i = skip_ws(L, 0);
    if (i >= L.n || L.p[i] != '.') return false;
    i++;
    for (int k = 0; k < 9; k++) {
        if (starts(L, i, kws[k])) {
            *kind = (DataKind)k;
            *arg = avp_str_strip(sub(L, i + strlen(kws[k]), L.n));
            return true;
        }
    }
    return false;
}

/* ^("[^"]*"|[^+\s]+)(?:\+(0x[0-9A-Fa-f]+|\d+))?$ on a 4byte operand. */
static bool addend_tail(AvpStr a, size_t i, AvpStr *num) {
    if (i == a.n) {
        *num = (AvpStr){0};
        return true;
    }
    if (a.p[i] != '+') return false;
    i++;
    if (starts(a, i, "0x")) {
        size_t e = skip_hex(a, i + 2);
        if (e > i + 2 && e == a.n) {
            *num = sub(a, i, e);
            return true;
        }
    }
    size_t e = skip_dec(a, i);
    if (e > i && e == a.n) {
        *num = sub(a, i, e);
        return true;
    }
    return false;
}
static bool match_symref(AvpStr a, AvpStr *name, AvpStr *num) {
    if (a.n && a.p[0] == '"') {
        const char *q = memchr(a.p + 1, '"', a.n - 1);
        if (q) {
            size_t e = (size_t)(q - a.p) + 1;
            if (addend_tail(a, e, num)) {
                *name = sub(a, 0, e);
                return true;
            }
        }
    }
    size_t i = 0;
    while (i < a.n && a.p[i] != '+' && ws_at(a, i) == 0) i += cp_len((u8)a.p[i]);
    if (i == 0) return false;
    if (!addend_tail(a, i, num)) return false;
    *name = sub(a, 0, i);
    return true;
}

/* RE_LOC ^# (\S+):0x[0-9A-Fa-f]+ \| 0x([0-9A-Fa-f]{8}) \| size: 0x([0-9A-Fa-f]+) on the stripped line. \S+ ends at
 * the first whitespace; what follows it in the run must be ":0x" and hex digits only, so the split is the run's last
 * ':' (group 1 non-empty). */
static bool match_loc(AvpStr s, i64 *addr, i64 *size) {
    if (!starts(s, 0, "# ")) return false;
    size_t r0 = 2, r1 = skip_nonws(s, r0);
    if (r1 == r0) return false;
    size_t k = r1;
    while (k > r0 && s.p[k - 1] != ':') k--;
    if (k == r0) return false;
    k--; /* the ':' */
    if (k == r0 || !starts(s, k, ":0x")) return false;
    size_t h = k + 3;
    if (h >= r1 || skip_hex(s, h) != r1) return false;
    size_t i = r1;
    if (!starts(s, i, " | 0x")) return false;
    i += 5;
    for (size_t t = 0; t < 8; t++)
        if (i + t >= s.n || !is_hex(s.p[i + t])) return false;
    AvpStr a8 = sub(s, i, i + 8);
    i += 8;
    if (!starts(s, i, " | size: 0x")) return false;
    i += 11;
    size_t e = skip_hex(s, i);
    if (e == i) return false;
    hex_value(a8, addr);
    return py_int(sub(s, i, e), 16, size);
}

/* ^\.fn\s+("[^"]*"|[^,\s]+) (also .obj and .section's [^,\s]+). */
static bool match_named(AvpStr s, const char *kw, bool quoted_alt, AvpStr *name) {
    if (!starts(s, 0, kw)) return false;
    size_t i = strlen(kw), j = skip_ws(s, i);
    if (j == i) return false;
    if (quoted_alt && j < s.n && s.p[j] == '"') {
        const char *q = memchr(s.p + j + 1, '"', s.n - j - 1);
        if (q) {
            *name = sub(s, j, (size_t)(q - s.p) + 1);
            return true;
        }
    }
    size_t e = j;
    while (e < s.n && s.p[e] != ',' && ws_at(s, e) == 0) e += cp_len((u8)s.p[e]);
    if (e == j) return false;
    *name = sub(s, j, e);
    return true;
}

static const char *const PLAIN_SECTIONS[] = {"text", "data", "rodata", "bss", "sdata", "sbss", "sdata2", "sbss2", "init"};

void classes_parse_asm_text(AvpArena *a, AvpStr text, AvpStr unit, AvpMap *funcs, AvpMap *objs) {
    AvpStr section = {0}, last_comment = {0};
    ClassesFunc *fn = NULL;
    ClassesObj *obj = NULL;
    bool has_loc = false;
    i64 loc_addr = 0, loc_size = 0;
    Lines it = {text.p, text.p + text.n};
    AvpStr line;
    while (next_line(&it, &line)) {
        if (fn) {
            i64 addr;
            AvpStr mn, rest;
            if (match_insn(line, &addr, &mn, &rest)) {
                if (!fn->has_addr) {
                    fn->has_addr = true;
                    fn->addr = addr;
                }
                ClassesInsn in = {addr, mn, NULL, 0};
                if (rest.n) {
                    AvpStrVec ops = classes_split_operands(a, rest);
                    in.ops = ops.v;
                    in.nops = (u32)ops.n;
                }
                avp_vec_push(&fn->insns, in);
                continue;
            }
            if (match_end(line, ".endfn")) {
                avp_map_put(funcs, fn->name, NULL)->p = fn;
                fn = NULL;
            }
            continue;
        }
        if (obj) {
            if (match_end(line, ".endobj")) {
                avp_map_put(objs, obj->name, NULL)->p = obj;
                obj = NULL;
                continue;
            }
            DataKind kind;
            AvpStr arg;
            if (match_data(line, &kind, &arg)) {
                add_bytes(obj, kind, a, arg);
                if (kind == K_4BYTE) {
                    AvpStrVec ops = classes_split_operands(a, arg);
                    for (size_t k = 0; k < ops.n; k++) {
                        AvpStr op = avp_str_strip(ops.v[k]);
                        ClassesItem item = {0};
                        AvpStr nm, num;
                        i64 v;
                        if (is_num(op)) {
                            classes_parse_int(op, &v);
                            item.kind = CLASSES_IT_INT;
                            item.v = (i64)((u64)v & 0xFFFFFFFFu);
                            avp_vec_push(&obj->items, item);
                        } else if (match_symref(op, &nm, &num)) {
                            item.kind = CLASSES_IT_SYM;
                            item.name = classes_unquote(nm);
                            item.v = 0;
                            if (!CLASSES_NONE(num)) classes_parse_int(num, &item.v);
                            avp_vec_push(&obj->items, item);
                        } else {
                            obj->pure_words = false;
                        }
                    }
                } else {
                    obj->pure_words = false;
                    AvpStr inner;
                    if (kind == K_STRING && obj->items.n == 0 && CLASSES_NONE(obj->string) && quoted_all(arg, &inner))
                        obj->string = inner;
                }
            }
            continue;
        }
        AvpStr s = avp_str_strip(line);
        if (s.n == 0) continue;
        if (s.p[0] == '#') {
            i64 la, ls;
            if (match_loc(s, &la, &ls)) {
                has_loc = true;
                loc_addr = la;
                loc_size = ls;
            } else if (!starts(s, 0, "# 0x")) {
                last_comment = avp_str_strip(avp_u8_slice(s, 2, AVP_END));
                if (!last_comment.p) last_comment = avp_strn(s.p + s.n, 0);
            }
            continue;
        }
        AvpStr name;
        if (match_named(s, ".fn", true, &name)) {
            fn = AVP_NEW(a, ClassesFunc);
            fn->name = classes_unquote(name);
            fn->unit = unit;
            fn->section = section;
            if (last_comment.n && avp_str_findc(last_comment, '(') >= 0) fn->demangled = last_comment;
            last_comment = (AvpStr){0};
            continue;
        }
        if (match_named(s, ".obj", true, &name)) {
            obj = AVP_NEW(a, ClassesObj);
            obj->name = classes_unquote(name);
            obj->unit = unit;
            obj->section = section;
            obj->has_loc = has_loc;
            obj->addr = loc_addr;
            obj->size = loc_size;
            obj->pure_words = true;
            obj->has_data = true;
            last_comment = (AvpStr){0};
            continue;
        }
        if (match_named(s, ".section", false, &name)) {
            section = name;
            continue;
        }
        if (s.n > 1 && s.p[0] == '.') {
            AvpStr w = avp_strn(s.p + 1, s.n - 1);
            for (int k = 0; k < 9; k++) {
                if (avp_str_eqz(w, PLAIN_SECTIONS[k])) {
                    section = s;
                    break;
                }
            }
        }
    }
}

/* ---- symbols.txt ---------------------------------------------------------------------------------------------- */

/* RE_SYMLINE ^(\S+) = ([^:]+):0x([0-9A-Fa-f]+); // (.*)$ on the stripped line; type: re.search(r"type:(\w+)"). */
AvpMap *classes_parse_symbols_text(AvpArena *a, AvpStr text) {
    AvpMap *out = avp_map_new();
    Lines it = {text.p, text.p + text.n};
    AvpStr line;
    while (next_line(&it, &line)) {
        AvpStr s = avp_str_strip(line);
        size_t e = skip_nonws(s, 0);
        if (e == 0 || !starts(s, e, " = ")) continue;
        size_t i = e + 3, c = i;
        while (c < s.n && s.p[c] != ':') c++;
        if (c == i || c >= s.n || !starts(s, c, ":0x")) continue;
        size_t h = c + 3, he = skip_hex(s, h);
        if (he == h || !starts(s, he, "; // ")) continue;
        AvpStr comment = sub(s, he + 5, s.n);
        ClassesSymbol *sym = AVP_NEW(a, ClassesSymbol);
        sym->section = sub(s, i, c);
        py_int(sub(s, h, he), 16, &sym->addr);
        for (size_t k = 0; k + 5 <= comment.n; k++) {
            if (memcmp(comment.p + k, "type:", 5) != 0) continue;
            size_t w = k + 5, wl;
            while (w < comment.n && word_at(comment, w, &wl)) w += wl;
            if (w > k + 5) {
                sym->type = sub(comment, k + 5, w);
                break;
            }
        }
        avp_map_put(out, sub(s, 0, e), NULL)->p = sym;
    }
    return out;
}

/* ---- CodeWarrior mangled names -------------------------------------------------------------------------------- */

/* ^(\d+) at s: the run's byte end and int() value. */
static bool dec_run(AvpStr s, size_t *end, i64 *v) {
    size_t e = skip_dec(s, 0);
    if (e == 0) return false;
    *end = e;
    return py_int(sub(s, 0, e), 10, v);
}

bool classes_parse_qual(AvpArena *a, AvpStr rest, AvpStr *cls, AvpStr *remainder) {
    if (rest.n > 1 && rest.p[0] == 'Q' && dec_at(rest, 1)) {
        /* rest[1].isdigit(): a decimal digit (a non-decimal Unicode digit makes Python's int() raise) */
        size_t dl = dec_at(rest, 1);
        i64 n;
        py_int(sub(rest, 1, 1 + dl), 10, &n);
        rest = sub(rest, 1 + dl, rest.n);
        AvpBuf b;
        avp_buf_init(&b, a);
        for (i64 q = 0; q < n; q++) {
            size_t e;
            i64 ln;
            if (!dec_run(rest, &e, &ln)) return false;
            rest = sub(rest, e, rest.n);
            AvpStr part = avp_u8_slice(rest, 0, ln);
            if ((i64)avp_u8_len(rest) < ln) return false;
            if (q) avp_buf_put(&b, AVP_S("::"));
            avp_buf_put(&b, part);
            rest = sub(rest, part.n, rest.n);
        }
        *cls = b.p ? avp_buf_str(&b) : avp_strn("", 0);
        *remainder = rest;
        return true;
    }
    size_t e;
    i64 ln;
    if (!dec_run(rest, &e, &ln)) return false;
    rest = sub(rest, e, rest.n);
    if (ln == 0 || (i64)avp_u8_len(rest) < ln) return false;
    AvpStr part = avp_u8_slice(rest, 0, ln);
    *cls = part;
    *remainder = sub(rest, part.n, rest.n);
    return true;
}

AvpStr classes_mangled_class(AvpArena *a, AvpStr name) {
    /* re.sub(r"^@\d+@", "", name) */
    if (name.n && name.p[0] == '@') {
        size_t e = skip_dec(name, 1);
        if (e > 1 && e < name.n && name.p[e] == '@') name = sub(name, e + 1, name.n);
    }
    AvpStr cls, rem;
    if (avp_str_startswith(name, AVP_S("__vt__"))) {
        if (classes_parse_qual(a, sub(name, 6, name.n), &cls, &rem) && rem.n == 0) return cls;
        return (AvpStr){0};
    }
    /* i = 1 counts code points: the search starts after the first character */
    size_t i = name.n ? cp_len((u8)name.p[0]) : 1;
    for (;;) {
        i64 f = avp_str_find(name, AVP_S("__"), (i64)i, AVP_END);
        if (f < 0) return (AvpStr){0};
        AvpStr rest = sub(name, (size_t)f + 2, name.n);
        if (classes_parse_qual(a, rest, &cls, &rem) &&
            (avp_str_startswith(rem, AVP_S("F")) || avp_str_startswith(rem, AVP_S("CF"))))
            return cls;
        i = (size_t)f + 1;
    }
}

AvpStr classes_vt_symbol_class(AvpArena *a, AvpStr name) {
    if (avp_str_startswith(name, AVP_S("__vt__"))) return classes_mangled_class(a, name);
    return (AvpStr){0};
}

AvpStr classes_method_text(const ClassesFunc *f) {
    if (!f || CLASSES_NONE(f->demangled) || f->demangled.n == 0) return (AvpStr){0};
    AvpStr d = f->demangled;
    i64 paren = avp_str_findc(d, '(');
    AvpStr head = paren >= 0 ? sub(d, 0, (size_t)paren) : d;
    i64 j = avp_str_rfind(head, AVP_S("::"), 0, AVP_END);
    return j >= 0 ? sub(d, (size_t)j + 2, d.n) : d;
}
