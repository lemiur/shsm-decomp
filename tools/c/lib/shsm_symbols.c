/* symbols.txt and splits.txt (tools/common/symbols.py). */
#include "shsm_symbols.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/sort.h"
#include "shsm_cli.h"
#include "shsm_text.h"

#include <stdio.h>

/* int(text, base) as Python: SHSM_EXC_VALUE "invalid literal for int() with base N: '...'" on failure. */
static i64 py_int(AvpStr text, int base) {
    i64 v = 0;
    if (!avp_parse_int(text, base, &v, NULL)) {
        AvpStr r = avp_repr_str(avp_arena(), text);
        shsm_raise(SHSM_EXC_VALUE, "invalid literal for int() with base %d: %.*s", base, AVP_SF(r));
    }
    return v;
}

static bool is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

AvpStr shsm_symbol_attr(const ShsmSymbol *s, const char *key) {
    for (size_t i = s->nattrs; i-- > 0;)
        if (avp_str_eqz(s->attrs[i].key, key)) return s->attrs[i].value;
    return (AvpStr){NULL, 0};
}

/* _LINE = ^(\S+) = ([^:\s]+):0x([0-9A-Fa-f]+);\s*(?://\s*(.*))?$ */
bool shsm_parse_symbol_line(AvpArena *a, AvpStr line, size_t lineno, ShsmSymbol *out) {
    line = avp_str_rstrip_chars(line, AVP_S("\r\n"));
    size_t i = 0, n = line.n;
    const char *p = line.p;
    while (i < n && !shsm_ws_len(line, i)) i++;
    if (i == 0) return false;
    size_t name_end = i;
    if (!(i + 3 <= n && p[i] == ' ' && p[i + 1] == '=' && p[i + 2] == ' ')) return false;
    i += 3;
    size_t sec_start = i;
    while (i < n && p[i] != ':' && !shsm_ws_len(line, i)) i++;
    if (i == sec_start) return false;
    size_t sec_end = i;
    if (!(i + 3 <= n && p[i] == ':' && p[i + 1] == '0' && p[i + 2] == 'x')) return false;
    i += 3;
    size_t addr_start = i;
    while (i < n && is_hex(p[i])) i++;
    if (i == addr_start || i >= n || p[i] != ';') return false;
    size_t addr_end = i;
    i = shsm_skip_ws(line, i + 1);
    AvpStr attrs_text = {0};
    if (i < n) {
        if (!(i + 2 <= n && p[i] == '/' && p[i + 1] == '/')) return false;
        attrs_text = avp_str_slice(line, (i64)shsm_skip_ws(line, i + 2), AVP_END);
    }
    memset(out, 0, sizeof *out);
    out->name = avp_cz(a, avp_strn(p, name_end));
    out->section = avp_cz(a, avp_str_slice(line, (i64)sec_start, (i64)sec_end));
    u64 addr = 0;
    avp_parse_uint(avp_str_slice(line, (i64)addr_start, (i64)addr_end), 16, &addr);
    out->address = addr;
    out->lineno = lineno;
    out->raw = avp_cz(a, line);
    AvpStrVec toks = attrs_text.n ? avp_str_split(attrs_text, AVP_S(""), -1) : (AvpStrVec){0};
    ShsmAttr *attrs = AVP_NEWN(a, ShsmAttr, toks.n + 1);
    AvpStr *flags = AVP_NEWN(a, AvpStr, toks.n + 1);
    size_t na = 0, nf = 0;
    bool hidden = false;
    for (size_t k = 0; k < toks.n; k++) {
        AvpStr key, val;
        if (avp_str_partition(toks.v[k], AVP_S(":"), &key, &val)) {
            attrs[na].key = avp_str(avp_cz(a, key));
            attrs[na].value = avp_str(avp_cz(a, val));
            na++;
        } else if (avp_str_eqz(toks.v[k], "hidden")) {
            hidden = true;
        } else {
            flags[nf++] = avp_str(avp_cz(a, toks.v[k]));
        }
    }
    avp_vec_free(&toks);
    out->attrs = attrs;
    out->nattrs = na;
    out->flags = flags;
    out->nflags = nf;
    out->hidden = hidden;
    AvpStr sz = shsm_symbol_attr(out, "size");
    if (sz.n) {
        out->has_size = true;
        out->size = (u64)py_int(sz, 16);
    }
    AvpStr ty = shsm_symbol_attr(out, "type");
    out->type = ty.p ? ty.p : NULL;
    AvpStr sc = shsm_symbol_attr(out, "scope");
    out->scope = sc.p ? sc.p : NULL;
    AvpStr al = shsm_symbol_attr(out, "align");
    if (al.n) {
        out->has_align = true;
        out->align = py_int(al, 0);
    }
    AvpStr da = shsm_symbol_attr(out, "data");
    out->data = da.p ? da.p : NULL;
    return true;
}

