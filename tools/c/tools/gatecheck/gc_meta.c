/* gatecheck: project metadata (parse_splits, parse_symbols_txt, parse_configure, read_text, SymbolMap,
 * elf_symbol_map, UnitMap). The regular expressions of the Python are hand scanners here (DEVIATIONS.md,
 * "consolidation"). */
#include "gatecheck.h"
#include "shsm_text.h"

static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
/* \w on str: ASCII letters, digits, underscore and (as an approximation) every non-ASCII code point */
static bool is_word(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (u8)c >= 0x80;
}
static bool is_ws(AvpStr s, size_t i) { return i < s.n && shsm_ws_len(s, i) > 0; }
static i64 hex_val(AvpStr s) {
    i64 v = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        v = v * 16 + (c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    }
    return v;
}
/* "0x" hex+ at s[i]: the end of the hex digits, or 0 when absent */
static size_t hex_lit(AvpStr s, size_t i) {
    if (i + 2 >= s.n + 0 || s.p[i] != '0' || s.p[i + 1] != 'x') return 0;
    size_t j = i + 2;
    while (j < s.n && is_hex(s.p[j])) j++;
    return j > i + 2 ? j : 0;
}

AvpStr gc_read_text(AvpArena *a, const char *path) {
    AvpStr out;
    if (!avp_read_text(a, path, AVP_TM_UTF8_REPLACE, &out))
        gc_raise(GC_EXC_GATE, "cannot read %s: %s", path, avp_io_err_str(a).p);
    return out;
}

/* ---- splits.txt -------------------------------------------------------------------------------------------------- */

static int cmp_split(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const GcSplit *a = x, *b = y;
    int c = avp_cmp_i64(a->start, b->start);
    if (!c) c = avp_cmp_i64(a->end, b->end);
    if (!c) c = strcmp(a->section, b->section);
    if (!c) c = strcmp(a->unit, b->unit);
    return c;
}

GcSplitVec gc_parse_splits(AvpArena *a, AvpStr text) {
    GcSplitVec out = {0};
    const char *unit = NULL;
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = lines.v[li];
        if (!avp_str_strip(line).n || avp_str_startswith(avp_str_lstrip(line), AVP_S("#"))) continue;
        if (line.p[0] != ' ' && line.p[0] != '\t') {
            /* ^(.+?):(?:\s.*)?$ on line.rstrip() */
            AvpStr l = avp_str_rstrip(line);
            unit = NULL;
            for (size_t i = 1; i < l.n; i++) {
                if (l.p[i] == ':' && (i + 1 == l.n || is_ws(l, i + 1))) {
                    AvpStr g = avp_strn(l.p, i);
                    if (!avp_str_eqz(g, "Sections")) unit = avp_cz(a, g);
                    break;
                }
            }
            continue;
        }
        if (!unit) continue;
        /* ^\s+(\S+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+) */
        size_t i = shsm_skip_ws(line, 0);
        if (i == 0) continue;
        size_t t0 = i;
        while (i < line.n && !is_ws(line, i)) i++;
        if (i == t0) continue;
        AvpStr tok = avp_strn(line.p + t0, i - t0);
        size_t j = shsm_skip_ws(line, i);
        if (j == i || !avp_str_startswith(avp_str_slice(line, (i64)j, AVP_END), AVP_S("start:"))) continue;
        j += 6;
        size_t h1 = hex_lit(line, j);
        if (!h1) continue;
        AvpStr start = avp_strn(line.p + j + 2, h1 - j - 2);
        size_t k = shsm_skip_ws(line, h1);
        if (k == h1 || !avp_str_startswith(avp_str_slice(line, (i64)k, AVP_END), AVP_S("end:"))) continue;
        k += 4;
        size_t h2 = hex_lit(line, k);
        if (!h2) continue;
        AvpStr end = avp_strn(line.p + k + 2, h2 - k - 2);
        AvpStr sec = tok;
        i64 dollar = avp_str_findc(tok, '$');
        if (dollar >= 0) sec = avp_strn(tok.p, (size_t)dollar);
        GcSplit s = {hex_val(start), hex_val(end), avp_cz(a, sec), unit};
        avp_vec_push(&out, s);
    }
    avp_vec_free(&lines);
    avp_sort(out.v, out.n, sizeof(GcSplit), cmp_split, NULL, false);
    return out;
}

/* ---- symbols.txt ------------------------------------------------------------------------------------------------- */

