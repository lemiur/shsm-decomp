/* Parser of dtk's retail asm listings: tools/common/asm.py, line for line. The regular expressions of the Python are
 * hand-written scanners here; each one names the pattern it replaces. */
#include "shsm_asm.h"

#include "avp/fs.h"
#include "avp/sort.h"
#include "shsm_cli.h"
#include "shsm_text.h"

#include <string.h>

#include <windows.h>

const char SHSM_REF_CALL[] = "call", SHSM_REF_BRANCH[] = "branch", SHSM_REF_HA[] = "ha", SHSM_REF_LO[] = "l",
           SHSM_REF_HI[] = "h", SHSM_REF_SDA21[] = "sda21", SHSM_REF_PTR[] = "ptr",
           SHSM_REF_JUMPTABLE[] = "jumptable";

static const char K_FUNCTION[] = "function", K_OBJECT[] = "object", K_LABEL[] = "label";
static const char *const RELOCS[] = {"ha", "l", "h", "sda21", "sda2", "sdarel", "got"}; /* the regex's order */
static const char REL24[] = "rel24";

/* ---- character helpers ---------------------------------------------------------------------------------------- */

static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_dig(char c) { return c >= '0' && c <= '9'; }
static bool is_word(char c) { return is_dig(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static size_t ws_at(AvpStr s, size_t i) { return i < s.n ? shsm_ws_len(s, i) : 0; }

/* str.strip() (Python whitespace). */
static AvpStr pystrip(AvpStr s) {
    size_t i = shsm_skip_ws(s, 0), end = i;
    for (size_t j = i; j < s.n;) {
        size_t w = shsm_ws_len(s, j);
        if (w) {
            j += w;
            continue;
        }
        j++;
        while (j < s.n && ((u8)s.p[j] & 0xC0) == 0x80) j++;
        end = j;
    }
    return avp_strn(s.p + i, end - i);
}
static AvpStr sub(AvpStr s, size_t from) { return from >= s.n ? avp_strn(s.p + s.n, 0) : avp_strn(s.p + from, s.n - from); }
static bool starts(AvpStr s, const char *lit) {
    size_t n = strlen(lit);
    return s.n >= n && memcmp(s.p, lit, n) == 0;
}
static bool ends(AvpStr s, const char *lit) {
    size_t n = strlen(lit);
    return s.n >= n && memcmp(s.p + s.n - n, lit, n) == 0;
}
static bool eqz(AvpStr s, const char *lit) { return s.n == strlen(lit) && memcmp(s.p, lit, s.n) == 0; }
static i64 hexval(AvpStr s) {
    u64 v = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        v = v * 16 + (u64)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return (i64)v;
}

/* asm._int: strip, one leading sign, int(tok, 16) after 0x else int(tok). False on ValueError. */
static bool py_int_tok(AvpStr tok, i64 *out) {
    tok = pystrip(tok);
    bool neg = tok.n && tok.p[0] == '-';
    if (tok.n == 0 || tok.p[0] == '+' || tok.p[0] == '-') tok = sub(tok, 1); /* "" in "+-" is true in Python */
    i64 v;
    if (tok.n >= 2 && tok.p[0] == '0' && (tok.p[1] | 0x20) == 'x') {
        /* int(tok, 16): the prefix, then one optional '_', then digits (no second sign, no space) */
        AvpStr d = sub(tok, 2);
        if (d.n && d.p[0] == '_') d = sub(d, 1);
        if (!d.n || !is_hex(d.p[0])) return false;
        if (!avp_parse_int(d, 16, &v, NULL)) return false;
    } else if (!avp_parse_int(tok, 10, &v, NULL)) return false;
    *out = neg ? -v : v;
    return true;
}

AvpStr shsm_asm_unquote(AvpArena *a, AvpStr name) {
    if (name.n >= 2 && name.p[0] == '"' && name.p[name.n - 1] == '"') {
        AvpStr in = avp_strn(name.p + 1, name.n - 2);
        if (!memchr(in.p, '\\', in.n)) return in;
        /* .replace('\\"', '"') then .replace('\\\\', '\\') */
        char *t = avp_alloc_noz(a, in.n + 1);
        size_t n = 0;
        for (size_t i = 0; i < in.n; i++) {
            if (in.p[i] == '\\' && i + 1 < in.n && in.p[i + 1] == '"') {
                t[n++] = '"';
                i++;
            } else t[n++] = in.p[i];
        }
        char *u = avp_alloc_noz(a, n + 1);
        size_t m = 0;
        for (size_t i = 0; i < n; i++) {
            if (t[i] == '\\' && i + 1 < n && t[i + 1] == '\\') {
                u[m++] = '\\';
                i++;
            } else u[m++] = t[i];
        }
        return avp_strn(u, m);
    }
    return name;
}

static size_t utf8_len(u8 c) { return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4; }

AvpStr shsm_asm_decode_string(AvpArena *a, AvpStr lit) {
    u8 *out = avp_alloc_noz(a, lit.n + 1);
    size_t o = 0, i = 0, n = lit.n;
    while (i < n) {
        char c = lit.p[i];
        if (c != '\\' || i + 1 >= n) {
            out[o++] = (u8)c;
            i++;
            continue;
        }
        char d = lit.p[i + 1];
        int simple = -1;
        switch (d) {
        case 'n': simple = 10; break;
        case 't': simple = 9; break;
        case 'r': simple = 13; break;
        case '\\': simple = 92; break;
        case '"': simple = 34; break;
        case '\'': simple = 39; break;
        case 'a': simple = 7; break;
        case 'b': simple = 8; break;
        case 'f': simple = 12; break;
        case 'v': simple = 11; break;
        default: break;
        }
        if (simple >= 0) {
            out[o++] = (u8)simple;
            i += 2;
        } else if (d >= '0' && d <= '7') {
            size_t j = i + 1;
            unsigned v = 0;
            while (j < n && j < i + 4 && lit.p[j] >= '0' && lit.p[j] <= '7') v = v * 8 + (unsigned)(lit.p[j++] - '0');
            out[o++] = (u8)(v & 0xFF);
            i = j;
        } else if (d == 'x') {
            size_t j = i + 2;
            unsigned v = 0;
            while (j < n && j < i + 4 && is_hex(lit.p[j])) {
                char h = lit.p[j++];
                v = v * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
            }
            out[o++] = (u8)(v & 0xFF);
            i = j;
        } else { /* the escaped character itself (one code point) */
            size_t k = utf8_len((u8)d);
            for (size_t m = 0; m < k && i + 1 + m < n; m++) out[o++] = (u8)lit.p[i + 1 + m];
            i += 1 + k;
        }
    }
    return avp_strn((const char *)out, o);
}

/* bytes.decode("utf-8") strictness: no overlongs, no surrogates, nothing above U+10FFFF. */
static bool valid_utf8(AvpStr s) {
    const u8 *p = (const u8 *)s.p;
    size_t i = 0, n = s.n;
    while (i < n) {
        u8 c = p[i];
        if (c < 0x80) {
            i++;
            continue;
        }
        size_t k;
        u32 cp;
        if (c >= 0xC2 && c <= 0xDF) k = 1, cp = c & 0x1F;
        else if (c >= 0xE0 && c <= 0xEF) k = 2, cp = c & 0x0F;
        else if (c >= 0xF0 && c <= 0xF4) k = 3, cp = c & 0x07;
        else return false;
        for (size_t m = 1; m <= k; m++) {
            if (i + m >= n || (p[i + m] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + m] & 0x3F);
        }
        if ((k == 2 && cp < 0x800) || (k == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += k + 1;
    }
    return true;
}

/* bytes.decode("cp932") as Python's codec: ASCII, half-width katakana 0xA1-0xDF, the single bytes Python maps
 * itself (0x80 -> U+0080, 0xA0 -> U+F8F0, 0xFD..0xFF -> U+F8F1..U+F8F3), double bytes through Windows' code page 932
 * (the table Python's codec was generated from). False on an invalid sequence. */
static bool decode_cp932(AvpBuf *out, AvpStr b) {
    const u8 *p = (const u8 *)b.p;
    for (size_t i = 0; i < b.n;) {
        u8 c = p[i];
        if (c < 0x80) {
            avp_buf_putc(out, (char)c);
            i++;
        } else if (c == 0x80) {
            avp_buf_putcp(out, 0x80), i++;
        } else if (c == 0xA0) {
            avp_buf_putcp(out, 0xF8F0), i++;
        } else if (c >= 0xFD) {
            avp_buf_putcp(out, 0xF8F1 + (u32)(c - 0xFD)), i++;
        } else if (c >= 0xA1 && c <= 0xDF) {
            avp_buf_putcp(out, 0xFF61 + (u32)(c - 0xA1)), i++;
        } else {
            if (i + 1 >= b.n) return false;
            wchar_t w[2];
            int n = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, (const char *)p + i, 2, w, 2);
            if (n != 1) return false;
            avp_buf_putcp(out, (u32)w[0]);
            i += 2;
        }
    }
    return true;
}

AvpStr shsm_asm_bytes_to_text(AvpArena *a, AvpStr b) {
    if (valid_utf8(b)) return b;
    AvpBuf out = {0};
    avp_buf_init(&out, a);
    if (decode_cp932(&out, b)) return avp_buf_str(&out);
    char *u = avp_alloc_noz(a, b.n * 2 + 1); /* Latin-1 */
    size_t o = 0;
    for (size_t i = 0; i < b.n; i++) {
        u8 c = (u8)b.p[i];
        if (c < 0x80) u[o++] = (char)c;
        else {
            u[o++] = (char)(0xC0 | (c >> 6));
            u[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return avp_strn(u, o);
}

/* len(text.encode("utf-16-be")) // 2 of UTF-8 text */
static size_t utf16_units(AvpStr s) {
    size_t n = 0;
    for (size_t i = 0; i < s.n;) {
        u8 c = (u8)s.p[i];
        size_t k = utf8_len(c);
        n += k == 4 ? 2 : 1;
        i += k;
    }
    return n;
}

/* ---- _SYMREF ---------------------------------------------------------------------------------------------------
 * ("(?:[^"\\]|\\.)*"|[A-Za-z_.$@?][^\s,()+\-@"]*)([+-]0x[0-9A-Fa-f]+|[+-]\d+)?(?:@(ha|l|h|sda21|sda2|sdarel|got))? */
typedef struct {
    size_t start, end;
    AvpStr g1, g2;
    const char *g3; /* NULL: no group 3 */
} SymRef;

/* group 1 at i: its end, or 0 when it does not match there */
static size_t symref_g1(AvpStr s, size_t i) {
    if (i >= s.n) return 0;
    char c = s.p[i];
    if (c == '"') {
        size_t j = i + 1;
        while (j < s.n) {
            if (s.p[j] == '"') return j + 1;
            if (s.p[j] == '\\') {
                if (j + 1 >= s.n || s.p[j + 1] == '\n') return 0;
                j += 1 + utf8_len((u8)s.p[j + 1]);
            } else j++;
        }
        return 0;
    }
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '.' || c == '$' || c == '@' || c == '?'))
        return 0;
    size_t j = i + 1;
    while (j < s.n) {
        char d = s.p[j];
        if (d == ',' || d == '(' || d == ')' || d == '+' || d == '-' || d == '@' || d == '"' || ws_at(s, j)) break;
        j++;
    }
    return j;
}

/* group 2 alternatives at i: alt1 end ([+-]0x hex+), alt2 end ([+-]\d+); 0 when the alternative fails */
static size_t g2_alt1(AvpStr s, size_t i) {
    if (i + 3 < s.n && (s.p[i] == '+' || s.p[i] == '-') && s.p[i + 1] == '0' && s.p[i + 2] == 'x' &&
        is_hex(s.p[i + 3])) {
        size_t j = i + 3;
        while (j < s.n && is_hex(s.p[j])) j++;
        return j;
    }
    return 0;
}
static size_t g2_alt2(AvpStr s, size_t i) {
    if (i + 1 < s.n && (s.p[i] == '+' || s.p[i] == '-') && is_dig(s.p[i + 1])) {
        size_t j = i + 1;
        while (j < s.n && is_dig(s.p[j])) j++;
        return j;
    }
    return 0;
}
/* group 3 at i (prefix match): the reloc and its end */
static const char *g3_at(AvpStr s, size_t i, size_t *end) {
    if (i >= s.n || s.p[i] != '@') return NULL;
    for (size_t k = 0; k < sizeof RELOCS / sizeof *RELOCS; k++) {
        size_t n = strlen(RELOCS[k]);
        if (i + 1 + n <= s.n && memcmp(s.p + i + 1, RELOCS[k], n) == 0) {
            *end = i + 1 + n;
            return RELOCS[k];
        }
    }
    return NULL;
}

/* _SYMREF.match at i (search semantics: optional groups greedy) */
static bool symref_match(AvpStr s, size_t i, SymRef *m) {
    size_t e = symref_g1(s, i);
    if (!e) return false;
    m->start = i;
    m->g1 = avp_strn(s.p + i, e - i);
    m->g2 = (AvpStr){NULL, 0};
    m->g3 = NULL;
    size_t g = g2_alt1(s, e);
    if (!g) g = g2_alt2(s, e);
    if (g) {
        m->g2 = avp_strn(s.p + e, g - e);
        e = g;
    }
    size_t g3e;
    const char *r = g3_at(s, e, &g3e);
    if (r) {
        m->g3 = r;
        e = g3e;
    }
    m->end = e;
    return true;
}

/* _SYMREF.fullmatch(s): group 2 tried as alternative 1, alternative 2, absent; the rest must be empty or one reloc */
static bool symref_fullmatch(AvpStr s, SymRef *m) {
    size_t e = symref_g1(s, 0);
    if (!e) return false;
    m->start = 0;
    m->end = s.n;
    m->g1 = avp_strn(s.p, e);
    size_t cands[3] = {g2_alt1(s, e), g2_alt2(s, e), e};
    for (int k = 0; k < 3; k++) {
        size_t g = cands[k];
        if (!g) continue;
        m->g2 = avp_strn(s.p + e, g - e);
        if (g == s.n) {
            m->g3 = NULL;
            return true;
        }
        if (s.p[g] != '@') continue;
        for (size_t q = 0; q < sizeof RELOCS / sizeof *RELOCS; q++) {
            size_t n = strlen(RELOCS[q]);
            if (g + 1 + n == s.n && memcmp(s.p + g + 1, RELOCS[q], n) == 0) {
                m->g3 = RELOCS[q];
                return true;
            }
        }
    }
    return false;
}

static bool g2_value(AvpStr g2, i64 *v) {
    if (!g2.n) {
        *v = 0;
        return true;
    }
    return py_int_tok(g2, v);
}

/* _LABEL: ^\.L_([0-9A-Fa-f]{8})$ */
static bool label_match(AvpStr s, i64 *addr) {
    if (s.n != 11 || !starts(s, ".L_")) return false;
    for (size_t i = 3; i < 11; i++)
        if (!is_hex(s.p[i])) return false;
    *addr = hexval(avp_strn(s.p + 3, 8));
    return true;
}

/* _NOT_SYM: ^(?:r\d+|f\d+|cr\d|lt|gt|eq|so|un|lr|ctr|qr\d|0x[0-9A-Fa-f]+|-?\d+)$ */
static bool all_dig(AvpStr s, size_t from) {
    if (from >= s.n) return false;
    for (size_t i = from; i < s.n; i++)
        if (!is_dig(s.p[i])) return false;
    return true;
}
static bool not_sym(AvpStr s) {
    if (s.n >= 2 && (s.p[0] == 'r' || s.p[0] == 'f') && all_dig(s, 1)) return true;
    if (s.n == 3 && ((s.p[0] == 'c' && s.p[1] == 'r') || (s.p[0] == 'q' && s.p[1] == 'r')) && is_dig(s.p[2])) return true;
    static const char *const words[] = {"lt", "gt", "eq", "so", "un", "lr", "ctr"};
    for (size_t i = 0; i < sizeof words / sizeof *words; i++)
        if (eqz(s, words[i])) return true;
    if (s.n >= 3 && s.p[0] == '0' && s.p[1] == 'x') {
        bool ok = true;
        for (size_t i = 2; i < s.n; i++) ok = ok && is_hex(s.p[i]);
        if (ok) return true;
    }
    return all_dig(s, s.n && s.p[0] == '-' ? 1 : 0);
}

void shsm_asm_parse_operands(AvpArena *a, ShsmAsmRow *row) {
    AvpStr ops = row->operands;
    if (!ops.n) return;
    if (memchr(ops.p, '@', ops.n)) {
        size_t i = 0;
        SymRef m;
        while (i < ops.n) {
            if (symref_match(ops, i, &m)) {
                if (m.g3) {
                    i64 off;
                    if (!g2_value(m.g2, &off)) off = 0;
                    row->sym = shsm_asm_unquote(a, m.g1);
                    row->sym_off = off;
                    row->reloc = m.g3;
                    return;
                }
                i = m.end > i ? m.end : i + 1;
            } else i++;
        }
    }
    if (!row->mnemonic.n || row->mnemonic.p[0] != 'b') return;
    AvpStr last = ops;
    for (size_t i = ops.n; i > 0; i--)
        if (ops.p[i - 1] == ',') {
            last = avp_strn(ops.p + i, ops.n - i);
            break;
        }
    last = pystrip(last);
    i64 lab;
    if (label_match(last, &lab)) {
        row->label = lab;
        return;
    }
    if (not_sym(last)) return;
    SymRef m;
    if (symref_fullmatch(last, &m) && !m.g3) {
        i64 off;
        if (!g2_value(m.g2, &off)) off = 0;
        row->sym = shsm_asm_unquote(a, m.g1);
        row->sym_off = off;
        row->reloc = REL24;
    }
}

static i64 branch_target(u32 word, i64 address) {
    u32 op = word >> 26;
    i64 d;
    if (op == 18) {
        d = word & 0x03FFFFFC;
        if (d & 0x02000000) d -= 0x04000000;
    } else if (op == 16) {
        d = word & 0xFFFC;
        if (d & 0x8000) d -= 0x10000;
    } else return SHSM_NONE;
    return (word & 2) ? SHSM_NONE : address + d;
}

/* asm._split_args */
static void split_args(AvpStr s, AvpStrVec *out) {
    out->n = 0;
    size_t start = 0;
    bool q = false, esc = false, any = false;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (esc) esc = false;
        else if (c == '\\' && q) esc = true;
        else if (c == '"') q = !q;
        else if (c == ',' && !q) {
            avp_vec_push(out, pystrip(avp_strn(s.p + start, i - start)));
            start = i + 1;
            any = true;
        }
    }
    if (start < s.n || any) avp_vec_push(out, pystrip(avp_strn(s.p + start, s.n - start)));
}

/* ---- the parser ------------------------------------------------------------------------------------------------ */

typedef struct {
    ShsmAsmBlock *blk; /* NULL for loose data */
    size_t item;       /* index in blk->items */
    size_t ref;        /* index in af->refs */
    i64 value;         /* the item's value (label address) or SHSM_NONE */
} Rel;
typedef AVP_VEC(Rel) RelVec;

typedef struct {
    ShsmAsmFile *af;
    AvpArena *a;
    unsigned flags;
    ShsmAsmBlock *cur;
    AvpStr section;
    i64 cursor;
    RelVec rels;
    AvpStrVec args;
} Parser;

static ShsmAsmBlock *new_block(Parser *p, const char *kind, AvpStr name, AvpStr scope, AvpStr section, i64 addr,
                               i64 size, AvpStr demangled, i64 ln, i64 byte_start) {
    ShsmAsmBlock *b = AVP_NEW(p->a, ShsmAsmBlock);
    b->kind = kind;
    b->name = name;
    b->scope = scope;
    b->section = section;
    b->address = addr;
    b->size = size;
    b->demangled = demangled;
    b->line_start = b->line_end = ln;
    b->byte_start = b->byte_end = byte_start;
    return b;
}

static void add_ref(Parser *p, AvpStr from, i64 from_addr, AvpStr to, i64 off, const char *kind, i64 owner,
                    i64 to_addr) {
    ShsmAsmRef r = {from, from_addr, to, off, kind, owner, SHSM_NONE, to_addr, p->section};
    avp_vec_push(&p->af->refs, r);
}

/* asm._data_line: one data directive; returns the advanced cursor */
static i64 data_line(Parser *p, AvpStr line) {
    ShsmAsmBlock *cur = p->cur;
    const char *sp = memchr(line.p, ' ', line.n);
    AvpStr d = sp ? avp_strn(line.p, (size_t)(sp - line.p)) : line;
    AvpStr args = sp ? avp_strn(sp + 1, line.n - (size_t)(sp - line.p) - 1) : avp_strn(line.p + line.n, 0);
    bool has_comment = false;
    for (size_t i = 0; i + 1 < args.n; i++)
        if (args.p[i] == '/' && args.p[i + 1] == '*') {
            has_comment = true;
            break;
        }
    if (has_comment && !(args.n && args.p[0] == '"')) {
        for (size_t i = 0; i + 1 < args.n; i++)
            if (args.p[i] == '/' && args.p[i + 1] == '*') {
                args.n = i;
                break;
            }
    }
    args = pystrip(args);
    AvpStr owner = cur ? cur->name : (AvpStr){NULL, 0};
    i64 owner_addr = cur ? cur->address : SHSM_NONE;
    i64 addr = p->cursor != SHSM_NONE ? p->cursor : 0;
    size_t nitems = 0;
    AVP_VEC(ShsmAsmItem) loose = {0};
#define PUSH_ITEM(it)                                    \
    do {                                                 \
        if (cur) avp_vec_push(&cur->items, (it));        \
        else avp_vec_push(&loose, (it));                 \
        nitems++;                                        \
    } while (0)
    AvpStrVec *av = &p->args;
    if (eqz(d, ".4byte") || eqz(d, ".2byte") || eqz(d, ".byte")) {
        i64 size = d.p[1] == '4' ? 4 : d.p[1] == '2' ? 2 : 1;
        const char *kind = size == 4 ? "4byte" : size == 2 ? "2byte" : "byte";
        split_args(args, av);
        for (size_t k = 0; k < av->n; k++) {
            AvpStr x = av->v[k];
            if (!x.n) continue;
            ShsmAsmItem it = {addr, kind, {SHSM_VAL_NONE}, {NULL, 0}, 0, size};
            i64 v;
            SymRef m;
            if (py_int_tok(x, &v)) {
                it.value.t = SHSM_VAL_INT;
                it.value.i = v;
            } else if (symref_fullmatch(x, &m)) {
                AvpStr sym = shsm_asm_unquote(p->a, m.g1);
                i64 off;
                if (!g2_value(m.g2, &off)) off = 0;
                it.kind = SHSM_REF_PTR;
                it.sym = sym;
                it.offset = off;
                add_ref(p, owner, addr, sym, off, SHSM_REF_PTR, owner_addr, SHSM_NONE);
            } else {
                it.value.t = SHSM_VAL_STR;
                it.value.s = x;
            }
            PUSH_ITEM(it);
            addr += size;
        }
    } else if (eqz(d, ".float") || eqz(d, ".double")) {
        i64 size = d.p[1] == 'f' ? 4 : 8;
        const char *kind = size == 4 ? "float" : "double";
        split_args(args, av);
        for (size_t k = 0; k < av->n; k++) {
            AvpStr x = av->v[k];
            double v;
            bool ok = avp_parse_float(x, &v);
            ShsmAsmItem it = {addr, kind, {ok ? SHSM_VAL_FLOAT : SHSM_VAL_NONE, 0, ok ? v : 0}, {NULL, 0}, 0, size};
            PUSH_ITEM(it);
            ShsmAsmFloat f = {owner, addr, kind, ok, ok ? v : 0, x};
            avp_vec_push(&p->af->floats, f);
            addr += size;
        }
    } else if (eqz(d, ".string") || eqz(d, ".asciz") || eqz(d, ".string16")) {
        bool wide = eqz(d, ".string16");
        split_args(args, av);
        for (size_t k = 0; k < av->n; k++) {
            AvpStr x = av->v[k];
            if (!(x.n && x.p[0] == '"' && x.p[x.n - 1] == '"')) continue;
            AvpStr body = x.n >= 2 ? avp_strn(x.p + 1, x.n - 2) : avp_strn(x.p, 0);
            AvpStr b = shsm_asm_decode_string(p->a, body);
            AvpStr text = shsm_asm_bytes_to_text(p->a, b);
            i64 size = wide ? (i64)(2 * (utf16_units(text) + 1)) : (i64)b.n + 1;
            const char *kind = wide ? "string16" : "string";
            ShsmAsmItem it = {addr, kind, {SHSM_VAL_STR}, {NULL, 0}, 0, size};
            it.value.s = text;
            PUSH_ITEM(it);
            ShsmAsmString s = {owner, addr, text, kind};
            avp_vec_push(&p->af->strings, s);
            addr += size;
        }
    } else if (eqz(d, ".skip")) {
        const char *c = memchr(args.p, ',', args.n);
        i64 n = 0;
        if (!py_int_tok(c ? avp_strn(args.p, (size_t)(c - args.p)) : args, &n)) n = 0;
        ShsmAsmItem it = {addr, "skip", {SHSM_VAL_INT, n}, {NULL, 0}, 0, n};
        PUSH_ITEM(it);
        addr += n;
    } else if (eqz(d, ".rel")) {
        split_args(args, av);
        if (av->n == 2) {
            AvpStr sym = shsm_asm_unquote(p->a, av->v[0]);
            i64 tgt;
            if (!label_match(av->v[1], &tgt)) tgt = SHSM_NONE;
            ShsmAsmItem it = {addr, "rel", {tgt == SHSM_NONE ? SHSM_VAL_NONE : SHSM_VAL_INT, tgt}, sym, 0, 4};
            PUSH_ITEM(it);
            add_ref(p, owner, addr, sym, 0, SHSM_REF_JUMPTABLE, owner_addr, tgt);
            Rel r = {cur, cur ? cur->items.n - 1 : 0, p->af->refs.n - 1, tgt};
            avp_vec_push(&p->rels, r);
            addr += 4;
        }
    } else {
        avp_vec_free(&loose);
        return p->cursor;
    }
#undef PUSH_ITEM
    avp_vec_free(&loose);
    return (p->cursor != SHSM_NONE || nitems) ? addr : p->cursor;
}

/* asm._resolve_refs */
static void resolve_refs(Parser *p) {
    ShsmAsmFile *af = p->af;
    AvpMap *blocks = avp_map_new_cap(af->blocks.n + 1);
    for (size_t i = 0; i < af->blocks.n; i++) {
        ShsmAsmBlock *b = af->blocks.v[i];
        if (b->address == SHSM_NONE) continue;
        AvpVal *v = avp_map_get(blocks, b->name);
        if (!v || ((ShsmAsmBlock *)v->p)->kind == K_LABEL) avp_map_put(blocks, b->name, NULL)->p = b;
    }
    for (size_t i = 0; i < af->refs.n; i++) {
        ShsmAsmRef *r = &af->refs.v[i];
        AvpVal *v = avp_map_get(blocks, r->to_sym);
        if (v && r->kind != SHSM_REF_JUMPTABLE) {
            ShsmAsmBlock *b = v->p;
            r->to_base = b->address;
            r->to_addr = b->address + r->to_off;
        }
    }
    for (size_t i = 0; i < p->rels.n; i++) {
        Rel *rel = &p->rels.v[i];
        ShsmAsmRef *r = &af->refs.v[rel->ref];
        AvpVal *v = avp_map_get(blocks, r->to_sym);
        if (!v) continue;
        ShsmAsmBlock *b = v->p;
        r->to_base = b->address;
        if (b->kind != K_FUNCTION && rel->value != SHSM_NONE) {
            i64 off = rel->value - b->address;
            if (rel->blk) {
                ShsmAsmItem *it = &rel->blk->items.v[rel->item];
                it->kind = SHSM_REF_PTR;
                it->value.t = SHSM_VAL_NONE;
                it->offset = off;
            }
            r->kind = SHSM_REF_PTR;
            r->to_off = off;
        }
    }
    avp_map_free(blocks);
}

/* _BLOCK_HEAD: ^# (\S+?):0x([0-9A-Fa-f]+) \| 0x([0-9A-Fa-f]+) \| size: 0x([0-9A-Fa-f]+) */
static size_t hex_run(AvpStr s, size_t i) {
    size_t j = i;
    while (j < s.n && is_hex(s.p[j])) j++;
    return j;
}
static bool lit_at(AvpStr s, size_t i, const char *lit) {
    size_t n = strlen(lit);
    return i + n <= s.n && memcmp(s.p + i, lit, n) == 0;
}
static bool block_head(AvpStr line, AvpStr *sec, i64 *addr, i64 *size) {
    if (!starts(line, "# ")) return false;
    for (size_t k = 3; k < line.n && !ws_at(line, k); k++) { /* \S+? : the first ':' the rest matches after */
        if (!lit_at(line, k, ":0x")) continue;
        size_t i = k + 3, e1 = hex_run(line, i);
        if (e1 == i || !lit_at(line, e1, " | 0x")) continue;
        size_t a0 = e1 + 5, e2 = hex_run(line, a0);
        if (e2 == a0 || !lit_at(line, e2, " | size: 0x")) continue;
        size_t s0 = e2 + 11, e3 = hex_run(line, s0);
        if (e3 == s0) continue;
        *sec = avp_strn(line.p + 2, k - 2);
        *addr = hexval(avp_strn(line.p + a0, e2 - a0));
        *size = hexval(avp_strn(line.p + s0, e3 - s0));
        return true;
    }
    return false;
}
/* _RUN_HEAD: ^# 0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+) \| size: 0x([0-9A-Fa-f]+) */
static bool run_head(AvpStr line, i64 *start, i64 *end) {
    if (!starts(line, "# 0x")) return false;
    size_t e1 = hex_run(line, 4);
    if (e1 == 4 || !lit_at(line, e1, "..0x")) return false;
    size_t e2 = hex_run(line, e1 + 4);
    if (e2 == e1 + 4 || !lit_at(line, e2, " | size: 0x")) return false;
    size_t e3 = hex_run(line, e2 + 11);
    if (e3 == e2 + 11) return false;
    *start = hexval(avp_strn(line.p + 4, e1 - 4));
    *end = hexval(avp_strn(line.p + e1 + 4, e2 - e1 - 4));
    return true;
}

/* _START: ^\.(fn|obj|sym) ("(?:[^"\\]|\\.)*"|[^\s,]+), (\w+) */
static bool start_match(AvpStr line, size_t i, AvpStr *name, AvpStr *scope) {
    size_t e = 0;
    if (i < line.n && line.p[i] == '"') {
        size_t q = symref_g1(line, i);
        if (q && lit_at(line, q, ", ")) e = q;
    }
    if (!e) {
        size_t j = i;
        while (j < line.n && line.p[j] != ',' && !ws_at(line, j)) j++;
        if (j == i || !lit_at(line, j, ", ")) return false;
        e = j;
    }
    size_t s0 = e + 2, s1 = s0;
    while (s1 < line.n && is_word(line.p[s1])) s1++;
    if (s1 == s0) return false;
    *name = avp_strn(line.p + i, e - i);
    *scope = avp_strn(line.p + s0, s1 - s0);
    return true;
}

/* _ROW: ^/\* ([0-9A-Fa-f]{8}) [0-9A-Fa-f]{8}  ((?:[0-9A-Fa-f]{2} ?)+)\*\/\s*(.*?)\s*$ */
static bool row_match(Parser *p, AvpStr line, i64 *addr, AvpStr *hexb, AvpStr *body) {
    if (line.n < 23 || !starts(line, "/* ")) return false;
    for (size_t i = 3; i < 11; i++)
        if (!is_hex(line.p[i])) return false;
    if (line.p[11] != ' ') return false;
    for (size_t i = 12; i < 20; i++)
        if (!is_hex(line.p[i])) return false;
    if (line.p[20] != ' ' || line.p[21] != ' ') return false;
    size_t j = 22, npairs = 0;
    char *hx = avp_alloc_noz(p->a, line.n);
    size_t hn = 0;
    while (j + 1 < line.n && is_hex(line.p[j]) && is_hex(line.p[j + 1])) {
        hx[hn++] = line.p[j];
        hx[hn++] = line.p[j + 1];
        j += 2;
        npairs++;
        if (j < line.n && line.p[j] == ' ') j++;
    }
    if (!npairs || !lit_at(line, j, "*/")) return false;
    *addr = hexval(avp_strn(line.p + 3, 8));
    *hexb = avp_strn(hx, hn);
    *body = pystrip(sub(line, j + 2));
    return true;
}

static void finish_row(Parser *p, ShsmAsmRow *row) {
    ShsmAsmBlock *cur = p->cur;
    cur->nrows++;
    if (!(p->flags & SHSM_ASM_NO_ROWS)) avp_vec_push(&cur->rows, *row);
}

static void parse_row(Parser *p, AvpStr line) {
    ShsmAsmBlock *cur = p->cur;
    i64 addr;
    AvpStr hexb, body;
    if (!row_match(p, line, &addr, &hexb, &body)) return;
    ShsmAsmRow row = {addr, SHSM_NONE, hexb, {0}, {0}, {NULL, 0}, 0, NULL, SHSM_NONE};
    if (hexb.n == 8) row.word = hexval(hexb);
    if (!body.n || body.p[0] == '.' || body.p[0] == '#') {
        if (starts(body, ".4byte")) {
            AvpStr rest = sub(body, 6);
            for (size_t i = 0; i + 1 < rest.n; i++)
                if (rest.p[i] == '/' && rest.p[i + 1] == '*') {
                    rest.n = i;
                    break;
                }
            row.mnemonic = AVP_S(".4byte");
            row.operands = pystrip(rest);
            finish_row(p, &row);
        }
        return;
    }
    size_t i = 0;
    while (i < body.n && !ws_at(body, i)) i++;
    row.mnemonic = avp_strn(body.p, i);
    row.operands = i < body.n ? pystrip(sub(body, i)) : avp_strn(body.p + body.n, 0);
    shsm_asm_parse_operands(p->a, &row);
    if (row.reloc == REL24 && avp_str_eq(row.sym, cur->name) && row.word != SHSM_NONE) {
        i64 tgt = branch_target((u32)row.word, row.address);
        if (tgt != SHSM_NONE && !eqz(row.mnemonic, "bl")) {
            row.label = tgt;
            row.sym = (AvpStr){NULL, 0};
            row.reloc = NULL;
        }
    }
    finish_row(p, &row);
    if (row.sym.p) {
        const char *kind;
        if (row.reloc == REL24)
            kind = (ends(row.mnemonic, "l") || ends(row.mnemonic, "la")) ? SHSM_REF_CALL : SHSM_REF_BRANCH;
        else kind = row.reloc;
        add_ref(p, cur->name, row.address, row.sym, row.sym_off, kind, cur->address, SHSM_NONE);
    }
}

static bool is_section_word(AvpStr line) {
    static const char *const names[] = {".text", ".data", ".rodata", ".bss", ".sdata", ".sbss", ".sdata2", ".sbss2",
                                        ".init"};
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (eqz(line, names[i])) return true;
    return false;
}

ShsmAsmFile *shsm_asm_parse_text(AvpStr text, const char *path, const char *unit, unsigned flags) {
    AvpArena *a = avp_arena_new(text.n + (1 << 20));
    ShsmAsmFile *af = AVP_NEW(a, ShsmAsmFile);
    af->arena = a;
    af->path = avp_strdup(a, path ? path : "");
    af->unit = avp_strdup(a, unit ? unit : "");
    AvpStr t = avp_strn(avp_memdup(a, text.p, text.n), text.n);
    Parser p = {af, a, flags, NULL, {NULL, 0}, SHSM_NONE, {0}, {0}};
    bool have_pending = false;
    AvpStr pend_sec = {0}, demangled = {NULL, 0};
    i64 pend_addr = 0, pend_size = 0, pending_pos = 0;
    bool have_run = false;
    i64 run_start = 0, run_end = 0;
    i64 byte_pos = 0, ln = 0;
    size_t pos = 0;
    for (;;) {
        const char *nl = memchr(t.p + pos, '\n', t.n - pos);
        size_t len = nl ? (size_t)(nl - (t.p + pos)) : t.n - pos;
        AvpStr raw = avp_strn(t.p + pos, len);
        ln++;
        i64 start_pos = byte_pos;
        byte_pos += (i64)len + 1;
        pos += len + 1;
        AvpStr line = pystrip(raw);
        if (line.n) {
            char c0 = line.p[0];
            ShsmAsmBlock *cur = p.cur;
            if (c0 == '/' && starts(line, "/* ") && cur && cur->kind == K_FUNCTION) {
                parse_row(&p, line);
            } else if (c0 == '#') {
                AvpStr sec;
                i64 x, y;
                if (block_head(line, &sec, &x, &y)) {
                    have_pending = true;
                    pend_sec = sec, pend_addr = x, pend_size = y;
                    pending_pos = start_pos;
                    demangled = (AvpStr){NULL, 0};
                } else if (run_head(line, &x, &y)) {
                    p.cursor = x;
                    have_run = true;
                    run_start = x, run_end = y;
                    have_pending = false;
                    demangled = (AvpStr){NULL, 0};
                } else if (have_pending && !cur) {
                    size_t k = 1 < line.n ? 1 + utf8_len((u8)line.p[1]) : line.n; /* line[2:] */
                    demangled = pystrip(sub(line, k));
                }
            } else if (c0 == '.') {
                if (starts(line, ".L_") && ends(line, ":")) {
                    if (cur && cur->kind == K_FUNCTION && line.n >= 11) {
                        AvpStr h = avp_strn(line.p + 3, line.n >= 12 ? 8 : line.n - 3);
                        bool ok = h.n == 8;
                        for (size_t i = 0; ok && i < h.n; i++) ok = is_hex(h.p[i]);
                        if (ok) {
                            i64 la = hexval(h);
                            AvpStr nm = avp_strn(line.p, line.n - 1);
                            bool found = false;
                            for (size_t i = 0; i < cur->labels.n; i++)
                                if (cur->labels.v[i].address == la) {
                                    cur->labels.v[i].name = nm;
                                    found = true;
                                    break;
                                }
                            if (!found) {
                                ShsmAsmLabel l = {la, nm};
                                avp_vec_push(&cur->labels, l);
                            }
                        }
                    }
                } else if (starts(line, ".fn ") || starts(line, ".obj ") || starts(line, ".sym ")) {
                    size_t kl = line.p[1] == 'f' ? 4 : 5; /* ".fn ", ".obj ", ".sym " */
                    AvpStr name, scope;
                    if (start_match(line, kl, &name, &scope)) {
                        const char *kind = line.p[1] == 'f' ? K_FUNCTION : line.p[1] == 'o' ? K_OBJECT : K_LABEL;
                        name = shsm_asm_unquote(a, name);
                        if (kind == K_LABEL) {
                            ShsmAsmBlock *b = new_block(&p, kind, name, scope, p.section, p.cursor, SHSM_NONE,
                                                        (AvpStr){NULL, 0}, ln, start_pos);
                            b->byte_end = byte_pos;
                            avp_vec_push(&af->blocks, b);
                        } else {
                            AvpStr sec = have_pending ? pend_sec : p.section;
                            i64 addr = have_pending ? pend_addr : p.cursor;
                            i64 size = have_pending ? pend_size : SHSM_NONE;
                            p.cur = new_block(&p, kind, name, scope, sec.p && sec.n ? sec : p.section, addr, size,
                                              demangled, ln, have_pending ? pending_pos : start_pos);
                            have_pending = false;
                            demangled = (AvpStr){NULL, 0};
                            if (addr != SHSM_NONE) p.cursor = addr;
                        }
                    }
                } else if (starts(line, ".endfn") || starts(line, ".endobj")) {
                    if (cur) {
                        cur->line_end = ln;
                        cur->byte_end = byte_pos;
                        avp_vec_push(&af->blocks, cur);
                        if (cur->size != SHSM_NONE && cur->address != SHSM_NONE) p.cursor = cur->address + cur->size;
                        p.cur = NULL;
                    }
                } else if (starts(line, ".hidden")) {
                    if (cur) cur->hidden = true;
                } else if (starts(line, ".section") || is_section_word(line)) {
                    if (starts(line, ".section")) {
                        size_t i = 0;
                        while (i < line.n && !ws_at(line, i)) i++;
                        AvpStr rest = sub(line, shsm_skip_ws(line, i));
                        const char *c = memchr(rest.p, ',', rest.n);
                        p.section = pystrip(c ? avp_strn(rest.p, (size_t)(c - rest.p)) : rest);
                    } else p.section = line;
                    if (have_run) {
                        ShsmAsmSection s = {p.section, run_start, run_end};
                        avp_vec_push(&af->sections, s);
                        have_run = false;
                    }
                } else if (starts(line, ".balign")) {
                    if (p.cursor != SHSM_NONE) {
                        size_t i = 0;
                        while (i < line.n && !ws_at(line, i)) i++;
                        i = shsm_skip_ws(line, i);
                        size_t j = i;
                        while (j < line.n && !ws_at(line, j)) j++;
                        i64 al;
                        if (py_int_tok(avp_strn(line.p + i, j - i), &al) && al > 0)
                            p.cursor = (p.cursor + al - 1) / al * al;
                    }
                } else if (starts(line, ".include") || starts(line, ".file")) {
                } else {
                    p.cursor = data_line(&p, line);
                }
            }
        }
        if (!nl) break;
    }
    if (p.cur) { /* dtk can leave a file's last block without its .endobj */
        p.cur->line_end = ln;
        p.cur->byte_end = byte_pos;
        avp_vec_push(&af->blocks, p.cur);
    }
    resolve_refs(&p);
    avp_vec_free(&p.rels);
    avp_vec_free(&p.args);
    return af;
}

ShsmAsmFile *shsm_asm_parse_file(const char *path, const char *unit, unsigned flags) {
    AvpArena *a = avp_arena_new(0);
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8_NL_EMPTY, &text)) {
        avp_arena_free(a);
        shsm_raise_io();
    }
    ShsmAsmFile *af = shsm_asm_parse_text(text, path, unit, flags);
    avp_arena_free(a);
    return af;
}

void shsm_asm_free(ShsmAsmFile *af) {
    if (!af) return;
    for (size_t i = 0; i < af->blocks.n; i++) {
        ShsmAsmBlock *b = af->blocks.v[i];
        avp_vec_free(&b->rows);
        avp_vec_free(&b->labels);
        avp_vec_free(&b->items);
    }
    avp_vec_free(&af->blocks);
    avp_vec_free(&af->refs);
    avp_vec_free(&af->strings);
    avp_vec_free(&af->floats);
    avp_vec_free(&af->sections);
    avp_arena_free(af->arena);
}

static ShsmAsmBlock *find_block(const ShsmAsmFile *af, AvpStr name, const char *kind) {
    for (size_t i = 0; i < af->blocks.n; i++) {
        ShsmAsmBlock *b = af->blocks.v[i];
        if ((!kind || b->kind == kind) && avp_str_eq(b->name, name)) return b;
    }
    return NULL;
}
ShsmAsmBlock *shsm_asm_function(const ShsmAsmFile *af, AvpStr name) { return find_block(af, name, K_FUNCTION); }
ShsmAsmBlock *shsm_asm_object(const ShsmAsmFile *af, AvpStr name) { return find_block(af, name, K_OBJECT); }
ShsmAsmBlock *shsm_asm_block(const ShsmAsmFile *af, AvpStr name) { return find_block(af, name, NULL); }

ShsmAsmBlock *shsm_asm_read_block(const char *path, i64 byte_start, i64 byte_end, AvpStr section,
                                  ShsmAsmFile **out_file) {
    *out_file = NULL;
    AvpArena *a = avp_arena_new(0);
    AvpBytes all;
    if (!avp_read_all(a, path, &all)) {
        avp_arena_free(a);
        shsm_raise_io();
    }
    size_t s = byte_start < 0 ? 0 : (size_t)byte_start > all.n ? all.n : (size_t)byte_start;
    size_t e = byte_end < (i64)s ? s : (size_t)byte_end > all.n ? all.n : (size_t)byte_end;
    AvpStr frag = avp_strn((const char *)all.v + s, e - s);
    if (!valid_utf8(frag)) {
        avp_arena_free(a);
        shsm_raise(SHSM_EXC_VALUE, "'utf-8' codec can't decode the block at %lld..%lld of %s", (long long)byte_start,
                   (long long)byte_end, path);
    }
    ShsmAsmFile *af = shsm_asm_parse_text(frag, path, "", 0);
    avp_arena_free(a);
    *out_file = af;
    ShsmAsmBlock *b = NULL;
    for (size_t i = 0; i < af->blocks.n && !b; i++)
        if (af->blocks.v[i]->kind != K_LABEL) b = af->blocks.v[i];
    if (!b && af->blocks.n) b = af->blocks.v[0];
    if (b && !b->section.p) b->section = section.p ? avp_str_dup(af->arena, section) : section;
    return b;
}

/* ---- JSON ---------------------------------------------------------------------------------------------------- */

AvpJson *shsm_json_opt_int(AvpArena *a, i64 v) { return v == SHSM_NONE ? avp_json_null() : avp_json_int(a, v); }
AvpJson *shsm_json_opt_str(AvpArena *a, AvpStr s) { return s.p ? avp_json_str(a, s) : avp_json_null(); }
static AvpJson *jz(AvpArena *a, const char *s) { return s ? avp_json_str(a, avp_str(s)) : avp_json_null(); }
static void set(AvpArena *a, AvpJson *o, const char *k, AvpJson *v) { avp_json_set(a, o, avp_str(k), v); }

AvpJson *shsm_asm_row_json(AvpArena *a, const ShsmAsmRow *r) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "address", avp_json_int(a, r->address));
    set(a, o, "bytes", avp_json_str(a, r->bytes));
    AvpStr text = r->operands.n ? avp_str(avp_sprintf(a, "%.*s %.*s", AVP_SF(r->mnemonic), AVP_SF(r->operands)))
                                : r->mnemonic;
    set(a, o, "text", avp_json_str(a, text));
    set(a, o, "mnemonic", avp_json_str(a, r->mnemonic));
    set(a, o, "operands", avp_json_str(a, r->operands));
    set(a, o, "sym", shsm_json_opt_str(a, r->sym));
    set(a, o, "sym_off", avp_json_int(a, r->sym_off));
    set(a, o, "reloc", jz(a, r->reloc));
    set(a, o, "label", shsm_json_opt_int(a, r->label));
    return o;
}