static u64 size_or0(const ShsmSymbol *s) { return s->has_size ? s->size : 0; }

static int cmp_sorted(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmSymbol *a = *(ShsmSymbol *const *)x, *b = *(ShsmSymbol *const *)y;
    if (a->address != b->address) return a->address < b->address ? -1 : 1;
    u64 sa = size_or0(a), sb = size_or0(b);
    if (sa != sb) return sa > sb ? -1 : 1; /* -(size or 0) */
    return 0;
}

ShsmSymbolTable *shsm_parse_symbols(AvpArena *a, AvpStr text, const ShsmSplits *splits) {
    ShsmSymbolTable *t = AVP_NEW(a, ShsmSymbolTable);
    AvpStrVec lines = avp_str_splitlines(text, false);
    t->symbols = AVP_NEWN(a, ShsmSymbol, lines.n + 1);
    for (size_t i = 0; i < lines.n; i++)
        if (shsm_parse_symbol_line(a, lines.v[i], i + 1, &t->symbols[t->n])) t->n++;
    avp_vec_free(&lines);
    t->splits = splits;
    t->sorted = AVP_NEWN(a, ShsmSymbol *, t->n + 1);
    t->addrs = AVP_NEWN(a, u64, t->n + 1);
    t->by_name = avp_map_new_cap(t->n);
    for (size_t i = 0; i < t->n; i++) {
        ShsmSymbol *s = &t->symbols[i];
        t->sorted[i] = s;
        bool created = false;
        AvpVal *v = avp_map_put(t->by_name, avp_str(s->name), &created);
        if (created) v->p = AVP_NEW(a, AvpPtrVec);
        avp_vec_push((AvpPtrVec *)v->p, s);
        if (size_or0(s) > t->max_size) t->max_size = size_or0(s);
    }
    avp_sort(t->sorted, t->n, sizeof *t->sorted, cmp_sorted, NULL, false);
    for (size_t i = 0; i < t->n; i++) t->addrs[i] = t->sorted[i]->address;
    return t;
}

/* os.stat then open(encoding="utf-8").read(), raising what the Python raised. */
static AvpStr read_cached_text(AvpArena *a, const char *path) {
    AvpStat st;
    if (!avp_stat(path, &st)) shsm_raise_io();
    AvpStr text;
    if (!avp_read_text(a, path, AVP_TM_UTF8, &text)) shsm_raise_io();
    return text;
}

ShsmSymbolTable *shsm_load_symbols(const ShsmRepo *r, bool with_splits) {
    AvpArena *a = avp_arena();
    ShsmSymbolTable *t = shsm_parse_symbols(a, read_cached_text(a, r->symbols_txt), NULL);
    if (with_splits && avp_exists(r->splits_txt)) t->splits = shsm_load_splits(r);
    return t;
}

