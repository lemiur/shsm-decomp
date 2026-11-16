/* hashes.py: the hash functions, value classification, the DOL and archive readers, the static-initialiser pair
 * reader, words, source names and pair classes. */
#include "hashes.h"

#include "avp/fmt.h"
#include "avp/fs.h"
#include "avp/hash.h"
#include "shsm_mangle.h"
#include "shsm_re.h"

#include <string.h>

/* ---- hash functions ---- */

static AvpStr cstr(AvpStr b) {
    const char *z = b.n ? memchr(b.p, 0, b.n) : NULL;
    return z ? avp_strn(b.p, (size_t)(z - b.p)) : b;
}

static u32 sc(u8 c) { return (u32)(i32)(i8)c; } /* lbz + extsb */

u32 hashes_arcname(AvpStr b) {
    b = cstr(b);
    i64 i = avp_str_rfind(b, AVP_S("\\"), 0, AVP_END), j = avp_str_rfind(b, AVP_S("/"), 0, AVP_END);
    b = avp_str_slice(b, (i > j ? i : j) + 1, AVP_END);
    u32 h = 0;
    for (size_t k = 0; k < b.n; k++) {
        u8 c = (u8)b.p[k];
        if (c >= 0x41 && c <= 0x5A) c = (u8)(c + 0x20);
        h = (h * 33u) ^ sc(c);
    }
    return h;
}

static u32 djb2_core(AvpStr b, i64 maxlen, u8 mask) {
    u32 h = 5381;
    b = cstr(b);
    for (size_t k = 0; k < b.n; k++) {
        if (maxlen == 0) break;
        h = h * 33u + sc((u8)((u8)b.p[k] & mask));
        maxlen--;
    }
    return h;
}

u32 hashes_djb2(AvpStr b, i64 maxlen) { return djb2_core(b, maxlen, 0xFF); }
u32 hashes_djb2fold(AvpStr b, i64 maxlen) { return djb2_core(b, maxlen, 0xDF); }

u32 hashes_eventxor(AvpStr b) {
    b = cstr(b);
    u32 h = 0;
    for (size_t k = 0; k < b.n; k++) h = (h << 1) ^ sc((u8)b.p[k]);
    return h;
}

u32 hashes_crc32(AvpStr b) { return avp_crc32(0, b.p, b.n); }

static u32 f_arcname(AvpStr b) { return hashes_arcname(b); }
static u32 f_djb2(AvpStr b) { return hashes_djb2(b, -1); }
static u32 f_djb2fold(AvpStr b) { return hashes_djb2fold(b, -1); }

const HashesFn HASHES[HASHES_COUNT] = {
    {"arcname", f_arcname, "fn_801B2C7C",
     "archive entry lookup: strlen, back to the last '\\\\' or '/', 'A'..'Z' + 0x20, slwi 5 + add + xor, "
     "compared with the 16-byte entries' first word",
     true},
    {"djb2", f_djb2, "fn_801B6020",
     "li 0x1505; extsb; slwi 5; add; add; max-length counter (123 callers, the class registry ctor among them)", true},
    {"djb2fold", f_djb2fold, "fn_801B605C",
     "as fn_801B6020 with rlwinm 0,27,25 (c & ~0x20) before extsb (100 callers; inlined in fn_801D2554, "
     "fn_801B651C)",
     true},
    {"eventxor", hashes_eventxor, "HashString (src/climax/EventHandler.cpp, inlined)",
     "matched source of climax/EventHandler: hash = (hash << 1) ^ *c", true},
    {"crc32", hashes_crc32, "fn_801B570C",
     "table from 0x04C11DB7 with bit reversal (= 0xEDB88320), init ~0, final xor ~0; a data checksum", false},
};

const HashesFn *hashes_by_name(const char *name) {
    for (int i = 0; i < HASHES_COUNT; i++)
        if (name && strcmp(HASHES[i].name, name) == 0) return &HASHES[i];
    return NULL;
}

