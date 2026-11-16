/* names.py check: elf_functions, def_name, dead_marked, check_objects, source_name_matches, check_addresses and
 * cmd_check. */
#include "names.h"
#include "names_priv.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "shsm_mangle.h"
#include "shsm_re.h"
#include "shsm_units.h"

#include <string.h>

#define CALLISH "((?:[A-Za-z_~][\\w~]*(?:<[^()<>]*>)?::)*(?:operator\\s*[^\\s(]+|~?[A-Za-z_]\\w*))\\s*\\("

static bool not_name(AvpStr w) {
    static const char *const NOT_NAME[] = {"if",     "for",       "while",         "switch", "return",
                                           "sizeof", "__declspec", "__attribute__", "asm",    "defined"};
    for (size_t i = 0; i < sizeof NOT_NAME / sizeof *NOT_NAME; i++)
        if (avp_str_eqz(w, NOT_NAME[i])) return true;
    return false;
}

static bool all_macro_chars(AvpStr s) {
    if (!s.n) return false;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

static AvpStr last_part(AvpStr n) {
    i64 i = avp_str_rfind(n, AVP_S("::"), 0, AVP_END);
    return i < 0 ? n : avp_str_slice(n, i + 2, AVP_END);
}

const char *names_def_name(AvpArena *a, AvpStr line) {
    AvpStr t = avp_str_rstrip(line);
    if (!t.n) return NULL;
    const char *q = t.p;
    u32 cp0 = avp_u8_next(&q, t.p + t.n);
    if (avp_cp_isspace(cp0)) return NULL;
    static const char *const PRE[] = {"//", "/*", "*", "#", "}", "{"};
    for (size_t i = 0; i < sizeof PRE / sizeof *PRE; i++)
        if (avp_str_startswith(t, avp_str(PRE[i]))) return NULL;
    if (avp_str_endswith(t, AVP_S(";"))) return NULL;
    i64 paren = avp_str_findc(t, '(');
    AvpStr head = paren < 0 ? t : avp_strn(t.p, (size_t)paren);
    AvpStrVec words = avp_str_split(head, (AvpStr){NULL, 0}, -1);
    bool reject = words.n == 0 || avp_str_contains(head, AVP_S("=")) || not_name(words.v[0]);
    avp_vec_free(&words);
    if (reject) return NULL;
    const ShsmRe *rx = shsm_re(CALLISH, 0);
    ShsmReIter it;
    ShsmReMatch m;
    bool any = false;
    shsm_re_iter_init(&it, rx, t, 0, AVP_END);
    while (shsm_re_iter_next(&it, &m)) {
        any = true;
        AvpStr n = shsm_re_group(&m, 1);
        AvpStr last = last_part(n);
        if (not_name(last)) return NULL;
        if (all_macro_chars(last)) continue; /* a macro (GLOBAL, METHODDEF, DECL_...) */
        return avp_cz(a, n);
    }
    return any ? "" : NULL;
}

/* ---- ELF ---- */

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

bool names_elf_functions(AvpArena *a, const char *path, NamesElfFuncs *out) {
    AvpBytes b = {0};
    if (!avp_read_all(a, path, &b)) return false;
    const u8 *d = b.v;
    size_t n = b.n;
    if (n < 6 || memcmp(d, "\x7f" "ELF", 4) != 0 || d[4] != 1 || d[5] != 2) return false;
    if (n < 0x24 || n < 0x32) return false;
    u64 shoff = be32(d + 0x20);
    u32 shentsize = be16(d + 0x2E), shnum = be16(d + 0x30);
    u32(*secs)[10] = avp_alloc(a, sizeof(u32[10]) * (shnum ? shnum : 1));
    for (u32 i = 0; i < shnum; i++) {
        u64 off = shoff + (u64)i * shentsize;
        if (off + 40 > n) return false;
        for (int k = 0; k < 10; k++) secs[i][k] = be32(d + off + 4 * k);
    }
    out->n = 0;
    for (u32 i = 0; i < shnum; i++) {
        if (secs[i][1] != 2) continue; /* SHT_SYMTAB */
        if (secs[i][6] >= shnum) return false;
        u64 stroff = secs[secs[i][6]][4];
        for (u64 k = 0; k < secs[i][5] / 16; k++) {
            u64 off = secs[i][4] + k * 16;
            if (off + 16 > n) return false;
            u32 st_name = be32(d + off);
            u8 info = d[off + 12];
            u16 shndx = be16(d + off + 14);
            if ((info & 0xF) != 2 || shndx == 0) continue;
            u64 s = stroff + st_name;
            if (s > n) return false;
            const u8 *z = memchr(d + s, 0, n - s);
            if (!z) return false;
            AvpBuf nb;
            avp_buf_init(&nb, a);
            for (const u8 *c = d + s; c < z; c++) avp_buf_putcp(&nb, *c); /* latin-1 */
            NamesElfFunc f = {avp_buf_cz(&nb), info >> 4};
            avp_vec_push(out, f);
        }
    }
    return true;
}

/* ---- check ---- */

static bool dead_marked(AvpArena *a, const AvpStrVec *lines, const char *plain) {
    AvpBuf pat;
    avp_buf_init(&pat, a);
    avp_buf_putz(&pat, "(?<![\\w])");
    avp_buf_put(&pat, shsm_re_escape(a, avp_str(plain)));
    avp_buf_putz(&pat, "\\s*\\(");
    const ShsmRe *rx = shsm_re(avp_buf_cz(&pat), 0);
    for (size_t i = 0; i < lines->n; i++) {
        if (!shsm_re_search(rx, lines->v[i], 0, AVP_END, NULL)) continue;
        const char *dn = names_def_name(a, lines->v[i]);
        if (!dn || !*dn) continue;
        size_t lim = lines->n < i + 4000 ? lines->n : i + 4000;
        for (size_t j = i; j < lim; j++)
            if (avp_str_startswith(lines->v[j], AVP_S("}"))) {
                AvpStr t = lines->v[j];
                return avp_str_contains(t, AVP_S("no copy in retail")) || avp_str_contains(t, AVP_S("dead-stripped"));
            }
    }
    return false;
}

typedef struct {
    i64 objects, missing_objects, weak, dead_marked, stripped, functions;
} Counted;

static AvpJson *check_objects(NamesCtx *cx, Counted *c) {
    AvpArena *a = cx->a;
    const ShsmRepo *repo = cx->repo;
    AvpJson *findings = avp_json_arr(a);
    const ShsmUnits *us = shsm_load_units(repo);
    for (size_t ui = 0; ui < us->n; ui++) {
        const ShsmUnit *u = us->v[ui];
        if (!u->source || !*u->source || (u->status && strcmp(u->status, "Asm") == 0) || !u->base_obj ||
            !*u->base_obj)
            continue;
        const char *obj = avp_join(a, repo->root, u->base_obj);
        if (!avp_isfile(obj)) {
            c->missing_objects++;
            continue;
        }
        c->objects++;
        NamesElfFuncs fns = {0};
        if (!names_elf_functions(a, obj, &fns)) {
            c->missing_objects++;
            continue;
        }
        bool have_src = false;
        AvpStrVec src_lines = {0};
        for (size_t k = 0; k < fns.n; k++) {
            const char *name = fns.v[k].name;
            c->functions++;
            if (names_has(cx, name)) continue;
            if (fns.v[k].bind == 2) {
                c->weak++;
                continue;
            }
            if (!have_src) {
                have_src = true;
                AvpBytes sb = {0};
                if (avp_read_all(a, avp_join(a, repo->root, u->source), &sb))
                    src_lines = names_split_nl(avp_strn((const char *)sb.v, sb.n));
            }
            if (dead_marked(a, &src_lines, names_plain_of(a, name).plain)) {
                c->dead_marked++;
                continue;
            }
            if (u->status && strcmp(u->status, "Matching") == 0) {
                c->stripped++;
                continue;
            }
            AvpJson *f = avp_json_obj(a);
            avp_json_set(a, f, AVP_S("check"), avp_json_strz(a, "no-symbol"));
            avp_json_set(a, f, AVP_S("unit"), avp_json_strz(a, u->name));
            avp_json_set(a, f, AVP_S("name"), avp_json_strz(a, name));
            avp_json_set(a, f, AVP_S("status"), u->status ? avp_json_strz(a, u->status) : avp_json_null());
            const char *dm = shsm_demangle(a, name);
            avp_json_set(a, f, AVP_S("demangled"), dm ? avp_json_strz(a, dm) : avp_json_null());
            avp_json_arr_push(a, findings, f);
        }
        avp_vec_free(&fns);
    }
    return findings;
}

/* re.sub(r"<.*>", "", x.replace(" ", "")) */
static AvpStr bare(AvpArena *a, AvpStr x) {
    AvpStr s = avp_str_replace(a, x, AVP_S(" "), AVP_S(""), -1);
    i64 i = avp_str_findc(s, '<');
    i64 j = avp_str_rfind(s, AVP_S(">"), 0, AVP_END);
    if (i < 0 || j < i) return s;
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_put(&b, avp_strn(s.p, (size_t)i));
    avp_buf_put(&b, avp_str_slice(s, j + 1, AVP_END));
    return avp_buf_str(&b);
}

static bool source_name_matches(NamesCtx *cx, const char *src_name, const ShsmSymbol *sym) {
    AvpArena *a = cx->a;
    const NamesPlain *pl = names_sym_plain(cx, sym);
    AvpStrVec parts = avp_str_split(avp_str(src_name), AVP_S("::"), -1);
    for (size_t i = 0; i < parts.n; i++) parts.v[i] = bare(a, parts.v[i]);
    AvpStr lastp = parts.v[parts.n - 1];
    bool r;
    if (avp_str_startswith(lastp, AVP_S("operator"))) {
        r = avp_str_eq(avp_str_replace(a, avp_str(pl->plain), AVP_S(" "), AVP_S(""), -1), lastp);
    } else {
        r = avp_str_eq(bare(a, avp_str(pl->plain)), lastp);
        if (r && parts.n > 1) {
            size_t k = parts.n - 1;
            size_t from = k >= pl->nscope ? 0 : pl->nscope - k;
            size_t m = pl->nscope - from;
            r = m == k;
            for (size_t i = 0; r && i < k; i++) r = avp_str_eq(bare(a, avp_str(pl->scope[from + i])), parts.v[i]);
        }
    }
    avp_vec_free(&parts);
    return r;
}

static AvpJson *check_addresses(NamesCtx *cx, i64 *checked) {
    AvpArena *a = cx->a;
    const ShsmRepo *repo = cx->repo;
    AvpJson *findings = avp_json_arr(a);
    const ShsmRe *addr_rx = shsm_re("//\\s*0x(8[0-9A-Fa-f]{7})\\b", 0);
    AvpWalk *w = avp_walk_open(repo->src_dir, true);
    AvpWalkItem it = {0};
    AvpPtrVec at = {0};
    while (avp_walk_next(w, &it)) {
        AvpStrVec fns = {0};
        avp_vec_extend(&fns, it.files.v, it.files.n);
        names_sort_strs(&fns);
        for (size_t fi = 0; fi < fns.n; fi++) {
            if (!names_src_ext(fns.v[fi])) continue;
            const char *path = avp_join(a, avp_cz(a, it.dirpath), avp_cz(a, fns.v[fi]));
            AvpStr text = names_read_text(a, path);
            AvpStrVec lines = names_split_nl(text);
            for (size_t i = 0; i < lines.n; i++) {
                ShsmReMatch m;
                if (!shsm_re_search(addr_rx, lines.v[i], 0, AVP_END, &m)) continue;
                AvpStr hex = shsm_re_group(&m, 1);
                u64 addr = 0;
                for (size_t k = 0; k < hex.n; k++) {
                    char ch = hex.p[k];
                    addr = addr * 16 + (u64)(ch <= '9' ? ch - '0' : (ch | 0x20) - 'a' + 10);
                }
                AvpStr before = avp_str_strip(avp_strn(lines.v[i].p, (size_t)shsm_re_start(&m, 0)));
                const char *name = NULL;
                if (before.n == 0) {
                    size_t lim = lines.n < i + 8 ? lines.n : i + 8;
                    for (size_t j = i + 1; j < lim; j++) {
                        name = names_def_name(a, lines.v[j]);
                        if (name && *name) break;
                        AvpStr ls = avp_str_lstrip(lines.v[j]);
                        if (name == NULL && avp_str_strip(lines.v[j]).n && !avp_str_startswith(ls, AVP_S("//")) &&
                            !avp_str_startswith(ls, AVP_S("/*")) && !avp_str_startswith(ls, AVP_S("*")))
                            break;
                    }
                } else if (avp_str_startswith(before, AVP_S("}"))) {
                    i64 lo = (i64)i - 4000 > -1 ? (i64)i - 4000 : -1;
                    for (i64 j = (i64)i - 1; j > lo; j--) {
                        if (avp_str_startswith(lines.v[j], AVP_S("}"))) break;
                        name = names_def_name(a, lines.v[j]);
                        if (name && !*name && (size_t)j + 1 < lines.n) name = names_def_name(a, lines.v[j + 1]);
                        if (name && *name) break;
                    }
                }
                if (!name || !*name) continue;
                (*checked)++;
                at.n = 0;
                shsm_sym_at(cx->st, addr, NULL, &at);
                AvpPtrVec fat = {0};
                for (size_t k = 0; k < at.n; k++) {
                    const ShsmSymbol *s = at.v[k];
                    if (s->type && strcmp(s->type, "function") == 0) avp_vec_push(&fat, (void *)s);
                }
                const AvpPtrVec *use = fat.n ? &fat : &at;
                bool ok = false;
                for (size_t k = 0; k < use->n && !ok; k++) ok = source_name_matches(cx, name, use->v[k]);
                if (ok) {
                    avp_vec_free(&fat);
                    continue;
                }
                AvpStr plain = last_part(avp_str_replace(a, avp_str(name), AVP_S(" "), AVP_S(""), -1));
                const AvpPtrVec *byp = names_by_plain(cx, avp_cz(a, plain));
                AvpJson *named = avp_json_arr(a);
                size_t nn = 0;
                for (size_t k = 0; k < byp->n; k++) {
                    const ShsmSymbol *s = byp->v[k];
                    if (!s->type || strcmp(s->type, "function") != 0 || !source_name_matches(cx, name, s)) continue;
                    if (nn++ < 4) avp_json_arr_push(a, named, avp_json_str(a, avp_pyfmt(a, "%s 0x%08X", s->name,
                                                                                        s->address)));
                }
                AvpJson *f = avp_json_obj(a);
                avp_json_set(a, f, AVP_S("check"), avp_json_strz(a, "address"));
                avp_json_set(a, f, AVP_S("file"), avp_json_strz(a, shsm_repo_rel(a, repo, path)));
                avp_json_set(a, f, AVP_S("line"), avp_json_int(a, (i64)i + 1));
                avp_json_set(a, f, AVP_S("function"), avp_json_strz(a, name));
                avp_json_set(a, f, AVP_S("comment"), avp_json_uint(a, addr));
                AvpJson *sat = avp_json_arr(a);
                for (size_t k = 0; k < use->n; k++)
                    avp_json_arr_push(a, sat, avp_json_strz(a, ((const ShsmSymbol *)use->v[k])->name));
                avp_json_set(a, f, AVP_S("symbol_at"), sat);
                avp_json_set(a, f, AVP_S("named_at"), named);
                avp_json_arr_push(a, findings, f);
                avp_vec_free(&fat);
            }
            avp_vec_free(&lines);
        }
        avp_vec_free(&fns);
    }
    avp_walk_close(w);
    avp_vec_free(&at);
    return findings;
}

static const char *join_json_strs(AvpArena *a, const AvpJson *arr) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < avp_json_len(arr); i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_put(&b, avp_json_str_or(avp_json_at(arr, i), AVP_S("")));
    }
    return avp_buf_cz(&b);
}

