/* unit: text files, symbols.txt, splits.txt, configure.py, config.yml, the DOL's .ctors table and the project
 * (tools/unit.py lines 116-360 and 813-903). */
#include "unit.h"
#include "unit_infer.h"

#include "shsm_re.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- errors and output ---- */

static AvpBuf g_err;
static AvpBuf *g_capture;

void unit_fail(const char *fmt, ...) {
    avp_buf_clear(&g_err);
    va_list ap;
    va_start(ap, fmt);
    avp_buf_vprintf(&g_err, fmt, ap);
    va_end(ap);
    avp_throw(UNIT_TOOL_ERROR, "%.1000s", avp_buf_cz(&g_err));
}

const char *unit_error_message(void) { return avp_buf_cz(&g_err); }

void unit_capture(AvpBuf *b) { g_capture = b; }

void unit_out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (g_capture) {
        avp_buf_vprintf(g_capture, fmt, ap);
        avp_buf_putc(g_capture, '\n');
    } else {
        vfprintf(stdout, fmt, ap);
        fputc('\n', stdout);
    }
    va_end(ap);
}

void unit_out_str(AvpStr s) {
    if (g_capture) {
        avp_buf_put(g_capture, s);
        avp_buf_putc(g_capture, '\n');
    } else {
        fwrite(s.p, 1, s.n, stdout);
        fputc('\n', stdout);
    }
}

const char *unit_repr(AvpArena *a, const char *s) { return avp_cz(a, avp_repr_str(a, avp_str(s))); }

/* ---- text files ---- */

AvpStr unit_read_text(AvpArena *a, const char *path, const char **eol) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) shsm_raise_io();
    AvpStr raw = avp_strn((const char *)b.v, b.n);
    bool crlf = avp_str_contains(raw, AVP_S("\r\n"));
    if (eol) *eol = crlf ? "\r\n" : "\n";
    return crlf ? avp_str_replace(a, raw, AVP_S("\r\n"), AVP_S("\n"), -1) : avp_str_dup(a, raw);
}

void unit_write_text(AvpArena *a, const char *path, AvpStr text, const char *eol) {
    AvpStr data = avp_str_replace(a, text, AVP_S("\r\n"), AVP_S("\n"), -1);
    if (!strcmp(eol, "\r\n")) data = avp_str_replace(a, data, AVP_S("\n"), AVP_S("\r\n"), -1);
    const char *dir = avp_cz(a, avp_path_dirname(path));
    if (*dir && !avp_makedirs(dir, true)) shsm_raise_io();
    if (!avp_write_file(path, data.p, data.n)) shsm_raise_io();
}

static int cmp_lower(const void *x, const void *y, void *ctx) {
    AvpArena *a = ctx;
    AvpStr l = avp_str_lower(a, *(const AvpStr *)x), r = avp_str_lower(a, *(const AvpStr *)y);
    return avp_str_cmp(l, r);
}

const char *unit_guess_eol(AvpArena *a, const char *dir) {
    AvpStrVec names = {0};
    if (!avp_isdir(dir) || !avp_listdir(a, dir, &names)) return "\n";
    /* sorted(Path.iterdir()): WindowsPath compares case-insensitively */
    avp_sort(names.v, names.n, sizeof(AvpStr), cmp_lower, a, false);
    for (size_t i = 0; i < names.n; i++) {
        const char *n = avp_cz(a, names.v[i]);
        AvpStr root, ext;
        avp_path_splitext(n, &root, &ext);
        if (avp_str_eqz(ext, ".cpp") || avp_str_eqz(ext, ".c") || avp_str_eqz(ext, ".h") || avp_str_eqz(ext, ".cp")) {
            AvpBytes b = {0};
            if (!avp_read_all(a, avp_join(a, dir, n), &b)) shsm_raise_io();
            return avp_str_contains(avp_strn((const char *)b.v, b.n), AVP_S("\r\n")) ? "\r\n" : "\n";
        }
    }
    return "\n";
}

/* ---- symbols.txt ---- */

