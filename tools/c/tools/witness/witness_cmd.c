/* witness_cmd: the Witness methods and the subcommands of tools/witness.py (header, resolve, resolve_ps2, wname,
 * ps2_label, helpers, _fmt_float, lib_matches, _pair_line, cmd_of, inlined_at, cmd_inlined, helper_table,
 * known_check, cmd_helpers, _range, order_agreement, cmd_order, cmd_ps2, compute_stats, cmd_stats). */
#include "witness_cmd.h"

#include "fragments_known.h"
#include "shsm_libraries.h"
#include "shsm_symbols.h"
#include "witness_ps2.h"

#include <stdlib.h>
#include <string.h>

typedef struct HelperInfo {
    i64 helper;
    AvpISet *sites; /* {wii} */
    i64 mapped;     /* WT_NONE = None */
} HelperInfo;

typedef struct Cx {
    AvpArena *a;
    WtWitness *w;
    WtCorr *c;
    const WtArgs *args;
    ShsmOutput *out;
    bool have_helpers;
    AVP_VEC(HelperInfo) helpers; /* Witness.helpers(), in first-row order */
    AvpIMap *helper_idx;         /* helper -> index into helpers */
} Cx;

/* ---- small Python equivalents ---- */

static void line(Cx *x, AvpStr s) { shsm_output_line(x->out, s); }

static AvpJson *jstr(AvpArena *a, AvpStr s) { return avp_json_str(a, s); }
static AvpJson *jstrz_or_null(AvpArena *a, const char *s) { return s ? avp_json_strz(a, s) : avp_json_null(); }
static AvpJson *jint(AvpArena *a, i64 v) { return avp_json_int(a, v); }
static AvpJson *jhex(AvpArena *a, const char *fmt, i64 v) { return avp_json_str(a, avp_pyfmt(a, fmt, v)); }

/* str(v) of a value read back from the meta JSON */
static AvpStr pystr(AvpArena *a, const AvpJson *v) {
    if (!v || v->kind == AVP_JSON_NULL) return AVP_S("None");
    switch (v->kind) {
    case AVP_JSON_BOOL: return v->b ? AVP_S("True") : AVP_S("False");
    case AVP_JSON_INT: return v->big ? v->u.str : avp_pyfmt(a, "%d", v->u.i);
    case AVP_JSON_FLOAT: return avp_pyfmt(a, "%v", v->u.f);
    case AVP_JSON_STRING: return v->u.str;
    default: return avp_json_dumps(a, v, NULL);
    }
}

static i64 jget_i(const AvpJson *o, const char *key) { return avp_json_i64(avp_json_getz(o, key), 0); }

/* text[:n] in code points */
static AvpStr head_cp(AvpStr s, size_t n) {
    size_t i = 0, cps = 0;
    while (i < s.n && cps < n) {
        i++;
        while (i < s.n && ((unsigned char)s.p[i] & 0xC0) == 0x80) i++;
        cps++;
    }
    return avp_strn(s.p, i);
}

static AvpStr json_dumps_str(AvpArena *a, AvpStr s) {
    AvpBuf b = {0};
    avp_json_put_string(&b, s, false);
    AvpStr r = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return r;
}

/* _fmt_float(bits): "%g" % the float32 */
static AvpStr fmt_float(AvpArena *a, i64 bits) {
    u32 u = (u32)((u64)bits & 0xFFFFFFFFu);
    float f;
    memcpy(&f, &u, sizeof f);
    return avp_pyfmt(a, "%g", (double)f);
}

static int cmp_i64(const void *x, const void *y, void *ctx) {
    (void)ctx;
    i64 a = *(const i64 *)x, b = *(const i64 *)y;
    return a < b ? -1 : a > b;
}

static AvpI64Vec pyintset_sorted(const AvpPyIntSet *s) {
    AvpI64Vec v = {0};
    if (!s) return v;
    size_t pos = 0;
    i64 m;
    while (avp_pyintset_next(s, &pos, &m)) avp_vec_push(&v, m);
    avp_sort_i64(v.v, v.n);
    return v;
}

static AvpI64Vec iset_sorted(const AvpISet *s) {
    AvpI64Vec v = {0};
    size_t pos = 0;
    u64 m;
    while (avp_iset_next(s, &pos, &m)) avp_vec_push(&v, (i64)m);
    avp_sort_i64(v.v, v.n);
    return v;
}

/* len(a - b) */
static size_t pyintset_minus_len(const AvpPyIntSet *a, const AvpPyIntSet *b) {
    size_t n = 0, pos = 0;
    i64 m;
    while (avp_pyintset_next(a, &pos, &m))
        if (!avp_pyintset_has(b, m)) n++;
    return n;
}

static bool rev(const Cx *x, i64 p, i64 *w) { return wt_corr_rev(x->c, p, w); }

static AvpI64Vec sorted_wii(const Cx *x) {
    AvpI64Vec v = {0};
    const WtSide *s = &x->w->m.wii;
    for (size_t k = 0; k < s->v.n; k++) avp_vec_push(&v, s->v.v[k]->addr);
    avp_sort_i64(v.v, v.n);
    return v;
}

/* ---- Witness methods ---- */

static AvpStr header(Cx *x) {
    const AvpJson *v = avp_json_getz(x->w->m.meta, "validation");
    AvpStr prec = AVP_S("no validation");
    if (avp_json_truthy(avp_json_getz(v, "held_out"))) {
        AvpStr p = pystr(x->a, avp_json_getz(v, "precision")), r = pystr(x->a, avp_json_getz(v, "recall"));
        prec = avp_pyfmt(x->a, "held-out string anchors: precision %S%%, recall %S%% (%d held out)", &p, &r,
                         jget_i(v, "held_out"));
    }
    return avp_pyfmt(x->a, "%s. Mapping %S.", WT_ADVISORY, &prec);
}

/* "0x%X" % int(text, 16) for a value beyond 64 bits (Python's unbounded int) */
static AvpStr big_hex(AvpArena *a, AvpStr text) {
    AvpStr t = avp_str_strip(text);
    bool neg = false;
    if (t.n && (t.p[0] == '+' || t.p[0] == '-')) {
        neg = t.p[0] == '-';
        t = avp_str_slice(t, 1, AVP_END);
    }
    if (t.n >= 2 && t.p[0] == '0' && (t.p[1] == 'x' || t.p[1] == 'X')) t = avp_str_slice(t, 2, AVP_END);
    AvpBuf b = {0};
    if (neg) avp_buf_putc(&b, '-');
    bool lead = true;
    for (size_t k = 0; k < t.n; k++) {
        char c = t.p[k];
        if (c == '_') continue;
        if (lead && c == '0') continue;
        lead = false;
        avp_buf_putc(&b, (c >= 'a' && c <= 'f') ? (char)(c - 32) : c);
    }
    AvpStr r = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_free(&b);
    return r;
}

