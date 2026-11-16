/* sibling: the link-map parser and the text transforms that turn `dtk elf config` output into a config dol split
 * accepts. */
#include "sibling.h"

#include "avp/fmt.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ---- the link map ---- */

static AvpStr next_word(AvpStr *s) {
    size_t i = 0;
    while (i < s->n && (s->p[i] == ' ' || s->p[i] == '\t')) i++;
    size_t j = i;
    while (j < s->n && s->p[j] != ' ' && s->p[j] != '\t') j++;
    AvpStr w = avp_strn(s->p + i, j - i);
    *s = avp_strn(s->p + j, s->n - j);
    return w;
}

static bool is_hex_word(AvpStr w, size_t len) {
    if (w.n != len) return false;
    for (size_t i = 0; i < w.n; i++)
        if (!isxdigit((unsigned char)w.p[i])) return false;
    return true;
}

static bool is_dec_word(AvpStr w) {
    if (!w.n) return false;
    for (size_t i = 0; i < w.n; i++)
        if (!isdigit((unsigned char)w.p[i])) return false;
    return true;
}

static i64 hex_of(AvpStr w) {
    char buf[32];
    size_t n = w.n < sizeof buf - 1 ? w.n : sizeof buf - 1;
    memcpy(buf, w.p, n);
    buf[n] = '\0';
    return (i64)strtoull(buf, NULL, 16);
}

void sibling_split_owner(AvpArena *a, AvpStr tail, const char **lib, const char **object) {
    tail = avp_str_strip(tail);
    AvpStr rest = tail;
    AvpStr first = next_word(&rest);
    rest = avp_str_strip(rest);
    if (rest.n && first.n > 2 && avp_str_endswith(first, AVP_S(".a"))) {
        *lib = avp_cz(a, first);
        *object = avp_cz(a, rest);
    } else {
        *lib = "";
        *object = avp_cz(a, tail);
    }
}

static i64 next_order(AvpMap *orders, AvpArena *a, const char *lib, const char *object) {
    bool created;
    AvpVal *v = avp_map_put(orders, avp_str(avp_sprintf(a, "%s|%s", lib, object)), &created);
    if (created) v->i = 0;
    return v->i++;
}

SibMap *sibling_parse_map(AvpArena *a, AvpStr text) {
    SibMap *m = avp_alloc(a, sizeof *m);
    AvpMap *orders = avp_map_new(), *uorders = avp_map_new();
    bool in_text = false;
    size_t pos = 0;
    while (pos < text.n) {
        size_t e = pos;
        while (e < text.n && text.p[e] != '\n') e++;
        AvpStr line = avp_strn(text.p + pos, e - pos);
        pos = e + 1;
        if (line.n && line.p[line.n - 1] == '\r') line.n--;
        if (avp_str_endswith(line, AVP_S("section layout"))) {
            in_text = avp_str_eqz(avp_str_strip(line), ".text section layout");
            continue;
        }
        if (!in_text) continue;
        AvpStr rest = line;
        AvpStr w0 = next_word(&rest);
        if (avp_str_eqz(w0, "UNUSED")) {
            AvpStr size = next_word(&rest);
            next_word(&rest);
            next_word(&rest);
            AvpStr name = next_word(&rest);
            if (!name.n || name.p[0] == '.') continue;
            SibMapSym sym = {0};
            sym.size = hex_of(size);
            sym.name = avp_cz(a, name);
            sibling_split_owner(a, rest, &sym.lib, &sym.object);
            sym.order = next_order(uorders, a, sym.lib, sym.object);
            AvpVal *used = avp_map_get(orders, avp_str(avp_sprintf(a, "%s|%s", sym.lib, sym.object)));
            sym.before = used ? used->i : 0; /* the map lists UNUSED members in place */
            avp_vec_push(&m->unused, sym);
            continue;
        }
        if (!is_hex_word(w0, 8)) continue; /* headers, blank lines */
        AvpStr size = next_word(&rest), vaddr = next_word(&rest), foff = next_word(&rest);
        if (!is_hex_word(size, 6) && !is_hex_word(size, 8)) continue;
        if (!is_hex_word(vaddr, 8) || !is_hex_word(foff, 8)) continue;
        i64 tab = avp_str_find(rest, AVP_S("\t"), 0, AVP_END);
        if (tab < 0) continue; /* *fill* */
        AvpStr head = avp_strn(rest.p, (size_t)tab), tail = avp_strn(rest.p + tab + 1, rest.n - (size_t)tab - 1);
        AvpStr hrest = head;
        AvpStr w = next_word(&hrest);
        if (is_dec_word(w) && avp_str_strip(hrest).n) head = hrest; /* the alignment column */
        head = avp_str_strip(head);
        if (!head.n || head.p[0] == '.' || avp_str_find(head, AVP_S(" (entry of "), 0, AVP_END) >= 0) continue;
        if (avp_str_eqz(head, "*fill*")) continue;
        SibMapSym sym = {0};
        sym.address = hex_of(vaddr);
        sym.size = hex_of(size);
        if (!sym.size) continue;
        sym.name = avp_cz(a, head);
        sibling_split_owner(a, tail, &sym.lib, &sym.object);
        sym.order = next_order(orders, a, sym.lib, sym.object);
        avp_vec_push(&m->text, sym);
    }
    avp_map_free(orders);
    avp_map_free(uorders);
    return m;
}