static i64 hex_of(AvpStr s) {
    u64 v = 0;
    if (!avp_parse_uint(s, 16, &v)) return 0;
    return (i64)v;
}

UnitSymVec unit_load_symbols(AvpArena *a, const char *path) {
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &text)) shsm_raise_io();
    UnitSymVec syms = {0};
    ShsmRe *sym_re = shsm_re("^(\\S+) = (\\S+):0x([0-9A-Fa-f]+);(.*)$", 0);
    ShsmRe *sz_re = shsm_re("size:0x([0-9A-Fa-f]+)", 0);
    ShsmRe *ty_re = shsm_re("type:(\\w+)", 0);
    ShsmRe *sc_re = shsm_re("scope:(\\w+)", 0);
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    ShsmReMatch m, mm;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = avp_str_rstrip_chars(lines.v[i], AVP_S("\r"));
        if (!shsm_re_match(sym_re, line, 0, AVP_END, &m)) continue;
        AvpStr rest = shsm_re_group(&m, 4);
        UnitSym s = {0};
        s.name = avp_cz(a, shsm_re_group(&m, 1));
        s.sec = avp_cz(a, shsm_re_group(&m, 2));
        s.addr = hex_of(shsm_re_group(&m, 3));
        s.rest = avp_cz(a, rest);
        s.size = shsm_re_search(sz_re, rest, 0, AVP_END, &mm) ? hex_of(shsm_re_group(&mm, 1)) : 0;
        s.kind = shsm_re_search(ty_re, rest, 0, AVP_END, &mm) ? avp_cz(a, shsm_re_group(&mm, 1)) : "";
        s.scope = shsm_re_search(sc_re, rest, 0, AVP_END, &mm) ? avp_cz(a, shsm_re_group(&mm, 1)) : "global";
        avp_vec_push(&syms, s);
    }
    avp_vec_free(&lines);
    return syms;
}

/* ---- splits.txt ---- */

const UnitRange *unit_ranges_get(const UnitRanges *r, const char *sec) {
    for (size_t i = 0; i < r->n; i++)
        if (!strcmp(r->v[i].sec, sec)) return &r->v[i];
    return NULL;
}

void unit_ranges_set(UnitRanges *r, const char *sec, i64 lo, i64 hi) {
    for (size_t i = 0; i < r->n; i++)
        if (!strcmp(r->v[i].sec, sec)) {
            r->v[i].lo = lo;
            r->v[i].hi = hi;
            return;
        }
    UnitRange x = {sec, lo, hi};
    avp_vec_push(r, x);
}

bool unit_overlaps(i64 a0, i64 a1, i64 b0, i64 b1) { return a0 < b1 && b0 < a1; }

static bool py_isspace_first(AvpStr line) {
    if (!line.n) return false;
    unsigned char c = (unsigned char)line.p[0];
    if (c < 0x80) return c == ' ' || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F);
    /* a non-ASCII first character: Python's str.isspace() on it */
    size_t k = 1;
    while (k < line.n && ((unsigned char)line.p[k] & 0xC0) == 0x80) k++;
    return avp_str_isspace(avp_strn(line.p, k));
}