static const WtFn *resolve(Cx *x, const char *text) {
    const WtSide *s = &x->w->m.wii;
    AVP_VEC(const WtFn *) by_name = {0};
    for (size_t k = 0; k < s->v.n; k++)
        if (s->v.v[k]->name && strcmp(s->v.v[k]->name, text) == 0) avp_vec_push(&by_name, s->v.v[k]);
    if (by_name.n > 1) {
        AvpBuf b = {0};
        for (size_t k = 0; k < by_name.n; k++) avp_buf_pyfmt(&b, k ? ", %08X" : "%08X", by_name.v[k]->addr);
        AvpStr addrs = avp_str_dup(x->a, avp_buf_str(&b));
        avp_buf_free(&b);
        size_t n = by_name.n;
        avp_vec_free(&by_name);
        shsm_raise(SHSM_EXC_USAGE, "%s",
                   avp_pyfmt(x->a, "%r names %d functions (%S): give an address", text, n, &addrs).p);
    }
    if (by_name.n) {
        const WtFn *f = by_name.v[0];
        avp_vec_free(&by_name);
        return f;
    }
    i64 a = 0;
    bool overflow = false;
    if (!avp_parse_int(avp_str(text), 16, &a, &overflow)) {
        if (!overflow) shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "no Wii function %r", text).p);
        shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "no Wii function at %r", text).p);
    }
    const WtFn *f = wt_side_get(s, a);
    if (f) return f;
    for (size_t k = 0; k < s->v.n; k++) {
        f = s->v.v[k];
        if (f->addr <= a && a < f->addr + f->size) return f;
    }
    shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "no Wii function at %r", text).p);
}

static const WtFn *resolve_ps2(Cx *x, const char *text) {
    i64 a = 0;
    bool overflow = false;
    if (!avp_parse_int(avp_str(text), 16, &a, &overflow)) {
        if (!overflow) shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "give a PS2 address in hex, not %r", text).p);
        AvpStr h = big_hex(x->a, avp_str(text));
        shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "no PS2 function at 0x%S", &h).p);
    }
    const AvpI64Vec *order = &x->w->ps2_order;
    /* bisect_right(order, a) - 1 */
    size_t lo = 0, hi = order->n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (a < order->v[mid])
            hi = mid;
        else
            lo = mid + 1;
    }
    if (lo > 0) {
        const WtFn *f = wt_side_get(&x->w->m.ps2, order->v[lo - 1]);
        if (a < order->v[lo - 1] + f->size) return f;
    }
    shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(x->a, "no PS2 function at 0x%X", a).p);
}

/* wname(a): the Wii name, "?%08X" when unknown, "?" for None */
static const char *wname(Cx *x, i64 a) {
    if (a == WT_NONE) return "?";
    const WtFn *f = wt_side_get(&x->w->m.wii, a);
    if (f) return f->name;
    return avp_pyfmt(x->a, "?%08X", a).p;
}

static AvpStr ps2_label(Cx *x, i64 p) {
    i64 w;
    if (rev(x, p, &w)) return avp_pyfmt(x->a, "P%06X = %s", p, wname(x, w));
    return avp_pyfmt(x->a, "P%06X", p);
}

static void build_helpers(Cx *x) {
    if (x->have_helpers) return;
    x->have_helpers = true;
    x->helper_idx = avp_imap_new();
    const WtInls *inl = &x->w->m.inl;
    for (size_t k = 0; k < inl->n; k++) {
        bool created;
        AvpVal *v = avp_imap_put(x->helper_idx, (u64)inl->v[k].helper, &created);
        if (created) {
            v->i = (i64)x->helpers.n;
            HelperInfo h = {inl->v[k].helper, avp_iset_new(), WT_NONE};
            i64 w;
            if (rev(x, h.helper, &w)) h.mapped = w;
            avp_vec_push(&x->helpers, h);
        }
        avp_iset_add(x->helpers.v[v->i].sites, (u64)inl->v[k].wii);
    }
}

/* ---- subcommands ---- */

static bool lib_matches(AvpArena *a, const char *lib, const char *want) {
    if (!want || !*want) return true;
    AvpStr wl = avp_str_strip_chars(avp_str_lower(a, avp_str(want)), AVP_S("/"));
    AvpStr name = avp_str_lower(a, avp_str(lib ? lib : "unknown"));
    if (avp_str_eq(wl, name)) return true;
    i64 slash = avp_str_rfind(name, AVP_S("/"), 0, AVP_END);
    AvpStr last = slash >= 0 ? avp_str_slice(name, slash + 1, AVP_END) : name;
    if (avp_str_eq(wl, last)) return true;
    return avp_str_eq(wl, shsm_library_group_of(avp_cz(a, name)));
}

static AvpStr pair_line(Cx *x, const WtFn *f) {
    const WtPairEntry *got = wt_corr_pair(x->c, f->addr);
    if (!got) return avp_pyfmt(x->a, "Wii %08X %s: no PS2 counterpart", f->addr, f->name);
    const WtFn *pf = wt_side_get(&x->w->m.ps2, got->p.ps2);
    return avp_pyfmt(x->a, "Wii %08X %s (%s, %d rows) <-> PS2 %08X (0x%X bytes, %d insns)  tier %s: %s", f->addr,
                     f->name, f->lib && *f->lib ? f->lib : "?", f->n, got->p.ps2, pf->size, pf->n, got->p.tier,
                     got->p.ev);
}

static bool vec_has(const AvpI64Vec *v, i64 m) {
    for (size_t k = 0; k < v->n; k++)
        if (v->v[k] == m) return true;
    return false;
}

static AvpStr join_strs(AvpArena *a, AvpStrVec *v, size_t cap) {
    return avp_str_join(a, AVP_S(", "), v->v, v->n < cap ? v->n : cap);
}