int names_cmd_check(const NamesArgs *na, ShsmOutput *out, const ShsmRepo *repo) {
    (void)na;
    NamesCtx *cx = names_ctx_new(repo, NULL);
    AvpArena *a = cx->a;
    Counted c = {0};
    AvpJson *f1 = check_objects(cx, &c);
    i64 checked = 0;
    AvpJson *f2 = check_addresses(cx, &checked);
    shsm_output_line(out, avp_pyfmt(a,
                                    "source objects: %d read (%d not built), %d functions; without a symbols.txt "
                                    "entry, not reported: %d weak, %d marked dead-stripped, %d in Matching units "
                                    "(dropped by the link)",
                                    c.objects, c.missing_objects, c.functions, c.weak, c.dead_marked, c.stripped));
    for (size_t i = 0; i < avp_json_len(f1); i++) {
        const AvpJson *f = avp_json_at(f1, i);
        AvpStr dm = avp_json_get_str(f, "demangled", (AvpStr){NULL, 0});
        AvpStr unit = avp_json_get_str(f, "unit", AVP_S("")), name = avp_json_get_str(f, "name", AVP_S("")),
               status = avp_json_get_str(f, "status", AVP_S("None"));
        AvpStr extra = dm.p ? avp_pyfmt(a, "  {%S}", &dm) : AVP_S("");
        shsm_output_line(out, avp_pyfmt(a, "NO-SYMBOL %S %S%S  (%S)", &unit, &name, &extra, &status));
    }
    shsm_output_line(out, avp_pyfmt(a, "address comments: %d checked against their functions", checked));
    for (size_t i = 0; i < avp_json_len(f2); i++) {
        const AvpJson *f = avp_json_at(f2, i);
        AvpStr file = avp_json_get_str(f, "file", AVP_S("")), fn = avp_json_get_str(f, "function", AVP_S(""));
        i64 line = avp_json_get_i64(f, "line", 0);
        u64 cm = (u64)avp_json_get_i64(f, "comment", 0);
        const AvpJson *sat = avp_json_getz(f, "symbol_at"), *nat = avp_json_getz(f, "named_at");
        const char *there = avp_json_len(sat) ? join_json_strs(a, sat) : "no symbol";
        AvpStr extra = avp_json_len(nat) ? avp_pyfmt(a, "; %S is at %s", &fn, join_json_strs(a, nat)) : AVP_S("");
        shsm_output_line(out, avp_pyfmt(a, "ADDRESS %S:%d %S says 0x%08X; there: %s%S", &file, line, &fn, cm, there,
                                        &extra));
    }
    shsm_output_data(out, "no_symbol", f1);
    shsm_output_data(out, "address", f2);
    AvpJson *counts = avp_json_obj(a);
    avp_json_set(a, counts, AVP_S("objects"), avp_json_int(a, c.objects));
    avp_json_set(a, counts, AVP_S("missing_objects"), avp_json_int(a, c.missing_objects));
    avp_json_set(a, counts, AVP_S("weak"), avp_json_int(a, c.weak));
    avp_json_set(a, counts, AVP_S("dead_marked"), avp_json_int(a, c.dead_marked));
    avp_json_set(a, counts, AVP_S("stripped"), avp_json_int(a, c.stripped));
    avp_json_set(a, counts, AVP_S("functions"), avp_json_int(a, c.functions));
    avp_json_set(a, counts, AVP_S("address_comments"), avp_json_int(a, checked));
    shsm_output_data(out, "counts", counts);
    return avp_json_len(f1) || avp_json_len(f2) ? SHSM_EXIT_FINDING : SHSM_EXIT_OK;
}