UnitSplits unit_load_splits(AvpArena *a, AvpStr text) {
    AvpStrVec lines = avp_str_split(text, AVP_S("\n"), -1);
    size_t i = 0;
    while (i < lines.n && avp_str_strip(lines.v[i]).n != 0) i++;
    UnitSplits splits = {0};
    ShsmRe *head = shsm_re("^(.+?):\\s*(.*)$", 0);
    ShsmRe *sec_re = shsm_re("^\\s+(\\.\\w+)\\s+(.*)$", 0);
    ShsmRe *rng = shsm_re("start:0x([0-9A-Fa-f]+)\\s+end:0x([0-9A-Fa-f]+)", 0);
    ShsmReMatch m, r;
    UnitSplit *cur = NULL;
    for (size_t n = i; n < lines.n; n++) {
        AvpStr line = lines.v[n];
        if (line.n && !py_isspace_first(line)) {
            if (!shsm_re_match(head, line, 0, AVP_END, &m)) continue;
            UnitSplit s = {0};
            s.path = avp_cz(a, avp_str_strip(shsm_re_group(&m, 1)));
            s.first_line = n;
            s.last_line = n + 1;
            avp_vec_push(&splits, s);
            cur = &splits.v[splits.n - 1];
        } else if (cur && avp_str_strip(line).n) {
            if (shsm_re_match(sec_re, line, 0, AVP_END, &m)) {
                if (shsm_re_search(rng, shsm_re_group(&m, 2), 0, AVP_END, &r))
                    unit_ranges_set(&cur->ranges, avp_cz(a, shsm_re_group(&m, 1)), hex_of(shsm_re_group(&r, 1)),
                                    hex_of(shsm_re_group(&r, 2)));
            }
            cur->last_line = n + 1;
        }
    }
    avp_vec_free(&lines);
    return splits;
}

/* ---- configure.py ---- */

static size_t statement_end(AvpStr text, size_t open_paren) {
    int depth = 0;
    size_t i = open_paren, n = text.n;
    bool in_str = false;
    while (i < n) {
        char c = text.p[i];
        if (in_str) {
            if (c == '"') in_str = false;
        } else if (c == '"') {
            in_str = true;
        } else if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth == 0) {
                i++;
                break;
            }
        }
        i++;
    }
    if (i < n && text.p[i] == ',') i++;
    i64 nl = avp_str_find(text, AVP_S("\n"), (i64)i, (i64)n);
    return nl < 0 ? n : (size_t)nl + 1;
}

UnitConfig *unit_parse_configure(AvpArena *a, AvpStr text) {
    UnitConfig *c = AVP_NEW(a, UnitConfig);
    c->text = text;
    ShsmReMatch m, close, mm;
    if (!shsm_re_search(shsm_re("^config\\.libs\\s*=\\s*\\[\\s*$", SHSM_RE_M), text, 0, AVP_END, &m))
        unit_fail("configure.py: `config.libs = [` not found");
    size_t mend = (size_t)shsm_re_end(&m, 0);
    AvpStr after = avp_str_slice(text, (i64)mend, (i64)text.n);
    if (!shsm_re_search(shsm_re("^\\]\\s*$", SHSM_RE_M), after, 0, AVP_END, &close))
        unit_fail("configure.py: end of config.libs not found");
    size_t body_start = mend + 1, body_end = mend + (size_t)shsm_re_start(&close, 0);
    AvpStr body = avp_str_slice(text, (i64)body_start, (i64)body_end);
    UnitSizeVec items = {0};
    ShsmReIter it;
    shsm_re_iter_init(&it, shsm_re("^    (?:\\{|\\w+\\()", SHSM_RE_M), body, 0, AVP_END);
    while (shsm_re_iter_next(&it, &mm)) avp_vec_push(&items, body_start + (size_t)shsm_re_start(&mm, 0));
    for (size_t k = 0; k < items.n; k++) {
        size_t s = items.v[k], e = k + 1 < items.n ? items.v[k + 1] : body_end;
        AvpStr chunk = avp_str_slice(text, (i64)s, (i64)e);
        AvpStr ls = avp_str_lstrip(chunk);
        const char *name = NULL;
        if (avp_str_startswith(ls, AVP_S("{"))) {
            if (shsm_re_search(shsm_re("\"lib\"\\s*:\\s*\"([^\"]+)\"", 0), chunk, 0, AVP_END, &mm))
                name = avp_cz(a, shsm_re_group(&mm, 1));
        } else if (avp_str_startswith(ls, AVP_S("JPEGLib"))) {
            name = "libjpeg";
        } else if (avp_str_startswith(ls, AVP_S("ZlibLib"))) {
            name = "zlib";
        } else if (avp_str_startswith(ls, AVP_S("HBMLib"))) {
            name = "hbm";
        } else if (shsm_re_search(shsm_re("\"([^\"]+)\"", 0), chunk, 0, AVP_END, &mm)) {
            name = avp_cz(a, shsm_re_group(&mm, 1));
        }
        UnitLib L = {name ? name : avp_sprintf(a, "lib%zu", k), s, e};
        avp_vec_push(&c->libs, L);
    }
    avp_vec_free(&items);
    ShsmRe *obj_re =
        shsm_re("Object\\(\\s*(MatchingFor\\([^)]*\\)|Matching|NonMatching|Equivalent)\\s*,\\s*\"([^\"]+)\"", 0);
    shsm_re_iter_init(&it, obj_re, text, (i64)body_start, (i64)body_end);
    while (shsm_re_iter_next(&it, &mm)) {
        size_t st = (size_t)shsm_re_start(&mm, 0);
        i64 nl = avp_str_rfind(text, AVP_S("\n"), 0, (i64)st);
        size_t ls = (size_t)(nl + 1);
        if (avp_str_startswith(avp_str_lstrip(avp_str_slice(text, (i64)ls, (i64)st)), AVP_S("#"))) continue;
        size_t lib = SIZE_MAX;
        for (size_t k = 0; k < c->libs.n; k++)
            if (c->libs.v[k].start <= st && st < c->libs.v[k].end) {
                lib = k;
                break;
            }
        if (lib == SIZE_MAX) continue;
        UnitObj o = {0};
        o.path = avp_cz(a, shsm_re_group(&mm, 2));
        o.status = avp_cz(a, shsm_re_group(&mm, 1));
        o.status_start = (size_t)shsm_re_start(&mm, 1);
        o.status_end = (size_t)shsm_re_end(&mm, 1);
        o.start = st;
        o.end = statement_end(text, st + strlen("Object"));
        o.lib = lib;
        avp_vec_push(&c->objects, o);
    }
    return c;
}