/* ---- dtk config transforms ---- */

static AvpStr line_at(AvpStr text, size_t *pos) {
    size_t e = *pos;
    while (e < text.n && text.p[e] != '\n') e++;
    AvpStr line = avp_strn(text.p + *pos, e - *pos);
    *pos = e + 1;
    if (line.n && line.p[line.n - 1] == '\r') line.n--;
    return line;
}

static char *unit_name_fix(AvpArena *a, AvpStr name) {
    char *s = avp_cz(a, name);
    size_t n = strlen(s);
    if (n >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':') s[1] = '_';
    for (size_t i = 0; i < n; i++)
        if (s[i] == '\\' || s[i] == ':') s[i] = s[i] == '\\' ? '/' : '_';
    return s;
}

/* dtk's object path for a unit: the name without its last extension, case-folded (Windows). */
static char *obj_key(AvpArena *a, const char *name, const char **ext) {
    const char *slash = strrchr(name, '/');
    const char *dot = strrchr(slash ? slash : name, '.');
    size_t n = dot ? (size_t)(dot - name) : strlen(name);
    *ext = dot ? dot : "";
    char *k = avp_cz(a, avp_strn(name, n));
    for (char *p = k; *p; p++) *p = (char)tolower((unsigned char)*p);
    return k;
}

AvpStr sibling_fix_splits(AvpArena *a, AvpStr text, u64 text_end) {
    AvpBuf b;
    avp_buf_init(&b, a);
    AvpSet *seen = avp_set_new();
    bool in_sections = false;
    size_t pos = 0;
    while (pos < text.n) {
        AvpStr line = line_at(text, &pos);
        if (avp_str_eqz(line, "Sections:")) {
            in_sections = true;
            avp_buf_printf(&b, "Sections:\n");
            continue;
        }
        if (!line.n) {
            in_sections = false;
            avp_buf_printf(&b, "\n");
            continue;
        }
        if (line.p[0] == '\t' || line.p[0] == ' ') {
            if (in_sections) {
                avp_buf_put(&b, line);
                avp_buf_printf(&b, "\n");
                continue;
            }
            AvpStr rest = line;
            AvpStr sec = next_word(&rest);
            if (!avp_str_eqz(sec, ".text")) continue;
            const char *p = avp_cz(a, rest);
            const char *ps = strstr(p, "start:0x"), *pe = strstr(p, "end:0x");
            if (!ps || !pe) continue;
            u64 start = strtoull(ps + 8, NULL, 16), end = strtoull(pe + 6, NULL, 16);
            if (text_end && end > text_end) end = text_end;
            if (start >= end) continue;
            avp_buf_printf(&b, "\t.text       start:0x%08llX end:0x%08llX\n", (unsigned long long)start,
                           (unsigned long long)end);
            continue;
        }
        in_sections = false;
        if (line.p[line.n - 1] != ':') {
            avp_buf_put(&b, line);
            avp_buf_printf(&b, "\n");
            continue;
        }
        char *name = unit_name_fix(a, avp_strn(line.p, line.n - 1));
        const char *ext;
        char *key = obj_key(a, name, &ext);
        if (avp_set_has(seen, avp_str(key))) {
            char *stem = avp_cz(a, avp_strn(name, strlen(name) - strlen(ext)));
            for (int k = 1;; k++) {
                char *cand = avp_sprintf(a, "%s_%d%s", stem, k, ext);
                const char *e2;
                char *k2 = obj_key(a, cand, &e2);
                if (!avp_set_has(seen, avp_str(k2))) {
                    name = cand;
                    key = k2;
                    break;
                }
            }
        }
        avp_set_add(seen, avp_str(key));
        avp_buf_printf(&b, "%s:\n", name);
    }
    avp_set_free(seen);
    return avp_buf_str(&b);
}

