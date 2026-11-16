/* ledger: hand scanners for the regular expressions of tools/ledger.py (no regex engine yet; DEVIATIONS.md,
 * "Consolidation", lists each one for the move to shsm_re). Each function names the pattern it implements and
 * follows its backtracking order, so the first match is the one Python's re finds. */
#include "ledger.h"
#include "shsm_text.h"
#include <string.h>

static bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_ascii_alnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool at(AvpStr s, size_t i, const char *lit) {
    size_t n = strlen(lit);
    return i <= s.n && s.n - i >= n && memcmp(s.p + i, lit, n) == 0;
}
static size_t cp_len(AvpStr s, size_t i) {
    const char *p = s.p + i;
    avp_u8_next(&p, s.p + s.n);
    return (size_t)(p - (s.p + i));
}

/* `\.(c|cc|cp|cpp|h|hpp|s)$` with re.I at the end of s (the caller handles `$` before a final newline). An
 * assembly source unit (`.s`, user's ruling 2026-11-14) is keyed without its extension like a C or C++ one. */
static bool source_ext(AvpStr s) {
    static const char *const exts[] = {".c", ".cc", ".cp", ".cpp", ".h", ".hpp", ".s"};
    for (size_t k = 0; k < sizeof exts / sizeof exts[0]; k++) {
        size_t n = strlen(exts[k]);
        if (s.n < n) continue;
        bool ok = true;
        for (size_t i = 0; i < n && ok; i++) {
            char c = s.p[s.n - n + i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            ok = c == exts[k][i];
        }
        if (ok) return true;
    }
    return false;
}

bool ledger_is_asm_source(AvpStr path) {
    return path.n >= 2 && path.p[path.n - 2] == '.' && (path.p[path.n - 1] == 's' || path.p[path.n - 1] == 'S');
}

AvpStr ledger_unit_key(AvpArena *a, AvpStr path) {
    AvpStr p = avp_str_replace(a, path, AVP_S("\\"), AVP_S("/"), -1);
    if (avp_str_startswith(p, AVP_S("src/"))) p = avp_str_slice(p, 4, (i64)p.n);
    if (avp_str_startswith(p, AVP_S("main/"))) p = avp_str_slice(p, 5, (i64)p.n);
    bool ext = source_ext(p);
    if (!ext && p.n && p.p[p.n - 1] == '\n') ext = source_ext(avp_strn(p.p, p.n - 1)); /* `$` before a final \n */
    if (!ext) return p;
    AvpStr root, e;
    char *cz = avp_cz(a, p);
    avp_path_splitext(cz, &root, &e);
    return root;
}

/* `Object\(\s*(Matching|NonMatching|Equivalent)\s*,\s*"([^"]+)"` over the lines that are not comments. */
LedgerCfgVec ledger_parse_configure(AvpArena *a, AvpStr text) {
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    AvpStrVec keep = {0};
    for (size_t i = 0; i < lines.n; i++)
        if (!avp_str_startswith(avp_str_lstrip(lines.v[i]), AVP_S("#"))) avp_vec_push(&keep, lines.v[i]);
    AvpStr body = avp_str_join(a, AVP_S("\n"), keep.v, keep.n);
    avp_vec_free(&keep);
    avp_vec_free(&lines);
    LedgerCfgVec out = {0};
    static const char *const words[] = {"Matching", "NonMatching", "Equivalent"};
    i64 from = 0;
    for (;;) {
        i64 k = avp_str_find(body, AVP_S("Object("), from, (i64)body.n);
        if (k < 0) break;
        from = k + 1;
        size_t i = shsm_skip_ws(body, (size_t)k + 7);
        const char *word = NULL;
        for (size_t w = 0; w < 3 && !word; w++)
            if (at(body, i, words[w])) word = words[w];
        if (!word) continue;
        i = shsm_skip_ws(body, i + strlen(word));
        if (!at(body, i, ",")) continue;
        i = shsm_skip_ws(body, i + 1);
        if (!at(body, i, "\"")) continue;
        size_t s = i + 1, e = s;
        while (e < body.n && body.p[e] != '"') e++;
        if (e >= body.n || e == s) continue;
        LedgerCfg c;
        c.status = avp_str(word);
        c.path = avp_strn(body.p + s, e - s);
        c.unit = ledger_unit_key(a, c.path);
        avp_vec_push(&out, c);
        from = (i64)e + 1;
    }
    return out;
}

/* `^(\S+) = (\S+?):0x([0-9A-Fa-f]+);(?: //(.*))?$` on ln.strip(), then `size:0x([0-9A-Fa-f]+)` in the comment. */
static bool parse_symbol_line(AvpStr s, AvpStr *name, AvpStr *section, u64 *addr, AvpStr *attrs) {
    size_t i = 0;
    while (i < s.n && !shsm_ws_len(s, i)) i += cp_len(s, i);
    if (i == 0 || !at(s, i, " = ")) return false;
    *name = avp_strn(s.p, i);
    size_t j0 = i + 3, k = j0;
    while (k < s.n && !shsm_ws_len(s, k)) {
        k += cp_len(s, k);
        if (!at(s, k, ":0x")) continue;
        size_t h = k + 3;
        while (h < s.n && is_hex(s.p[h])) h++;
        if (h == k + 3 || !at(s, h, ";")) continue;
        size_t r = h + 1;
        AvpStr com = AVP_S("");
        if (r != s.n) {
            if (!at(s, r, " //")) continue;
            com = avp_strn(s.p + r + 3, s.n - r - 3);
            if (avp_str_findc(com, '\n') >= 0) continue;
        }
        *section = avp_strn(s.p + j0, k - j0);
        avp_parse_uint(avp_strn(s.p + k + 3, h - k - 3), 16, addr);
        *attrs = com;
        return true;
    }
    return false;
}

AvpMap *ledger_parse_symbols(AvpArena *a, AvpStr text) {
    AvpMap *syms = avp_map_new();
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr name, section, attrs;
        u64 addr = 0;
        if (!parse_symbol_line(avp_str_strip(lines.v[li]), &name, &section, &addr, &attrs)) continue;
        LedgerSym sym = {0};
        sym.section = section;
        sym.addr = (i64)addr;
        i64 from = 0;
        for (;;) {
            i64 k = avp_str_find(attrs, AVP_S("size:0x"), from, (i64)attrs.n);
            if (k < 0) break;
            size_t h = (size_t)k + 7;
            while (h < attrs.n && is_hex(attrs.p[h])) h++;
            if (h > (size_t)k + 7) {
                u64 v = 0;
                avp_parse_uint(avp_strn(attrs.p + k + 7, h - (size_t)k - 7), 16, &v);
                sym.has_size = true;
                sym.size = (i64)v;
                break;
            }
            from = k + 1;
        }
        sym.attrs = avp_str_strip(attrs);
        bool created = false;
        AvpVal *v = avp_map_put(syms, name, &created);
        if (created) v->p = AVP_NEW(a, LedgerSymVec);
        avp_vec_push((LedgerSymVec *)v->p, sym);
    }
    avp_vec_free(&lines);
    return syms;
}