const UnitObj *unit_config_find(const UnitConfig *c, const char *path) {
    for (size_t i = 0; i < c->objects.n; i++)
        if (!strcmp(c->objects.v[i].path, path)) return &c->objects.v[i];
    return NULL;
}

/* ---- config.yml ---- */

AvpStrVec unit_load_force_active(AvpArena *a, const char *path) {
    AvpStrVec names = {0};
    if (!avp_exists(path)) return names;
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TEXT(AVP_ENC_UTF8, AVP_ERR_STRICT, AVP_NL_DEFAULT), &text)) shsm_raise_io();
    AvpStrVec lines = avp_str_splitlines(text, false);
    ShsmRe *fa = shsm_re("^force_active\\s*:", 0), *item = shsm_re("^\\s*-\\s*(\\S+)", 0);
    ShsmReMatch m;
    bool on = false;
    for (size_t i = 0; i < lines.n; i++) {
        AvpStr line = lines.v[i];
        if (shsm_re_match(fa, line, 0, AVP_END, &m)) {
            on = true;
            continue;
        }
        if (!on) continue;
        if (shsm_re_match(item, line, 0, AVP_END, &m)) {
            avp_vec_push(&names, avp_str_dup(a, shsm_re_group(&m, 1)));
        } else if (avp_str_strip(line).n && !avp_str_startswith(avp_str_lstrip(line), AVP_S("#")) &&
                   !py_isspace_first(line)) {
            break;
        }
    }
    avp_vec_free(&lines);
    return names;
}

/* ---- the project ---- */

UnitProject *unit_project_new(AvpArena *a, const char *root) {
    UnitProject *p = AVP_NEW(a, UnitProject);
    p->a = a;
    p->root = avp_path_realpath(a, avp_path_abspath(a, root));
    p->symbols_path = avp_join(a, p->root, "config", UNIT_GAME, "symbols.txt");
    p->splits_path = avp_join(a, p->root, "config", UNIT_GAME, "splits.txt");
    p->config_yml = avp_join(a, p->root, "config", UNIT_GAME, "config.yml");
    p->configure_path = avp_join(a, p->root, "configure.py");
    p->dol_path = avp_join(a, p->root, "orig", UNIT_GAME, "sys", "main.dol");
    const char *need[] = {p->symbols_path, p->splits_path, p->configure_path};
    for (size_t i = 0; i < 3; i++)
        if (!avp_exists(need[i])) unit_fail("%s not found (is --root the repository root?)", need[i]);
    return p;
}