AvpJson *hashes_fn_json(AvpArena *a, const HashesFn *h) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("name"), avp_json_strz(a, h->name));
    avp_json_set(a, o, AVP_S("routine"), avp_json_strz(a, h->routine));
    avp_json_set(a, o, AVP_S("evidence"), avp_json_strz(a, h->evidence));
    avp_json_set(a, o, AVP_S("name_hash"), avp_json_bool(h->name_hash));
    return o;
}

/* ---- value classification ---- */

static bool contiguous(u64 v) {
    if (v == 0) return true;
    while (!(v & 1)) v >>= 1;
    return (v & (v + 1)) == 0;
}

const char *hashes_excluded(u32 v) {
    if (v < 0x10000u || v >= 0xFFFF0000u) return "small integer";
    if ((v >= 0x80000000u && v < 0x81800000u) || (v >= 0x90000000u && v < 0x94000000u) ||
        (v >= 0xC0000000u && v < 0xD0000000u))
        return "address";
    if (contiguous(v) || contiguous((u32)~v)) return "bit mask";
    return NULL;
}

double hashes_expected_chance(i64 corpus_size, i64 candidates) {
    return (double)(corpus_size * candidates) / 4294967296.0;
}

const char *hashes_strength(double e) { return e < 0.01 ? "strong" : e < 0.1 ? "medium" : "weak"; }

/* ---- DOL and archives ---- */

bool hashes_cstring(const ShsmDol *d, i64 address, AvpStr *out) {
    for (size_t k = 0; k < d->nsegs; k++) {
        const ShsmDolSegment *s = &d->segs[k];
        if (s->addr <= address && address < s->addr + s->size) {
            i64 start = s->off + address - s->addr;
            i64 end = s->off + (s->size < address - s->addr + 512 ? s->size : address - s->addr + 512);
            if (end > (i64)d->size) end = (i64)d->size;
            if (start > end) start = end;
            AvpStr blob = avp_strn((const char *)d->data + start, (size_t)(end - start));
            i64 i = avp_str_findc(blob, '\0');
            if (i <= 0) return false;
            for (i64 k2 = 0; k2 < i; k2++) {
                u8 c = (u8)blob.p[k2];
                if (!((c >= 0x20 && c < 0x7F) || c >= 0x80)) return false;
            }
            *out = avp_strn(blob.p, (size_t)i);
            return true;
        }
    }
    return false;
}

bool hashes_dol_read(const ShsmDol *d, i64 address, i64 size, AvpStr *out) {
    return shsm_dol_read_within(d, address, size, out);
}

AvpU64Vec hashes_read_arc_table(const char *path) {
    AvpU64Vec out = {0};
    AvpFile *f = avp_fopen(path, "rb", AVP_TM_UTF8);
    if (!f) shsm_raise_io();
    u8 head[16];
    i64 got = avp_fread(f, head, 16);
    if (got == 16) {
        u32 magic = head[0] | head[1] << 8 | head[2] << 16 | (u32)head[3] << 24;
        u32 count = head[4] | head[5] << 8 | head[6] << 16 | (u32)head[7] << 24;
        if (magic == HASHES_ARC_MAGIC && count <= 1000000) {
            size_t want = 16 * (size_t)count;
            u8 *ent = avp_xmalloc(want ? want : 1);
            size_t have = 0;
            while (have < want) {
                i64 r = avp_fread(f, ent + have, want - have);
                if (r <= 0) break;
                have += (size_t)r;
            }
            for (size_t i = 0; i < have / 16; i++) {
                const u8 *e = ent + 16 * i;
                avp_vec_push(&out, (u64)(e[0] | e[1] << 8 | e[2] << 16 | (u32)e[3] << 24));
            }
            avp_xfree(ent);
        }
    }
    avp_fclose(f);
    return out;
}

static bool printable(u8 c) { return c >= 0x20 && c <= 0x7E; }