const ShsmSymbol *shsm_sym_by_name(const ShsmSymbolTable *t, AvpStr name) {
    AvpVal *v = avp_map_get(t->by_name, name);
    return v ? (const ShsmSymbol *)((AvpPtrVec *)v->p)->v[0] : NULL;
}

size_t shsm_sym_all_by_name(const ShsmSymbolTable *t, AvpStr name, AvpPtrVec *out) {
    AvpVal *v = avp_map_get(t->by_name, name);
    if (!v) return 0;
    AvpPtrVec *lst = (AvpPtrVec *)v->p;
    avp_vec_extend(out, lst->v, lst->n);
    return lst->n;
}

static int cmp_at(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmSymbol *a = *(const ShsmSymbol *const *)x, *b = *(const ShsmSymbol *const *)y;
    bool la = a->type && strcmp(a->type, "label") == 0, lb = b->type && strcmp(b->type, "label") == 0;
    if (la != lb) return la ? 1 : -1;
    u64 sa = size_or0(a), sb = size_or0(b);
    if (sa != sb) return sa > sb ? -1 : 1;
    return 0;
}

static int cmp_ptr(const void *x, const void *y, void *ctx) {
    (void)ctx;
    uintptr_t a = (uintptr_t)*(void *const *)x, b = (uintptr_t)*(void *const *)y;
    return a < b ? -1 : a > b;
}

size_t shsm_sym_at(const ShsmSymbolTable *t, u64 address, const char *section, AvpPtrVec *out) {
    size_t lo = avp_bisect_left_u64(t->addrs, t->n, address), hi = avp_bisect_right_u64(t->addrs, t->n, address);
    size_t first = out->n;
    for (size_t i = lo; i < hi; i++)
        if (!section || strcmp(t->sorted[i]->section, section) == 0) avp_vec_push(out, t->sorted[i]);
    /* Python filters _by_addr (file order), then sorts stably */
    avp_sort(out->v + first, out->n - first, sizeof(void *), cmp_ptr, NULL, false);
    avp_sort(out->v + first, out->n - first, sizeof(void *), cmp_at, NULL, false);
    return out->n - first;
}

const ShsmSymbol *shsm_sym_first_at(const ShsmSymbolTable *t, u64 address, const char *section) {
    AvpPtrVec v = {0};
    const ShsmSymbol *r = shsm_sym_at(t, address, section, &v) ? (const ShsmSymbol *)v.v[0] : NULL;
    avp_vec_free(&v);
    return r;
}

const ShsmSymbol *shsm_sym_containing(const ShsmSymbolTable *t, u64 address) {
    size_t i = avp_bisect_right_u64(t->addrs, t->n, address);
    const ShsmSymbol *best = NULL;
    while (i-- > 0) {
        const ShsmSymbol *s = t->sorted[i];
        if (address - s->address > t->max_size) break;
        if (s->has_size && s->size && address < s->address + s->size && (!best || s->size < best->size)) best = s;
    }
    return best;
}

const char *shsm_sym_describe(AvpArena *a, const ShsmSymbolTable *t, u64 address) {
    const ShsmSymbol *e = shsm_sym_first_at(t, address, NULL);
    if (e) return e->name;
    const ShsmSymbol *s = shsm_sym_containing(t, address);
    if (s) return avp_sprintf(a, "%s+0x%llX", s->name, (unsigned long long)(address - s->address));
    return avp_sprintf(a, "0x%08llX", (unsigned long long)address);
}

size_t shsm_sym_in_section(const ShsmSymbolTable *t, const char *section, AvpPtrVec *out) {
    size_t before = out->n;
    for (size_t i = 0; i < t->n; i++)
        if (strcmp(t->sorted[i]->section, section) == 0) avp_vec_push(out, t->sorted[i]);
    return out->n - before;
}

size_t shsm_sym_functions(const ShsmSymbolTable *t, AvpPtrVec *out) {
    size_t before = out->n;
    for (size_t i = 0; i < t->n; i++)
        if (t->sorted[i]->type && strcmp(t->sorted[i]->type, "function") == 0) avp_vec_push(out, t->sorted[i]);
    return out->n - before;
}