static int cmp_sym_addr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 l = (*(const UnitSym *const *)x)->addr, r = (*(const UnitSym *const *)y)->addr;
    return l < r ? -1 : l > r;
}

const UnitSymVec *unit_syms(UnitProject *p) {
    if (!p->syms_loaded) {
        p->syms = unit_load_symbols(p->a, p->symbols_path);
        p->sym_by_name = avp_map_new();
        for (size_t i = 0; i < p->syms.n; i++) {
            UnitSym *s = &p->syms.v[i];
            avp_map_put(p->sym_by_name, avp_str(s->name), NULL)->p = s;
            if (!strcmp(s->sec, ".text") && !strcmp(s->kind, "function")) avp_vec_push(&p->text_sorted, s);
        }
        avp_sort(p->text_sorted.v, p->text_sorted.n, sizeof(UnitSym *), cmp_sym_addr, NULL, false);
        p->syms_loaded = true;
    }
    return &p->syms;
}

const UnitSym *unit_sym(UnitProject *p, const char *name) {
    unit_syms(p);
    AvpVal *v = avp_map_get(p->sym_by_name, avp_str(name));
    return v ? v->p : NULL;
}

UnitSymPtrVec unit_text_funcs(UnitProject *p, i64 lo, i64 hi, const char *sec) {
    unit_syms(p);
    UnitSymPtrVec res = {0};
    if (!strcmp(sec, ".text")) {
        for (size_t i = 0; i < p->text_sorted.n; i++) {
            const UnitSym *s = p->text_sorted.v[i];
            if (lo <= s->addr && s->addr < hi) avp_vec_push(&res, s);
        }
        return res;
    }
    for (size_t i = 0; i < p->syms.n; i++) {
        const UnitSym *s = &p->syms.v[i];
        if (!strcmp(s->sec, sec) && !strcmp(s->kind, "function") && lo <= s->addr && s->addr < hi)
            avp_vec_push(&res, s);
    }
    avp_sort(res.v, res.n, sizeof(UnitSym *), cmp_sym_addr, NULL, false);
    return res;
}

UnitSplits unit_project_splits(UnitProject *p) { return unit_load_splits(p->a, unit_read_text(p->a, p->splits_path, NULL)); }

UnitConfig *unit_project_config(UnitProject *p) {
    return unit_parse_configure(p->a, unit_read_text(p->a, p->configure_path, NULL));
}

UnitIndex *unit_project_index(UnitProject *p) {
    if (!p->index) p->index = unit_index_load(p->a, p->root);
    return p->index;
}

static u32 be32(const u8 *b) { return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3]; }

static void load_ctors(UnitProject *p) {
    if (p->ctors_loaded) return;
    p->ctors_loaded = true;
    const UnitSym *c0 = NULL, *d0 = NULL;
    const UnitSymVec *syms = unit_syms(p);
    for (size_t i = 0; i < syms->n; i++) {
        if (!c0 && !strcmp(syms->v[i].name, "_ctors")) c0 = &syms->v[i];
        if (!d0 && !strcmp(syms->v[i].name, "_dtors")) d0 = &syms->v[i];
    }
    if (!c0 || !d0 || !avp_exists(p->dol_path)) return;
    AvpBytes dol = {0};
    if (!avp_read_all(p->a, p->dol_path, &dol)) shsm_raise_io();
    if (dol.n < 216) return;
    i64 addr = c0->addr, n = d0->addr - c0->addr;
    for (int i = 0; i < 18; i++) {
        i64 off = be32(dol.v + 4 * i), base = be32(dol.v + 72 + 4 * i), size = be32(dol.v + 144 + 4 * i);
        if (!size || !(base <= addr && addr < base + size)) continue;
        i64 o = off + addr - base, len = n < base + size - addr ? n : base + size - addr;
        if (len < 0) len = 0;
        if (o + len > (i64)dol.n) len = o < (i64)dol.n ? (i64)dol.n - o : 0;
        for (i64 k = 0; k + 4 <= len; k += 4) {
            u32 w = be32(dol.v + o + k);
            if (w) {
                UnitCtorEnt e = {w, addr + k};
                avp_vec_push(&p->ctors, e);
            }
        }
        break;
    }
}