HashesDolStrings hashes_dol_strings(AvpArena *a, const ShsmDol *d) {
    HashesDolStrings out = {0};
    AvpMap *seen = avp_map_new();
    for (size_t k = 0; k < d->nsegs; k++) {
        const ShsmDolSegment *s = &d->segs[k];
        if (shsm_dol_seg_is_text(s)) continue;
        const u8 *p = s->data;
        size_t n = (size_t)s->size, i = 0;
        while (i < n) {
            if (!printable(p[i])) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < n && printable(p[j])) j++;
            if (j < n && p[j] == 0 && j - i >= 2) {
                AvpStr t = avp_strn((const char *)p + i, j - i);
                bool created;
                avp_map_put(seen, t, &created);
                if (created) avp_vec_push(&out, ((HashesDolString){avp_str_dup(a, t), s->addr + (i64)i}));
            }
            i = j;
        }
    }
    avp_map_free(seen);
    return out;
}

void hashes_scan_code_words(const u32 *words, size_t n, i64 base, HashesConsts *out) {
    for (size_t i = 0; i < n; i++) {
        u32 w = words[i];
        if (w >> 26 != 15) continue;
        u32 rd = (w >> 21) & 31, ra = (w >> 16) & 31, imm = w & 0xFFFF;
        if (ra == 0) { /* lis rd, imm */
            size_t end = n < i + 17 ? n : i + 17;
            for (size_t j = i + 1; j < end; j++) {
                u32 x = words[j], op = x >> 26;
                if (op == 24 && ((x >> 21) & 31) == rd) { /* ori rx, rd, lo */
                    avp_vec_push(out, ((HashesConst){(imm << 16) | (x & 0xFFFF), "lis/ori", base + 4 * (i64)i}));
                    break;
                }
                if (op == 14 && ((x >> 16) & 31) == rd) { /* addi rx, rd, lo */
                    u32 lo = x & 0xFFFF;
                    u32 v = (imm << 16) + (u32)(i32)(i16)lo;
                    avp_vec_push(out, ((HashesConst){v, "lis/addi", base + 4 * (i64)i}));
                    break;
                }
                if ((op == 14 || op == 15) && ((x >> 21) & 31) == rd) break; /* rd redefined */
            }
        } else { /* addis rd, ra, imm */
            size_t end = n < i + 4 ? n : i + 4;
            for (size_t j = i + 1; j < end; j++) {
                u32 x = words[j];
                if (x >> 26 == 10 && ((x >> 16) & 31) == rd) { /* cmplwi rd, lo */
                    avp_vec_push(out, ((HashesConst){(u32)(0u - (imm << 16)) + (x & 0xFFFF), "addis/cmplwi",
                                                     base + 4 * (i64)i}));
                    break;
                }
            }
        }
    }
}