ShsmSymbol *const *shsm_sym_in_range(const ShsmSymbolTable *t, u64 start, u64 end, size_t *count) {
    size_t lo = avp_bisect_left_u64(t->addrs, t->n, start), hi = avp_bisect_left_u64(t->addrs, t->n, end);
    *count = hi > lo ? hi - lo : 0;
    return t->sorted + lo;
}

const char *shsm_sym_unit_of(const ShsmSymbolTable *t, u64 address) {
    return t->splits ? shsm_splits_unit_of(t->splits, address) : NULL;
}

/* ---- splits ---- */

/* dict[key] = value over an attribute array: replaces in place or appends. */
static void attr_set(ShsmAttr **v, size_t *n, size_t *cap, AvpStr key, AvpStr value) {
    for (size_t i = 0; i < *n; i++) {
        if (avp_str_eq((*v)[i].key, key)) {
            (*v)[i].value = value;
            return;
        }
    }
    avp_vec__grow((void **)v, cap, *n + 1, sizeof **v);
    (*v)[*n].key = key;
    (*v)[*n].value = value;
    (*n)++;
}

/* dict(t.split(":", 1) for t in toks if ":" in t) */
static void attrs_from(AvpArena *a, const AvpStr *toks, size_t n, ShsmAttr **v, size_t *nv, size_t *cap) {
    *v = NULL;
    *nv = *cap = 0;
    for (size_t i = 0; i < n; i++) {
        AvpStr k, val;
        if (avp_str_partition(toks[i], AVP_S(":"), &k, &val))
            attr_set(v, nv, cap, avp_str(avp_cz(a, k)), avp_str(avp_cz(a, val)));
    }
}

static int cmp_span(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmSpan *a = x, *b = y;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    if (a->end != b->end) return a->end < b->end ? -1 : 1;
    int c = strcmp(a->unit, b->unit);
    if (c) return c;
    return strcmp(a->section, b->section);
}