static int cmd_of(Cx *x) {
    AvpArena *a = x->a;
    const WtFn *f = resolve(x, x->args->function);
    line(x, header(x));
    line(x, pair_line(x, f));
    const WtPairEntry *got = wt_corr_pair(x->c, f->addr);
    shsm_output_data(x->out, "wii", jhex(a, "0x%08X", f->addr));
    shsm_output_data(x->out, "name", jstrz_or_null(a, f->name));
    shsm_output_data(x->out, "lib", jstrz_or_null(a, f->lib));
    if (!got) {
        shsm_output_data(x->out, "ps2", avp_json_null());
        return 0;
    }
    i64 p = got->p.ps2;
    const WtFn *pf = wt_side_get(&x->w->m.ps2, p);
    line(x, avp_pyfmt(a,
                      "shape: Wii %d rows, frame %d, %d back-edge(s), %d call(s) | PS2 %d insns, frame %d, "
                      "%d back-edge(s), %d call(s)",
                      f->n, f->frame, f->back, f->calls.n, pf->n, pf->frame, pf->back, pf->calls.n));
    WtCallDiff diff;
    wt_call_diff(x->c, f->addr, p, &diff);
    AvpISet *wset = avp_iset_new();
    for (size_t k = 0; k < f->calls.n; k++)
        if (f->calls.v[k] != WT_NONE) avp_iset_add(wset, (u64)f->calls.v[k]);
    line(x, AVP_S("PS2 calls in order:"));
    AvpJson *rows = avp_json_arr(a);
    size_t ncall = pf->sites.n < pf->calls.n ? pf->sites.n : pf->calls.n;
    for (size_t k = 0; k < ncall; k++) {
        i64 site = pf->sites.v[k], c = pf->calls.v[k], wc;
        AvpStr note;
        if (c == WT_NONE)
            note = AVP_S("call through a register");
        else if (vec_has(&diff.replaced, c))
            note = AVP_S("differs: the Wii build calls an unmapped function of its own in this slot");
        else if (rev(x, c, &wc))
            note = avp_iset_has(wset, (u64)wc)
                       ? avp_pyfmt(a, "= %s", wname(x, wc))
                       : avp_pyfmt(a, "INLINED here on Wii; out-of-line Wii copy %s %08X", wname(x, wc), wc);
        else
            note = AVP_S("INLINED here on Wii (or PS2-only code); no Wii counterpart found");
        AvpJson *r = avp_json_obj(a);
        avp_json_set(a, r, AVP_S("site"), jhex(a, "0x%X", site));
        avp_json_set(a, r, AVP_S("callee"), c == WT_NONE ? avp_json_null() : jhex(a, "0x%X", c));
        avp_json_set(a, r, AVP_S("note"), jstr(a, note));
        avp_json_arr_push(a, rows, r);
        AvpStr callee = c == WT_NONE ? AVP_S("?") : avp_pyfmt(a, "%06X", c);
        line(x, avp_pyfmt(a, "  %06X  jal %-8S %S", site, &callee, &note));
    }
    if (diff.wii_only.n) {
        AvpStrVec names = {0};
        for (size_t k = 0; k < diff.wii_only.n; k++) avp_vec_push(&names, avp_str(wname(x, diff.wii_only.v[k])));
        AvpStr j = join_strs(a, &names, names.n);
        line(x, avp_pyfmt(a, "Wii-only calls: %S", &j));
        avp_vec_free(&names);
    }
    /* strings: sorted(json.dumps(t) for t in a - b)[:8] */
    const WtFn *sides[2] = {f, pf};
    for (int s = 0; s < 2; s++) {
        const AvpSet *sa = sides[s]->strs, *sb = sides[1 - s]->strs;
        AvpStrVec v = {0};
        size_t pos = 0;
        AvpStr m;
        while (sa && avp_set_next(sa, &pos, &m))
            if (!sb || !avp_set_has(sb, m)) avp_vec_push(&v, json_dumps_str(a, m));
        if (v.n) {
            avp_sort_strs(v.v, v.n);
            AvpStr j = join_strs(a, &v, 8);
            line(x, avp_pyfmt(a, "strings only on %s: %S", s ? "PS2" : "Wii", &j));
        }
        avp_vec_free(&v);
    }
    /* floats: sorted({_fmt_float(x) for x in a - b})[:8] */
    for (int s = 0; s < 2; s++) {
        const AvpPyIntSet *sa = sides[s]->floats, *sb = sides[1 - s]->floats;
        AvpSet *texts = avp_set_new();
        size_t pos = 0;
        i64 m;
        while (avp_pyintset_next(sa, &pos, &m))
            if (!avp_pyintset_has(sb, m)) avp_set_add(texts, fmt_float(a, m));
        if (avp_set_len(texts)) {
            AvpStrVec v = avp_set_sorted(texts);
            AvpStr j = join_strs(a, &v, 8);
            line(x, avp_pyfmt(a, "floats only on %s: %S", s ? "PS2" : "Wii", &j));
            avp_vec_free(&v);
        }
        avp_set_free(texts);
    }
    size_t shared = avp_pyintset_len(f->offs) - pyintset_minus_len(f->offs, pf->offs);
    line(x, avp_pyfmt(a, "memory offsets: %d shared, %d Wii only, %d PS2 only", shared,
                      pyintset_minus_len(f->offs, pf->offs), pyintset_minus_len(pf->offs, f->offs)));
    shsm_output_data(x->out, "ps2", jhex(a, "0x%X", p));
    shsm_output_data(x->out, "tier", avp_json_strz(a, got->p.tier));
    shsm_output_data(x->out, "evidence", avp_json_strz(a, got->p.ev));
    shsm_output_data(x->out, "calls", rows);
    AvpJson *po = avp_json_arr(a), *wo = avp_json_arr(a);
    for (size_t k = 0; k < diff.ps2_only.n; k++) avp_json_arr_push(a, po, jhex(a, "0x%X", diff.ps2_only.v[k]));
    for (size_t k = 0; k < diff.wii_only.n; k++)
        avp_json_arr_push(a, wo, jstrz_or_null(a, wname(x, diff.wii_only.v[k])));
    shsm_output_data(x->out, "ps2_only", po);
    shsm_output_data(x->out, "wii_only", wo);
    avp_iset_free(wset);
    wt_call_diff_free(&diff);
    return 0;
}

/* inlined_at(wt, w): [(helper, evidence, other Wii sites, Wii out-of-line copy or None)] */
typedef struct InlAt {
    i64 helper;
    const char *ev;
    AvpI64Vec others;
    i64 copy; /* WT_NONE = None */
} InlAt;
typedef AVP_VEC(InlAt) InlAts;

static InlAts inlined_at(Cx *x, i64 w) {
    build_helpers(x);
    InlAts res = {0};
    const WtInls *inl = &x->w->m.inl;
    for (size_t k = 0; k < inl->n; k++) {
        if (inl->v[k].wii != w) continue;
        InlAt r = {inl->v[k].helper, inl->v[k].ev, {0}, WT_NONE};
        const HelperInfo *h = &x->helpers.v[avp_imap_get(x->helper_idx, (u64)r.helper)->i];
        AvpI64Vec all = iset_sorted(h->sites);
        for (size_t j = 0; j < all.n; j++)
            if (all.v[j] != w) avp_vec_push(&r.others, all.v[j]);
        avp_vec_free(&all);
        i64 c;
        if (rev(x, r.helper, &c)) r.copy = c;
        avp_vec_push(&res, r);
    }
    return res;
}

static void inlats_free(InlAts *v) {
    for (size_t k = 0; k < v->n; k++) avp_vec_free(&v->v[k].others);
    avp_vec_free(v);
}