/* re.search(key + "(\w+)") / key + "0x([0-9A-Fa-f]+)": the first match, or false */
static bool search_word(AvpStr rest, const char *key, bool hex, AvpStr *out) {
    AvpStr k = avp_str(key);
    i64 from = 0;
    for (;;) {
        i64 at = avp_str_find(rest, k, from, AVP_END);
        if (at < 0) return false;
        size_t i = (size_t)at + k.n, j = i;
        if (hex) {
            size_t h = hex_lit(rest, i);
            if (h) {
                *out = avp_strn(rest.p + i + 2, h - i - 2);
                return true;
            }
        } else {
            while (j < rest.n && is_word(rest.p[j])) j++;
            if (j > i) {
                *out = avp_strn(rest.p + i, j - i);
                return true;
            }
        }
        from = at + 1;
    }
}

static int cmp_symtxt(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const GcSymTxt *a = x, *b = y;
    int c = avp_cmp_i64(a->addr, b->addr);
    return c ? c : avp_cmp_i64(b->size, a->size);
}

/* ^(.+?) = (\S+?):0x([0-9A-Fa-f]+);(.*)$ */
static bool match_symbol_line(AvpStr line, AvpStr *name, AvpStr *sec, AvpStr *addr, AvpStr *rest) {
    for (size_t e = 1; e + 3 <= line.n; e++) {
        if (line.p[e] != ' ' || line.p[e + 1] != '=' || line.p[e + 2] != ' ') continue;
        size_t s0 = e + 3;
        for (size_t c = s0 + 1; c < line.n && !is_ws(line, c - 1); c++) {
            if (line.p[c] != ':') continue;
            size_t h = hex_lit(line, c + 1);
            if (h && h < line.n && line.p[h] == ';') {
                *name = avp_strn(line.p, e);
                *sec = avp_strn(line.p + s0, c - s0);
                *addr = avp_strn(line.p + c + 3, h - c - 3);
                *rest = avp_strn(line.p + h + 1, line.n - h - 1);
                return true;
            }
        }
    }
    return false;
}

GcSymTxtVec gc_parse_symbols_txt(AvpArena *a, AvpStr text) {
    GcSymTxtVec out = {0};
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr name, sec, addr, rest, v;
        if (!match_symbol_line(lines.v[li], &name, &sec, &addr, &rest)) continue;
        GcSymTxt t = {0};
        t.addr = hex_val(addr);
        t.size = search_word(rest, "size:", true, &v) ? hex_val(v) : 0;
        t.name = avp_cz(a, name);
        t.section = avp_cz(a, sec);
        t.type = search_word(rest, "type:", false, &v) ? avp_cz(a, v) : "";
        t.scope = search_word(rest, "scope:", false, &v) ? avp_cz(a, v) : "";
        avp_vec_push(&out, t);
    }
    avp_vec_free(&lines);
    avp_sort(out.v, out.n, sizeof(GcSymTxt), cmp_symtxt, NULL, false);
    return out;
}

/* ---- configure.py ------------------------------------------------------------------------------------------------ */

AvpMap *gc_parse_configure(AvpArena *a, AvpStr text) {
    AvpMap *out = avp_map_new();
    AvpStrVec parts = avp_str_split(text, AVP_S("\n"), -1);
    AvpBuf b;
    avp_buf_init(&b, a);
    bool first = true;
    for (size_t i = 0; i < parts.n; i++) {
        if (avp_str_startswith(avp_str_lstrip(parts.v[i]), AVP_S("#"))) continue;
        if (!first) avp_buf_putc(&b, '\n');
        avp_buf_put(&b, parts.v[i]);
        first = false;
    }
    avp_vec_free(&parts);
    AvpStr t = avp_buf_str(&b);
    /* Object\(\s*(Matching|NonMatching|Equivalent|MatchingFor\([^)]*\))\s*,\s*"([^"]+)" */
    static const char *const alts[] = {"Matching", "NonMatching", "Equivalent"};
    i64 from = 0;
    for (;;) {
        i64 at = avp_str_find(t, AVP_S("Object("), from, AVP_END);
        if (at < 0) break;
        size_t i = shsm_skip_ws(t, (size_t)at + 7);
        size_t end = 0;
        AvpStr path = {0};
        const char *status = NULL;
        for (int alt = 0; alt < 4 && !end; alt++) {
            size_t j;
            const char *st;
            if (alt < 3) {
                AvpStr w = avp_str(alts[alt]);
                if (!avp_str_startswith(avp_str_slice(t, (i64)i, AVP_END), w)) continue;
                j = i + w.n;
                st = alts[alt];
            } else {
                if (!avp_str_startswith(avp_str_slice(t, (i64)i, AVP_END), AVP_S("MatchingFor("))) continue;
                j = i + 12;
                while (j < t.n && t.p[j] != ')') j++;
                if (j >= t.n) continue;
                j++;
                st = "Matching";
            }
            j = shsm_skip_ws(t, j);
            if (j >= t.n || t.p[j] != ',') continue;
            j = shsm_skip_ws(t, j + 1);
            if (j >= t.n || t.p[j] != '"') continue;
            size_t q = j + 1;
            while (q < t.n && t.p[q] != '"') q++;
            if (q >= t.n || q == j + 1) continue;
            path = avp_strn(t.p + j + 1, q - j - 1);
            status = st;
            end = q + 1;
        }
        if (end) {
            avp_map_put(out, path, NULL)->p = (void *)status;
            from = (i64)end;
        } else {
            from = at + 1;
        }
    }
    return out;
}

