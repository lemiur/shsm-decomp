/* tools/codecmp.py: Project, Dol, Context, compare_function, compare_object (see cc_codecmp.h). */
#include "cc_codecmp.h"

#include <stdlib.h>

static bool is_space(u8 c) { return cc_is_space(c); }
static bool is_uhex(u8 c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); }
static bool is_hex(u8 c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_digit(u8 c) { return c >= '0' && c <= '9'; }
static bool is_word(u8 c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_' || c >= 0x80; }
static int hexval(u8 c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static i64 hex_of(const char *p, size_t n) {
    i64 v = 0;
    for (size_t k = 0; k < n; k++) v = v * 16 + hexval((u8)p[k]);
    return v;
}
static bool all(AvpStr s, size_t i, size_t j, bool (*pred)(u8)) {
    if (j <= i) return false;
    for (size_t k = i; k < j; k++)
        if (!pred((u8)s.p[k])) return false;
    return true;
}
static bool starts(AvpStr s, const char *pre) { return avp_str_startswith(s, avp_str(pre)); }

/* ---- Python formatting ------------------------------------------------------------------------------------------ */

/* "%+#x" % v */
static const char *sx(AvpArena *a, i64 v) {
    return v < 0 ? avp_sprintf(a, "-0x%llx", (unsigned long long)(-v)) : avp_sprintf(a, "+0x%llx", (unsigned long long)v);
}
/* "%#x" % v */
static const char *hx(AvpArena *a, i64 v) {
    return v < 0 ? avp_sprintf(a, "-0x%llx", (unsigned long long)(-v)) : avp_sprintf(a, "0x%llx", (unsigned long long)v);
}
/* "0x%08X" % v */
static const char *h08(AvpArena *a, i64 v) {
    return v < 0 ? avp_sprintf(a, "0x-%07llX", (unsigned long long)(-v)) : avp_sprintf(a, "0x%08llX", (unsigned long long)v);
}
/* bytes[:16].hex() */
static const char *hex16(AvpArena *a, AvpStr b) {
    size_t n = b.n < 16 ? b.n : 16;
    char *s = avp_alloc_noz(a, 2 * n + 1);
    for (size_t k = 0; k < n; k++) {
        static const char d[] = "0123456789abcdef";
        s[2 * k] = d[(u8)b.p[k] >> 4];
        s[2 * k + 1] = d[(u8)b.p[k] & 15];
    }
    s[2 * n] = 0;
    return s;
}
static AvpStr S(const char *cz) { return avp_str(cz); }

/* ---- names ------------------------------------------------------------------------------------------------------ */

bool cc_embedded_address(AvpStr n, i64 *addr) {
    static const char *const pre[] = {"fn_", "lbl_", "jumptable_", "dtor_", "sinit_"};
    for (size_t k = 0; k < AVP_ARRAY_LEN(pre); k++) {
        size_t pl = strlen(pre[k]);
        if (!starts(n, pre[k]) || n.n < pl + 8 || !all(n, pl, pl + 8, is_uhex)) continue;
        if (n.n == pl + 8 || (n.n >= pl + 10 && n.p[pl + 8] == '_' && n.p[pl + 9] == '_')) {
            *addr = hex_of(n.p + pl, 8);
            return true;
        }
    }
    if (starts(n, "gap_")) {
        size_t p = 4;
        while (p < n.n && is_digit((u8)n.p[p])) p++;
        if (p > 4 && p + 10 < n.n && n.p[p] == '_' && all(n, p + 1, p + 9, is_uhex) && n.p[p + 9] == '_' &&
            all(n, p + 10, n.n, is_word)) {
            *addr = hex_of(n.p + p + 1, 8);
            return true;
        }
    }
    /* ^@\w+?_([0-9A-F]{8})$ */
    if (n.n >= 11 && n.p[0] == '@' && n.p[n.n - 9] == '_' && all(n, n.n - 8, n.n, is_uhex) && all(n, 1, n.n - 9, is_word)) {
        *addr = hex_of(n.p + n.n - 8, 8);
        return true;
    }
    return false;
}

bool cc_is_compiler_local(AvpStr n) {
    if (n.n >= 2 && n.p[0] == '@' && all(n, 1, n.n, is_digit)) return true;
    if (starts(n, "@stringBase") && all(n, 11, n.n, is_digit)) return true;
    if (n.n >= 11 && n.p[0] == '@' && n.p[n.n - 9] == '_' && all(n, n.n - 8, n.n, is_uhex) && all(n, 1, n.n - 9, is_word))
        return true;
    if (starts(n, "...")) {
        size_t p = 3;
        while (p < n.n && is_word((u8)n.p[p])) p++;
        if (p > 3 && p < n.n && n.p[p] == '.' && all(n, p + 1, n.n, is_digit)) return true;
    }
    if ((starts(n, "@etb_") || starts(n, "@eti_")) && all(n, 5, n.n, is_word)) return true;
    return false;
}

/* ---- symbols.txt, splits.txt -------------------------------------------------------------------------------------- */

static int cmp_span(const void *x, const void *y) {
    const CcSpan *a = x, *b = y;
    if (a->addr != b->addr) return a->addr < b->addr ? -1 : 1;
    if (a->size != b->size) return a->size < b->size ? -1 : 1;
    return avp_str_cmp(a->name, b->name);
}

static AvpStr dupz(AvpArena *a, AvpStr s) {
    char *p = avp_alloc_noz(a, s.n + 1);
    if (s.n) memcpy(p, s.p, s.n);
    p[s.n] = 0;
    return avp_strn(p, s.n);
}

void cc_parse_symbols(AvpArena *a, AvpStr text, AvpMap **names_out, CcSpan **spans_out, size_t *nspans) {
    AvpMap *names = avp_map_new();
    AVP_VEC(CcSpan) spans = {0};
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr l = lines.v[li];
        /* ^(\S+) = ([^:\s]+):(0x[hex]+);(.*)$ */
        size_t p = 0;
        while (p < l.n && !is_space((u8)l.p[p])) p++;
        if (p == 0 || l.n - p < 3 || memcmp(l.p + p, " = ", 3) != 0) continue;
        AvpStr name = avp_strn(l.p, p);
        size_t q = p + 3, s0 = q;
        while (q < l.n && l.p[q] != ':' && !is_space((u8)l.p[q])) q++;
        if (q == s0 || q >= l.n || l.p[q] != ':') continue;
        q++;
        if (l.n - q < 3 || l.p[q] != '0' || l.p[q + 1] != 'x' || !is_hex((u8)l.p[q + 2])) continue;
        size_t h0 = q + 2;
        q = h0;
        while (q < l.n && is_hex((u8)l.p[q])) q++;
        if (q >= l.n || l.p[q] != ';') continue;
        i64 addr = hex_of(l.p + h0, q - h0);
        AvpStr rest = avp_strn(l.p + q + 1, l.n - q - 1);
        bool created;
        AvpVal *v = avp_map_put(names, name, &created);
        if (created) v->p = avp_xcalloc(1, sizeof(AvpI64Vec));
        avp_vec_push((AvpI64Vec *)v->p, addr);
        i64 size = 0;
        for (size_t k = 0; k + 7 < rest.n; k++) {
            if (k > 0 && is_word((u8)rest.p[k - 1])) continue;
            if (memcmp(rest.p + k, "size:0x", 7) != 0 || !is_hex((u8)rest.p[k + 7])) continue;
            size_t e = k + 7;
            while (e < rest.n && is_hex((u8)rest.p[e])) e++;
            size = hex_of(rest.p + k + 7, e - k - 7);
            break;
        }
        CcSpan sp = {addr, size, {0}};
        avp_vec_push(&spans, sp);
        spans.v[spans.n - 1].name = name;
    }
    /* the span names point into the map's own key copies (stable), not into the text */
    for (size_t k = 0; k < spans.n; k++) spans.v[k].name = dupz(a, spans.v[k].name);
    qsort(spans.v, spans.n, sizeof(CcSpan), cmp_span);
    CcSpan *arr = AVP_NEWN(a, CcSpan, spans.n ? spans.n : 1);
    if (spans.n) memcpy(arr, spans.v, spans.n * sizeof(CcSpan));
    *names_out = names;
    *spans_out = arr;
    *nspans = spans.n;
    avp_vec_free(&spans);
    avp_vec_free(&lines);
}

AvpMap *cc_parse_splits(AvpArena *a, AvpStr text) {
    AvpMap *out = avp_map_new();
    AvpStrVec lines = avp_str_splitlines(text, false);
    AvpVal *cur = NULL;
    AVP_VEC(CcSecRange) secs = {0};
    AvpStr cur_name = {0};
#define FLUSH()                                                                       \
    do {                                                                              \
        if (cur) {                                                                    \
            CcUnitSecs *u = avp_alloc(a, sizeof(CcUnitSecs));                         \
            u->n = secs.n;                                                            \
            u->v = AVP_NEWN(a, CcSecRange, secs.n ? secs.n : 1);                      \
            if (secs.n) memcpy(u->v, secs.v, secs.n * sizeof(CcSecRange));            \
            avp_map_put(out, cur_name, NULL)->p = u;                                  \
        }                                                                             \
        cur = NULL;                                                                   \
        secs.n = 0;                                                                   \
    } while (0)
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr l = lines.v[li];
        if (!avp_str_strip(l).n) continue;
        AvpStr first = avp_u8_slice(l, 0, 1);
        if (!avp_str_isspace(first)) {
            FLUSH();
            AvpStr r = avp_str_rstrip(l);
            if (avp_str_endswith(r, AVP_S(":")) && !avp_str_startswith(l, AVP_S("Sections"))) {
                cur_name = dupz(a, avp_str_slice(r, 0, -1));
                /* out[cur] = {}: a repeated path is reset, keeping its first position */
                AvpVal *v = avp_map_put(out, cur_name, NULL);
                v->p = NULL;
                cur = v;
            }
            continue;
        }
        if (!cur) continue;
        /* \s+(\S+)\s+start:(0x[hex]+) end:(0x[hex]+) */
        size_t p = 0;
        while (p < l.n && is_space((u8)l.p[p])) p++;
        size_t n0 = p;
        while (p < l.n && !is_space((u8)l.p[p])) p++;
        if (p == n0) continue;
        AvpStr sec = avp_strn(l.p + n0, p - n0);
        size_t w = p;
        while (p < l.n && is_space((u8)l.p[p])) p++;
        if (p == w || l.n - p < 9 || memcmp(l.p + p, "start:0x", 8) != 0 || !is_hex((u8)l.p[p + 8])) continue;
        size_t h0 = p + 8;
        p = h0;
        while (p < l.n && is_hex((u8)l.p[p])) p++;
        i64 start = hex_of(l.p + h0, p - h0);
        if (l.n - p < 8 || memcmp(l.p + p, " end:0x", 7) != 0 || !is_hex((u8)l.p[p + 7])) continue;
        size_t e0 = p + 7;
        p = e0;
        while (p < l.n && is_hex((u8)l.p[p])) p++;
        i64 end = hex_of(l.p + e0, p - e0);
        size_t k;
        for (k = 0; k < secs.n; k++)
            if (avp_str_eq(secs.v[k].name, sec)) break;
        CcSecRange r = {dupz(a, sec), start, end};
        if (k < secs.n) secs.v[k] = r;
        else avp_vec_push(&secs, r);
    }
    FLUSH();
#undef FLUSH
    /* a path line with no block body leaves NULL: make it an empty record */
    AvpMapIter it = {0};
    while (avp_map_next(out, &it))
        if (!it.val->p) it.val->p = avp_alloc(a, sizeof(CcUnitSecs));
    avp_vec_free(&secs);
    avp_vec_free(&lines);
    return out;
}

/* ---- project ---------------------------------------------------------------------------------------------------- */

struct CcProject {
    char *root;
    AvpArena *arena;
    AvpMutex lock;
    bool syms_loaded;
    AvpMap *names;
    CcSpan *spans;
    size_t nspans;
    bool dol_loaded;
    CcDol *dol;
    bool splits_loaded;
    AvpMap *splits;
};

CcProject *cc_project_new(const char *root) {
    AvpArena *a = avp_arena_new(0);
    CcProject *p = AVP_NEW(a, CcProject);
    p->arena = a;
    p->root = avp_strdup(a, root);
    return p;
}

const char *cc_project_root(const CcProject *p) { return p->root; }

static AvpStr read_or_die(CcProject *p, const char *rel) {
    char *path = avp_path_join(p->arena, p->root, rel);
    AvpStr text;
    if (!avp_read_text(p->arena, path, AVP_TM_UTF8, &text)) avp_fatal("%s", avp_io_err_str(p->arena).p);
    return text;
}

static void load_syms(CcProject *p) {
    avp_mutex_lock(&p->lock);
    if (!p->syms_loaded) {
        cc_parse_symbols(p->arena, read_or_die(p, "config\\R5WEA4\\symbols.txt"), &p->names, &p->spans, &p->nspans);
        p->syms_loaded = true;
    }
    avp_mutex_unlock(&p->lock);
}

const AvpI64Vec *cc_project_addresses(CcProject *p, AvpStr name) {
    if (!p->syms_loaded) load_syms(p);
    AvpVal *v = avp_map_get(p->names, name);
    return v ? v->p : NULL;
}

const CcSpan *cc_project_spans(CcProject *p, size_t *n) {
    if (!p->syms_loaded) load_syms(p);
    *n = p->nspans;
    return p->spans;
}

const CcDol *cc_project_dol(CcProject *p) {
    if (p->dol_loaded) return p->dol;
    avp_mutex_lock(&p->lock);
    if (!p->dol_loaded) {
        char *path = avp_path_join(p->arena, p->root, "orig\\R5WEA4\\sys\\main.dol");
        AvpBytes data = {0};
        if (avp_exists(path)) {
            if (!avp_read_all(p->arena, path, &data)) avp_fatal("%s", avp_io_err_str(p->arena).p);
            if (data.n < 224) avp_fatal("%s: not a DOL", path);
            AvpTry t;
            if (AVP_TRY(&t)) {
                p->dol = shsm_dol_parse(p->arena, data.v, data.n);
                avp_try_end(&t);
            } else {
                avp_fatal("%s: not a DOL (%s)", path, t.msg ? t.msg : "");
            }
        }
        p->dol_loaded = true;
    }
    avp_mutex_unlock(&p->lock);
    return p->dol;
}

CcUnitSecs cc_project_unit_sections(CcProject *p, AvpStr unit) {
    if (!p->splits_loaded) {
        avp_mutex_lock(&p->lock);
        if (!p->splits_loaded) {
            p->splits = cc_parse_splits(p->arena, read_or_die(p, "config\\R5WEA4\\splits.txt"));
            p->splits_loaded = true;
        }
        avp_mutex_unlock(&p->lock);
    }
    if (starts(unit, "main/")) unit = avp_strn(unit.p + 5, unit.n - 5);
    AvpMapIter it = {0};
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    CcUnitSecs found = {0};
    while (avp_map_next(p->splits, &it)) {
        AvpStr root, ext;
        avp_path_splitext(avp_cz(avp_scratch(), it.key), &root, &ext);
        if (avp_str_eq(root, unit)) {
            found = *(const CcUnitSecs *)it.val->p;
            break;
        }
    }
    avp_arena_reset(avp_scratch(), mk);
    return found;
}

AvpStr cc_project_describe(AvpArena *a, CcProject *p, i64 addr) {
    size_t n;
    const CcSpan *sp = cc_project_spans(p, &n);
    /* bisect_right(spans, (addr, 1 << 62, "")) - 1: the last span with address <= addr */
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (sp[mid].addr <= addr) lo = mid + 1;
        else hi = mid;
    }
    for (i64 i = (i64)lo - 1; i >= 0; i--) {
        const CcSpan *s = &sp[i];
        if (addr == s->addr || (s->size && addr < s->addr + s->size))
            return addr == s->addr ? s->name : S(avp_sprintf(a, "%s+0x%llX", s->name.p, (unsigned long long)(addr - s->addr)));
        if (addr - s->addr > 0x10000) break;
    }
    return S(h08(a, addr));
}