static int cmd_inlined(Cx *x) {
    AvpArena *a = x->a;
    const WtFn *f = resolve(x, x->args->function);
    line(x, header(x));
    line(x, pair_line(x, f));
    InlAts res = inlined_at(x, f->addr);
    AvpJson *items = avp_json_arr(a);
    for (size_t k = 0; k < res.n; k++) {
        const InlAt *r = &res.v[k];
        const WtFn *hf = wt_side_get(&x->w->m.ps2, r->helper);
        AvpI64Vec pc = pyintset_sorted(wt_corr_pcallers(x->c, r->helper));
        size_t mapped = 0;
        for (size_t j = 0; j < pc.n; j++) {
            i64 w;
            if (rev(x, pc.v[j], &w)) mapped++;
        }
        AvpStr copy = r->copy != WT_NONE ? avp_pyfmt(a, "%s %08X", wname(x, r->copy), r->copy) : AVP_S("none");
        line(x, avp_pyfmt(a, "PS2 %06X (0x%X bytes, %d PS2 callers, %d mapped) %s; Wii out-of-line copy: %S", r->helper,
                          hf ? hf->size : 0, pc.n, mapped, r->ev, &copy));
        if (r->others.n) {
            AvpStrVec names = {0};
            for (size_t j = 0; j < r->others.n && j < 8; j++) avp_vec_push(&names, avp_str(wname(x, r->others.v[j])));
            AvpStr j = join_strs(a, &names, names.n);
            AvpStr more = r->others.n > 8 ? avp_pyfmt(a, " ... +%d", r->others.n - 8) : AVP_S("");
            line(x, avp_pyfmt(a, "    also inlined into: %S%S", &j, &more));
            avp_vec_free(&names);
        }
        AvpJson *it = avp_json_obj(a);
        avp_json_set(a, it, AVP_S("helper"), jhex(a, "0x%X", r->helper));
        avp_json_set(a, it, AVP_S("size"), hf ? jint(a, hf->size) : avp_json_null());
        avp_json_set(a, it, AVP_S("evidence"), jstrz_or_null(a, r->ev));
        avp_json_set(a, it, AVP_S("ps2_callers"), jint(a, (i64)pc.n));
        avp_json_set(a, it, AVP_S("wii_copy"), r->copy != WT_NONE ? jstrz_or_null(a, wname(x, r->copy)) : avp_json_null());
        AvpJson *os = avp_json_arr(a);
        for (size_t j = 0; j < r->others.n; j++) avp_json_arr_push(a, os, jstrz_or_null(a, wname(x, r->others.v[j])));
        avp_json_set(a, it, AVP_S("other_sites"), os);
        avp_json_arr_push(a, items, it);
        avp_vec_free(&pc);
    }
    if (!res.n) line(x, AVP_S("no PS2 helper kept out of line at this site"));
    shsm_output_data(x->out, "wii", jstrz_or_null(a, f->name));
    shsm_output_data(x->out, "helpers", items);
    inlats_free(&res);
    return 0;
}

typedef struct HelperRow {
    i64 helper, npc;
    AvpI64Vec sites;
} HelperRow;
typedef AVP_VEC(HelperRow) HelperRows;

static int cmp_helper_row(const void *p, const void *q, void *ctx) {
    (void)ctx;
    const HelperRow *a = p, *b = q;
    /* key (-len(sites), -npc, helper) */
    if (a->sites.n != b->sites.n) return a->sites.n > b->sites.n ? -1 : 1;
    if (a->npc != b->npc) return a->npc > b->npc ? -1 : 1;
    return a->helper < b->helper ? -1 : a->helper > b->helper;
}

static size_t pcallers_len(Cx *x, i64 h) {
    const AvpPyIntSet *s = wt_corr_pcallers(x->c, h);
    return s ? avp_pyintset_len(s) : 0;
}

static HelperRows helper_table(Cx *x, const char *lib, i64 min_callers) {
    build_helpers(x);
    HelperRows rows = {0};
    for (size_t k = 0; k < x->helpers.n; k++) {
        const HelperInfo *h = &x->helpers.v[k];
        if (h->mapped != WT_NONE) continue;
        AvpI64Vec all = iset_sorted(h->sites), sites = {0};
        for (size_t j = 0; j < all.n; j++) {
            const WtFn *f = wt_side_get(&x->w->m.wii, all.v[j]);
            if (!f) shsm_raise(SHSM_EXC_KEY, "%s", avp_pyfmt(x->a, "%d", all.v[j]).p);
            if (lib_matches(x->a, f->lib, lib)) avp_vec_push(&sites, all.v[j]);
        }
        avp_vec_free(&all);
        if ((i64)sites.n < min_callers) {
            avp_vec_free(&sites);
            continue;
        }
        HelperRow r = {h->helper, (i64)pcallers_len(x, h->helper), sites};
        avp_vec_push(&rows, r);
    }
    avp_sort(rows.v, rows.n, sizeof rows.v[0], cmp_helper_row, NULL, false);
    return rows;
}

/* known_check(wt): the known helpers (fragments KNOWN) against the inlined rows */
static AvpJson *known_check(Cx *x) {
    AvpArena *a = x->a;
    AvpSet *sites[FRAGMENTS_NKNOWN];
    fragments_known_functions(a, x->w->repo, x->w->ix, sites);
    AvpJson *res = avp_json_arr(a);
    const WtSide *wii = &x->w->m.wii;
    for (int k = 0; k < FRAGMENTS_NKNOWN; k++) {
        AvpI64Vec fns = {0};
        for (size_t j = 0; j < wii->v.n; j++) {
            const WtFn *f = wii->v.v[j];
            if (f->name && avp_set_has(sites[k], avp_str(f->name))) avp_vec_push(&fns, f->addr);
        }
        avp_sort_i64(fns.v, fns.n);
        AvpI64Vec mapped = {0};
        for (size_t j = 0; j < fns.n; j++)
            if (wt_corr_pair(x->c, fns.v[j])) avp_vec_push(&mapped, fns.v[j]);
        /* Counter in first-seen order; most_common(1) is the first of the largest counts */
        AvpIMap *cnt = avp_imap_new();
        AvpI64Vec order = {0};
        size_t with_helper = 0;
        for (size_t j = 0; j < mapped.n; j++) {
            InlAts r = inlined_at(x, mapped.v[j]);
            if (r.n) with_helper++;
            for (size_t q = 0; q < r.n; q++) {
                bool created;
                AvpVal *v = avp_imap_put(cnt, (u64)r.v[q].helper, &created);
                if (created) avp_vec_push(&order, r.v[q].helper);
                v->i++;
            }
            inlats_free(&r);
        }
        bool top = order.n > 0;
        i64 th = 0, tc = 0;
        for (size_t j = 0; j < order.n; j++) {
            i64 c = avp_imap_get(cnt, (u64)order.v[j])->i;
            if (j == 0 || c > tc) {
                th = order.v[j];
                tc = c;
            }
        }
        i64 need = ((i64)mapped.n + 1) / 2;
        if (need < 2) need = 2;
        bool found = top && tc >= need;
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("helper"), avp_json_strz(a, FRAGMENTS_KNOWN[k].name));
        avp_json_set(a, o, AVP_S("wii_sites"), jint(a, (i64)fns.n));
        avp_json_set(a, o, AVP_S("mapped"), jint(a, (i64)mapped.n));
        avp_json_set(a, o, AVP_S("found"), avp_json_bool(found));
        avp_json_set(a, o, AVP_S("sites_with_ps2_helper"), jint(a, (i64)with_helper));
        avp_json_set(a, o, AVP_S("distinct_ps2_helpers"), jint(a, (i64)order.n));
        avp_json_set(a, o, AVP_S("ps2_helper"), top ? jhex(a, "0x%X", th) : avp_json_null());
        avp_json_set(a, o, AVP_S("sites_seen"), jint(a, top ? tc : 0));
        const WtFn *hf = top ? wt_side_get(&x->w->m.ps2, th) : NULL;
        avp_json_set(a, o, AVP_S("ps2_size"), hf ? jint(a, hf->size) : avp_json_null());
        avp_json_set(a, o, AVP_S("ps2_callers"), jint(a, top ? (i64)pcallers_len(x, th) : 0));
        i64 cw;
        avp_json_set(a, o, AVP_S("ps2_copy_on_wii"),
                     top && rev(x, th, &cw) ? jstrz_or_null(a, wname(x, cw)) : avp_json_null());
        avp_json_arr_push(a, res, o);
        avp_imap_free(cnt);
        avp_vec_free(&order);
        avp_vec_free(&mapped);
        avp_vec_free(&fns);
        avp_set_free(sites[k]);
    }
    return res;
}