size_t unit_ctor_hits(UnitProject *p, i64 addr, i64 *out, size_t cap) {
    load_ctors(p);
    size_t n = 0;
    for (size_t i = 0; i < p->ctors.n; i++)
        if (p->ctors.v[i].word == addr) {
            if (n < cap) out[n] = p->ctors.v[i].entry;
            n++;
        }
    return n;
}

bool unit_in_ctor_table(UnitProject *p, i64 addr) { return unit_ctor_hits(p, addr, NULL, 0) > 0; }

UnitSymPtrVec unit_sinit_funcs(UnitProject *p, const UnitSymPtrVec *funcs) {
    UnitSymPtrVec res = {0};
    for (size_t i = 0; i < funcs->n; i++) {
        const UnitSym *f = funcs->v[i];
        if (!strncmp(f->name, "__sinit_", 8) || unit_in_ctor_table(p, f->addr)) avp_vec_push(&res, f);
    }
    return res;
}

const char *unit_stem(AvpArena *a, const char *path) {
    AvpStr root, ext;
    avp_path_splitext(path, &root, &ext);
    return avp_cz(a, root);
}

const UnitSplit *unit_find_split(UnitProject *p, const char *arg, const UnitSplits *splits) {
    AvpArena *a = p->a;
    AvpStr s = avp_str_replace(a, avp_str(arg), AVP_S("\\"), AVP_S("/"), -1);
    s = avp_str_removeprefix(s, AVP_S("src/"));
    s = avp_str_removeprefix(s, AVP_S("main/"));
    const char *path = avp_cz(a, s);
    for (size_t i = 0; i < splits->n; i++)
        if (!strcmp(splits->v[i].path, path)) return &splits->v[i];
    const char *stem = unit_stem(a, path);
    const UnitSplit *hit = NULL;
    size_t hits = 0;
    for (size_t i = 0; i < splits->n; i++)
        if (!strcmp(unit_stem(a, splits->v[i].path), stem)) {
            hit = &splits->v[i];
            hits++;
        }
    if (hits == 1) return hit;
    unit_fail("no split named %s in splits.txt", unit_repr(a, arg));
}

bool unit_parse_range(AvpArena *a, const char *text, i64 *lo, i64 *hi) {
    ShsmReMatch m;
    AvpStr t = avp_str_strip(avp_str(text));
    if (!shsm_re_match(shsm_re("^(?:0x)?([0-9A-Fa-f]+)\\s*[-:]\\s*(?:0x)?([0-9A-Fa-f]+)$", 0), t, 0, AVP_END, &m))
        unit_fail("bad range %s: expected START-END in hex", unit_repr(a, text));
    *lo = hex_of(shsm_re_group(&m, 1));
    *hi = hex_of(shsm_re_group(&m, 2));
    if (*lo >= *hi) unit_fail("empty range %s", unit_repr(a, text));
    return true;
}

int unit_func_align_for(const char *path) { return !strncmp(path, "revolution/", 11) ? 16 : 4; }

bool unit_is_game_path(const char *path) { return !strncmp(path, "game/", 5) || !strncmp(path, "climax/", 7); }

/* ---- progress lines ---- */

static double g_step_t0 = -1;

void unit_step_reset(void) { g_step_t0 = avp_time(); }

void unit_step(const char *name, const char *note) {
    if (g_step_t0 < 0) unit_step_reset();
    double s = avp_time() - g_step_t0;
    if (note && *note) unit_out("[%4.0fs] step %s: %s", s, name, note);
    else unit_out("[%4.0fs] step %s", s, name);
    if (!g_capture) fflush(stdout);
}