static AvpJson *val_json(AvpArena *a, const ShsmAsmVal *v) {
    switch (v->t) {
    case SHSM_VAL_INT: return avp_json_int(a, v->i);
    case SHSM_VAL_FLOAT: return avp_json_float(a, v->f);
    case SHSM_VAL_STR: return avp_json_str(a, v->s);
    default: return avp_json_null();
    }
}

AvpJson *shsm_asm_item_json(AvpArena *a, const ShsmAsmItem *it) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "address", avp_json_int(a, it->address));
    set(a, o, "kind", jz(a, it->kind));
    set(a, o, "value", val_json(a, &it->value));
    set(a, o, "size", avp_json_int(a, it->size));
    if (it->sym.p) {
        set(a, o, "sym", avp_json_str(a, it->sym));
        set(a, o, "offset", avp_json_int(a, it->offset));
    }
    return o;
}

AvpJson *shsm_asm_block_json(AvpArena *a, const ShsmAsmBlock *b, bool rows) {
    AvpJson *o = avp_json_obj(a);
    set(a, o, "name", avp_json_str(a, b->name));
    set(a, o, "kind", jz(a, b->kind));
    set(a, o, "scope", avp_json_str(a, b->scope));
    set(a, o, "section", shsm_json_opt_str(a, b->section));
    set(a, o, "address", shsm_json_opt_int(a, b->address));
    set(a, o, "size", shsm_json_opt_int(a, b->size));
    set(a, o, "hidden", avp_json_bool(b->hidden));
    set(a, o, "demangled", shsm_json_opt_str(a, b->demangled));
    if (b->kind == K_FUNCTION && rows) {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < b->rows.n; i++) avp_json_arr_push(a, arr, shsm_asm_row_json(a, &b->rows.v[i]));
        set(a, o, "rows", arr);
    }
    if (b->kind == K_OBJECT) {
        AvpJson *arr = avp_json_arr(a);
        for (size_t i = 0; i < b->items.n; i++) avp_json_arr_push(a, arr, shsm_asm_item_json(a, &b->items.v[i]));
        set(a, o, "items", arr);
    }
    return o;
}