static int cmd_helpers(Cx *x) {
    AvpArena *a = x->a;
    line(x, header(x));
    HelperRows rows = helper_table(x, x->args->lib, x->args->min_callers);
    size_t shown = (x->args->all || !x->args->limit) ? rows.n
                   : (x->args->limit < 0 ? (rows.n > (size_t)-x->args->limit ? rows.n - (size_t)-x->args->limit : 0)
                                         : (rows.n < (size_t)x->args->limit ? rows.n : (size_t)x->args->limit));
    line(x, AVP_S("PS2 functions with no out-of-line Wii counterpart, by Wii sites they are inlined into:"));
    AvpJson *items = avp_json_arr(a);
    for (size_t k = 0; k < shown; k++) {
        const HelperRow *r = &rows.v[k];
        const WtFn *hf = wt_side_get(&x->w->m.ps2, r->helper);
        AvpStrVec names = {0};
        for (size_t j = 0; j < r->sites.n && j < 4; j++) avp_vec_push(&names, avp_str(wname(x, r->sites.v[j])));
        AvpStr j = join_strs(a, &names, names.n);
        avp_vec_free(&names);
        line(x, avp_pyfmt(a, "  P%06X  0x%04X bytes  %3d PS2 callers  %3d Wii sites: %S%s", r->helper,
                          hf ? hf->size : 0, r->npc, r->sites.n, &j, r->sites.n > 4 ? " ..." : ""));
        AvpJson *it = avp_json_obj(a);
        avp_json_set(a, it, AVP_S("helper"), jhex(a, "0x%X", r->helper));
        avp_json_set(a, it, AVP_S("size"), hf ? jint(a, hf->size) : avp_json_null());
        avp_json_set(a, it, AVP_S("ps2_callers"), jint(a, r->npc));
        AvpJson *ws = avp_json_arr(a);
        for (size_t q = 0; q < r->sites.n; q++) avp_json_arr_push(a, ws, jstrz_or_null(a, wname(x, r->sites.v[q])));
        avp_json_set(a, it, AVP_S("wii_sites"), ws);
        avp_json_arr_push(a, items, it);
    }
    if (rows.n > shown) line(x, avp_pyfmt(a, "... %d more helpers, use --all", rows.n - shown));
    AvpJson *known = x->args->no_known ? avp_json_arr(a) : known_check(x);
    for (size_t k = 0; k < avp_json_len(known); k++) {
        const AvpJson *o = avp_json_at(known, k);
        AvpStr hn = avp_json_get_str(o, "helper", AVP_S(""));
        AvpStr name = avp_pyfmt(a, "%S:", &hn);
        AvpStr head = avp_pyfmt(a, "known %-20S %d Wii sites, %d mapped; ", &name, jget_i(o, "wii_sites"),
                                jget_i(o, "mapped"));
        const AvpJson *ph = avp_json_getz(o, "ps2_helper"), *copy = avp_json_getz(o, "ps2_copy_on_wii");
        AvpStr best;
        if (!avp_json_is_null(ph)) {
            AvpStr cp = avp_json_truthy(copy) ? avp_pyfmt(a, ", Wii copy %S", &copy->u.str) : AVP_S("");
            best = avp_pyfmt(a, "commonest PS2 callee missing on Wii: %S (0x%X bytes, %d PS2 callers%S) at %d site(s)",
                             &ph->u.str, avp_json_i64(avp_json_getz(o, "ps2_size"), 0), jget_i(o, "ps2_callers"), &cp,
                             jget_i(o, "sites_seen"));
        } else {
            best = AVP_S("no PS2 callee missing on Wii");
        }
        line(x, avp_pyfmt(a, "%S%s%S; sites with any PS2-only callee %d (%d distinct)", &head,
                          avp_json_get_bool(o, "found", false) ? "APPEARS: " : "no one PS2 helper at most sites; ",
                          &best, jget_i(o, "sites_with_ps2_helper"), jget_i(o, "distinct_ps2_helpers")));
    }
    shsm_output_data(x->out, "helpers", items);
    shsm_output_data(x->out, "total", jint(a, (i64)rows.n));
    shsm_output_data(x->out, "known", known);
    for (size_t k = 0; k < rows.n; k++) avp_vec_free(&rows.v[k].sites);
    avp_vec_free(&rows);
    return 0;
}