/* ---- context ---------------------------------------------------------------------------------------------------- */

const char *cc_status_name(CcStatus s) { return s == CC_MATCH ? "match" : s == CC_MISMATCH ? "mismatch" : "unresolved"; }

CcContext *cc_context_new(CcProject *p, AvpStr unit) {
    avp_mutex_lock(&p->lock);
    CcContext *c = AVP_NEW(p->arena, CcContext);
    c->unit = dupz(p->arena, unit);
    avp_mutex_unlock(&p->lock);
    c->project = p;
    return c;
}

static const CcUnitSecs *ctx_sections(CcContext *c) {
    if (!c->secs_loaded) {
        if (c->unit.n) c->secs = cc_project_unit_sections(c->project, c->unit);
        c->secs_loaded = true;
    }
    return &c->secs;
}

static const CcSecRange *ctx_section(CcContext *c, AvpStr name) {
    const CcUnitSecs *s = ctx_sections(c);
    for (size_t k = 0; k < s->n; k++)
        if (avp_str_eq(s->v[k].name, name)) return &s->v[k];
    return NULL;
}

static bool retail_address(AvpArena *a, CcContext *c, const CcRef *ref, i64 *ra, AvpStr *why) {
    const AvpI64Vec *cands = cc_project_addresses(c->project, ref->name);
    if (cands && cands->n) {
        i64 pick = cands->v[0];
        if (cands->n > 1) {
            const CcUnitSecs *s = ctx_sections(c);
            size_t inside = 0;
            for (size_t k = 0; k < cands->n; k++) {
                bool in = false;
                for (size_t q = 0; q < s->n && !in; q++) in = s->v[q].start <= cands->v[k] && cands->v[k] < s->v[q].end;
                if (in) inside++, pick = cands->v[k];
            }
            if (inside != 1) {
                *why = S(avp_sprintf(a, "ambiguous retail name %s", ref->name.p));
                return false;
            }
        }
        *ra = pick + ref->off;
        return true;
    }
    i64 emb;
    if (cc_embedded_address(ref->name, &emb)) {
        *ra = emb + ref->off;
        return true;
    }
    *why = S(avp_sprintf(a, "retail symbol %s not in symbols.txt", ref->name.p));
    return false;
}

