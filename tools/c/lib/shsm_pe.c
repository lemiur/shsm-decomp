/* shsm_pe: PE headers and the CodeView NB11 reader of tools/mwcc_trace/symbols.py. See shsm_pe.h and shsm_pe.md. */
#include "shsm_pe.h"

#include <string.h>

#define SST_MODULE 0x120
#define SST_GLOBAL_PUB 0x12A
#define SST_ALIGN_SYM 0x125
#define S_PUB32 0x1009
#define S_LPROC32 0x100A
#define S_GPROC32 0x100B

const char *shsm_pe_exc_class(int code) {
    switch (code) {
    case SHSM_PE_EXC_NO_DEBUG_INFO: return "NoDebugInfo";
    case SHSM_PE_EXC_STRUCT: return "struct.error";
    case SHSM_PE_EXC_INDEX: return "IndexError";
    case AVP_ERR_IO: return "OSError";
    default: return "Exception";
    }
}

/* ---- struct.unpack_from ------------------------------------------------------------------------------------- */

static void need(size_t n, u64 off, unsigned size) {
    if (off + size > n)
        avp_throw(SHSM_PE_EXC_STRUCT,
                  "unpack_from requires a buffer of at least %llu bytes for unpacking %u bytes at offset %llu "
                  "(actual buffer size is %zu)",
                  (unsigned long long)(off + size), size, (unsigned long long)off, n);
}

u32 shsm_pe_u32(const u8 *d, size_t n, u64 off) {
    need(n, off, 4);
    const u8 *p = d + off;
    return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}

u16 shsm_pe_u16(const u8 *d, size_t n, u64 off) {
    need(n, off, 2);
    const u8 *p = d + off;
    return (u16)(p[0] | p[1] << 8);
}

/* rd32/rd16 inside a record whose whole size was checked with need() */
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

/* bytes[i] (IndexError past the end) */
static u8 byte_at(const u8 *d, size_t n, u64 i) {
    if (i >= n) avp_throw(SHSM_PE_EXC_INDEX, "index out of range");
    return d[i];
}