/* ---- SymbolMap --------------------------------------------------------------------------------------------------- */

static int cmp_entry(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const GcEntry *a = x, *b = y;
    int c = avp_cmp_i64(a->addr, b->addr);
    return c ? c : avp_cmp_i64(b->size, a->size);
}

GcSymbolMap *gc_symbol_map(AvpArena *a, GcEntry *entries, size_t n) {
    GcSymbolMap *m = AVP_NEW(a, GcSymbolMap);
    m->e = entries;
    m->n = n;
    avp_sort(entries, n, sizeof(GcEntry), cmp_entry, NULL, false);
    m->addrs = AVP_NEWN(a, i64, n + 1);
    for (size_t i = 0; i < n; i++) m->addrs[i] = entries[i].addr;
    return m;
}

bool gc_symbol_lookup(const GcSymbolMap *m, i64 addr, GcHit *out) {
    i64 i = (i64)avp_bisect_right_i64(m->addrs, m->n, addr) - 1;
    if (i < 0) return false;
    bool have = false;
    i64 stop = i - 24 > -1 ? i - 24 : -1;
    for (i64 j = i; j > stop; j--) {
        const GcEntry *e = &m->e[j];
        if (e->size && e->addr <= addr && addr < e->addr + e->size) {
            *out = (GcHit){e->name, e->addr, e->size, true};
            have = true;
            break;
        }
        if (!have && e->addr == addr && e->size == 0) {
            *out = (GcHit){e->name, e->addr, e->size, true};
            have = true;
        }
    }
    if (!have) *out = (GcHit){m->e[i].name, m->e[i].addr, m->e[i].size, false};
    return true;
}

bool gc_symbol_next_start(const GcSymbolMap *m, i64 addr, i64 *out) {
    size_t i = avp_bisect_right_i64(m->addrs, m->n, addr);
    if (i >= m->n) return false;
    *out = m->addrs[i];
    return true;
}

GcSymbolMap *gc_elf_symbol_map(AvpArena *a, GcElf *e) {
    gc_elf_symbols(a, e);
    GcEntry *v = AVP_NEWN(a, GcEntry, e->nsyms + 1);
    size_t n = 0;
    for (size_t k = 0; k < e->nsyms; k++) {
        const GcSym *s = &e->syms[k];
        if (s->type == GC_STT_SECTION || s->type == GC_STT_FILE || !s->name[0] || s->shndx == 0 ||
            s->shndx == GC_SHN_ABS)
            continue;
        if (s->shndx >= (i64)e->nsecs || e->secs[s->shndx].addr == 0) continue;
        v[n++] = (GcEntry){s->value, s->size, s->name};
    }
    return gc_symbol_map(a, v, n);
}

/* ---- UnitMap ----------------------------------------------------------------------------------------------------- */

GcUnitMap *gc_unit_map(AvpArena *a, const GcSplitVec *splits, AvpMap *statuses) {
    GcUnitMap *u = AVP_NEW(a, GcUnitMap);
    u->ranges = splits->v;
    u->n = splits->n;
    u->starts = AVP_NEWN(a, i64, splits->n + 1);
    for (size_t i = 0; i < splits->n; i++) u->starts[i] = splits->v[i].start;
    u->statuses = statuses;
    return u;
}

const char *gc_unit_section_name(const GcUnitMap *u, i64 addr) {
    for (size_t i = 0; i < u->nelf; i++)
        if (u->elf_sections[i].addr <= addr && addr < u->elf_sections[i].end) return u->elf_sections[i].name;
    return NULL;
}

bool gc_unit_lookup(const GcUnitMap *u, i64 addr, const char **unit, const char **section, size_t *index) {
    i64 i = (i64)avp_bisect_right_i64(u->starts, u->n, addr) - 1;
    if (i >= 0 && u->ranges[i].start <= addr && addr < u->ranges[i].end) {
        *unit = u->ranges[i].unit;
        *section = u->ranges[i].section;
        if (index) *index = (size_t)i;
        return true;
    }
    *unit = *section = NULL;
    return false;
}

const char *gc_unit_status(const GcUnitMap *u, const char *unit) {
    if (!unit) return "unsplit";
    AvpVal *v = u->statuses ? avp_map_get(u->statuses, avp_str(unit)) : NULL;
    return v ? v->p : "asm";
}