HashesConsts hashes_code_constants(const ShsmDol *d) {
    HashesConsts out = {0};
    for (size_t k = 0; k < d->nsegs; k++) {
        const ShsmDolSegment *s = &d->segs[k];
        size_t n = (size_t)s->size / 4;
        u32 *words = avp_xmalloc((n ? n : 1) * sizeof *words);
        for (size_t i = 0; i < n; i++) {
            const u8 *p = s->data + 4 * i;
            words[i] = (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
        }
        if (!shsm_dol_seg_is_text(s)) {
            for (size_t i = 0; i < n; i++) avp_vec_push(&out, ((HashesConst){words[i], "data", s->addr + 4 * (i64)i}));
        } else {
            hashes_scan_code_words(words, n, s->addr, &out);
        }
        avp_xfree(words);
    }
    return out;
}

/* ---- the pair reader ---- */

const HashesRoutine HASHES_CALL_ROUTINES[5] = {
    {0x801A7A84u, {"intern", "r3", false}}, {0x801B61E4u, {"intern", "r3", false}},
    {0x801B6020u, {"djb2", "r3", false}},   {0x801B605Cu, {"djb2fold", "r3", false}},
    {0x801A8F6Cu, {"registry", "r4", true}},
};

const char *hashes_call_term(const char *call) {
    if (!call) return NULL;
    if (strcmp(call, "intern") == 0) return "interned name";
    if (strcmp(call, "djb2") == 0) return "djb2 hash";
    if (strcmp(call, "djb2fold") == 0) return "djb2fold hash";
    if (strcmp(call, "registry") == 0) return "class-registry object";
    return call;
}

enum { RV_HA, RV_ADDR, RV_VAL };
typedef struct {
    int t;
    AvpStr sym;          /* ha, addr (p NULL: None) */
    i64 off;             /* ha, addr */
    bool ptr;            /* addr: ("ptr", a) */
    i64 addr;            /* addr */
    const char *kind;    /* val */
    AvpStr text;         /* val */
    const char *via;     /* val */
    i64 site;            /* val */
} RegVal;

static const char *const VOLATILE[] = {"r0", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12"};

static RegVal *reg_get(const AvpMap *regs, AvpStr name) {
    if (!name.p) return NULL;
    AvpVal *v = avp_map_get(regs, name);
    return v ? v->p : NULL;
}

static bool sym_eq(AvpStr a, AvpStr b) {
    if (!a.p || !b.p) return !a.p && !b.p;
    return avp_str_eq(a, b);
}

static bool is_reg(AvpStr s) { /* ^r(\d+)$ */
    if (s.n < 2 || s.p[0] != 'r') return false;
    for (size_t i = 1; i < s.n; i++)
        if (s.p[i] < '0' || s.p[i] > '9') return false;
    return true;
}

/* _MEM: ^(.*)\((r\d+)\)$ -> the base register, p NULL when it does not match. */
static AvpStr mem_base(AvpStr op) {
    if (op.n < 4 || op.p[op.n - 1] != ')') return (AvpStr){NULL, 0};
    i64 i = avp_str_rfind(op, AVP_S("("), 0, AVP_END);
    if (i < 0) return (AvpStr){NULL, 0};
    AvpStr r = avp_strn(op.p + i + 1, op.n - (size_t)i - 2);
    return is_reg(r) ? r : (AvpStr){NULL, 0};
}

static i64 row_off(const ShsmAsmRow *row) { return row->sym_off == SHSM_NONE ? 0 : row->sym_off; }

static bool reloc_is(const ShsmAsmRow *row, const char *r) { return row->reloc && strcmp(row->reloc, r) == 0; }

static bool addr_of(const HashesReader *rd, const ShsmAsmRow *row, i64 *out) {
    if (rd->target_of) return rd->target_of(rd->ctx, row, out);
    if (!row->sym.p || !rd->sym_addr(rd->ctx, row->sym, out)) return false;
    *out += row_off(row);
    return true;
}

HashesPairs hashes_read_pairs(AvpArena *a, const ShsmAsmBlock *b, const AvpMap *calls, const HashesReader *rd) {
    HashesPairs out = {0};
    AvpMap *regs = avp_map_new();
    AvpISet *labels = avp_iset_new();
    for (size_t i = 0; i < b->labels.n; i++) avp_iset_add(labels, (u64)b->labels.v[i].address);
    for (size_t r = 0; r < b->rows.n; r++) {
        const ShsmAsmRow *row = &b->rows.v[r];
        if (avp_iset_has(labels, (u64)row->address)) avp_map_clear(regs);
        AvpStr m = row->mnemonic;
        AvpStrVec ops = {0};
        if (row->operands.p && row->operands.n) {
            AvpStrVec parts = avp_str_split(row->operands, AVP_S(","), -1);
            for (size_t k = 0; k < parts.n; k++) avp_vec_push(&ops, avp_str_strip(parts.v[k]));
            avp_vec_free(&parts);
        }
        if (!ops.n) continue;
        AvpStr dst = ops.v[0];
        const AvpVal *cv = row->sym.p ? avp_map_get(calls, row->sym) : NULL;
        if (avp_str_eqz(m, "bl") && cv) {
            const HashesCallSpec *spec = cv->p;
            RegVal *src = reg_get(regs, avp_str(spec->sreg));
            AvpStr text = {NULL, 0};
            const char *via = NULL;
            if (src && src->t == RV_ADDR) {
                if (!rd->deref(rd->ctx, src->ptr, src->addr, &text, &via)) text = (AvpStr){NULL, 0};
            }
            RegVal *res = NULL;
            if (text.p) {
                if (spec->obj) {
                    RegVal *var = reg_get(regs, AVP_S("r3"));
                    if (var && var->t == RV_ADDR && !var->ptr) /* a pointer value cannot be a variable address */
                        avp_vec_push(&out, ((HashesPair){var->addr, var->sym, var->off, text, spec->kind, via,
                                                         row->address}));
                } else {
                    res = AVP_NEW(a, RegVal);
                    *res = (RegVal){.t = RV_VAL, .kind = spec->kind, .text = text, .via = via, .site = row->address};
                }
            }
            for (size_t k = 0; k < sizeof VOLATILE / sizeof *VOLATILE; k++) avp_map_del(regs, avp_str(VOLATILE[k]));
            if (res) avp_map_put(regs, AVP_S("r3"), NULL)->p = res;
            avp_vec_free(&ops);
            continue;
        }
        if (avp_str_eqz(m, "bl")) {
            for (size_t k = 0; k < sizeof VOLATILE / sizeof *VOLATILE; k++) avp_map_del(regs, avp_str(VOLATILE[k]));
            avp_vec_free(&ops);
            continue;
        }
        if (m.n && m.p[0] == 'b') {
            avp_vec_free(&ops);
            continue;
        }
        if ((avp_str_eqz(m, "stw") || avp_str_eqz(m, "stwu")) && ops.n == 2) {
            RegVal *v = reg_get(regs, dst);
            if (v && v->t == RV_VAL && row->sym.p && row->sym.n) {
                AvpStr base = mem_base(ops.v[1]);
                RegVal *bv = reg_get(regs, base);
                bool ok = reloc_is(row, "sda21") ||
                          (reloc_is(row, "l") && bv && bv->t == RV_HA && sym_eq(bv->sym, row->sym));
                i64 at;
                if (ok && addr_of(rd, row, &at))
                    avp_vec_push(&out, ((HashesPair){at, row->sym, row_off(row), v->text, v->kind, v->via, v->site}));
            }
            avp_vec_free(&ops);
            continue;
        }
        if (!is_reg(dst)) {
            avp_vec_free(&ops);
            continue;
        }
        RegVal *val = NULL;
        i64 at;
        if (avp_str_eqz(m, "lis") && reloc_is(row, "ha")) {
            val = AVP_NEW(a, RegVal);
            *val = (RegVal){.t = RV_HA, .sym = row->sym, .off = row_off(row)};
        } else if (avp_str_eqz(m, "li") && reloc_is(row, "sda21")) {
            if (addr_of(rd, row, &at)) {
                val = AVP_NEW(a, RegVal);
                *val = (RegVal){.t = RV_ADDR, .addr = at, .sym = row->sym, .off = row_off(row)};
            }
        } else if (avp_str_eqz(m, "addi") && (reloc_is(row, "l") || reloc_is(row, "sda21")) && ops.n == 3) {
            RegVal *src = reg_get(regs, ops.v[1]);
            if (reloc_is(row, "sda21") || (src && src->t == RV_HA && sym_eq(src->sym, row->sym))) {
                if (addr_of(rd, row, &at)) {
                    val = AVP_NEW(a, RegVal);
                    *val = (RegVal){.t = RV_ADDR, .addr = at, .sym = row->sym, .off = row_off(row)};
                }
            }
        } else if (avp_str_eqz(m, "addi") && !(row->reloc && *row->reloc) && ops.n == 3) {
            RegVal *src = reg_get(regs, ops.v[1]);
            if (src && src->t == RV_ADDR && !src->ptr) {
                i64 k;
                if (avp_parse_int(ops.v[2], 0, &k, NULL)) {
                    val = AVP_NEW(a, RegVal);
                    *val = (RegVal){.t = RV_ADDR, .addr = src->addr + k, .sym = src->sym, .off = src->off + k};
                }
            }
        } else if (avp_str_eqz(m, "lwz") && (reloc_is(row, "l") || reloc_is(row, "sda21")) && ops.n == 2) {
            AvpStr base = mem_base(ops.v[1]);
            RegVal *src = reg_get(regs, base);
            if (reloc_is(row, "sda21") || (src && src->t == RV_HA && sym_eq(src->sym, row->sym))) {
                if (addr_of(rd, row, &at)) {
                    val = AVP_NEW(a, RegVal);
                    *val = (RegVal){.t = RV_ADDR, .ptr = true, .addr = at, .sym = row->sym, .off = row_off(row)};
                }
            }
        } else if (avp_str_eqz(m, "mr") && ops.n == 2) {
            val = reg_get(regs, ops.v[1]);
        }
        avp_map_del(regs, dst);
        if (val) avp_map_put(regs, dst, NULL)->p = val;
        avp_vec_free(&ops);
    }
    avp_iset_free(labels);
    avp_map_free(regs);
    return out;
}

/* ---- corpus helpers ---- */

static bool bytes_ws(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0B || c == 0x0C; }

static AvpStr bytes_strip(AvpStr s) {
    while (s.n && bytes_ws((u8)s.p[0])) s.p++, s.n--;
    while (s.n && bytes_ws((u8)s.p[s.n - 1])) s.n--;
    return s;
}

/* re.match(pattern + "$", b) where the body is all of b, or all of b but a final "\n" ($ before a trailing newline). */
static AvpStr dollar_body(AvpStr b) {
    if (b.n && b.p[b.n - 1] == '\n') return avp_strn(b.p, b.n - 1);
    return b;
}

static bool base_like(AvpStr b) { /* ^[A-Za-z_][A-Za-z0-9_.\-]{1,63}$ */
    AvpStr bodies[2] = {b, dollar_body(b)};
    for (int k = 0; k < (bodies[1].n != b.n ? 2 : 1); k++) {
        AvpStr s = bodies[k];
        if (s.n < 2 || s.n > 64) continue;
        u8 c = (u8)s.p[0];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) continue;
        bool ok = true;
        for (size_t i = 1; i < s.n && ok; i++) {
            c = (u8)s.p[i];
            ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
                 c == '-';
        }
        if (ok) return true;
    }
    return false;
}

static bool format_like(AvpStr b) {
    if (avp_str_count(b, AVP_S("%")) != 1 || !avp_str_contains(b, AVP_S("%s")) || b.n > 48) return false;
    AvpStr st = bytes_strip(b);
    if (st.n != b.n) return false;
    AvpStr body = dollar_body(b); /* ^[\x21-\x7e]+$ */
    if (!body.n) return false;
    for (size_t i = 0; i < body.n; i++)
        if ((u8)body.p[i] < 0x21 || (u8)body.p[i] > 0x7E) return false;
    return true;
}

void hashes_format_derivations(AvpArena *a, const AvpStr *corpus, size_t n, AvpStrVec *fmts, AvpStrVec *bases) {
    (void)a;
    for (size_t i = 0; i < n; i++)
        if (format_like(corpus[i])) avp_vec_push(fmts, corpus[i]);
    for (size_t i = 0; i < n; i++)
        if (base_like(corpus[i])) avp_vec_push(bases, corpus[i]);
}

static bool split_byte(u8 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0C || c == 0x0B || c == ',' || c == ';' ||
           c == '=' || c == '"' || c == '\'' || c == '<' || c == '>' || c == '(' || c == ')' || c == '[' || c == ']' ||
           c == '{' || c == '}';
}

AvpSet *hashes_read_words(const AvpStrVec *paths) {
    AvpSet *words = avp_set_new();
    for (size_t pi = 0; paths && pi < paths->n; pi++) {
        AvpBytes data = {0};
        const char *path = avp_cz(avp_scratch(), paths->v[pi]);
        if (!avp_read_all(NULL, path, &data)) shsm_raise_io();
        const u8 *p = data.v;
        size_t n = data.n, ns = n < 4096 ? n : 4096, ctl = 0;
        bool binary = ns && memchr(p, 0, ns) != NULL;
        for (size_t i = 0; i < ns; i++)
            if (p[i] < 9 || (p[i] > 13 && p[i] < 32)) ctl++;
        if (ctl > ns / 20) binary = true;
        if (binary) {
            size_t i = 0;
            while (i < n) {
                if (!printable(p[i])) {
                    i++;
                    continue;
                }
                size_t j = i;
                while (j < n && printable(p[j])) j++;
                if (j - i >= 4) avp_set_add(words, avp_strn((const char *)p + i, j - i));
                i = j;
            }
            avp_vec_free(&data);
            continue;
        }
        size_t i = 0;
        while (i < n) { /* bytes.splitlines(): \n, \r, \r\n */
            size_t j = i;
            while (j < n && p[j] != '\n' && p[j] != '\r') j++;
            AvpStr line = bytes_strip(avp_strn((const char *)p + i, j - i));
            if (j < n && p[j] == '\r' && j + 1 < n && p[j + 1] == '\n') j++;
            i = j + 1;
            if (!line.n) continue;
            avp_set_add(words, line);
            size_t k = 0;
            while (k <= line.n) { /* re.split(rb"[\s,;=\"'<>()\[\]{}]+", line) */
                size_t e = k;
                while (e < line.n && !split_byte((u8)line.p[e])) e++;
                if (e - k >= 2) avp_set_add(words, avp_strn(line.p + k, e - k));
                if (e >= line.n) break;
                while (e < line.n && split_byte((u8)line.p[e])) e++;
                k = e;
            }
        }
        avp_vec_free(&data);
    }
    return words;
}

static bool has_suffix(AvpStr s) {
    static const char *const SUFFIXES[] = {".c", ".cpp", ".h", ".hpp", ".inc"};
    for (size_t i = 0; i < 5; i++)
        if (avp_str_endswith(s, avp_str(SUFFIXES[i]))) return true;
    return false;
}

AvpMap *hashes_source_names(const ShsmRepo *r) {
    AvpMap *out = avp_map_new();
    const char *tops[2] = {r->src_dir, r->include_dir};
    const ShsmRe *lbl = shsm_re("\\b(lbl_[0-9A-F]{8})\\b", 0);
    const ShsmRe *quoted = shsm_re("\"((?:[^\"\\\\]|\\\\.)*)\"", 0);
    AvpArena *s = avp_scratch();
    for (int t = 0; t < 2; t++) {
        AvpWalk *w = avp_walk_open(tops[t], true);
        if (!w) continue;
        AvpWalkItem item = {0};
        while (avp_walk_next(w, &item)) {
            for (size_t f = 0; f < item.files.n; f++) {
                if (!has_suffix(item.files.v[f])) continue;
                AvpArenaMark mk = avp_arena_mark(s);
                const char *path = avp_path_join(s, avp_cz(s, item.dirpath), avp_cz(s, item.files.v[f]));
                AvpStrVec lines = {0};
                if (avp_read_lines(s, path, AVP_TM_UTF8_REPLACE, &lines)) {
                    for (size_t l = 0; l < lines.n; l++) {
                        AvpStr line = lines.v[l];
                        if (!avp_str_contains(line, AVP_S("lbl_")) || !avp_str_contains(line, AVP_S("\""))) continue;
                        AvpStrVec qs = {0};
                        ShsmReIter it;
                        ShsmReMatch mm;
                        shsm_re_iter_init(&it, quoted, line, 0, AVP_END);
                        while (shsm_re_iter_next(&it, &mm)) avp_vec_push(&qs, shsm_re_group(&mm, 1));
                        shsm_re_iter_init(&it, lbl, line, 0, AVP_END);
                        while (shsm_re_iter_next(&it, &mm)) {
                            bool created;
                            AvpVal *v = avp_map_put(out, shsm_re_group(&mm, 1), &created);
                            if (created) v->p = avp_set_new();
                            for (size_t q = 0; q < qs.n; q++) avp_set_add(v->p, qs.v[q]);
                        }
                        avp_vec_free(&qs);
                    }
                }
                avp_vec_free(&lines);
                avp_arena_reset(s, mk);
            }
        }
        avp_walk_close(w);
    }
    return out;
}

const char *hashes_var_status(const AvpMap *names, AvpStr var, AvpStr text) {
    const AvpVal *v = var.p ? avp_map_get(names, var) : NULL;
    if (!v || !v->p || !avp_set_len(v->p)) return "new";
    return avp_set_has(v->p, text) ? "named" : "mismatch";
}

/* The scope's last part of a mangled function name (mangle.parse(fn).scope[-1]), from the demangled qualified name:
 * the parts split at "::" outside <> and (), the one before the name, without a return type in front. */
static AvpStr scope_last(AvpArena *a, AvpStr fn) {
    const char *name = avp_cz(a, fn);
    const char *q = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        if (shsm_demangle(a, name)) q = shsm_qualified(a, name);
        avp_try_end(&t);
    } else {
        q = NULL;
    }
    if (!q) return (AvpStr){NULL, 0};
    AvpStr s = avp_str(q);
    AVP_VEC(AvpStr) parts = {0};
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.n; i++) {
        char c = s.p[i];
        if (c == '<' || c == '(') depth++;
        else if ((c == '>' || c == ')') && depth > 0) depth--;
        else if (depth == 0 && c == ':' && i + 1 < s.n && s.p[i + 1] == ':') {
            avp_vec_push(&parts, avp_strn(s.p + start, i - start));
            start = i + 2;
            i++;
        }
    }
    avp_vec_push(&parts, avp_strn(s.p + start, s.n - start));
    AvpStr out = {NULL, 0};
    if (parts.n >= 2) {
        out = parts.v[parts.n - 2];
        depth = 0;
        size_t cut = 0;
        for (size_t i = 0; i < out.n; i++) {
            char c = out.p[i];
            if (c == '<' || c == '(') depth++;
            else if ((c == '>' || c == ')') && depth > 0) depth--;
            else if (depth == 0 && c == ' ') cut = i + 1;
        }
        out = avp_strn(out.p + cut, out.n - cut);
    }
    avp_vec_free(&parts);
    return out;
}