AvpStrVec ledger_table_cells(AvpArena *a, AvpStr line) {
    (void)a;
    AvpStr s = avp_str_strip_chars(avp_str_strip(line), AVP_S("|"));
    AvpStrVec v = avp_str_split(s, AVP_S("|"), -1);
    for (size_t i = 0; i < v.n; i++) v.v[i] = avp_str_strip(v.v[i]);
    return v;
}

AvpStrVec ledger_ticks(AvpStr cell) {
    AvpStrVec out = {0};
    size_t i = 0;
    while (i < cell.n) {
        if (cell.p[i] != '`') {
            i++;
            continue;
        }
        size_t j = i + 1;
        while (j < cell.n && cell.p[j] != '`') j++;
        if (j >= cell.n) break;          /* no closing backtick: no later match either */
        if (j == i + 1) {                /* "``": the regex fails here and retries at the second backtick */
            i++;
            continue;
        }
        avp_vec_push(&out, avp_strn(cell.p + i + 1, j - i - 1));
        i = j + 1;
    }
    return out;
}

static bool hex_run(AvpStr s, size_t i, size_t n) {
    if (s.n < i + n) return false;
    for (size_t k = 0; k < n; k++)
        if (!is_hex(s.p[i + k])) return false;
    return true;
}

/* PLACEHOLDER_RE: (?<![A-Za-z0-9])(?:fn|lbl)_[0-9A-Fa-f]{8}(?![0-9A-Fa-f]) | unk_[0-9A-Fa-f]+ | unk_[A-Z]
 * | field_0x[0-9A-Fa-f]+ | (?:dtor|sinit|jumptable|gap)_[0-9A-Fa-f]{8} */