typedef struct {
    i64 *v;
    size_t n;
} Cands;

/* compiled_addresses(elf, reloc): false when the name says nothing. */
static bool compiled_addresses(AvpArena *a, CcContext *c, const CcElf *elf, const CcReloc *reloc, Cands *out) {
    const CcSym *sym = &elf->symbols[reloc->sym];
    if (!sym->name.n || sym->type == CC_STT_SECTION || cc_is_compiler_local(sym->name)) return false;
    const AvpI64Vec *cands = cc_project_addresses(c->project, sym->name);
    if (cands && cands->n) {
        out->n = cands->n;
        out->v = AVP_NEWN(a, i64, cands->n);
        for (size_t k = 0; k < cands->n; k++) out->v[k] = cands->v[k] + reloc->addend;
        return true;
    }
    i64 emb;
    if (cc_embedded_address(sym->name, &emb)) {
        out->n = 1;
        out->v = AVP_NEWN(a, i64, 1);
        out->v[0] = emb + reloc->addend;
        return true;
    }
    return false;
}

static bool cands_has(const Cands *c, i64 v) {
    for (size_t k = 0; k < c->n; k++)
        if (c->v[k] == v) return true;
    return false;
}

/* ", ".join("0x%08X" % c for c in cands[:3]) */
static const char *cands_text(AvpArena *a, const Cands *c) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t k = 0; k < c->n && k < 3; k++) {
        if (k) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, h08(a, c->v[k]));
    }
    return avp_buf_cz(&b);
}

