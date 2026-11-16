/* gatecheck layout, retail side: the dtk asm of a split (gatecheck.py is_anon, base_section, DataObj, AsmSection,
 * AsmCode, AsmUnit, unescape_asm_string, advance_directive, parse_asm_data). The regular expressions are hand
 * scanners (DEVIATIONS.md, "consolidation"). */
#include "gatecheck.h"
#include "shsm_text.h"

const char *const gc_data_sections[11] = {".rodata", ".data", ".sdata",      ".sdata2", ".ctors", ".dtors",
                                          "extab",   "extabindex", ".bss", ".sbss",   ".sbss2"};

static bool in_list(const char *name, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(name, list[i]) == 0) return true;
    return false;
}
bool gc_is_data_section(const char *name) { return name && in_list(name, gc_data_sections, 11); }
bool gc_is_nobits_section(const char *name) {
    static const char *const l[] = {".bss", ".sbss", ".sbss2"};
    return name && in_list(name, l, 3);
}
bool gc_is_code_section(const char *name) {
    static const char *const l[] = {".init", ".text"};
    return name && in_list(name, l, 2);
}
bool gc_is_root_section(const char *name) {
    static const char *const l[] = {".ctors", ".dtors", "extab", "extabindex"};
    return name && in_list(name, l, 4);
}

AvpStr gc_base_section(AvpStr name) {
    i64 d = avp_str_findc(name, '$');
    return d >= 0 ? avp_strn(name.p, (size_t)d) : name;
}