bool ledger_is_placeholder(AvpStr s) {
    static const char *const eight[] = {"dtor_", "sinit_", "jumptable_", "gap_"};
    for (size_t i = 0; i < s.n; i++) {
        if (i == 0 || !is_ascii_alnum(s.p[i - 1])) {
            size_t k = at(s, i, "fn_") ? i + 3 : at(s, i, "lbl_") ? i + 4 : 0;
            if (k && hex_run(s, k, 8) && !(k + 8 < s.n && is_hex(s.p[k + 8]))) return true;
        }
        if (at(s, i, "unk_") && i + 4 < s.n) {
            char c = s.p[i + 4];
            if (is_hex(c) || (c >= 'A' && c <= 'Z')) return true;
        }
        if (at(s, i, "field_0x") && i + 8 < s.n && is_hex(s.p[i + 8])) return true;
        for (size_t w = 0; w < 4; w++)
            if (at(s, i, eight[w]) && hex_run(s, i + strlen(eight[w]), 8)) return true;
    }
    return false;
}

/* THUNK_RE `^@\d+@(.*)$` */
bool ledger_thunk_target(AvpStr s, AvpStr *target) {
    if (!s.n || s.p[0] != '@') return false;
    size_t i = 1;
    while (i < s.n && is_digit(s.p[i])) i++;
    if (i == 1 || !at(s, i, "@")) return false;
    AvpStr rest = avp_strn(s.p + i + 1, s.n - i - 1);
    i64 nl = avp_str_findc(rest, '\n');
    if (nl >= 0) {
        if ((size_t)nl != rest.n - 1) return false; /* `.` stops at a newline; `$` allows only a final one */
        rest.n--;
    }
    *target = rest;
    return true;
}

/* One code point of the text against one ASCII letter of the pattern under re.IGNORECASE (Unicode): the letter, its
 * upper case, and for 'i' the dotless i U+0131 and dotted capital I U+0130 (sre's case fixes). */
static size_t ci_char(AvpStr s, size_t i, char pc) {
    if (i >= s.n) return 0;
    char c = s.p[i];
    if (c == pc || (pc >= 'a' && pc <= 'z' && c == pc - 'a' + 'A')) return 1;
    if (pc == 'i' && i + 1 < s.n && (u8)c == 0xC4 && ((u8)s.p[i + 1] == 0xB0 || (u8)s.p[i + 1] == 0xB1)) return 2;
    return 0;
}
static size_t ci_at(AvpStr s, size_t i, const char *lit) {
    size_t k = i;
    for (const char *p = lit; *p; p++) {
        size_t n = ci_char(s, k, *p);
        if (!n) return 0;
        k += n;
    }
    return k;
}

/* UNCONFIRMED_RE `unit name[^\n]*unconfirmed` with re.I */
bool ledger_unconfirmed_text(AvpStr s) {
    for (size_t i = 0; i < s.n; i++) {
        size_t e = ci_at(s, i, "unit name");
        if (!e) continue;
        for (size_t k = e; k < s.n && s.p[k] != '\n'; k++)
            if (ci_at(s, k, "unconfirmed")) return true;
    }
    return false;
}

/* `^(.*?)(?: \((.*)\))?$` on cell.strip() */
void ledger_parse_status_cell(AvpStr cell, AvpStr *base, AvpStr *note) {
    AvpStr s = avp_str_strip(cell);
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] == '\n') break;
        if (at(s, i, " (") && s.n - i >= 3 && s.p[s.n - 1] == ')') {
            AvpStr inner = avp_strn(s.p + i + 2, s.n - i - 3);
            if (avp_str_findc(inner, '\n') >= 0) continue;
            *base = avp_strn(s.p, i);
            *note = inner;
            return;
        }
    }
    if (avp_str_findc(s, '\n') >= 0) { /* not reached for table cells (lines have no newline) */
        *base = cell;
        *note = (AvpStr){NULL, 0};
        return;
    }
    *base = s;
    *note = (AvpStr){NULL, 0};
}