AvpJson *shsm_asm_ref_json(AvpArena *a, const ShsmAsmRef *r) {
    AvpJson *l = avp_json_arr(a);
    avp_json_arr_push(a, l, shsm_json_opt_str(a, r->from_sym));
    avp_json_arr_push(a, l, avp_json_int(a, r->from_addr));
    avp_json_arr_push(a, l, avp_json_str(a, r->to_sym));
    avp_json_arr_push(a, l, avp_json_int(a, r->to_off));
    avp_json_arr_push(a, l, jz(a, r->kind));
    avp_json_arr_push(a, l, shsm_json_opt_int(a, r->owner_addr));
    avp_json_arr_push(a, l, shsm_json_opt_int(a, r->to_base));
    avp_json_arr_push(a, l, shsm_json_opt_int(a, r->to_addr));
    avp_json_arr_push(a, l, shsm_json_opt_str(a, r->section));
    return l;
}

/* ---- which listings are current ------------------------------------------------------------------------------- */

const char *shsm_asm_listing_rel(AvpArena *a, const char *unit) {
    char *u = avp_strdup(a, unit);
    for (char *c = u; *c; c++)
        if (*c == '\\') *c = '/';
    AvpStr root, ext;
    avp_path_splitext(u, &root, &ext);
    static const char *const exts[] = {".c", ".cpp", ".cp", ".s", ".o", ".C", ".cc"};
    bool src = false;
    for (size_t i = 0; i < sizeof exts / sizeof *exts; i++) src = src || eqz(ext, exts[i]);
    return src ? avp_sprintf(a, "%.*s.s", AVP_SF(root)) : avp_sprintf(a, "%s.s", u);
}