static bool hex8(const char *p) {
    for (int k = 0; k < 8; k++) {
        char c = p[k];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

/* _range(text): re.fullmatch(r"(?:0x)?([0-9A-Fa-f]{8})-(?:0x)?([0-9A-Fa-f]{8})", text or "") */
static void parse_range(const char *text, i64 *lo, i64 *hi) {
    const char *s = text ? text : "";
    const char *p = s;
    bool ok = false;
    const char *g1 = NULL, *g2 = NULL;
    for (int pre1 = 1; pre1 >= 0 && !ok; pre1--) {
        p = s;
        if (pre1) {
            if (strncmp(p, "0x", 2) != 0) continue;
            p += 2;
        }
        if (strlen(p) < 9 || !hex8(p) || p[8] != '-') continue;
        g1 = p;
        const char *q = p + 9;
        for (int pre2 = 1; pre2 >= 0 && !ok; pre2--) {
            const char *r = q;
            if (pre2) {
                if (strncmp(r, "0x", 2) != 0) continue;
                r += 2;
            }
            if (strlen(r) == 8 && hex8(r)) {
                g2 = r;
                ok = true;
            }
        }
    }
    if (!ok) shsm_raise(SHSM_EXC_USAGE, "give a Wii range START-END (hex), or --lib L");
    u64 a = 0, b = 0;
    avp_parse_uint(avp_strn(g1, 8), 16, &a);
    avp_parse_uint(avp_strn(g2, 8), 16, &b);
    *lo = (i64)a;
    *hi = (i64)b;
}

static i64 rank_of(Cx *x, i64 p) { return avp_imap_get(x->w->ps2_rank, (u64)p)->i; }

static AvpJson *order_agreement(Cx *x, i64 gap) {
    AvpArena *a = x->a;
    ShsmSymbolTable *st = shsm_load_symbols(x->w->repo, true);
    AvpI64Vec seq = sorted_wii(x);
    i64 sn = 0, sf = 0, cn = 0, cf = 0;
    for (size_t k = 0; k + 1 < seq.n; k++) {
        i64 p = seq.v[k], q = seq.v[k + 1];
        const WtPairEntry *ea = wt_corr_pair(x->c, p), *eb = wt_corr_pair(x->c, q);
        if (!ea || !eb) continue;
        const char *ua = shsm_sym_unit_of(st, (u64)p), *ub = shsm_sym_unit_of(st, (u64)q);
        if (!ua || !ub) continue;
        i64 d = rank_of(x, eb->p.ps2) - rank_of(x, ea->p.ps2);
        bool near = (d < 0 ? -d : d) <= gap;
        if (strcmp(ua, ub) == 0) {
            if (near) sn++; else sf++;
        } else {
            if (near) cn++; else cf++;
        }
    }
    avp_vec_free(&seq);
    i64 same = sn + sf, cut = cn + cf, agree = sn + cf, tot = same + cut;
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("pairs"), jint(a, tot));
    avp_json_set(a, o, AVP_S("same_unit"), jint(a, same));
    avp_json_set(a, o, AVP_S("same_unit_ps2_near"), jint(a, sn));
    avp_json_set(a, o, AVP_S("unit_cut"), jint(a, cut));
    avp_json_set(a, o, AVP_S("unit_cut_ps2_far"), jint(a, cf));
    avp_json_set(a, o, AVP_S("agreement"),
                 tot ? avp_json_float(a, avp_py_round(100.0 * (double)agree / (double)tot, 1)) : avp_json_null());
    avp_json_set(a, o, AVP_S("gap"), jint(a, gap));
    return o;
}

static int cmd_order(Cx *x) {
    AvpArena *a = x->a;
    const WtArgs *args = x->args;
    line(x, header(x));
    i64 lo, hi;
    AvpI64Vec all = sorted_wii(x);
    if (args->lib && *args->lib) {
        AvpI64Vec fns = {0};
        for (size_t k = 0; k < all.n; k++)
            if (lib_matches(a, wt_side_get(&x->w->m.wii, all.v[k])->lib, args->lib)) avp_vec_push(&fns, all.v[k]);
        if (!fns.n) {
            avp_vec_free(&fns);
            avp_vec_free(&all);
            shsm_raise(SHSM_EXC_USAGE, "%s", avp_pyfmt(a, "no Wii function in library %r", args->lib).p);
        }
        const WtFn *last = wt_side_get(&x->w->m.wii, fns.v[fns.n - 1]);
        lo = fns.v[0];
        hi = last->addr + last->size;
        avp_vec_free(&fns);
    } else {
        AvpTry t;
        if (AVP_TRY(&t)) {
            parse_range(args->range, &lo, &hi);
            avp_try_end(&t);
        } else {
            avp_vec_free(&all);
            avp_throw(t.code, "%s", t.msg);
        }
    }
    AvpI64Vec seq = {0}, mapped = {0};
    for (size_t k = 0; k < all.n; k++)
        if (lo <= all.v[k] && all.v[k] < hi) avp_vec_push(&seq, all.v[k]);
    avp_vec_free(&all);
    for (size_t k = 0; k < seq.n; k++)
        if (wt_corr_pair(x->c, seq.v[k])) avp_vec_push(&mapped, seq.v[k]);
    AvpStrVec lines = {0};
    AvpJson *cuts = avp_json_arr(a);
    bool have_prev = false;
    i64 prev = 0;
    size_t si = 0; /* seq index past prev */
    for (size_t k = 0; k < mapped.n; k++) {
        i64 m = mapped.v[k];
        const WtPairEntry *e = wt_corr_pair(x->c, m);
        i64 p = e->p.ps2, rk = rank_of(x, p);
        if (have_prev) {
            i64 d = rk - rank_of(x, wt_corr_pair(x->c, prev)->p.ps2);
            if ((d < 0 ? -d : d) > args->gap) {
                while (si < seq.n && seq.v[si] <= prev) si++;
                i64 left = (si < seq.n && seq.v[si] <= m) ? seq.v[si] : m;
                AvpJson *c = avp_json_obj(a);
                avp_json_set(a, c, AVP_S("after"), jhex(a, "0x%08X", prev));
                avp_json_set(a, c, AVP_S("start"), jhex(a, "0x%08X", left));
                avp_json_set(a, c, AVP_S("end"), jhex(a, "0x%08X", m));
                avp_json_set(a, c, AVP_S("ps2_jump"), jint(a, d));
                avp_json_arr_push(a, cuts, c);
                avp_vec_push(&lines, avp_pyfmt(a, "  ---- PS2 neighbourhood changes (rank %+d): cut in %08X..%08X", d,
                                               left, m));
            }
        }
        AvpStr nm = head_cp(avp_str(wname(x, m)), 40);
        avp_vec_push(&lines, avp_pyfmt(a, "  %08X %-40S P%06X  rank %5d  %s", m, &nm, p, rk, e->p.tier));
        prev = m;
        have_prev = true;
    }
    line(x, avp_pyfmt(a, "%d Wii functions in %08X-%08X, %d mapped", seq.n, lo, hi, mapped.n));
    AvpStrVec capped = shsm_cap_lines(a, lines.v, lines.n, args->limit, args->all, "use --all");
    for (size_t k = 0; k < capped.n; k++) line(x, capped.v[k]);
    avp_vec_free(&capped);
    avp_vec_free(&lines);
    line(x, AVP_S("proposed cuts (unit_plan gap form `cut START-END ps2`):"));
    for (size_t k = 0; k < avp_json_len(cuts); k++) {
        const AvpJson *c = avp_json_at(cuts, k);
        AvpStr s = avp_str_slice(avp_json_get_str(c, "start", AVP_S("")), 2, AVP_END);
        AvpStr e = avp_str_slice(avp_json_get_str(c, "end", AVP_S("")), 2, AVP_END);
        line(x, avp_pyfmt(a, "cut %S-%S ps2 jump %+d", &s, &e, jget_i(c, "ps2_jump")));
    }
    AvpJson *ag = order_agreement(x, args->gap);
    AvpStr agr = pystr(a, avp_json_getz(ag, "agreement"));
    line(x, avp_pyfmt(a,
                      "agreement with splits.txt units (gap %d): %S%% of %d labelled neighbour pairs; same-unit pairs "
                      "PS2-near %d/%d, unit cuts PS2-far %d/%d",
                      jget_i(ag, "gap"), &agr, jget_i(ag, "pairs"), jget_i(ag, "same_unit_ps2_near"),
                      jget_i(ag, "same_unit"), jget_i(ag, "unit_cut_ps2_far"), jget_i(ag, "unit_cut")));
    AvpJson *range = avp_json_arr(a);
    avp_json_arr_push(a, range, jhex(a, "0x%08X", lo));
    avp_json_arr_push(a, range, jhex(a, "0x%08X", hi));
    shsm_output_data(x->out, "range", range);
    shsm_output_data(x->out, "functions", jint(a, (i64)seq.n));
    shsm_output_data(x->out, "mapped", jint(a, (i64)mapped.n));
    shsm_output_data(x->out, "cuts", cuts);
    shsm_output_data(x->out, "agreement", ag);
    AvpJson *order = avp_json_arr(a);
    for (size_t k = 0; k < mapped.n; k++) {
        AvpJson *o = avp_json_obj(a);
        avp_json_set(a, o, AVP_S("wii"), jhex(a, "0x%08X", mapped.v[k]));
        avp_json_set(a, o, AVP_S("ps2"), jhex(a, "0x%X", wt_corr_pair(x->c, mapped.v[k])->p.ps2));
        avp_json_arr_push(a, order, o);
    }
    shsm_output_data(x->out, "order", order);
    avp_vec_free(&seq);
    avp_vec_free(&mapped);
    return 0;
}

static int cmd_ps2(Cx *x) {
    AvpArena *a = x->a;
    const WtFn *pf = resolve_ps2(x, x->args->address);
    AvpBytes data = {0};
    wt_load_exe(a, x->w->exe, &data);
    WtElf *elf = wt_elf_new(a, data.v, data.n);
    i64 w = WT_NONE;
    bool has_w = rev(x, pf->addr, &w) && w != 0;
    line(x, header(x));
    AvpStr wii = has_w ? pair_line(x, wt_side_get(&x->w->m.wii, w)) : AVP_S("none");
    line(x, avp_pyfmt(a, "PS2 %08X, 0x%X bytes, %d insns, frame %d; Wii: %S", pf->addr, pf->size, pf->n, pf->frame,
                      &wii));
    AvpStrVec rows = {0};
    bool have[32] = {false};
    i64 val[32] = {0};
    for (i64 ad = pf->addr; ad < pf->addr + pf->size; ad += 4) {
        u32 word = 0;
        wt_elf_word(elf, ad, &word);
        WtInsn i;
        wt_decode(word, (u32)ad, &i);
        AvpStr note = AVP_S("");
        if ((i.kind == WT_K_CALL || i.kind == WT_K_JUMP) && wt_side_get(&x->w->m.ps2, (i64)i.target) &&
            !(pf->addr <= (i64)i.target && (i64)i.target < pf->addr + pf->size))
            note = ps2_label(x, (i64)i.target);
        if (i.kind == WT_K_LUI) {
            val[i.rt] = (i64)i.imm * 65536;
            have[i.rt] = true;
        } else if (i.kind == WT_K_IMM && have[i.rs] && (strcmp(i.mn, "addiu") == 0 || strcmp(i.mn, "daddiu") == 0)) {
            const char *t = wt_elf_cstring(a, elf, (i64)((u64)(val[i.rs] + i.imm) & 0xFFFFFFFFu));
            if (t && *t) note = json_dumps_str(a, head_cp(avp_str(t), 60));
        } else if (i.kind == WT_K_LOAD && have[i.rs] && strcmp(i.mn, "lwc1") == 0) {
            u32 bits;
            if (wt_elf_float_at(elf, (i64)((u64)(val[i.rs] + i.imm) & 0xFFFFFFFFu), &bits)) {
                AvpStr fs = fmt_float(a, (i64)bits);
                note = avp_pyfmt(a, "= %S", &fs);
            }
        } else if (i.kind == WT_K_MTC1 && have[i.rt]) {
            AvpStr fs = fmt_float(a, val[i.rt]);
            note = avp_pyfmt(a, "= %S", &fs);
        }
        char buf[WT_INSN_TEXT];
        avp_vec_push(&rows, avp_pyfmt(a, "  %06X  %-34s %S", ad, wt_format_insn(&i, buf), &note));
    }
    AvpStrVec capped = shsm_cap_lines(a, rows.v, rows.n, x->args->limit, x->args->all, "use --all");
    for (size_t k = 0; k < capped.n; k++) line(x, capped.v[k]);
    avp_vec_free(&capped);
    shsm_output_data(x->out, "address", jhex(a, "0x%X", pf->addr));
    shsm_output_data(x->out, "size", jint(a, pf->size));
    shsm_output_data(x->out, "wii", has_w ? jstrz_or_null(a, wname(x, w)) : avp_json_null());
    AvpJson *rj = avp_json_arr(a);
    for (size_t k = 0; k < rows.n; k++) avp_json_arr_push(a, rj, jstr(a, rows.v[k]));
    shsm_output_data(x->out, "rows", rj);
    avp_vec_free(&rows);
    return 0;
}

static int cmp_group(const void *p, const void *q, void *ctx) {
    (void)ctx;
    const i64 *a = p, *b = q; /* {n, k, index}: key -k */
    return a[1] > b[1] ? -1 : a[1] < b[1];
}

static int cmd_stats(Cx *x) {
    AvpArena *a = x->a;
    const AvpJson *m = x->w->m.meta;
    const WtSide *wii = &x->w->m.wii;
    i64 nps2 = (i64)x->w->m.ps2.v.n, nwii = (i64)wii->v.n, npairs = (i64)x->c->pairs.n;
    /* compute_stats */
    AvpJson *tiers = avp_json_obj(a);
    for (int t = 0; t < WT_NTIERS; t++) {
        i64 n = 0;
        for (size_t k = 0; k < x->c->pairs.n; k++)
            if (strcmp(x->c->pairs.v[k].p.tier, WT_TIERS[t]) == 0) n++;
        avp_json_set(a, tiers, avp_str(WT_TIERS[t]), jint(a, n));
    }
    AvpMap *libs = avp_map_new();
    AVP_VEC(i64) cnt = {0}; /* pairs of (n, mapped) per library index */
    for (size_t k = 0; k < wii->v.n; k++) {
        const WtFn *f = wii->v.v[k];
        bool created;
        AvpVal *v = avp_map_put(libs, avp_str(f->lib && *f->lib ? f->lib : "unknown"), &created);
        if (created) {
            v->i = (i64)cnt.n / 2;
            avp_vec_push(&cnt, 0);
            avp_vec_push(&cnt, 0);
        }
        cnt.v[2 * v->i]++;
        if (wt_corr_pair(x->c, f->addr)) cnt.v[2 * v->i + 1]++;
    }
    AvpStrVec names = avp_map_keys(libs);
    avp_sort_strs(names.v, names.n);
    AvpJson *libj = avp_json_obj(a);
    for (size_t k = 0; k < names.n; k++) {
        i64 ix = avp_map_get(libs, names.v[k])->i;
        AvpJson *pr = avp_json_arr(a);
        avp_json_arr_push(a, pr, jint(a, cnt.v[2 * ix]));
        avp_json_arr_push(a, pr, jint(a, cnt.v[2 * ix + 1]));
        avp_json_set(a, libj, names.v[k], pr);
    }
    build_helpers(x);
    i64 unmapped = 0;
    for (size_t k = 0; k < x->helpers.n; k++)
        if (x->helpers.v[k].mapped == WT_NONE) unmapped++;
    const AvpJson *v = avp_json_getz(m, "validation");
    /* cmd_stats */
    line(x, header(x));
    const AvpJson *f = avp_json_getz(m, "facts"), *secs = avp_json_getz(f, "sections");
    i64 text = 0;
    for (size_t k = 0; k < avp_json_len(secs); k++) {
        const AvpJson *s = avp_json_at(secs, k);
        if (avp_str_eqz(avp_json_str_or(avp_json_at(s, 3), AVP_S("")), "text"))
            text += avp_json_i64(avp_json_at(s, 2), 0);
    }
    const AvpJson *gp = avp_json_getz(f, "gp");
    AvpStr gps = avp_json_is_null(gp) ? AVP_S("?") : avp_pyfmt(a, "0x%X", avp_json_i64(gp, 0));
    AvpStr exe = pystr(a, avp_json_getz(m, "exe"));
    line(x, avp_pyfmt(a,
                      "PS2 executable %S: %d bytes, machine %d, entry 0x%X, gp %S, text 0x%X bytes (%d words), %d "
                      "functions recovered",
                      &exe, jget_i(m, "exe_size"), jget_i(f, "machine"), jget_i(f, "entry"), &gps, text,
                      jget_i(f, "words"), nps2));
    line(x, avp_pyfmt(a, "Wii functions %d; pairs %d (%.1f%% of Wii, %.1f%% of PS2) after %d propagation round(s)",
                      nwii, npairs, 100.0 * (double)npairs / (double)(nwii > 1 ? nwii : 1),
                      100.0 * (double)npairs / (double)(nps2 > 1 ? nps2 : 1), jget_i(m, "rounds")));
    AvpBuf b = {0};
    avp_buf_put(&b, AVP_S("by tier: "));
    for (size_t k = 0; k < avp_json_len(tiers); k++) {
        const AvpJsonMember *mm = avp_json_member(tiers, k);
        avp_buf_pyfmt(&b, k ? ", %S %d" : "%S %d", &mm->key, mm->val->u.i);
    }
    line(x, avp_buf_str(&b));
    avp_buf_clear(&b);
    const AvpJson *bt = avp_json_getz(v, "by_tier");
    for (size_t k = 0; k < avp_json_len(bt); k++) {
        const AvpJsonMember *mm = avp_json_member(bt, k);
        i64 c = jget_i(mm->val, "correct"), w = jget_i(mm->val, "wrong");
        avp_buf_pyfmt(&b, k ? ", %S %d/%d" : "%S %d/%d", &mm->key, c, c + w);
    }
    AvpStr bytier = avp_str_dup(a, avp_buf_str(&b));
    avp_buf_clear(&b);
    AvpStr prec = pystr(a, avp_json_getz(v, "precision")), rec = pystr(a, avp_json_getz(v, "recall"));
    line(x, avp_pyfmt(a,
                      "validation: %d string anchors held out (strings hidden): %d rediscovered, %d mapped elsewhere, "
                      "%d missed; precision %S%%, recall %S%%; by tier %S",
                      jget_i(v, "held_out"), jget_i(v, "correct"), jget_i(v, "wrong"), jget_i(v, "missed"), &prec, &rec,
                      &bytier));
    /* groups: {group: [n, mapped]} in sorted-library order, then sorted by -mapped (stable) */
    AvpMap *gmap = avp_map_new();
    AVP_VEC(i64) g3 = {0}; /* (n, mapped, name index) triples */
    AvpStrVec gnames = {0};
    for (size_t k = 0; k < names.n; k++) {
        const char *lib = avp_cz(a, names.v[k]);
        AvpStr g = strcmp(lib, "unknown") != 0 ? shsm_library_group_of(lib) : AVP_S("unknown");
        bool created;
        AvpVal *gv = avp_map_put(gmap, g, &created);
        if (created) {
            gv->i = (i64)gnames.n;
            avp_vec_push(&gnames, g);
            avp_vec_push(&g3, 0);
            avp_vec_push(&g3, 0);
            avp_vec_push(&g3, gv->i);
        }
        i64 ix = avp_map_get(libs, names.v[k])->i;
        g3.v[3 * gv->i] += cnt.v[2 * ix];
        g3.v[3 * gv->i + 1] += cnt.v[2 * ix + 1];
    }
    avp_sort(g3.v, gnames.n, 3 * sizeof(i64), cmp_group, NULL, false);
    avp_buf_put(&b, AVP_S("by library group (mapped/Wii functions): "));
    for (size_t k = 0; k < gnames.n; k++) {
        const i64 *t = &g3.v[3 * k];
        avp_buf_pyfmt(&b, k ? ", %S %d/%d" : "%S %d/%d", &gnames.v[t[2]], t[1], t[0]);
    }
    line(x, avp_buf_str(&b));
    avp_buf_free(&b);
    line(x, avp_pyfmt(a, "inlined-helper rows %d; PS2 functions inlined somewhere with no Wii counterpart %d",
                      x->w->m.inl.n, unmapped));
    const AvpJson *sec = avp_json_getz(m, "seconds");
    AvpStr s_ps2 = pystr(a, avp_json_getz(sec, "ps2")), s_wii = pystr(a, avp_json_getz(sec, "wii")),
           s_map = pystr(a, avp_json_getz(sec, "map")), s_val = pystr(a, avp_json_getz(sec, "validate"));
    AvpStr rebuilt = x->w->has_built && x->w->built != 0.0 ? avp_pyfmt(a, "; this run rebuilt in %v s", x->w->built)
                                                           : AVP_S("");
    line(x, avp_pyfmt(a, "build seconds: PS2 %S, Wii %S, mapping %S, validation %S%S; load %v s", &s_ps2, &s_wii,
                      &s_map, &s_val, &rebuilt, x->w->load_seconds));
    shsm_output_data(x->out, "ps2_functions", jint(a, nps2));
    shsm_output_data(x->out, "wii_functions", jint(a, nwii));
    shsm_output_data(x->out, "pairs", jint(a, npairs));
    shsm_output_data(x->out, "tiers", tiers);
    shsm_output_data(x->out, "libraries", libj);
    shsm_output_data(x->out, "inlined_rows", jint(a, (i64)x->w->m.inl.n));
    shsm_output_data(x->out, "helpers_unmapped", jint(a, unmapped));
    shsm_output_data(x->out, "validation", (AvpJson *)v);
    shsm_output_data(x->out, "facts", (AvpJson *)f);
    shsm_output_data(x->out, "seconds", (AvpJson *)sec);
    avp_vec_free(&g3);
    avp_vec_free(&gnames);
    avp_map_free(gmap);
    avp_vec_free(&names);
    avp_vec_free(&cnt);
    avp_map_free(libs);
    return 0;
}

int wt_cmd(const WtArgs *args, WtWitness *w, ShsmOutput *out) {
    /* the context lives on the heap: it is read after a throw out of the subcommand (setjmp) */
    Cx *x = avp_xcalloc(1, sizeof *x);
    x->a = w->arena;
    x->w = w;
    x->c = w->m.c;
    x->args = args;
    x->out = out;
    int rc = -1;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (strcmp(args->cmd, "of") == 0) rc = cmd_of(x);
        else if (strcmp(args->cmd, "inlined") == 0) rc = cmd_inlined(x);
        else if (strcmp(args->cmd, "helpers") == 0) rc = cmd_helpers(x);
        else if (strcmp(args->cmd, "order") == 0) rc = cmd_order(x);
        else if (strcmp(args->cmd, "ps2") == 0) rc = cmd_ps2(x);
        else rc = cmd_stats(x);
        avp_try_end(&t);
        t.code = 0;
    }
    for (size_t k = 0; k < x->helpers.n; k++) avp_iset_free(x->helpers.v[k].sites);
    avp_vec_free(&x->helpers);
    if (x->helper_idx) avp_imap_free(x->helper_idx);
    free(x);
    if (t.code) avp_throw(t.code, "%s", t.msg);
    return rc;
}