/* bytes[lo:lo+len].decode('latin-1') as UTF-8 (the slice is clamped as Python clamps it) */
static AvpStr latin1(AvpArena *a, const u8 *d, size_t n, u64 lo, u64 len) {
    if (lo >= n) return AVP_S("");
    u64 hi = lo + len > n ? n : lo + len;
    size_t k = (size_t)(hi - lo), extra = 0;
    for (size_t i = 0; i < k; i++) extra += d[lo + i] >= 0x80;
    char *s = avp_alloc(a, k + extra + 1);
    size_t o = 0;
    for (size_t i = 0; i < k; i++) {
        u8 c = d[lo + i];
        if (c < 0x80) {
            s[o++] = (char)c;
        } else {
            s[o++] = (char)(0xC0 | c >> 6);
            s[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return (AvpStr){s, o};
}

/* ---- PE headers ------------------------------------------------------------------------------------------------ */

void shsm_pe_headers(AvpArena *a, const u8 *d, size_t n, ShsmPeRead how, ShsmPeHeaders *out) {
    memset(out, 0, sizeof *out);
    out->pe = shsm_pe_u32(d, n, 0x3C);
    u64 pe = out->pe;
    out->pe_sig = pe + 4 <= n && memcmp(d + pe, "PE\0\0", 4) == 0;
    if (how == SHSM_PE_SYMBOLS && !out->pe_sig) avp_throw(SHSM_PE_EXC_NO_DEBUG_INFO, "not a PE file");
    out->nsec = shsm_pe_u16(d, n, pe + 6);
    out->optsz = shsm_pe_u16(d, n, pe + 20);
    u64 opt = pe + 24;
    out->opt = (u32)opt;
    out->base = shsm_pe_u32(d, n, opt + 28);
    if (how == SHSM_PE_SYMBOLS) out->ndd = shsm_pe_u32(d, n, opt + 92);
    out->secs = AVP_NEWN(a, ShsmPeSection, out->nsec ? out->nsec : 1);
    out->nsecs = out->nsec;
    u64 so = opt + out->optsz;
    for (size_t i = 0; i < out->nsec; i++) {
        u64 o = so + 40 * (u64)i;
        ShsmPeSection *s = &out->secs[i];
        if (how == SHSM_PE_SYMBOLS) {
            need(n, o, 24); /* '<8sIIII' */
        } else {
            /* name = d[o:o+8] (a slice), then '<IIII' at o+8 and '<I' at o+36 */
            need(n, o + 8, 16);
            need(n, o + 36, 4);
        }
        size_t k = 0;
        for (; k < 8 && o + k < n && d[o + k]; k++) s->name[k] = (char)d[o + k];
        s->name[k] = 0;
        s->vsize = rd32(d + o + 8);
        s->va = rd32(d + o + 12);
        s->rsize = rd32(d + o + 16);
        s->raw = rd32(d + o + 20);
        if (how == SHSM_PE_DISASM) s->ch = rd32(d + o + 36);
    }
    if (how == SHSM_PE_SYMBOLS && out->ndd > 6) {
        out->dbg_rva = shsm_pe_u32(d, n, opt + 96 + 6 * 8);
        out->dbg_size = shsm_pe_u32(d, n, opt + 96 + 6 * 8 + 4);
    }
}

/* ---- NB11 ------------------------------------------------------------------------------------------------------ */

typedef struct {
    u16 st, im;
    u32 lfo, cb;
} Ent;

typedef struct {
    u32 plen;
    AvpStr name;
} Proc;

typedef struct {
    i64 va;
    AvpStr name;
} VaName;

typedef struct {
    u32 seg, o, n;
} SegRange;

/* sec_va(seg) = base + secs[seg - 1][0], with Python's negative index (seg 0 is the last section) */
static i64 sec_va(const ShsmPeHeaders *h, u32 seg) {
    i64 i = (i64)seg - 1;
    if (i < 0) i += (i64)h->nsecs;
    if (i < 0 || i >= (i64)h->nsecs) avp_throw(SHSM_PE_EXC_INDEX, "list index out of range");
    return (i64)h->base + h->secs[i].va;
}

static int cmp_vaname(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const VaName *p = x, *q = y;
    if (p->va != q->va) return p->va < q->va ? -1 : 1;
    return avp_str_cmp(p->name, q->name);
}

static int cmp_range(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const ShsmPeRange *p = x, *q = y;
    if (p->lo != q->lo) return p->lo < q->lo ? -1 : 1;
    if (p->hi != q->hi) return p->hi < q->hi ? -1 : 1;
    return avp_str_cmp(p->module, q->module);
}

static u64 seg_key(u32 seg, u32 off) { return (u64)seg << 32 | off; }

ShsmPeSymbols *shsm_pe_symbols_parse(AvpArena *a, const u8 *d, size_t n) {
    ShsmPeHeaders h;
    shsm_pe_headers(a, d, n, SHSM_PE_SYMBOLS, &h);

    /* _find_nb11 */
    if (!h.dbg_size) avp_throw(SHSM_PE_EXC_NO_DEBUG_INFO, "no PE debug directory");
    bool found = false;
    u64 fo = 0;
    for (size_t i = 0; i < h.nsecs; i++) {
        const ShsmPeSection *s = &h.secs[i];
        u64 span = s->vsize > s->rsize ? s->vsize : s->rsize;
        if (s->va <= h.dbg_rva && h.dbg_rva < (u64)s->va + span) {
            fo = (u64)h.dbg_rva - s->va + s->raw;
            found = true;
        }
    }
    if (!found) avp_throw(SHSM_PE_EXC_NO_DEBUG_INFO, "debug directory outside any section");
    u64 off = 0, size = 0;
    found = false;
    for (u64 i = 0; i < h.dbg_size / 28; i++) {
        u64 e = fo + i * 28;
        need(n, e, 28); /* '<IIHHIIII' */
        u32 typ = rd32(d + e + 12), sz = rd32(d + e + 16), ptr = rd32(d + e + 24);
        if (typ == 2 && (u64)ptr + 4 <= n && memcmp(d + ptr, "NB11", 4) == 0) {
            off = ptr;
            size = sz;
            found = true;
            break;
        }
    }
    if (!found) avp_throw(SHSM_PE_EXC_NO_DEBUG_INFO, "debug directory has no CodeView NB11 entry");

    /* blob = d[off:off + size] */
    const u8 *blob = d + off;
    size_t bn = (size_t)((off + size > n ? n : off + size) - off);

    u64 dir_off = shsm_pe_u32(blob, bn, 4);
    need(bn, dir_off, 8); /* '<HHI' */
    u16 hs = rd16(blob + dir_off), es = rd16(blob + dir_off + 2);
    u32 cnt = rd32(blob + dir_off + 4);
    AVP_VEC(Ent) ents = {0};
    for (u32 i = 0; i < cnt; i++) {
        u64 e = dir_off + hs + (u64)i * es;
        need(bn, e, 12); /* '<HHII' */
        Ent x = {rd16(blob + e), rd16(blob + e + 2), rd32(blob + e + 4), rd32(blob + e + 8)};
        avp_vec_push(&ents, x);
    }

    /* sstModule: module ranges */
    AVP_VEC(ShsmPeRange) ranges = {0};
    for (size_t k = 0; k < ents.n; k++) {
        const Ent *e = &ents.v[k];
        if (e->st != SST_MODULE) continue;
        need(bn, e->lfo, 8); /* '<HHHH' */
        u16 cseg = rd16(blob + e->lfo + 4);
        u64 p = (u64)e->lfo + 8;
        AVP_VEC(SegRange) segs = {0};
        for (u32 i = 0; i < cseg; i++) {
            need(bn, p, 12); /* '<HHII' */
            avp_vec_push(&segs, ((SegRange){rd16(blob + p), rd32(blob + p + 4), rd32(blob + p + 8)}));
            p += 12;
        }
        u8 nlen = byte_at(blob, bn, p);
        AvpStr name = latin1(a, blob, bn, p + 1, nlen);
        for (size_t i = 0; i < segs.n; i++) {
            u32 seg = segs.v[i].seg, o = segs.v[i].o, cnt2 = segs.v[i].n;
            if (seg && cnt2 && seg <= h.nsecs) {
                i64 lo = sec_va(&h, seg) + o;
                avp_vec_push(&ranges, ((ShsmPeRange){lo, lo + cnt2, name}));
            }
        }
        avp_vec_free(&segs);
    }
    avp_sort(ranges.v, ranges.n, sizeof *ranges.v, cmp_range, NULL, false);

    /* sstGlobalPub: S_PUB32 records */
    const Ent *pub = NULL;
    for (size_t k = 0; k < ents.n && !pub; k++)
        if (ents.v[k].st == SST_GLOBAL_PUB) pub = &ents.v[k];
    if (!pub) avp_throw(SHSM_PE_EXC_NO_DEBUG_INFO, "NB11 has no sstGlobalPub");
    AvpIMap *named = avp_imap_new(); /* (seg, o) -> index into names, dict order */
    AVP_VEC(AvpStr) names = {0};
    {
        u64 lfo = pub->lfo;
        u32 cbsym = shsm_pe_u32(blob, bn, lfo + 4);
        u64 p = lfo + 16, end = lfo + 16 + cbsym;
        while (p + 4 <= end) {
            need(bn, p, 4);
            u16 rl = rd16(blob + p), rt = rd16(blob + p + 2);
            if (rt == S_PUB32) {
                need(bn, p + 4, 10); /* '<IIH' */
                u32 o = rd32(blob + p + 8);
                u16 seg = rd16(blob + p + 12);
                u8 nlen = byte_at(blob, bn, p + 14);
                AvpStr name = shsm_pe_strip(latin1(a, blob, bn, p + 15, nlen));
                bool created;
                AvpVal *v = avp_imap_put(named, seg_key(seg, o), &created);
                if (created) {
                    v->i = (i64)names.n;
                    avp_vec_push(&names, name);
                } else {
                    names.v[v->i] = name;
                }
            }
            p += 2 + (u64)rl;
        }
    }

    /* sstAlignSym: S_LPROC32 / S_GPROC32 */
    AvpIMap *procs = avp_imap_new();
    AVP_VEC(Proc) procv = {0};
    for (size_t k = 0; k < ents.n; k++) {
        const Ent *e = &ents.v[k];
        if (e->st != SST_ALIGN_SYM) continue;
        u64 q = (u64)e->lfo + 4, qend = (u64)e->lfo + e->cb;
        while (q + 4 <= qend) {
            need(bn, q, 4);
            u16 rl = rd16(blob + q), rt = rd16(blob + q + 2);
            if (rt == S_LPROC32 || rt == S_GPROC32) {
                u32 plen = shsm_pe_u32(blob, bn, q + 16);
                need(bn, q + 32, 6); /* '<IH' */
                u32 o = rd32(blob + q + 32);
                u16 seg = rd16(blob + q + 36);
                u8 nlen = byte_at(blob, bn, q + 39);
                AvpStr name = shsm_pe_strip(latin1(a, blob, bn, q + 40, nlen));
                bool created;
                AvpVal *v = avp_imap_put(procs, seg_key(seg, o), &created);
                if (created) {
                    v->i = (i64)procv.n;
                    avp_vec_push(&procv, ((Proc){plen, name}));
                } else {
                    procv.v[v->i] = (Proc){plen, name};
                }
            }
            q += 2 + (u64)rl;
        }
    }

    /* named.setdefault for every procedure */
    {
        AvpIMapIter it = {0};
        while (avp_imap_next(procs, &it)) {
            bool created;
            AvpVal *v = avp_imap_put(named, it.key, &created);
            if (created) {
                v->i = (i64)names.n;
                avp_vec_push(&names, procv.v[it.val->i].name);
            }
        }
    }

    const u32 text_seg = 1;
    AVP_VEC(VaName) code = {0};
    {
        AvpIMapIter it = {0};
        while (avp_imap_next(named, &it)) {
            u32 s = (u32)(it.key >> 32), o = (u32)it.key;
            if (s == text_seg) avp_vec_push(&code, ((VaName){sec_va(&h, s) + o, names.v[it.val->i]}));
        }
    }
    avp_sort(code.v, code.n, sizeof *code.v, cmp_vaname, NULL, false);
    AvpIMap *exact = avp_imap_new();
    {
        AvpIMapIter it = {0};
        while (avp_imap_next(procs, &it)) {
            u32 s = (u32)(it.key >> 32), o = (u32)it.key;
            avp_imap_put(exact, (u64)(sec_va(&h, s) + o), NULL)->i = procv.v[it.val->i].plen;
        }
    }
    i64 t_lo = sec_va(&h, text_seg);
    const ShsmPeSection *ts = &h.secs[0];
    i64 t_hi = t_lo + (ts->vsize > ts->rsize ? ts->vsize : ts->rsize);

    ShsmPeSymbols *t = AVP_NEW(a, ShsmPeSymbols);
    t->base = h.base;
    t->ranges = AVP_NEWN(a, ShsmPeRange, ranges.n ? ranges.n : 1);
    if (ranges.n) memcpy(t->ranges, ranges.v, ranges.n * sizeof *ranges.v);
    t->nranges = ranges.n;
    t->funcs = AVP_NEWN(a, ShsmPeFunc, code.n ? code.n : 1);
    t->nfuncs = code.n;
    for (size_t i = 0; i < code.n; i++) {
        i64 va = code.v[i].va;
        AvpStr mod = shsm_pe_symbols_module_at(t, va);
        i64 nxt = i + 1 < code.n ? code.v[i + 1].va : t_hi;
        t->funcs[i] = (ShsmPeFunc){va, nxt - va, code.v[i].name, mod.n ? mod : AVP_S("?")};
    }
    for (size_t i = 0; i < code.n; i++) {
        ShsmPeFunc *f = &t->funcs[i];
        for (size_t r = 0; r < t->nranges; r++) {
            if (t->ranges[r].lo <= f->va && f->va < t->ranges[r].hi) {
                if (t->ranges[r].hi - f->va < f->size) f->size = t->ranges[r].hi - f->va;
                break;
            }
        }
        AvpVal *x = avp_imap_get(exact, (u64)f->va);
        if (x) f->size = x->i;
    }
    AVP_VEC(VaName) data = {0};
    {
        AvpIMapIter it = {0};
        while (avp_imap_next(named, &it)) {
            u32 s = (u32)(it.key >> 32), o = (u32)it.key;
            if (s != text_seg) avp_vec_push(&data, ((VaName){sec_va(&h, s) + o, names.v[it.val->i]}));
        }
    }
    avp_sort(data.v, data.n, sizeof *data.v, cmp_vaname, NULL, false);
    t->data = AVP_NEWN(a, ShsmPeDatum, data.n ? data.n : 1);
    t->ndata = data.n;
    for (size_t i = 0; i < data.n; i++) t->data[i] = (ShsmPeDatum){data.v[i].va, data.v[i].name};

    avp_vec_free(&ents);
    avp_vec_free(&ranges);
    avp_vec_free(&names);
    avp_vec_free(&procv);
    avp_vec_free(&code);
    avp_vec_free(&data);
    avp_imap_free(named);
    avp_imap_free(procs);
    avp_imap_free(exact);
    return t;
}

ShsmPeSymbols *shsm_pe_symbols_load(AvpArena *a, const char *exe) {
    AvpBytes b = {0};
    if (!avp_read_all(NULL, exe, &b)) {
        AvpStr msg = avp_io_err_str(a);
        avp_throw(AVP_ERR_IO, "%.*s", AVP_SF(msg));
    }
    AvpTry t;
    ShsmPeSymbols *r = NULL;
    if (AVP_TRY(&t)) {
        r = shsm_pe_symbols_parse(a, b.v, b.n);
        avp_try_end(&t);
    } else {
        avp_vec_free(&b);
        avp_throw(t.code, "%s", t.msg);
    }
    avp_vec_free(&b);
    return r;
}

/* ---- queries --------------------------------------------------------------------------------------------------- */

/* bisect.bisect_right(addrs, va) - 1 */
static i64 index_le(const ShsmPeSymbols *t, i64 va) {
    size_t lo = 0, hi = t->nfuncs;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (va < t->funcs[mid].va) hi = mid;
        else lo = mid + 1;
    }
    return (i64)lo - 1;
}

const ShsmPeFunc *shsm_pe_symbols_lookup(const ShsmPeSymbols *t, i64 va) {
    i64 i = index_le(t, va);
    return i >= 0 ? &t->funcs[i] : NULL;
}

size_t shsm_pe_symbols_find(AvpArena *a, const ShsmPeSymbols *t, AvpStr text, const ShsmPeFunc ***out) {
    AvpStr low = avp_str_lower(a, text);
    const ShsmPeFunc **v = AVP_NEWN(a, const ShsmPeFunc *, t->nfuncs ? t->nfuncs : 1);
    AvpStr *lows = AVP_NEWN(a, AvpStr, t->nfuncs ? t->nfuncs : 1);
    size_t n = 0;
    for (size_t i = 0; i < t->nfuncs; i++) {
        lows[i] = avp_str_lower(a, t->funcs[i].name);
        if (avp_str_eq(lows[i], low)) v[n++] = &t->funcs[i];
    }
    if (!n)
        for (size_t i = 0; i < t->nfuncs; i++)
            if (avp_str_find(lows[i], low, 0, AVP_END) >= 0) v[n++] = &t->funcs[i];
    *out = v;
    return n;
}

AvpStr shsm_pe_symbols_module_at(const ShsmPeSymbols *t, i64 va) {
    for (size_t i = 0; i < t->nranges; i++)
        if (t->ranges[i].lo <= va && va < t->ranges[i].hi) return t->ranges[i].module;
    return (AvpStr){0};
}

AvpStr shsm_pe_symbols_describe(AvpArena *a, const ShsmPeSymbols *t, i64 va) {
    i64 i = index_le(t, va);
    const ShsmPeFunc *prev = i >= 0 ? &t->funcs[i] : NULL;
    const ShsmPeFunc *nxt = i + 1 < (i64)t->nfuncs ? &t->funcs[i + 1] : NULL;
    if (prev && va < prev->va + prev->size) {
        if (prev->va == va) return avp_str(avp_sprintf(a, "%.*s (%.*s)", AVP_SF(prev->name), AVP_SF(prev->module)));
        return avp_str(avp_sprintf(a, "%.*s+0x%llx (%.*s)", AVP_SF(prev->name), (unsigned long long)(va - prev->va),
                                   AVP_SF(prev->module)));
    }
    AvpStr mod = shsm_pe_symbols_module_at(t, va);
    if (!mod.p) return (AvpStr){0};
    if (prev && nxt && avp_str_eq(prev->module, nxt->module) && avp_str_eq(nxt->module, mod))
        return avp_str(avp_sprintf(a, "static in %.*s between %.*s and %.*s", AVP_SF(mod), AVP_SF(prev->name),
                                   AVP_SF(nxt->name)));
    if (prev && avp_str_eq(prev->module, mod))
        return avp_str(avp_sprintf(a, "static in %.*s after %.*s", AVP_SF(mod), AVP_SF(prev->name)));
    if (nxt && avp_str_eq(nxt->module, mod))
        return avp_str(avp_sprintf(a, "static in %.*s before %.*s", AVP_SF(mod), AVP_SF(nxt->name)));
    return avp_str(avp_sprintf(a, "static in %.*s", AVP_SF(mod)));
}

AvpStr shsm_pe_symbols_describe_data(const ShsmPeSymbols *t, i64 va) {
    size_t lo = 0, hi = t->ndata;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (va < t->data[mid].va) hi = mid;
        else lo = mid + 1;
    }
    if (lo == 0) return (AvpStr){0};
    const ShsmPeDatum *x = &t->data[lo - 1];
    return x->va == va ? x->name : (AvpStr){0};
}

/* ---- names and versions ---------------------------------------------------------------------------------------- */

AvpStr shsm_pe_strip(AvpStr name) {
    if (name.n && name.p[0] == '?') return name;
    if (name.n && name.p[0] == '_') name = (AvpStr){name.p + 1, name.n - 1};
    /* re.match(r'(.*)@[0-9]+$', name): '.' stops at a newline, '$' also matches before a final newline; the
     * greedy group takes the last '@' followed only by digits up to the end (or the end less one '\n'). */
    size_t len = name.n, first_nl = len;
    for (size_t i = 0; i < len; i++)
        if (name.p[i] == '\n') {
            first_nl = i;
            break;
        }
    /* the digit run must end where '$' matches: the end, or before a final newline */
    size_t ends[2] = {len, len};
    if (len && name.p[len - 1] == '\n') ends[1] = len - 1;
    size_t top = first_nl < len ? first_nl : len; /* (.*) cannot cross a newline: at <= first_nl */
    for (size_t at = top + 1; at-- > 0;) {        /* greedy: the rightmost '@' first */
        if (at >= len || name.p[at] != '@') continue;
        for (int k = 0; k < 2; k++) {
            size_t e = ends[k];
            if (e < at + 2) continue;
            bool digits = true;
            for (size_t q = at + 1; q < e && digits; q++) digits = name.p[q] >= '0' && name.p[q] <= '9';
            if (digits) return (AvpStr){name.p, at};
        }
    }
    return name;
}

char *shsm_pe_version_of(AvpArena *a, const char *root, const char *path) {
    char *p = avp_path_abspath(a, path);
    char *comp = avp_path_join(a, avp_path_join(a, root, "tools"), "compilers");
    AvpStr dir = avp_path_dirname(p);
    char *dirz = avp_sprintf(a, "%.*s", AVP_SF(dir));
    char *rel;
    if (strncmp(p, comp, strlen(comp)) == 0) {
        if (!avp_path_relpath(a, dirz, comp, &rel)) {
            AvpStr msg = avp_io_err_str(a);
            avp_throw(AVP_ERR_OTHER, "%.*s", AVP_SF(msg));
        }
    } else {
        AvpStr b = avp_path_basename(dirz);
        rel = b.n ? avp_sprintf(a, "%.*s", AVP_SF(b)) : avp_strdup(a, "compiler");
    }
    for (char *c = rel; *c; c++)
        if (*c == '\\' || *c == '/') *c = '-';
    return rel;
}
