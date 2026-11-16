/* query.py find-float and find-const (const_hits, const_line). */
#include <math.h>
#include <string.h>
#include "query_a.h"

static u32 be32(const unsigned char *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* _f32(v): struct.pack(">f", v) as the bits; false on OverflowError (a finite double beyond the float range). */
static bool f32_bits(double v, u32 *bits) {
    float f = (float)v;
    if (isinf(f) && !isinf(v)) return false;
    memcpy(bits, &f, 4);
    return true;
}

/* ---- const_hits ---- */

static const char *dform(u32 op) {
    switch (op) {
    case 14: return "addi";
    case 32: return "lwz";
    case 33: return "lwzu";
    case 34: return "lbz";
    case 35: return "lbzu";
    case 36: return "stw";
    case 37: return "stwu";
    case 38: return "stb";
    case 39: return "stbu";
    case 40: return "lhz";
    case 41: return "lhzu";
    case 42: return "lha";
    case 43: return "lhau";
    case 44: return "sth";
    case 45: return "sthu";
    case 48: return "lfs";
    case 49: return "lfsu";
    case 50: return "lfd";
    case 51: return "lfdu";
    case 52: return "stfs";
    case 53: return "stfsu";
    case 54: return "stfd";
    case 55: return "stfdu";
    default: return NULL;
    }
}

static void push_hit(QueryConstHits *hits, i64 address, const char *kind, i64 pair, AvpStr detail) {
    avp_vec_push(hits, ((QueryConstHit){address, kind, pair, detail, {NULL, 0}, SHSM_NONE, {NULL, 0}, "?"}));
}

QueryConstHits query_const_hits(QueryCtx *ctx, i64 value, bool data) {
    AvpArena *a = ctx->a;
    const QueryDol *dol = query_dol(ctx);
    if (!dol) shsm_raise(SHSM_EXC_USAGE, "no retail DOL at %s", ctx->repo->retail_dol);
    u32 v = (u32)(value & 0xFFFFFFFF);
    QueryConstHits hits = {0};
    u32 hi = v >> 16, lo = v & 0xFFFF;
    u32 hi_adj = ((v + 0x8000u) >> 16) & 0xFFFF; /* the wrap past 32 bits is what & 0xFFFF drops */
    i64 lo_s = (lo & 0x8000) ? (i64)lo - 0x10000 : (i64)lo;
    for (size_t si = 0; si < dol->nsections; si++) {
        const QueryDolSection *sec = &dol->sections[si];
        size_t off = sec->offset, sz = sec->size;
        if (off > dol->data.n) off = dol->data.n;
        if (sz > dol->data.n - off) sz = dol->data.n - off;
        const unsigned char *blob = (const unsigned char *)dol->data.p + off;
        if (!sec->text) {
            if (!data) continue;
            for (size_t i = 0; i + 4 <= sz; i += 4)
                if (be32(blob + i) == v)
                    push_hit(&hits, (i64)sec->address + (i64)i, "data word", SHSM_NONE,
                             avp_pyfmt(a, ".4byte 0x%08X", v));
            continue;
        }
        /* lis.finditer(blob): [\x3C-\x3F][\x00\x20..\xE0](hi|hi_adj), non-overlapping */
        size_t i = 0;
        while (i + 4 <= sz) {
            u32 h16 = (u32)blob[i + 2] << 8 | blob[i + 3];
            if (!(blob[i] >= 0x3C && blob[i] <= 0x3F && (blob[i + 1] & 0x1F) == 0 && (h16 == hi || h16 == hi_adj))) {
                i++;
                continue;
            }
            size_t at = i;
            i += 4;
            if (at % 4) continue;
            u32 w = be32(blob + at);
            u32 imm = w & 0xFFFF, rd = (w >> 21) & 31;
            i64 addr = (i64)sec->address + (i64)at;
            if (lo == 0 && imm == hi) {
                push_hit(&hits, addr, "lis", SHSM_NONE, avp_pyfmt(a, "lis r%d, 0x%X", rd, hi));
                continue;
            }
            size_t jend = at + 68 < sz - 3 ? at + 68 : sz - 3;
            for (size_t j = at + 4; j < jend; j += 4) {
                u32 x = be32(blob + j);
                u32 op = x >> 26;
                if (op == 24 && imm == hi && ((x >> 21) & 31) == rd && (x & 0xFFFF) == lo) {
                    push_hit(&hits, addr, "lis/ori", (i64)sec->address + (i64)j,
                             avp_pyfmt(a, "lis r%d, 0x%X; ori r%d, r%d, 0x%X", rd, hi, (x >> 16) & 31, rd, lo));
                    break;
                }
                const char *mn = dform(op);
                if (mn && imm == hi_adj && ((x >> 16) & 31) == rd) {
                    i64 s16 = x & 0xFFFF;
                    if (s16 & 0x8000) s16 -= 0x10000;
                    if (s16 == lo_s) {
                        const char *kind = avp_cz(a, avp_pyfmt(a, "lis/%s", mn));
                        push_hit(&hits, addr, kind, (i64)sec->address + (i64)j,
                                 avp_pyfmt(a, "lis r%d, 0x%X; %s ..., %d(r%d)", rd, hi_adj, mn, s16, rd));
                        break;
                    }
                }
                if (op == 15 && ((x >> 21) & 31) == rd) break; /* register reloaded */
            }
        }
    }
    for (size_t k = 0; k < hits.n; k++) {
        QueryConstHit *h = &hits.v[k];
        i64 base = SHSM_NONE;
        AvpStr unit = {NULL, 0};
        h->symbol = query_locate(ctx, h->address, &base, &unit);
        h->offset = h->symbol.p && h->symbol.n ? h->address - base : SHSM_NONE;
        h->unit = unit;
        h->status = query_status(ctx, unit);
    }
    return hits;
}

AvpJson *query_const_hit_json(AvpArena *a, const QueryConstHit *h) {
    AvpJson *j = avp_json_obj(a);
    avp_json_set(a, j, AVP_S("address"), avp_json_int(a, h->address));
    avp_json_set(a, j, AVP_S("kind"), avp_json_strz(a, h->kind));
    if (h->pair != SHSM_NONE) avp_json_set(a, j, AVP_S("pair"), avp_json_int(a, h->pair));
    avp_json_set(a, j, AVP_S("detail"), avp_json_str(a, h->detail));
    avp_json_set(a, j, AVP_S("symbol"), shsm_json_opt_str(a, h->symbol));
    avp_json_set(a, j, AVP_S("offset"), shsm_json_opt_int(a, h->offset));
    avp_json_set(a, j, AVP_S("unit"), shsm_json_opt_str(a, h->unit));
    avp_json_set(a, j, AVP_S("status"), avp_json_strz(a, h->status));
    return j;
}

AvpStr query_const_line(AvpArena *a, const QueryConstHit *h) {
    AvpStr where = h->symbol.p && h->symbol.n ? avp_pyfmt(a, "%S+0x%X", &h->symbol, h->offset) : AVP_S("?");
    AvpStr unit = h->unit.p && h->unit.n ? h->unit : AVP_S("?");
    return avp_pyfmt(a, "%08X %-10s %-44S in %S  %S %s", h->address, h->kind, &h->detail, &where, &unit, h->status);
}

static AvpJson *hits_json(AvpArena *a, const QueryConstHits *hits) {
    AvpJson *arr = avp_json_arr(a);
    for (size_t i = 0; i < hits->n; i++) avp_json_arr_push(a, arr, query_const_hit_json(a, &hits->v[i]));
    return arr;
}

/* ---- find-float ---- */

/* str.rstrip("fF") */
static AvpStr rstrip_f(AvpStr s) {
    while (s.n && (s.p[s.n - 1] == 'f' || s.p[s.n - 1] == 'F')) s.n--;
    return s;
}

int query_cmd_find_float(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr value = avp_args_str(args, "value");
    double v = 0;
    if (!avp_str_startswith(avp_str_lower(a, value), AVP_S("0x"))) {
        if (!avp_parse_float(rstrip_f(value), &v)) shsm_raise(SHSM_EXC_USAGE, "not a float: %s", avp_cz(a, value));
    } else {
        i64 n = 0;
        bool overflow = false;
        if (!avp_parse_int(value, 16, &n, &overflow)) {
            if (!overflow) shsm_raise(SHSM_EXC_USAGE, "not a float: %s", avp_cz(a, value));
            n = -1; /* beyond 64 bits: struct.pack(">I") refuses it below */
        }
        if (n < 0 || n > 0xFFFFFFFF)
            shsm_raise(SHSM_EXC_VALUE, "'I' format requires 0 <= number <= 4294967295");
        u32 bits = (u32)n;
        float f;
        memcpy(&f, &bits, 4);
        v = (double)f;
    }
    u32 want32 = 0;
    bool has32 = f32_bits(v, &want32);
    AvpJson *rows = query_sql(ctx, "SELECT owner, address, kind, value, text FROM floats ORDER BY address", 0);
    AvpJson *hits = avp_json_arr(a);
    size_t nrows = avp_json_len(rows);
    for (size_t i = 0; i < nrows; i++) {
        AvpJson *r = avp_json_at(rows, i);
        AvpStr kind = avp_json_str_or(avp_json_at(r, 2), AVP_S(""));
        double value_ = avp_json_f64(avp_json_at(r, 3), 0);
        bool ok;
        if (avp_str_eqz(kind, "float")) {
            u32 bits = 0;
            ok = has32 && f32_bits(value_, &bits) && bits == want32;
        } else {
            ok = value_ == v || (isfinite(v) && fabs(value_ - v) <= 1e-12 * fmax(1.0, fabs(v)));
        }
        if (!ok) continue;
        i64 addr = avp_json_at(r, 1)->u.i;
        AvpStr owner = avp_json_str_or(avp_json_at(r, 0), (AvpStr){NULL, 0});
        AvpStr text = avp_json_str_or(avp_json_at(r, 4), (AvpStr){NULL, 0});
        QueryUsers users = query_string_users(ctx, addr);
        AvpJson *h = avp_json_obj(a);
        avp_json_set(a, h, AVP_S("object"), avp_json_at(r, 0));
        avp_json_set(a, h, AVP_S("address"), avp_json_int(a, addr));
        avp_json_set(a, h, AVP_S("kind"), avp_json_at(r, 2));
        avp_json_set(a, h, AVP_S("text"), avp_json_at(r, 4));
        const char *unit = shsm_asmindex_unit_of(a, query_ix(ctx), addr);
        avp_json_set(a, h, AVP_S("unit"), unit ? avp_json_strz(a, unit) : avp_json_null());
        AvpJson *ju = avp_json_arr(a);
        AvpStr *names = AVP_NEWN(a, AvpStr, users.n + 1);
        for (size_t k = 0; k < users.n; k++) {
            AvpJson *x = avp_json_obj(a);
            avp_json_set(a, x, AVP_S("name"), avp_json_str(a, users.v[k].name));
            avp_json_set(a, x, AVP_S("kind"), avp_json_str(a, users.v[k].kind));
            avp_json_arr_push(a, ju, x);
            names[k] = users.v[k].name;
        }
        avp_json_set(a, h, AVP_S("users"), ju);
        avp_json_arr_push(a, hits, h);
        AvpStr us = avp_str_join(a, AVP_S(", "), names, users.n < 4 ? users.n : 4);
        if (users.n > 4) us = avp_pyfmt(a, "%S +%d", &us, users.n - 4);
        if (!us.n) us = AVP_S("(no direct reference)");
        AvpStr ko = owner.p ? owner : AVP_S("None"), kt = text.p ? text : AVP_S("None");
        shsm_output_line(out, avp_pyfmt(a, "%08X %-6S %-12S %-14S <- %S", addr, &kind, &kt, &ko, &us));
        avp_vec_free(&users);
    }
    QueryConstHits imm = {0};
    if (avp_args_bool(args, "imm") && has32) {
        imm = query_const_hits(ctx, want32, false);
        for (size_t k = 0; k < imm.n; k++) shsm_output_line(out, query_const_line(a, &imm.v[k]));
    }
    shsm_output_data(out, "floats", hits);
    shsm_output_data(out, "immediates", hits_json(a, &imm));
    int rc = SHSM_EXIT_OK;
    if (!avp_json_len(hits) && !imm.n) {
        char buf[32];
        avp_float_repr(v, buf);
        shsm_output_line(out, avp_pyfmt(a, "no float literal %s", buf));
        rc = SHSM_EXIT_FINDING;
    }
    avp_vec_free(&imm);
    return rc;
}

/* ---- find-const ---- */

/* parse_int(text), with Python's unbounded ints: *wide set when the value does not fit 64 bits (it is then out of
 * the 32-bit range whatever its sign). query_parse_int raises "not a number" for those. */
static i64 parse_int_wide(AvpArena *a, AvpStr text, bool *wide) {
    AvpStr t = avp_str_replace(a, avp_str_strip(text), AVP_S("_"), AVP_S(""), -1);
    bool neg = avp_str_startswith(t, AVP_S("-"));
    while (t.n && (t.p[0] == '+' || t.p[0] == '-')) t = avp_str_slice(t, 1, AVP_END);
    AvpStr low = avp_str_lower(a, avp_str_slice(t, 0, 2));
    i64 v = 0;
    bool ok, overflow = false;
    if (avp_str_eqz(low, "0d")) ok = avp_parse_int(avp_str_slice(t, 2, AVP_END), 10, &v, &overflow);
    else ok = avp_parse_int(avp_str_eqz(low, "0x") ? avp_str_slice(t, 2, AVP_END) : t, 16, &v, &overflow);
    *wide = !ok && overflow;
    if (!ok && !overflow) shsm_raise(SHSM_EXC_USAGE, "not a number: %s", avp_cz(a, text));
    return neg ? -v : v;
}

int query_cmd_find_const(QueryCtx *ctx, AvpArgs *args, ShsmOutput *out) {
    AvpArena *a = ctx->a;
    AvpStr text = avp_args_str(args, "value");
    bool wide = false;
    i64 v = parse_int_wide(a, text, &wide);
    if (wide || v < -0x80000000LL || v > 0xFFFFFFFFLL)
        shsm_raise(SHSM_EXC_USAGE, "not a 32-bit value: %s", avp_cz(a, text));
    QueryConstHits hits = query_const_hits(ctx, v, true);
    u32 u = (u32)(v & 0xFFFFFFFF);
    const char *d = NULL;
    if (u >= 0x80000000u && u < 0x81800000u) d = shsm_sym_describe(a, query_syms(ctx), u);
    if (d && strncmp(d, "0x", 2) != 0) shsm_output_line(out, avp_pyfmt(a, "0x%08X is the address %s", u, d));
    for (size_t k = 0; k < hits.n; k++) shsm_output_line(out, query_const_line(a, &hits.v[k]));
    shsm_output_data(out, "value", avp_json_int(a, u));
    shsm_output_data(out, "address_of", d ? avp_json_strz(a, d) : avp_json_null());
    shsm_output_data(out, "hits", hits_json(a, &hits));
    int rc = SHSM_EXIT_OK;
    if (!hits.n) {
        shsm_output_line(out, avp_pyfmt(a, "0x%08X not found", u));
        rc = SHSM_EXIT_FINDING;
    }
    avp_vec_free(&hits);
    return rc;
}