const char *shsm_asm_split_record(AvpArena *a, const char *asm_dir, AvpSet **named) {
    *named = NULL;
    const char *norm = avp_path_normpath(a, asm_dir);
    const char *path = avp_path_join(a, avp_cz(a, avp_path_dirname(norm)), "config.json");
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) return path;
    AvpJsonError err;
    AvpJson *d = avp_json_parse(a, text, &err);
    if (!d || !avp_json_is_obj(d)) return path;
    AvpJson *units = avp_json_get(d, AVP_S("units"));
    if (!units) return path;
    AvpJson *mods = avp_json_get(d, AVP_S("modules"));
    if (mods && avp_json_truthy(mods)) return path;
    if (!avp_json_is_arr(units)) return path;
    AvpSet *s = avp_set_new();
    for (size_t i = 0; i < avp_json_len(units); i++) {
        AvpJson *u = avp_json_at(units, i);
        AvpJson *n = avp_json_is_obj(u) ? avp_json_get(u, AVP_S("name")) : NULL;
        if (!n || !avp_json_is_str(n)) return path;
        avp_set_add(s, avp_str(shsm_asm_listing_rel(a, avp_cz(a, n->u.str))));
    }
    *named = s;
    return path;
}

const char *shsm_asm_stale_note(AvpArena *a, const AvpStrVec *stale, const char *record, size_t limit) {
    if (!stale->n) return NULL;
    AvpBuf b = {0};
    for (size_t i = 0; i < stale->n && i < limit; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, stale->v[i]);
    }
    if (stale->n > limit) avp_buf_putz(&b, ", ...");
    const char *r = avp_sprintf(a, "%zu stale asm listing%s from an earlier split, not named by %s, ignored: %.*s",
                                stale->n, stale->n == 1 ? "" : "s", record, (int)b.n, b.p);
    avp_buf_free(&b);
    return r;
}