static bool layout_address(CcContext *c, const CcElf *elf, const CcSym *sym, i64 addend, i64 *out) {
    const CcSecRange *r = ctx_section(c, cc_elf_section_name(elf, sym->shndx));
    if (!r) return false;
    *out = r->start + sym->value + addend;
    return true;
}

static const char *or_text(const CcSym *sym, const char *dflt) { return sym->name.n ? sym->name.p : dflt; }

static CcStatus layout_verdict(AvpArena *a, bool has_lay, i64 lay, i64 ra, AvpStr secname, const CcSym *sym,
                               AvpStr *detail) {
    if (!has_lay) {
        *detail = S(avp_sprintf(a, "no layout record for %s (%s) in splits.txt", secname.p, or_text(sym, "section symbol")));
        return CC_UNRESOLVED;
    }
    if (lay == ra) {
        *detail = AVP_S("layout");
        return CC_MATCH;
    }
    *detail = S(avp_sprintf(a, "layout: %s in %s would sit at %s, retail target is %s", or_text(sym, "section symbol"),
                            secname.p, h08(a, lay), h08(a, ra)));
    return CC_MISMATCH;
}

static CcStatus data_check(AvpArena *a, CcContext *c, const CcFn *cfn, const CcFn *rfn, const CcSym *sym, i64 addend,
                           i64 ra, int depth, AvpStr *detail);