void hashes_pair_classes(AvpArena *a, AvpStr function, AvpStr text, AvpStr call, AvpStr *primary, AvpStr *fcls) {
    AvpStr fn = function.p ? function : AVP_S("");
    AvpStr fc = AVP_S("");
    static const AvpStr PRE = {"__sinit_\\", 9}, POST = {"_cpp", 4};
    if (fn.n >= PRE.n + 1 + POST.n && avp_str_startswith(fn, PRE) && avp_str_endswith(fn, POST) &&
        !avp_str_contains(avp_strn(fn.p + PRE.n, fn.n - PRE.n - POST.n), AVP_S("\n"))) {
        fc = avp_strn(fn.p + PRE.n, fn.n - PRE.n - POST.n);
    } else {
        AvpStr sl = scope_last(a, fn);
        if (sl.p) fc = sl;
    }
    AvpStr pr = (avp_str_eqz(call, "intern") || avp_str_eqz(call, "registry")) ? text : fc;
    *primary = (pr.p && pr.n) ? pr : AVP_S("-");
    *fcls = fc;
}

/* ---- text ---- */

AvpStr hashes_text(AvpArena *a, AvpStr bytes) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_reserve(&b, bytes.n + 1);
    for (size_t i = 0; i < bytes.n; i++) avp_buf_putcp(&b, (u8)bytes.p[i]);
    if (!b.p) return AVP_S("");
    return avp_buf_str(&b);
}

AvpStr hashes_bytes(AvpArena *a, AvpStr text) {
    AvpBuf b;
    avp_buf_init(&b, a);
    const char *p = text.p, *end = text.p + text.n;
    while (p < end) {
        u32 cp = avp_u8_next(&p, end);
        avp_buf_putc(&b, cp <= 0xFF ? (char)(u8)cp : '?');
    }
    if (!b.p) return AVP_S("");
    return avp_buf_str(&b);
}

AvpStr hashes_show(AvpArena *a, AvpStr t) {
    AvpStr s = avp_str_replace(a, t, AVP_S("\\"), AVP_S("\\\\"), -1);
    s = avp_str_replace(a, s, AVP_S("\""), AVP_S("\\\""), -1);
    s = avp_str_replace(a, s, AVP_S("\n"), AVP_S("\\n"), -1);
    return avp_pyfmt(a, "\"%S\"", &s);
}