typedef struct {
    AvpStr rel, path;
} RelPath;
static int cmp_relpath(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_str_cmp(((const RelPath *)x)->rel, ((const RelPath *)y)->rel);
}

ShsmListings *shsm_asm_listings(AvpArena *a, const char *asm_dir) {
    AVP_VEC(RelPath) disk = {0};
    AvpWalk *w = avp_walk_open(asm_dir, true);
    AvpWalkItem item = {0};
    size_t base = strlen(asm_dir);
    while (w && avp_walk_next(w, &item)) {
        for (size_t i = 0; i < item.files.n; i++) {
            AvpStr fn = item.files.v[i];
            if (!ends(fn, ".s")) continue;
            const char *p = avp_path_join(a, avp_cz(a, item.dirpath), avp_cz(a, fn));
            /* os.path.relpath(p, asm_dir) of a path under asm_dir: the part after it */
            const char *r = p + base;
            while (*r == '\\' || *r == '/') r++;
            char *rel = avp_strdup(a, r);
            for (char *c = rel; *c; c++)
                if (*c == '\\') *c = '/';
            RelPath rp = {avp_str(rel), avp_str(p)};
            avp_vec_push(&disk, rp);
        }
    }
    if (w) avp_walk_close(w);
    avp_sort(disk.v, disk.n, sizeof *disk.v, cmp_relpath, NULL, false);
    ShsmListings *l = AVP_NEW(a, ShsmListings);
    AvpSet *named;
    const char *record = shsm_asm_split_record(a, asm_dir, &named);
    for (size_t i = 0; i < disk.n; i++) {
        if (named && !avp_set_has(named, disk.v[i].rel)) {
            avp_vec_push(&l->stale, disk.v[i].rel);
            continue;
        }
        avp_vec_push(&l->rels, disk.v[i].rel);
        avp_vec_push(&l->paths, disk.v[i].path);
    }
    if (!named) {
        l->record = NULL;
        l->note = disk.n ? avp_sprintf(a,
                                       "no usable split record at %s: reading every .s file (listings left by an "
                                       "earlier split cannot be told apart)",
                                       record)
                         : NULL;
    } else {
        l->record = record;
        l->note = shsm_asm_stale_note(a, &l->stale, record, 10);
        avp_set_free(named);
    }
    avp_vec_free(&disk);
    return l;
}