static CcStatus pointer_check(AvpArena *a, CcContext *c, const CcElf *elf, const CcReloc *rel, i64 retail_word,
                              const CcFn *rfn, const CcFn *cfn, int depth, AvpStr *detail) {
    const CcSym *sym = &elf->symbols[rel->sym];
    bool own_code = sym->shndx && cfn->has_shndx && cfn->shndx == sym->shndx &&
                    (sym->type == CC_STT_SECTION || (sym->type == CC_STT_FUNC && avp_str_eq(sym->name, cfn->name)));
    if (own_code) {
        i64 want = retail_word - rfn->start, got = (i64)sym->value + rel->addend - cfn->start;
        if (want == got) {
            *detail = AVP_S("");
            return CC_MATCH;
        }
        *detail = S(avp_sprintf(a, "points to function offset %s, retail %s is offset %s", sx(a, got), h08(a, retail_word),
                                sx(a, want)));
        return CC_MISMATCH;
    }
    Cands cands;
    if (compiled_addresses(a, c, elf, rel, &cands)) {
        if (cands_has(&cands, retail_word)) {
            *detail = AVP_S("");
            return CC_MATCH;
        }
        *detail = S(avp_sprintf(a, "points to %s%s (%s), retail %s is %s", sym->name.p, sx(a, rel->addend),
                                cands_text(a, &cands), h08(a, retail_word), cc_project_describe(a, c->project, retail_word).p));
        return CC_MISMATCH;
    }
    if (depth < 4 && 0 < sym->shndx && sym->shndx < elf->nsections)
        return data_check(a, c, cfn, rfn, sym, rel->addend, retail_word, depth + 1, detail);
    *detail = S(avp_sprintf(a, "pointer to %s cannot be resolved", or_text(sym, "(section)")));
    return CC_UNRESOLVED;
}

static CcStatus data_check(AvpArena *a, CcContext *c, const CcFn *cfn, const CcFn *rfn, const CcSym *sym, i64 addend,
                           i64 ra, int depth, AvpStr *detail) {
    const CcElf *elf = cfn->elf;
    const CcSection *sec = 0 < sym->shndx && sym->shndx < elf->nsections ? &elf->sections[sym->shndx] : NULL;
    if (!sec) {
        *detail = S(avp_sprintf(a, "compiler-local symbol %s is not defined in the object", or_text(sym, "(section)")));
        return CC_UNRESOLVED;
    }
    i64 off = (i64)sym->value + addend;
    i64 lay = 0;
    bool has_lay = layout_address(c, elf, sym, addend, &lay);
    const CcDol *dol = cc_project_dol(c->project);
    if (sec->type == CC_SHT_NOBITS) return layout_verdict(a, has_lay, lay, ra, sec->name, sym, detail);
    i64 n = -1;
    const char *how = "";
    if (starts(sym->name, "@stringBase")) {
        i64 end = avp_str_find(sec->data, AVP_S("\0"), off, AVP_END);
        if (end < 0) {
            *detail = S(avp_sprintf(a, "unterminated string in %s", sec->name.p));
            return CC_UNRESOLVED;
        }
        n = end - off + 1;
        how = "string";
    } else if (sym->size && sym->type == CC_STT_OBJECT && addend < (i64)sym->size) {
        n = (i64)sym->size - addend;
        how = "object";
    }
    if (n < 0 || !dol) return layout_verdict(a, has_lay, lay, ra, sec->name, sym, detail);
    AvpStr want;
    if (!cc_dol_read(dol, ra, n, &want)) {
        if (cc_dol_in_bss(dol, ra))
            *detail = S(avp_sprintf(a, "retail target %s is bss, compiled %s has initial bytes", h08(a, ra), sym->name.p));
        else
            *detail = S(avp_sprintf(a, "retail target %s lies outside the image", h08(a, ra)));
        return CC_UNRESOLVED;
    }
    AvpStr got = avp_str_slice(sec->data, off, off + n);
    if ((i64)got.n != n) {
        *detail = S(avp_sprintf(a, "compiled data of %s is short", sym->name.p));
        return CC_UNRESOLVED;
    }
    bool *masked = avp_alloc(a, (size_t)n + 1);
    const char *first_unresolved = NULL;
    for (size_t k = 0; k < sec->nrelocs; k++) {
        const CcReloc *rel = &sec->relocs[k];
        i64 roff = rel->offset;
        if (!(off <= roff && roff < off + n)) continue;
        for (i64 q = roff - off; q < roff - off + 4; q++)
            if (q < n) masked[q] = true;
        AvpStr wb = avp_str_slice(want, roff - off, roff - off + 4);
        i64 word = 0;
        for (size_t q = 0; q < wb.n; q++) word = (word << 8) | (u8)wb.p[q];
        AvpStr pd;
        CcStatus st = pointer_check(a, c, elf, rel, word, rfn, cfn, depth, &pd);
        if (st == CC_MISMATCH) {
            *detail = S(avp_sprintf(a, "%s %s%s: pointer at +%s %s", how, sym->name.p, sx(a, addend), hx(a, roff - off), pd.p));
            return CC_MISMATCH;
        }
        if (st == CC_UNRESOLVED && !first_unresolved) first_unresolved = pd.p;
    }
    for (i64 i = 0; i < n; i++) {
        if (!masked[i] && got.p[i] != want.p[i]) {
            *detail = S(avp_sprintf(a, "%s %s%s differs from retail %s: compiled %s, retail %s", how, sym->name.p,
                                    sx(a, addend), h08(a, ra), hex16(a, got), hex16(a, want)));
            return CC_MISMATCH;
        }
    }
    if (first_unresolved) {
        *detail = S(avp_sprintf(a, "%s %s: %s", how, sym->name.p, first_unresolved));
        return CC_UNRESOLVED;
    }
    *detail = AVP_S("content");
    return CC_MATCH;
}

/* ---- the context's methods, exported for tools/eqcheck.py's relocation resolver (thin wrappers) ---------------- */

const CcUnitSecs *cc_context_sections(CcContext *c) { return ctx_sections(c); }

bool cc_context_retail_address(AvpArena *a, CcContext *c, const CcRef *ref, i64 *addr, AvpStr *why) {
    AvpStr dummy;
    return retail_address(a, c, ref, addr, why ? why : &dummy);
}