AvpStr sibling_fix_symbols(AvpArena *a, AvpStr text) {
    AvpBuf b;
    avp_buf_init(&b, a);
    size_t pos = 0;
    while (pos < text.n) {
        AvpStr line = line_at(text, &pos);
        if (!line.n) continue;
        bool code = avp_str_find(line, AVP_S("= .text:"), 0, AVP_END) >= 0 ||
                    avp_str_find(line, AVP_S("= .init:"), 0, AVP_END) >= 0;
        if (code) {
            if (avp_str_find(line, AVP_S("type:function"), 0, AVP_END) < 0) continue;
            avp_buf_put(&b, line);
            avp_buf_printf(&b, "\n");
            continue;
        }
        if (avp_str_find(line, AVP_S("type:label"), 0, AVP_END) >= 0) continue;
        i64 eq = avp_str_find(line, AVP_S(" = "), 0, AVP_END);
        i64 ad = avp_str_find(line, AVP_S(":0x"), eq < 0 ? 0 : eq, AVP_END);
        if (eq < 0 || ad < 0) continue;
        if (avp_str_find(line, AVP_S("scope:local"), 0, AVP_END) >= 0) {
            size_t k = (size_t)ad + 3, e = k;
            while (e < line.n && isxdigit((unsigned char)line.p[e])) e++;
            avp_buf_put(&b, avp_strn(line.p, (size_t)eq));
            avp_buf_printf(&b, "_a");
            avp_buf_put(&b, avp_strn(line.p + k, e - k));
            avp_buf_put(&b, avp_strn(line.p + eq, line.n - (size_t)eq));
        } else {
            avp_buf_put(&b, line);
        }
        avp_buf_printf(&b, "\n");
    }
    return avp_buf_str(&b);
}

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

u64 sibling_elf_text_end(AvpStr elf) {
    const u8 *d = (const u8 *)elf.p;
    if (elf.n < 0x34 || memcmp(d, "\x7f" "ELF", 4) != 0 || d[4] != 1 || d[5] != 2) return 0;
    u32 shoff = be32(d + 0x20);
    u16 shentsize = be16(d + 0x2E), shnum = be16(d + 0x30), shstrndx = be16(d + 0x32);
    if (shstrndx >= shnum || (u64)shoff + (u64)shnum * shentsize > elf.n || shentsize < 0x28) return 0;
    const u8 *strsh = d + shoff + (size_t)shstrndx * shentsize;
    u32 stroff = be32(strsh + 0x10), strsize = be32(strsh + 0x14);
    if ((u64)stroff + strsize > elf.n) return 0;
    for (u16 i = 0; i < shnum; i++) {
        const u8 *sh = d + shoff + (size_t)i * shentsize;
        u32 nm = be32(sh);
        if (nm >= strsize) continue;
        const char *name = (const char *)d + stroff + nm;
        if (strncmp(name, ".text", strsize - nm) == 0 && strlen(name) == 5) return (u64)be32(sh + 0xC) + be32(sh + 0x14);
    }
    return 0;
}

/* ---- names ---- */

bool sibling_is_placeholder(const char *name) {
    static const char *const prefixes[] = {"fn_", "lbl_", "unk_", "sinit_", "dtor_", "jumptable_", "gap_", "@", NULL};
    if (!name || !*name) return true;
    for (int i = 0; prefixes[i]; i++)
        if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0) return true;
    return strstr(name, "unk_") != NULL;
}

/* "__sinit_<unit>_cpp" and dtk's "sinit_<address>_<unit>": the compiler's static initialisers. Their bodies differ only
 * in the data they touch, which the fingerprints do not see, so they are never paired. */
bool sibling_is_initialiser(const char *name) {
    if (!name) return false;
    return strncmp(name, "__sinit", 7) == 0 || strncmp(name, "sinit_", 6) == 0;
}

/* ---- data symbols of the link map ---- */

bool sibling_map_find(AvpArena *a, AvpStr map, const char *name, i64 address, SibMapData *out) {
    const char *section = "";
    AvpStr rest = map;
    while (rest.n) {
        i64 nl = avp_str_findc(rest, '\n');
        AvpStr line = nl < 0 ? rest : avp_strn(rest.p, (size_t)nl);
        rest = nl < 0 ? avp_strn(rest.p + rest.n, 0) : avp_strn(rest.p + nl + 1, rest.n - (size_t)nl - 1);
        line = avp_str_rstrip(line);
        if (avp_str_endswith(line, AVP_S(" section layout"))) {
            section = avp_cz(a, avp_strn(line.p, line.n - 15));
            continue;
        }
        if (!*section || section[0] != '.' || strncmp(section, ".debug", 6) == 0 || strcmp(section, ".text") == 0 ||
            strcmp(section, ".init") == 0)
            continue;
        AvpStr w = line, off = next_word(&w), size = next_word(&w), addr = next_word(&w), file = next_word(&w);
        AvpStr align = next_word(&w), sym = next_word(&w);
        if (!is_hex_word(off, 8) || !is_hex_word(size, 6) || !is_hex_word(addr, 8) || !is_hex_word(file, 8) ||
            !is_dec_word(align) || !sym.n || sym.p[0] == '.')
            continue;
        i64 at = hex_of(addr), sz = hex_of(size);
        bool hit = name ? avp_str_eqz(sym, name) : (address >= at && address < at + (sz ? sz : 1));
        if (!hit) continue;
        out->section = section;
        out->name = avp_cz(a, sym);
        out->address = at;
        out->size = sz;
        out->align = (i64)strtoll(avp_cz(a, align), NULL, 10);
        sibling_split_owner(a, w, &out->lib, &out->object);
        return true;
    }
    return false;
}