static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alnum(char c) { return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_word(char c) { return is_alnum(c) || c == '_' || (u8)c >= 0x80; }
static bool is_ws(AvpStr s, size_t i) { return i < s.n && shsm_ws_len(s, i) > 0; }

/* gap_\d{2}_[0-9A-Fa-f]{8}_[A-Za-z0-9]+ from s[i] to the end */
static bool gap_at(const char *s) {
    if (strncmp(s, "gap_", 4) != 0) return false;
    s += 4;
    if (!is_digit(s[0]) || !is_digit(s[1]) || s[2] != '_') return false;
    s += 3;
    for (int k = 0; k < 8; k++)
        if (!is_hex(s[k])) return false;
    s += 8;
    if (*s++ != '_' || !*s) return false;
    for (; *s; s++)
        if (!is_alnum(*s)) return false;
    return true;
}
bool gc_is_gap_name(const char *name) { return gap_at(name); }

bool gc_is_anon(const char *name) {
    if (!name[0]) return true;
    if (strncmp(name, "lbl_", 4) == 0 && name[4]) {
        const char *p = name + 4;
        while (*p && is_hex(*p)) p++;
        if (!*p) return true;
    }
    if (gap_at(name)) return true;
    if (name[0] == '@' || strncmp(name, "jumptable_", 10) == 0 || strncmp(name, ".L", 2) == 0) return true;
    if (strcmp(name, "<unnamed>") == 0) return true;
    const char *d = strrchr(name, '$');
    if (d && d[1]) {
        const char *p = d + 1;
        while (*p && is_digit(*p)) p++;
        if (!*p) return true;
    }
    return false;
}

/* ---- relocation dicts ------------------------------------------------------------------------------------------- */

static int cmp_reloc(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64(((const GcReloc *)x)->off, ((const GcReloc *)y)->off);
}

void gc_relocs_dict(GcRelocVec *v) {
    avp_sort(v->v, v->n, sizeof(GcReloc), cmp_reloc, NULL, false);
    size_t m = 0;
    for (size_t i = 0; i < v->n; i++) {
        if (m && v->v[m - 1].off == v->v[i].off)
            v->v[m - 1] = v->v[i]; /* the later value wins */
        else
            v->v[m++] = v->v[i];
    }
    v->n = m;
}

const GcReloc *gc_relocs_get(const GcRelocVec *v, i64 off) {
    size_t lo = 0, hi = v->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (v->v[mid].off < off)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < v->n && v->v[lo].off == off ? &v->v[lo] : NULL;
}

GcObj *gc_obj_new(AvpArena *a, const char *name, i64 off, i64 size, const u8 *data, i64 dlen, bool gap,
                  bool synthetic) {
    GcObj *o = AVP_NEW(a, GcObj);
    o->name = name;
    o->off = off;
    o->size = size;
    o->data = data;
    o->dlen = dlen;
    o->gap = gap;
    o->synthetic = synthetic;
    o->anon = gc_is_anon(name);
    return o;
}

GcAsmSection *gc_asm_section(const GcAsmUnit *u, const char *name) {
    for (size_t i = 0; i < u->sections.n; i++)
        if (strcmp(u->sections.v[i]->name, name) == 0) return u->sections.v[i];
    return NULL;
}
GcAsmCode *gc_asm_code(const GcAsmUnit *u, const char *name) {
    for (size_t i = 0; i < u->code.n; i++)
        if (strcmp(u->code.v[i]->name, name) == 0) return u->code.v[i];
    return NULL;
}

/* ---- directives -------------------------------------------------------------------------------------------------- */

/* the string as code points */
static AvpU32Vec code_points(AvpStr s) {
    AvpU32Vec v = {0};
    size_t i = 0;
    while (i < s.n) {
        u8 c = (u8)s.p[i];
        int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        u32 cp = len == 1 ? c : c & (0xFF >> (len + 1));
        for (int k = 1; k < len && i + (size_t)k < s.n; k++) cp = cp << 6 | ((u8)s.p[i + (size_t)k] & 0x3F);
        avp_vec_push(&v, cp);
        i += (size_t)len;
    }
    return v;
}

i64 gc_unescape_asm_string(AvpStr str) {
    AvpU32Vec s = code_points(str);
    i64 n = 0;
    size_t i = 0;
    while (i < s.n) {
        u32 c = s.v[i];
        if (c == '\\' && i + 1 < s.n) {
            u32 d = s.v[i + 1];
            if (d >= '0' && d <= '7') {
                size_t j = i + 1;
                while (j < s.n && j < i + 4 && s.v[j] >= '0' && s.v[j] <= '7') j++;
                i = j;
            } else {
                i += 2;
            }
        } else {
            i += 1;
        }
        n++;
    }
    avp_vec_free(&s);
    return n;
}

/* len(re.sub(r"\\0x[0-9A-Fa-f]+|\\.", "x", s)) */
static i64 string16_len(AvpStr str) {
    AvpU32Vec s = code_points(str);
    i64 n = 0;
    size_t i = 0;
    while (i < s.n) {
        if (s.v[i] == '\\' && i + 1 < s.n) {
            size_t j = i + 1;
            if (j + 1 < s.n && s.v[j] == '0' && s.v[j + 1] == 'x') {
                size_t h = j + 2;
                while (h < s.n && s.v[h] < 128 && is_hex((char)s.v[h])) h++;
                if (h > j + 2) {
                    i = h;
                    n++;
                    continue;
                }
            }
            if (s.v[j] != '\n') {
                i += 2;
                n++;
                continue;
            }
        }
        i++;
        n++;
    }
    avp_vec_free(&s);
    return n;
}

/* ^\.(string|ascii|string16)\s+"(.*)"\s*$ : the body, or false */
static bool quoted_body(AvpStr d, size_t i, AvpStr *body) {
    size_t j = shsm_skip_ws(d, i);
    if (j == i || j >= d.n || d.p[j] != '"') return false;
    /* (.*) greedy: the last quote followed only by whitespace */
    for (size_t q = d.n; q-- > j + 1;) {
        if (d.p[q] != '"') continue;
        if (shsm_skip_ws(d, q + 1) == d.n) {
            *body = avp_strn(d.p + j + 1, q - j - 1);
            return true;
        }
    }
    return false;
}

/* int(text, 0) */
static i64 py_int0(AvpArena *a, AvpStr t) {
    AvpStr s = avp_str_strip(t);
    size_t i = 0;
    bool neg = false;
    if (i < s.n && (s.p[i] == '+' || s.p[i] == '-')) neg = s.p[i++] == '-';
    int base = 10;
    if (i + 1 < s.n && s.p[i] == '0' && strchr("xXoObB", s.p[i + 1])) {
        char c = (char)(s.p[i + 1] | 0x20);
        base = c == 'x' ? 16 : c == 'o' ? 8 : 2;
        i += 2;
        if (i < s.n && s.p[i] == '_') i++;
    }
    i64 v = 0;
    bool ok = i < s.n, prev_us = false, nonzero = false;
    size_t first = i;
    for (; ok && i < s.n; i++) {
        char c = s.p[i];
        if (c == '_') {
            if (prev_us || i == first) ok = false;
            prev_us = true;
            continue;
        }
        prev_us = false;
        int dv = is_digit(c) ? c - '0' : (c >= 'a' && c <= 'z') ? c - 'a' + 10 : (c >= 'A' && c <= 'Z') ? c - 'A' + 10 : 99;
        if (dv >= base) {
            ok = false;
            break;
        }
        if (dv) nonzero = true;
        v = v * base + dv;
    }
    if (ok && prev_us) ok = false;
    if (ok && base == 10 && s.p[first] == '0' && nonzero) ok = false;
    if (!ok)
        gc_raise(GC_EXC_VALUE, "invalid literal for int() with base 0: %s", avp_cz(a, avp_repr_str(a, t)));
    return neg ? -v : v;
}

static AvpStr strip_quotes(AvpStr s) { return avp_str_strip_chars(s, AVP_S("\"")); }

static AVP_NORETURN void crash(void) { gc_raise(GC_EXC_CRASH, "AttributeError: 'NoneType' object has no attribute 'group'"); }

i64 gc_advance_directive(AvpArena *a, AvpStr line, i64 pos, GcRelocVec *relocs) {
    bool str = avp_str_startswith(line, AVP_S(".string")) || avp_str_startswith(line, AVP_S(".ascii"));
    AvpStr d = line;
    if (!str) {
        i64 h = avp_str_findc(line, '#');
        d = avp_str_strip(h >= 0 ? avp_strn(line.p, (size_t)h) : line);
    }
    if (avp_str_startswith(d, AVP_S(".rel "))) {
        /* ^\.rel\s+(\S+),\s*(\S+) */
        size_t i = shsm_skip_ws(d, 4);
        if (i == 4) crash();
        size_t t = i;
        while (t < d.n && !is_ws(d, t)) t++;
        for (size_t c = t; c-- > i + 1;) {
            if (d.p[c] != ',') continue;
            size_t k = shsm_skip_ws(d, c + 1);
            if (k < d.n && !is_ws(d, k)) {
                size_t e = k;
                while (e < d.n && !is_ws(d, e)) e++;
                GcReloc r = {pos, avp_cz(a, strip_quotes(avp_strn(d.p + i, c - i))), avp_cz(a, avp_strn(d.p + k, e - k))};
                avp_vec_push(relocs, r);
                return pos + 4;
            }
        }
        crash();
    }
    if (avp_str_startswith(d, AVP_S(".4byte"))) {
        AvpStr arg = avp_str_strip(avp_str_slice(d, 6, AVP_END));
        bool hexlit = arg.n > 2 && arg.p[0] == '0' && arg.p[1] == 'x';
        for (size_t i = 2; hexlit && i < arg.n; i++) hexlit = is_hex(arg.p[i]);
        if (arg.n && !hexlit) {
            /* ^("[^"]*"|[^+\-]+)([+\-]0x[0-9A-Fa-f]+)?$ */
            AvpStr g1 = {0}, g2 = {0};
            bool m = false;
            if (arg.p[0] == '"') {
                size_t q = 1;
                while (q < arg.n && arg.p[q] != '"') q++;
                if (q < arg.n) {
                    size_t e = q + 1;
                    if (e == arg.n) {
                        m = true;
                        g1 = avp_strn(arg.p, e);
                    } else if (arg.p[e] == '+' || arg.p[e] == '-') {
                        size_t h = e + 1;
                        if (h + 1 < arg.n && arg.p[h] == '0' && arg.p[h + 1] == 'x') {
                            size_t z = h + 2;
                            while (z < arg.n && is_hex(arg.p[z])) z++;
                            if (z > h + 2 && z == arg.n) {
                                m = true;
                                g1 = avp_strn(arg.p, e);
                                g2 = avp_strn(arg.p + e, z - e);
                            }
                        }
                    }
                }
            }
            if (!m) {
                size_t e = 0;
                while (e < arg.n && arg.p[e] != '+' && arg.p[e] != '-') e++;
                if (e > 0) {
                    if (e == arg.n) {
                        m = true;
                        g1 = arg;
                    } else {
                        size_t h = e + 1;
                        if (h + 1 < arg.n && arg.p[h] == '0' && arg.p[h + 1] == 'x') {
                            size_t z = h + 2;
                            while (z < arg.n && is_hex(arg.p[z])) z++;
                            if (z > h + 2 && z == arg.n) {
                                m = true;
                                g1 = avp_strn(arg.p, e);
                                g2 = avp_strn(arg.p + e, z - e);
                            }
                        }
                    }
                }
            }
            GcReloc r = {pos, avp_cz(a, strip_quotes(m ? g1 : arg)), m && g2.n ? avp_cz(a, g2) : ""};
            avp_vec_push(relocs, r);
        }
        return pos + 4;
    }
    if (avp_str_startswith(d, AVP_S(".int")) || avp_str_startswith(d, AVP_S(".float"))) return pos + 4;
    if (avp_str_startswith(d, AVP_S(".2byte")) || avp_str_startswith(d, AVP_S(".short"))) return pos + 2;
    if (avp_str_startswith(d, AVP_S(".8byte")) || avp_str_startswith(d, AVP_S(".double"))) return pos + 8;
    if (avp_str_startswith(d, AVP_S(".byte"))) return pos + (i64)avp_str_count(d, AVP_S(",")) + 1;
    if (avp_str_startswith(d, AVP_S(".skip"))) {
        AvpStrVec parts = avp_str_split(d, AVP_S(""), -1);
        if (parts.n < 2) gc_raise(GC_EXC_INDEX, "list index out of range");
        i64 v = py_int0(a, parts.v[1]);
        avp_vec_free(&parts);
        return pos + v;
    }
    if (avp_str_startswith(d, AVP_S(".string16"))) {
        AvpStr body;
        i64 n = quoted_body(d, 9, &body) ? string16_len(body) : 0;
        return pos + 2 * (n + 1);
    }
    if (str) {
        bool is_string = avp_str_startswith(d, AVP_S(".string"));
        AvpStr body;
        /* ^\.(string|ascii)\s+ : after ".string" or ".ascii" (6 bytes each) */
        i64 n = quoted_body(d, 7 - (is_string ? 0 : 1), &body) ? gc_unescape_asm_string(body) : 0;
        return pos + n + (is_string ? 1 : 0);
    }
    return pos;
}

/* the maximal run of \w after the leading '.' */
static AvpStr word_after_dot(AvpStr line) {
    size_t i = 1;
    while (i < line.n && is_word(line.p[i])) i++;
    return avp_strn(line.p + 1, i - 1);
}

static bool word_in(AvpStr w, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (avp_str_eqz(w, list[i])) return true;
    return false;
}

static i64 hexv(AvpStr s) {
    i64 v = 0;
    for (size_t i = 0; i < s.n; i++) v = v * 16 + (is_digit(s.p[i]) ? s.p[i] - '0' : (s.p[i] | 0x20) - 'a' + 10);
    return v;
}
/* "0x" hex+ at s[i]: the end, or 0 */
static size_t hex_lit(AvpStr s, size_t i) {
    if (i + 2 >= s.n || s.p[i] != '0' || s.p[i + 1] != 'x') return 0;
    size_t j = i + 2;
    while (j < s.n && is_hex(s.p[j])) j++;
    return j > i + 2 ? j : 0;
}
static bool lit_at(AvpStr s, size_t i, const char *lit) {
    size_t n = strlen(lit);
    return i + n <= s.n && memcmp(s.p + i, lit, n) == 0;
}

typedef struct {
    bool active;
    const char *name;
    bool has_addr;
    i64 addr, size;
    GcRelocVec relocs;
    i64 pos;
} OpenObj;

GcAsmUnit *gc_parse_asm_data(AvpArena *a, AvpStr text, const ShsmDol *retail) {
    static const char *const loose[] = {"4byte", "2byte", "short", "byte", "8byte", "double",
                                        "float", "int",   "skip",  "string", "ascii", "rel"};
    static const char *const secw[] = {"text", "data", "rodata", "init", "bss", "sbss", "sdata", "sdata2", "sbss2"};
    GcAsmUnit *unit = AVP_NEW(a, GcAsmUnit);
    unit->names = avp_set_new();
    const char *cur = NULL;
    bool has_span = false, has_pending = false;
    i64 span0 = 0, span1 = 0, pend_addr = 0, pend_size = 0;
    OpenObj obj = {0};
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = avp_str_strip(lines.v[li]);
        if (!line.n) continue;
        if (!obj.active) {
            char c0 = line.p[0];
            if (c0 == '#') {
                /* ^# 0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+) \| size */
                size_t h1 = lit_at(line, 0, "# ") ? hex_lit(line, 2) : 0;
                if (h1 && lit_at(line, h1, "..")) {
                    size_t h2 = hex_lit(line, h1 + 2);
                    if (h2 && lit_at(line, h2, " | size")) {
                        has_span = true;
                        span0 = hexv(avp_strn(line.p + 4, h1 - 4));
                        span1 = hexv(avp_strn(line.p + h1 + 4, h2 - h1 - 4));
                        continue;
                    }
                }
                /* ^# (\S+?):0x([0-9A-Fa-f]+) \| 0x([0-9A-Fa-f]+) \| size: 0x([0-9A-Fa-f]+) */
                if (lit_at(line, 0, "# ")) {
                    for (size_t c = 3; c < line.n && !is_ws(line, c - 1); c++) {
                        if (line.p[c] != ':') continue;
                        size_t x1 = hex_lit(line, c + 1);
                        if (!x1 || !lit_at(line, x1, " | ")) continue;
                        size_t x2 = hex_lit(line, x1 + 3);
                        if (!x2 || !lit_at(line, x2, " | size: ")) continue;
                        size_t x3 = hex_lit(line, x2 + 9);
                        if (!x3) continue;
                        has_pending = true;
                        pend_addr = hexv(avp_strn(line.p + x1 + 5, x2 - x1 - 5));
                        pend_size = hexv(avp_strn(line.p + x2 + 11, x3 - x2 - 11));
                        break;
                    }
                }
                continue;
            }
            if (c0 != '.') continue;
            if (avp_str_startswith(line, AVP_S(".fn "))) {
                AvpStrVec parts = avp_str_split(line, AVP_S(""), -1);
                AvpStr fname = strip_quotes(avp_str_rstrip_chars(parts.v[1], AVP_S(",")));
                avp_vec_free(&parts);
                avp_set_add(unit->names, fname);
                GcAsmCode *code = cur ? gc_asm_code(unit, cur) : NULL;
                if (code) avp_vec_push(&code->fns, avp_str_dup(a, fname));
                continue;
            }
            AvpStr w = word_after_dot(line);
            if (gc_is_data_section(cur) && gc_asm_section(unit, cur) && word_in(w, loose, 12)) {
                GcAsmSection *sec = gc_asm_section(unit, cur);
                sec->loose_pos = gc_advance_directive(a, line, sec->loose_pos, &sec->loose_relocs);
                continue;
            }
            if (avp_str_startswith(line, AVP_S(".obj "))) {
                if (!gc_is_data_section(cur)) continue;
                /* ^\.obj\s+("([^"]*)"|[^,\s]+)\s*,\s*(\w+) */
                size_t i = shsm_skip_ws(line, 4);
                AvpStr name = {0};
                bool m = false;
                if (i > 4) {
                    for (int alt = 0; alt < 2 && !m; alt++) {
                        size_t e;
                        AvpStr nm;
                        if (alt == 0) {
                            if (i >= line.n || line.p[i] != '"') continue;
                            size_t q = i + 1;
                            while (q < line.n && line.p[q] != '"') q++;
                            if (q >= line.n) continue;
                            nm = avp_strn(line.p + i + 1, q - i - 1);
                            e = q + 1;
                        } else {
                            e = i;
                            while (e < line.n && line.p[e] != ',' && !is_ws(line, e)) e++;
                            if (e == i) continue;
                            nm = avp_strn(line.p + i, e - i);
                        }
                        size_t k = shsm_skip_ws(line, e);
                        if (k >= line.n || line.p[k] != ',') continue;
                        k = shsm_skip_ws(line, k + 1);
                        if (k >= line.n || !is_word(line.p[k])) continue;
                        name = nm;
                        m = true;
                    }
                }
                if (!m) crash();
                avp_set_add(unit->names, name);
                GcAsmSection *sec = gc_asm_section(unit, cur);
                obj = (OpenObj){0};
                obj.active = true;
                obj.name = avp_cz(a, name);
                if (!has_pending) {
                    avp_vec_push(&sec->warnings,
                                 avp_str(avp_sprintf(a, "object %s has no address comment", obj.name)));
                } else {
                    obj.has_addr = true;
                    obj.addr = pend_addr;
                    obj.size = pend_size;
                }
                has_pending = false;
                continue;
            }
            if (word_in(w, secw, 9) && !avp_str_startswith(line, AVP_S(".section"))) {
                cur = avp_sprintf(a, ".%.*s", AVP_SF(w));
            } else if (avp_str_startswith(line, AVP_S(".section"))) {
                size_t i = shsm_skip_ws(line, 8);
                size_t e = i;
                while (e < line.n && line.p[e] != ',' && !is_ws(line, e)) e++;
                if (i == 8 || e == i) crash();
                cur = avp_cz(a, gc_base_section(avp_strn(line.p + i, e - i)));
            } else {
                continue;
            }
            if (gc_is_code_section(cur)) {
                GcAsmCode *code = gc_asm_code(unit, cur);
                if (!code) {
                    code = AVP_NEW(a, GcAsmCode);
                    code->name = cur;
                    avp_vec_push(&unit->code, code);
                }
                if (has_span) {
                    code->start = !code->has_start ? span0 : (span0 < code->start ? span0 : code->start);
                    code->end = !code->has_start ? span1 : (span1 > code->end ? span1 : code->end);
                    code->has_start = true;
                }
            }
            if (gc_is_data_section(cur)) {
                GcAsmSection *sec = gc_asm_section(unit, cur);
                if (!sec) {
                    sec = AVP_NEW(a, GcAsmSection);
                    sec->name = cur;
                    avp_vec_push(&unit->sections, sec);
                }
                if (has_span) {
                    sec->start = !sec->has_start ? span0 : (span0 < sec->start ? span0 : sec->start);
                    sec->end = !sec->has_end ? span1 : (span1 > sec->end ? span1 : sec->end);
                    sec->has_start = sec->has_end = true;
                }
            }
            has_span = false;
            continue;
        }
        /* inside an object */
        if (avp_str_startswith(line, AVP_S(".endobj"))) {
            GcAsmSection *sec = gc_asm_section(unit, cur);
            const u8 *data = NULL;
            i64 dlen = 0;
            if (obj.has_addr && !gc_is_nobits_section(cur) && retail) {
                AvpStr got;
                if (shsm_dol_read(a, retail, obj.addr, obj.size, &got)) {
                    data = got.p ? (const u8 *)got.p : (const u8 *)"";
                    dlen = (i64)got.n;
                }
            }
            gc_relocs_dict(&obj.relocs);
            GcObj *o = gc_obj_new(a, obj.name, 0, obj.has_addr ? obj.size : obj.pos, data, dlen,
                                  gc_is_gap_name(obj.name), false);
            o->relocs = obj.relocs;
            o->has_addr = obj.has_addr;
            o->addr = obj.addr;
            if (obj.has_addr && obj.pos != obj.size && !gc_is_nobits_section(cur))
                avp_vec_push(&sec->warnings, avp_str(avp_sprintf(a, "object %s: directives cover 0x%s bytes of 0x%s",
                                                                 obj.name, gc_hex(a, obj.pos), gc_hex(a, obj.size))));
            avp_vec_push(&sec->objs, o);
            obj.active = false;
            continue;
        }
        if (line.p[0] == '#' || line.p[0] != '.') continue;
        obj.pos = gc_advance_directive(a, line, obj.pos, &obj.relocs);
    }
    avp_vec_free(&lines);
    for (size_t si = 0; si < unit->sections.n; si++) {
        GcAsmSection *sec = unit->sections.v[si];
        if (sec->loose_pos && !sec->objs.n && sec->has_start) {
            const u8 *data = NULL;
            i64 dlen = 0;
            if (!gc_is_nobits_section(sec->name) && retail) {
                AvpStr got;
                if (shsm_dol_read(a, retail, sec->start, sec->loose_pos, &got)) {
                    data = got.p ? (const u8 *)got.p : (const u8 *)"";
                    dlen = (i64)got.n;
                }
            }
            GcRelocVec rel = {0};
            avp_vec_extend(&rel, sec->loose_relocs.v, sec->loose_relocs.n);
            gc_relocs_dict(&rel);
            GcObj *o = gc_obj_new(a, "<unnamed>", 0, sec->loose_pos, data, dlen, false, false);
            o->relocs = rel;
            o->has_addr = true;
            o->addr = sec->start;
            avp_vec_push(&sec->objs, o);
        }
        bool any = false;
        i64 mn = 0;
        for (size_t k = 0; k < sec->objs.n; k++)
            if (sec->objs.v[k]->has_addr) {
                if (!any || sec->objs.v[k]->addr < mn) mn = sec->objs.v[k]->addr;
                any = true;
            }
        if (!sec->has_start && any) {
            sec->start = mn;
            sec->has_start = true;
        }
        for (size_t k = 0; k < sec->objs.n; k++) {
            GcObj *o = sec->objs.v[k];
            if (o->has_addr && sec->has_start) o->off = o->addr - sec->start;
        }
    }
    return unit;
}