bool cc_context_compiled_addresses(AvpArena *a, CcContext *c, const CcElf *elf, const CcReloc *reloc, i64 **cands,
                                   size_t *n) {
    Cands cs = {0};
    if (!compiled_addresses(a, c, elf, reloc, &cs)) return false;
    *cands = cs.v;
    *n = cs.n;
    return true;
}

bool cc_context_layout_address(CcContext *c, const CcElf *elf, const CcSym *sym, i64 addend, i64 *out) {
    return layout_address(c, elf, sym, addend, out);
}

CcStatus cc_context_data_check(AvpArena *a, CcContext *c, const CcFn *cfn, const CcFn *rfn, const CcSym *sym,
                               i64 addend, i64 ra, AvpStr *detail) {
    AvpStr dummy;
    return data_check(a, c, cfn, rfn, sym, addend, ra, 0, detail ? detail : &dummy);
}

/* -?(?:0x[0-9A-Fa-f]+|\d+), then int(s, 0): false where the regex fails (or int() would raise) */
static bool parse_imm(AvpStr s, i64 *v) {
    size_t p = 0;
    bool neg = false;
    if (p < s.n && s.p[p] == '-') neg = true, p++;
    i64 x = 0;
    if (s.n - p > 2 && s.p[p] == '0' && s.p[p + 1] == 'x' && all(s, p + 2, s.n, is_hex)) {
        x = hex_of(s.p + p + 2, s.n - p - 2);
    } else if (all(s, p, s.n, is_digit)) {
        if (s.n - p > 1 && s.p[p] == '0') {
            /* int("012", 0) raises (no proof here); "00" is 0 */
            for (size_t k = p; k < s.n; k++)
                if (s.p[k] != '0') return false;
        }
        for (size_t k = p; k < s.n; k++) x = x * 10 + (s.p[k] - '0');
    } else {
        return false;
    }
    *v = neg ? -x : x;
    return true;
}

bool cc_context_raw_operand_check(AvpArena *a, CcContext *c, const CcRow *r, const CcRow *cr, const CcFn *rfn,
                                  const CcFn *cfn, AvpStr *proof) {
    const CcReloc *reloc = cr->reloc;
    const char *kind = NULL;
    if (reloc) kind = reloc->type == 4 ? "l" : reloc->type == 5 ? "h" : reloc->type == 6 ? "ha" : NULL;
    const CcElf *elf = cfn->elf;
    if (!kind || !elf || r->has_ref || r->has_label) return false;
    if (reloc->sym >= elf->nsymbols) return false;
    const CcSym *sym = &elf->symbols[reloc->sym];
    if (!sym->name.n || !cc_is_compiler_local(sym->name) || !(0 < sym->shndx && sym->shndx < elf->nsections)) return false;
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    CcRow re = cc_parse_row(avp_scratch(), cr->addr, cr->text);
    AvpStr sep = S(avp_sprintf(avp_scratch(), "SYM@%s", kind));
    bool ok = avp_str_count(re.norm, sep) == 1;
    AvpStr p0 = {0}, p1 = {0}, mid = {0};
    if (ok) {
        avp_str_partition(re.norm, sep, &p0, &p1);
        ok = r->norm.n >= p0.n + p1.n && avp_str_startswith(r->norm, p0) && avp_str_endswith(r->norm, p1);
    }
    i64 imm = 0;
    if (ok) {
        mid = avp_strn(r->norm.p + p0.n, r->norm.n - p0.n - p1.n);
        ok = parse_imm(mid, &imm);
    }
    i64 addr = 0;
    if (ok) ok = layout_address(c, elf, sym, reloc->addend, &addr);
    if (ok) {
        i64 half = kind[0] == 'l' ? (addr & 0xFFFF) : kind[1] == 0 ? ((addr >> 16) & 0xFFFF) : (((addr + 0x8000) >> 16) & 0xFFFF);
        ok = half == (imm & 0xFFFF);
    }
    if (ok) {
        AvpStr how;
        ok = data_check(a, c, cfn, rfn, sym, reloc->addend, addr, 0, &how) == CC_MATCH && avp_str_eqz(how, "content");
    }
    if (ok)
        *proof = S(avp_sprintf(a, "raw operand %.*s is %s@%s: by layout the literal sits at %s, where retail holds its bytes",
                               AVP_SF(mid), sym->name.p, kind, h08(a, addr)));
    avp_arena_reset(avp_scratch(), mk);
    return ok;
}