ShsmSplits *shsm_parse_splits(AvpArena *a, AvpStr text) {
    ShsmSplits *s = AVP_NEW(a, ShsmSplits);
    AVP_VEC(ShsmSplitSection) sections = {0};
    AVP_VEC(ShsmSplitUnit) units = {0};
    s->unit_index = avp_map_new();
    AvpMap *sec_index = avp_map_new();
    i64 cur = -1;
    bool in_sections = false;
    AvpStrVec lines = avp_str_splitlines(text, false);
    for (size_t li = 0; li < lines.n; li++) {
        AvpStr line = lines.v[li];
        if (!avp_str_strip(line).n || avp_str_startswith(avp_str_lstrip(line), AVP_S("//"))) continue;
        if (!shsm_ws_len(line, 0)) {
            AvpStr head = avp_str_strip(line);
            in_sections = avp_str_eqz(head, "Sections:");
            cur = -1;
            if (!in_sections) {
                AvpStr name, rest;
                avp_str_partition(head, AVP_S(":"), &name, &rest);
                const char *path = shsm_unit_path(a, avp_cz(a, name));
                bool created = false;
                AvpVal *v = avp_map_put(s->unit_index, avp_str(path), &created);
                if (created) {
                    v->i = (i64)units.n;
                    ShsmSplitUnit *u = avp_vec_push_zero(&units);
                    u->path = path;
                }
                cur = v->i;
                ShsmSplitUnit *u = &units.v[cur];
                u->nranges = 0; /* units[cur] = {} */
                u->file = avp_cz(a, name);
                AvpStrVec toks = avp_str_split(rest, AVP_S(""), -1);
                attrs_from(a, toks.v, toks.n, &u->attrs, &u->nattrs, &u->attrs_cap);
                avp_vec_free(&toks);
            }
            continue;
        }
        AvpStrVec toks = avp_str_split(line, AVP_S(""), -1);
        if (in_sections) {
            const char *name = avp_cz(a, toks.v[0]);
            bool created = false;
            AvpVal *v = avp_map_put(sec_index, avp_str(name), &created);
            if (created) {
                v->i = (i64)sections.n;
                avp_vec_push_zero(&sections)->name = name;
            }
            ShsmSplitSection *sec = &sections.v[v->i];
            attrs_from(a, toks.v + 1, toks.n - 1, &sec->attrs, &sec->nattrs, &sec->attrs_cap);
        } else if (cur >= 0) {
            ShsmAttr *kv = NULL;
            size_t nkv = 0, cap = 0;
            attrs_from(a, toks.v + 1, toks.n - 1, &kv, &nkv, &cap);
            AvpStr st = {0}, en = {0};
            bool has_st = false, has_en = false;
            for (size_t k = 0; k < nkv; k++) {
                if (avp_str_eqz(kv[k].key, "start")) st = kv[k].value, has_st = true;
                if (avp_str_eqz(kv[k].key, "end")) en = kv[k].value, has_en = true;
            }
            avp_xfree(kv);
            if (has_st && has_en) {
                u64 vs = (u64)py_int(st, 16), ve = (u64)py_int(en, 16);
                ShsmSplitUnit *u = &units.v[cur];
                const char *secname = avp_cz(a, toks.v[0]);
                size_t k = 0;
                while (k < u->nranges && strcmp(u->ranges[k].section, secname) != 0) k++;
                if (k == u->nranges) {
                    avp_vec__grow((void **)&u->ranges, &u->cap, u->nranges + 1, sizeof *u->ranges);
                    u->nranges++;
                }
                u->ranges[k] = (ShsmSplitRange){secname, vs, ve};
            }
        }
        avp_vec_free(&toks);
    }
    avp_vec_free(&lines);
    avp_map_free(sec_index);
    s->sections = sections.v;
    s->nsections = sections.n;
    s->units = units.v;
    s->nunits = units.n;
    AVP_VEC(ShsmSpan) spans = {0};
    for (size_t i = 0; i < s->nunits; i++)
        for (size_t k = 0; k < s->units[i].nranges; k++) {
            const ShsmSplitRange *r = &s->units[i].ranges[k];
            avp_vec_push(&spans, ((ShsmSpan){r->start, r->end, s->units[i].path, r->section}));
        }
    avp_sort(spans.v, spans.n, sizeof *spans.v, cmp_span, NULL, false);
    s->spans = spans.v;
    s->nspans = spans.n;
    return s;
}

ShsmSplits *shsm_load_splits(const ShsmRepo *r) {
    AvpArena *a = avp_arena();
    return shsm_parse_splits(a, read_cached_text(a, r->splits_txt));
}

const ShsmSplitUnit *shsm_splits_unit(const ShsmSplits *s, const char *unit) {
    AvpVal *v = avp_map_get(s->unit_index, avp_str(shsm_unit_path(avp_arena(), unit)));
    return v ? &s->units[v->i] : NULL;
}

static int cmp_span_start(const void *probe, const void *elem, void *ctx) {
    (void)ctx;
    u64 p = *(const u64 *)probe, e = ((const ShsmSpan *)elem)->start;
    return p < e ? -1 : p > e;
}

const ShsmSpan *shsm_splits_range_of(const ShsmSplits *s, u64 address) {
    size_t i = avp_bisect_right(s->spans, s->nspans, sizeof *s->spans, &address, cmp_span_start, NULL);
    if (i == 0) return NULL;
    const ShsmSpan *sp = &s->spans[i - 1];
    return (sp->start <= address && address < sp->end) ? sp : NULL;
}

const char *shsm_splits_unit_of(const ShsmSplits *s, u64 address) {
    const ShsmSpan *sp = shsm_splits_range_of(s, address);
    return sp ? sp->unit : NULL;
}