/* ROW_RE `^\| (.+?) \| `(.+?)` \| (0x[0-9A-Fa-f]+) \| (\d+) \| (.*?) \| (.*?) \|\s*$` (lines carry no newline) */
bool ledger_match_row(AvpStr s, AvpStr g[6]) {
    if (!at(s, 0, "| ")) return false;
    if (avp_str_findc(s, '\n') >= 0) return false;
    size_t p1 = 2;
    for (size_t e1 = p1 + 1; e1 <= s.n; e1++) {
        if (!at(s, e1, " | `")) continue;
        size_t p2 = e1 + 4;
        for (size_t e2 = p2 + 1; e2 <= s.n; e2++) {
            if (!at(s, e2, "` | ")) continue;
            size_t p3 = e2 + 4;
            if (!at(s, p3, "0x")) continue;
            size_t h = p3 + 2;
            while (h < s.n && is_hex(s.p[h])) h++;
            if (h == p3 + 2 || !at(s, h, " | ")) continue;
            size_t p4 = h + 3, d = p4;
            while (d < s.n && is_digit(s.p[d])) d++;
            if (d == p4 || !at(s, d, " | ")) continue;
            size_t p5 = d + 3;
            for (size_t e5 = p5; e5 <= s.n; e5++) {
                if (!at(s, e5, " | ")) continue;
                size_t p6 = e5 + 3;
                for (size_t e6 = p6; e6 <= s.n; e6++) {
                    if (!at(s, e6, " |")) continue;
                    if (shsm_skip_ws(s, e6 + 2) != s.n) continue;
                    g[0] = avp_strn(s.p + p1, e1 - p1);
                    g[1] = avp_strn(s.p + p2, e2 - p2);
                    g[2] = avp_strn(s.p + p3, h - p3);
                    g[3] = avp_strn(s.p + p4, d - p4);
                    g[4] = avp_strn(s.p + p5, e5 - p5);
                    g[5] = avp_strn(s.p + p6, e6 - p6);
                    return true;
                }
            }
        }
    }
    return false;
}

bool ledger_count_note(AvpStr s, AvpStr *nfn, AvpStr *nbytes) {
    for (size_t i = 0; i < s.n; i++) {
        if (s.p[i] != '(') continue;
        size_t a = i + 1, b = a;
        while (b < s.n && is_digit(s.p[b])) b++;
        if (b == a || !at(s, b, " function")) continue;
        size_t c = b + 9;
        if (at(s, c, "s")) c++;
        if (!at(s, c, ", ")) {
            /* `s?` is greedy: when the s was taken and ", " does not follow, the regex retries without it */
            if (!(c == b + 10 && at(s, b + 9, ", "))) continue;
            c = b + 9;
        }
        size_t d = c + 2, e = d;
        while (e < s.n && is_digit(s.p[e])) e++;
        if (e == d || !at(s, e, " bytes)")) continue;
        *nfn = avp_strn(s.p + a, b - a);
        *nbytes = avp_strn(s.p + d, e - d);
        return true;
    }
    return false;
}

AvpStrVec ledger_rule_ids(AvpStr s) {
    AvpStrVec out = {0};
    size_t i = 0;
    while (i < s.n) {
        if (s.p[i] == 'R' && i + 1 < s.n && is_digit(s.p[i + 1])) {
            size_t j = i + 1;
            while (j < s.n && is_digit(s.p[j])) j++;
            avp_vec_push(&out, avp_strn(s.p + i, j - i));
            i = j;
        } else i++;
    }
    return out;
}

bool ledger_find_gross(AvpStr s, size_t *start, size_t *end) {
    for (size_t i = 0; i < s.n; i++) {
        if (!is_digit(s.p[i])) continue;
        size_t j = i;
        while (j < s.n && is_digit(s.p[j])) j++;
        if (!at(s, j, " functions and ")) continue;
        size_t k = j + 15, l = k;
        while (l < s.n && is_digit(s.p[l])) l++;
        if (l == k || !at(s, l, " code bytes") || !at(s, l + 11, " in all")) continue;
        *start = i;
        *end = l + 11;
        return true;
    }
    return false;
}

/* \w of a str pattern: str.isalnum() or '_' (one code point). */
static size_t word_char(AvpStr s, size_t i) {
    u8 c = (u8)s.p[i];
    if (c < 0x80) return (is_ascii_alnum((char)c) || c == '_') ? 1 : 0;
    size_t n = cp_len(s, i);
    return avp_str_isalnum(avp_strn(s.p + i, n)) ? n : 0;
}

void ledger_source_words(AvpStr s, AvpSet *out) {
    size_t i = 0;
    while (i < s.n) {
        char c = s.p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '@' || c == '\\') {
            size_t j = i + 1;
            for (;;) {
                if (j >= s.n) break;
                if (s.p[j] == '\\' || s.p[j] == '@') {
                    j++;
                    continue;
                }
                size_t n = word_char(s, j);
                if (!n) break;
                j += n;
            }
            avp_set_add(out, avp_strn(s.p + i, j - i));
            i = j;
        } else i += ((u8)c < 0x80) ? 1 : cp_len(s, i);
    }
}