CcStatus cc_context_check(AvpArena *a, CcContext *c, const CcRow *r, const CcRow *cr, const CcFn *rfn, const CcFn *cfn,
                          AvpStr *detail) {
    AVP_UNUSED(rfn);
    if (!r->has_ref && !cr->reloc) {
        *detail = AVP_S("");
        return CC_MATCH;
    }
    if (!cr->reloc) {
        *detail = AVP_S("compiled row has no ELF relocation");
        return CC_UNRESOLVED;
    }
    if (!r->has_ref) {
        *detail = AVP_S("retail row has no symbolic operand");
        return CC_UNRESOLVED;
    }
    const CcElf *elf = cfn->elf;
    const CcReloc *reloc = cr->reloc;
    /* the Python raised IndexError here (never seen: mwcceppc writes valid symbol indices) */
    AVP_ASSERT(reloc->sym < elf->nsymbols);
    const CcSym *sym = &elf->symbols[reloc->sym];
    if (sym->name.n && sym->type != CC_STT_SECTION && avp_str_eq(sym->name, r->ref.name) && !cc_is_compiler_local(sym->name)) {
        if (reloc->addend == r->ref.off) {
            *detail = AVP_S("same name");
            return CC_MATCH;
        }
        *detail = S(avp_sprintf(a, "%s: retail offset %s, compiled addend %s", sym->name.p, sx(a, r->ref.off), sx(a, reloc->addend)));
        return CC_MISMATCH;
    }
    i64 ra = 0;
    AvpStr why = {0};
    bool has_ra = retail_address(a, c, &r->ref, &ra, &why);
    Cands cands;
    if (compiled_addresses(a, c, elf, reloc, &cands)) {
        if (!has_ra) {
            *detail = why;
            return CC_UNRESOLVED;
        }
        if (cands_has(&cands, ra)) {
            *detail = AVP_S("");
            return CC_MATCH;
        }
        *detail = S(avp_sprintf(a, "retail %s (%s) != compiled %s%s (%s)", cc_project_describe(a, c->project, ra).p, h08(a, ra),
                                sym->name.p, reloc->addend ? sx(a, reloc->addend) : "", cands_text(a, &cands)));
        return CC_MISMATCH;
    }
    if (!has_ra) {
        *detail = why;
        return CC_UNRESOLVED;
    }
    if (!sym->name.n && sym->shndx == 0) {
        *detail = AVP_S("relocation against the null symbol");
        return CC_UNRESOLVED;
    }
    if (sym->shndx == 0) {
        *detail = S(avp_sprintf(a, "compiled symbol %s is not in symbols.txt", sym->name.p));
        return CC_UNRESOLVED;
    }
    return data_check(a, c, cfn, rfn, sym, reloc->addend, ra, 0, detail);
}

/* ---- comparison ------------------------------------------------------------------------------------------------- */

bool cc_fn_result_exact(const CcFnResult *r) { return r->edit == 0 && r->npositional == 0 && r->targets.n == 0; }

static size_t edit_of(AvpSeqMatcher *m) {
    size_t n, e = 0;
    const AvpOpcode *ops = avp_seqm_opcodes(m, &n);
    for (size_t k = 0; k < n; k++)
        if (ops[k].tag != AVP_OP_EQUAL) e += AVP_MAX(ops[k].i2 - ops[k].i1, ops[k].j2 - ops[k].j1);
    return e;
}

size_t cc_edit_count(const AvpStr *want, size_t nw, const AvpStr *got, size_t ng) {
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    size_t e = edit_of(avp_seqm_new_str(avp_scratch(), want, nw, got, ng, &avp_seqm_no_autojunk));
    avp_arena_reset(avp_scratch(), mk);
    return e;
}

typedef AVP_VEC(CcTargetDiff) TdVec;

static void td_push(TdVec *v, size_t i, size_t j, const CcRow *r, const CcRow *c, AvpStr detail, const char *kind) {
    CcTargetDiff t = {i, j, r->text, c->text, detail, kind};
    avp_vec_push(v, t);
}

static CcTdList td_freeze(AvpArena *a, TdVec *v) {
    CcTdList l = {AVP_NEWN(a, CcTargetDiff, v->n ? v->n : 1), v->n};
    if (v->n) memcpy(l.v, v->v, v->n * sizeof(CcTargetDiff));
    avp_vec_free(v);
    return l;
}

/* _equate_raw_operands: give a compiled row the retail row's norm where a value proof holds (equal-length
 * replaced runs only). */
static void equate_raw_operands(AvpArena *a, const CcFn *rfn, CcFn *cfn, CcContext *ctx) {
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpStr *want = AVP_NEWN(avp_scratch(), AvpStr, rfn->nrows + 1), *got = AVP_NEWN(avp_scratch(), AvpStr, cfn->nrows + 1);
    cc_fn_norms(rfn, want);
    cc_fn_norms(cfn, got);
    AvpSeqMatcher *m = avp_seqm_new_str(avp_scratch(), want, rfn->nrows, got, cfn->nrows, &avp_seqm_no_autojunk);
    size_t n;
    const AvpOpcode *ops = avp_seqm_opcodes(m, &n);
    for (size_t k = 0; k < n; k++) {
        if (ops[k].tag != AVP_OP_REPLACE || ops[k].i2 - ops[k].i1 != ops[k].j2 - ops[k].j1) continue;
        for (size_t q = 0; q < ops[k].i2 - ops[k].i1; q++) {
            const CcRow *r = &rfn->rows[ops[k].i1 + q];
            CcRow *c = &cfn->rows[ops[k].j1 + q];
            AvpStr proof;
            if (!r->has_ref && c->reloc && cc_context_raw_operand_check(a, ctx, r, c, rfn, cfn, &proof)) c->norm = r->norm;
        }
    }
    avp_arena_reset(avp_scratch(), mk);
}

CcFnResult *cc_compare_function(AvpArena *a, const CcFn *rfn, CcFn *cfn, CcContext *ctx, bool targets) {
    bool tgt = targets && ctx;
    if (tgt) equate_raw_operands(a, rfn, cfn, ctx);
    size_t nw = rfn->nrows, ng = cfn->nrows;
    CcFnResult *res = AVP_NEW(a, CcFnResult);
    res->name = rfn->name;
    res->rows = AVP_MAX(nw, ng);
    AvpStr *want = AVP_NEWN(a, AvpStr, nw + 1), *got = AVP_NEWN(a, AvpStr, ng + 1);
    cc_fn_norms(rfn, want);
    cc_fn_norms(cfn, got);
    AVP_VEC(CcPositional) pos = {0};
    size_t mn = AVP_MIN(nw, ng);
    for (size_t i = 0; i < mn; i++)
        if (!avp_str_eq(want[i], got[i])) {
            CcPositional p = {i, want[i], got[i]};
            avp_vec_push(&pos, p);
        }
    for (size_t i = mn; i < res->rows; i++) {
        CcPositional p = {i, i < nw ? want[i] : AVP_S(""), i < ng ? got[i] : AVP_S("")};
        avp_vec_push(&pos, p);
    }
    res->npositional = pos.n;
    res->positional = AVP_NEWN(a, CcPositional, pos.n ? pos.n : 1);
    if (pos.n) memcpy(res->positional, pos.v, pos.n * sizeof(CcPositional));
    avp_vec_free(&pos);
    AvpArenaMark mk = avp_arena_mark(avp_scratch());
    AvpSeqMatcher *m = avp_seqm_new_str(avp_scratch(), want, nw, got, ng, &avp_seqm_no_autojunk);
    res->edit = edit_of(m);
    TdVec tdiffs = {0}, unres = {0}, notes = {0};
    size_t checked = 0;
    if (tgt) {
        i64 *pair = AVP_NEWN(avp_scratch(), i64, nw + 1), *back = AVP_NEWN(avp_scratch(), i64, ng + 1);
        for (size_t i = 0; i < nw; i++) pair[i] = -1;
        for (size_t j = 0; j < ng; j++) back[j] = -1;
        size_t n;
        const AvpOpcode *ops = avp_seqm_opcodes(m, &n);
        for (size_t k = 0; k < n; k++)
            if (ops[k].tag == AVP_OP_EQUAL)
                for (size_t q = 0; q < ops[k].i2 - ops[k].i1; q++) {
                    pair[ops[k].i1 + q] = (i64)(ops[k].j1 + q);
                    back[ops[k].j1 + q] = (i64)(ops[k].i1 + q);
                }
        for (size_t i = 0; i < nw; i++) {
            if (pair[i] < 0) continue;
            size_t j = (size_t)pair[i];
            const CcRow *r = &rfn->rows[i];
            const CcRow *c = &cfn->rows[j];
            if (r->has_label || c->has_label) {
                if (r->has_label && c->has_label && r->bidx >= 0 && c->bidx >= 0 && pair[r->bidx] >= 0 && back[c->bidx] >= 0) {
                    checked++;
                    if (pair[r->bidx] != c->bidx)
                        td_push(&tdiffs, i, j, r, c,
                                S(avp_sprintf(a, "branches to retail row %lld, compiled row %lld is retail row %lld",
                                              (long long)r->bidx, (long long)c->bidx, (long long)back[c->bidx])),
                                "branch");
                }
                continue;
            }
            if (!r->has_ref && !c->reloc) {
                if (r->stray || c->stray)
                    td_push(&unres, i, j, r, c, AVP_S("operand with a symbol-like name not parsed"), "stray");
                continue;
            }
            checked++;
            if (!r->has_ref && c->reloc) {
                AvpStr proof;
                if (cc_context_raw_operand_check(a, ctx, r, c, rfn, cfn, &proof)) {
                    td_push(&notes, i, j, r, c, proof, "value");
                    continue;
                }
            }
            AvpStr detail;
            CcStatus st = cc_context_check(a, ctx, r, c, rfn, cfn, &detail);
            if (st == CC_MISMATCH) td_push(&tdiffs, i, j, r, c, detail, "target");
            else if (st == CC_UNRESOLVED) td_push(&unres, i, j, r, c, detail, "unresolved");
        }
    }
    avp_arena_reset(avp_scratch(), mk);
    res->targets = td_freeze(a, &tdiffs);
    res->unresolved = td_freeze(a, &unres);
    res->notes = td_freeze(a, &notes);
    res->checked_targets = checked;
    return res;
}

AvpStr cc_retail_name_for(AvpStr cname, const AvpMap *retail) {
    if (avp_map_has(retail, cname)) return cname;
    if (cname.n >= 11 && memcmp(cname.p, "fn_", 3) == 0 && all(cname, 3, 11, is_uhex)) {
        AvpStr pre = avp_strn(cname.p, 11);
        AvpMapIter it = {0};
        while (avp_map_next(retail, &it))
            if (avp_str_eq(it.key, pre)) return it.key;
        it.pos = 0;
        while (avp_map_next(retail, &it))
            if (avp_str_startswith(it.key, pre)) return it.key;
    }
    AvpStr none = {NULL, 0};
    return none;
}

AvpMap *cc_codecmp_object(AvpArena *a, const AvpMap *retail, const AvpMap *compiled, CcContext *ctx, bool targets,
                          const AvpMap *names) {
    AvpMap *out = avp_map_new();
    AvpMapIter it = {0};
    while (avp_map_next(compiled, &it)) {
        AvpStr rname = {NULL, 0};
        AvpVal *ov = names ? avp_map_get(names, it.key) : NULL;
        if (ov && ov->p && ((AvpStr *)ov->p)->n) rname = *(AvpStr *)ov->p;
        else rname = cc_retail_name_for(it.key, retail);
        if (!rname.p) continue;
        AvpVal *rv = avp_map_get(retail, rname);
        AVP_ASSERT(rv); /* the Python raised KeyError for an override naming no retail function */
        avp_map_put(out, it.key, NULL)->p = cc_compare_function(a, rv->p, it.val->p, ctx, targets);
    }
    return out;
}
