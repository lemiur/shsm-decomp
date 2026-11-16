/* The Wii side of the witness model: tools/witness.py Dol, _wii_file and build_wii (the retail listings and the asm
 * index, plus string literals and float constants read from the retail DOL). */
#include "witness_ps2.h"

#include "avp/fs.h"
#include "avp/pool.h"
#include "shsm_asm.h"
#include "shsm_asmindex.h"
#include "shsm_cli.h"
#include "shsm_libraries.h"
#include "shsm_text.h"

#include <sqlite3.h>
#include <string.h>

/* ---- Dol ---- */

typedef struct {
    i64 a, e, o;
    bool is_data;
} DolSec;

typedef struct {
    const u8 *data;
    size_t size;
    DolSec secs[18];
    int n;
} Dol;

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static int dolsec_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const DolSec *a = x, *b = y;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    if (a->e != b->e) return a->e < b->e ? -1 : 1;
    if (a->o != b->o) return a->o < b->o ? -1 : 1;
    return (int)a->is_data - (int)b->is_data;
}

static bool dol_init(Dol *d, const u8 *data, size_t size) {
    memset(d, 0, sizeof *d);
    d->data = data;
    d->size = size;
    if (size < 0x90 + 18 * 4) shsm_raise(SHSM_EXC_VALUE, "unpack_from requires a buffer of at least 216 bytes");
    for (int k = 0; k < 18; k++) {
        u32 o = be32(data + 4 * k), a = be32(data + 0x48 + 4 * k), n = be32(data + 0x90 + 4 * k);
        if (n) d->secs[d->n++] = (DolSec){a, (i64)a + n, o, k >= 7};
    }
    avp_sort(d->secs, (size_t)d->n, sizeof *d->secs, dolsec_cmp, NULL, false);
    return true;
}

static const DolSec *dol_find(const Dol *d, i64 addr) {
    for (int k = 0; k < d->n; k++)
        if (d->secs[k].a <= addr && addr < d->secs[k].e) return &d->secs[k];
    return NULL;
}

static bool dol_word(const Dol *d, i64 addr, u32 *out) {
    const DolSec *s = dol_find(d, addr);
    if (!s || addr + 4 > s->e) return false;
    i64 off = s->o + addr - s->a;
    if (off < 0 || (u64)off + 4 > d->size) shsm_raise(SHSM_EXC_VALUE, "DOL offset %lld is outside the file", (long long)off);
    *out = be32(d->data + off);
    return true;
}

static const char *dol_cstring(AvpArena *a, const Dol *d, i64 addr) {
    const DolSec *s = dol_find(d, addr);
    if (!s || !s->is_data) return NULL;
    return wt_cstring_at(a, d->data, d->size, s->o + addr - s->a, s->o, s->o + s->e - s->a, 512);
}

/* ---- _wii_file ---- */

typedef struct {
    i64 site;
    AvpStr sym;
} WiiCall;

typedef struct {
    i64 addr;
    const char *name;
    i64 size, nrows, back, frame;
    AVP_VEC(WiiCall) calls;
    AvpI64Vec ints, offs, lfs;
} WiiRow;

typedef struct {
    const char *path, *unit;
} WiiWork;

typedef struct {
    AvpArena *arena;
    AVP_VEC(WiiRow) rows;
    char *error; /* heap, or NULL */
    int code;
} WiiPart;

static int hexval(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* -?0x[0-9A-Fa-f]+|-?\d+ at s[*i], as int(text, 0). */
static bool p_num(AvpStr s, size_t *i, i64 *v) {
    size_t k = *i;
    bool neg = false;
    if (k < s.n && s.p[k] == '-') {
        neg = true;
        k++;
    }
    u64 x = 0;
    if (k + 2 < s.n && s.p[k] == '0' && s.p[k + 1] == 'x' && hexval(s.p[k + 2]) >= 0) {
        k += 2;
        for (int d; k < s.n && (d = hexval(s.p[k])) >= 0; k++) x = x * 16 + (u64)d;
    } else {
        size_t st = k;
        while (k < s.n && s.p[k] >= '0' && s.p[k] <= '9') x = x * 10 + (u64)(s.p[k++] - '0');
        if (k == st) return false;
    }
    *v = neg ? -(i64)x : (i64)x;
    *i = k;
    return true;
}

/* \d+ at s[*i]: the digits as text and value. */
static bool p_digits(AvpStr s, size_t *i, AvpStr *text, i64 *v) {
    size_t k = *i;
    u64 x = 0;
    while (k < s.n && s.p[k] >= '0' && s.p[k] <= '9') x = x * 10 + (u64)(s.p[k++] - '0');
    if (k == *i) return false;
    if (text) *text = avp_strn(s.p + *i, k - *i);
    *v = (i64)x;
    *i = k;
    return true;
}

static bool p_lit(AvpStr s, size_t *i, const char *lit) {
    size_t n = strlen(lit);
    if (*i + n > s.n || memcmp(s.p + *i, lit, n) != 0) return false;
    *i += n;
    return true;
}

/* `$`: the end, or a final newline (operands are stripped: the end). */
static bool p_end(AvpStr s, size_t i) { return i == s.n || (i + 1 == s.n && s.p[i] == '\n'); }

/* _LI: ^r(\d+),\s*(num)$ */
static bool m_li(AvpStr s, i64 *reg, i64 *v) {
    size_t i = 0;
    if (!p_lit(s, &i, "r") || !p_digits(s, &i, NULL, reg) || !p_lit(s, &i, ",")) return false;
    i = shsm_skip_ws(s, i);
    return p_num(s, &i, v) && p_end(s, i);
}

/* _RRI: ^r(\d+),\s*r(\d+),\s*(num)$ */
static bool m_rri(AvpStr s, i64 *src, i64 *v) {
    size_t i = 0;
    i64 dst;
    if (!p_lit(s, &i, "r") || !p_digits(s, &i, NULL, &dst) || !p_lit(s, &i, ",")) return false;
    i = shsm_skip_ws(s, i);
    if (!p_lit(s, &i, "r") || !p_digits(s, &i, NULL, src) || !p_lit(s, &i, ",")) return false;
    i = shsm_skip_ws(s, i);
    return p_num(s, &i, v) && p_end(s, i);
}

/* _MEM: ^[rf]\d+,\s*(num)\(r(\d+)\)$ */
static bool m_mem(AvpStr s, i64 *off, AvpStr *base) {
    size_t i = 0;
    i64 x;
    if (!(s.n && (s.p[0] == 'r' || s.p[0] == 'f'))) return false;
    i = 1;
    if (!p_digits(s, &i, NULL, &x) || !p_lit(s, &i, ",")) return false;
    i = shsm_skip_ws(s, i);
    if (!p_num(s, &i, off) || !p_lit(s, &i, "(r") || !p_digits(s, &i, base, &x) || !p_lit(s, &i, ")")) return false;
    return p_end(s, i);
}

/* _CMP: ^(?:cr\d,\s*)?r\d+,\s*(num)$ */
static bool m_cmp_at(AvpStr s, size_t i, i64 *v) {
    i64 x;
    if (!p_lit(s, &i, "r") || !p_digits(s, &i, NULL, &x) || !p_lit(s, &i, ",")) return false;
    i = shsm_skip_ws(s, i);
    return p_num(s, &i, v) && p_end(s, i);
}

static bool m_cmp(AvpStr s, i64 *v) {
    if (s.n >= 4 && s.p[0] == 'c' && s.p[1] == 'r' && s.p[2] >= '0' && s.p[2] <= '9' && s.p[3] == ',') {
        if (m_cmp_at(s, shsm_skip_ws(s, 4), v)) return true;
    }
    return m_cmp_at(s, 0, v);
}

static bool eqz(AvpStr s, const char *z) { return avp_str_eqz(s, z); }

static int i64_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_cmp_i64(*(const i64 *)x, *(const i64 *)y);
}

static void sorted_set(AvpISet *s, AvpI64Vec *out) {
    size_t pos = 0;
    u64 v;
    while (avp_iset_next(s, &pos, &v)) avp_vec_push(out, (i64)v);
    avp_sort(out->v, out->n, sizeof *out->v, i64_cmp, NULL, false);
}

static void wii_block(AvpArena *a, const ShsmAsmBlock *b, const char *unit, WiiRow *out, AvpIMap *hi) {
    (void)unit;
    memset(out, 0, sizeof *out);
    AvpISet *ints = avp_iset_new(), *offs = avp_iset_new();
    avp_imap_clear(hi);
    i64 back = 0, frame = 0;
    for (size_t r = 0; r < b->rows.n; r++) {
        const ShsmAsmRow *row = &b->rows.v[r];
        AvpStr m = row->mnemonic;
        AvpStr ops = row->operands.p ? avp_str_strip(row->operands) : AVP_S("");
        bool has_sym = row->sym.p && row->sym.n;
        if (row->reloc && strcmp(row->reloc, "rel24") == 0 && has_sym && row->label == SHSM_NONE) {
            WiiCall cl = {row->address, avp_strn(avp_memdup(a, row->sym.p, row->sym.n), row->sym.n)};
            avp_vec_push(&out->calls, cl);
            continue;
        }
        if (row->label != SHSM_NONE && row->label <= row->address && m.n && m.p[0] == 'b') back++;
        if (has_sym) {
            if (eqz(m, "lfs") || eqz(m, "lfsu")) avp_vec_push(&out->lfs, row->address);
            continue;
        }
        i64 reg, v;
        AvpStr base;
        if (eqz(m, "stwu") && avp_str_endswith(ops, AVP_S("(r1)")) && avp_str_startswith(ops, AVP_S("r1,"))) {
            if (m_mem(ops, &v, &base) && !frame) frame = -v;
        } else if (eqz(m, "li")) {
            if (m_li(ops, &reg, &v)) {
                avp_iset_add(ints, (u64)(v & 0xFFFFFFFF));
                avp_imap_del(hi, (u64)reg);
            }
        } else if (eqz(m, "lis")) {
            if (m_li(ops, &reg, &v)) {
                bool created;
                avp_imap_put(hi, (u64)reg, &created)->i = (i64)(((u64)v << 16) & 0xFFFFFFFF);
            }
        } else if (eqz(m, "addi") || eqz(m, "ori") || eqz(m, "subi")) {
            AvpVal *h;
            if (m_rri(ops, &reg, &v) && (h = avp_imap_get(hi, (u64)reg)) != NULL) {
                i64 hb = h->i;
                i64 x = eqz(m, "ori") ? (hb | v) : ((hb + (eqz(m, "subi") ? -v : v)) & 0xFFFFFFFF);
                avp_iset_add(ints, (u64)x);
            }
        } else if (eqz(m, "cmpwi") || eqz(m, "cmplwi")) {
            if (m_cmp(ops, &v)) avp_iset_add(ints, (u64)(v & 0xFFFFFFFF));
        } else if (m.n && (m.p[0] == 'l' || m.p[0] == 's') && memchr(ops.p, '(', ops.n)) {
            if (m_mem(ops, &v, &base) && !eqz(base, "1") && !eqz(base, "2") && !eqz(base, "13"))
                if (0 <= v && v < 0x10000) avp_iset_add(offs, (u64)v);
        }
    }
    /* ints = {v for v in ints if abs(v - (1 << 32) if v & 0x80000000 else v) >= 16} */
    AvpISet *keep = avp_iset_new();
    size_t pos = 0;
    u64 u;
    while (avp_iset_next(ints, &pos, &u)) {
        i64 x = (i64)u, y = (x & 0x80000000LL) ? x - (1LL << 32) : x;
        if ((y < 0 ? -y : y) >= 16) avp_iset_add(keep, u);
    }
    sorted_set(keep, &out->ints);
    sorted_set(offs, &out->offs);
    avp_iset_free(keep);
    avp_iset_free(ints);
    avp_iset_free(offs);
    out->addr = b->address;
    out->name = b->name.p ? avp_cz(a, b->name) : NULL;
    out->size = (b->size != SHSM_NONE && b->size) ? b->size : 4 * (i64)b->rows.n;
    out->nrows = (i64)b->rows.n;
    out->back = back;
    out->frame = frame;
}

static int wii_file(void *ctx, size_t index, const void *item, void *result) {
    (void)ctx;
    (void)index;
    const WiiWork *w = item;
    WiiPart *res = result;
    res->arena = avp_arena_new(0);
    AvpTry t;
    ShsmAsmFile *af = NULL;
    AvpIMap *hi = avp_imap_new();
    if (AVP_TRY(&t)) {
        af = shsm_asm_parse_file(w->path, w->unit, 0);
        for (size_t i = 0; i < af->blocks.n; i++) {
            const ShsmAsmBlock *b = af->blocks.v[i];
            if (strcmp(b->kind, "function") != 0) continue;
            if (!(eqz(b->section, ".text") || eqz(b->section, ".init")) || !b->rows.n) continue;
            WiiRow r;
            wii_block(res->arena, b, w->unit, &r, hi);
            avp_vec_push(&res->rows, r);
        }
        avp_try_end(&t);
    } else {
        res->code = t.code;
        res->error = _strdup(t.msg ? t.msg : "");
    }
    avp_imap_free(hi);
    if (af) shsm_asm_free(af);
    return 0;
}

/* ---- build_wii ---- */

AVP_NORETURN static void sql_fail(sqlite3 *db) { shsm_raise(SHSM_EXC_SQLITE, "%s", sqlite3_errmsg(db)); }

static sqlite3_stmt *prep(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) sql_fail(db);
    return st;
}

static int step(sqlite3 *db, sqlite3_stmt *st) {
    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        sqlite3_finalize(st);
        sql_fail(db);
    }
    return rc;
}

static i64 col_int(sqlite3_stmt *st, int c) {
    return sqlite3_column_type(st, c) == SQLITE_NULL ? WT_NONE : sqlite3_column_int64(st, c);
}

static AvpStr col_text(sqlite3_stmt *st, int c) {
    if (sqlite3_column_type(st, c) == SQLITE_NULL) return (AvpStr){NULL, 0};
    const char *p = (const char *)sqlite3_column_text(st, c);
    return avp_strn(p, (size_t)sqlite3_column_bytes(st, c));
}

static WtFn *get_fn(const WtSide *s, i64 a) { return a == WT_NONE ? NULL : wt_side_get(s, a); }

void wt_build_wii(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix, int jobs, WtSide *fns, WtRuns *runs) {
    sqlite3 *db = ix->db;
    wt_side_init(fns);
    memset(runs, 0, sizeof *runs);
    AVP_VEC(WiiWork) work = {0};
    sqlite3_stmt *st = prep(db, "select distinct f.path, f.unit from files f join symbols s on s.file_id = f.id"
                                " where s.kind = 'function'");
    while (step(db, st) == SQLITE_ROW) {
        AvpStr p = col_text(st, 0), u = col_text(st, 1);
        WiiWork w = {avp_path_join(a, ix->asm_dir ? ix->asm_dir : r->asm_dir, avp_cz(a, p)),
                     u.p ? avp_cz(a, u) : NULL};
        avp_vec_push(&work, w);
    }
    sqlite3_finalize(st);
    WiiPart *parts = avp_xcalloc(work.n ? work.n : 1, sizeof *parts);
    avp_pool_map(NULL, jobs, wii_file, NULL, work.v, work.n, sizeof *work.v, parts, sizeof *parts);
    for (size_t k = 0; k < work.n; k++)
        if (parts[k].error) shsm_raise(parts[k].code, "%s", parts[k].error);
    const ShsmLibraries *libs = shsm_load_libraries(r);
    /* best = {name: sorted(v)[0][1]}: (not global/weak, address) minimal */
    AvpMap *best = avp_map_new();
    st = prep(db, "select name, address, scope from symbols where kind = 'function'");
    while (step(db, st) == SQLITE_ROW) {
        AvpStr name = col_text(st, 0), scope = col_text(st, 2);
        i64 addr = col_int(st, 1);
        i64 local = !(scope.p && (avp_str_eqz(scope, "global") || avp_str_eqz(scope, "weak")));
        bool created;
        AvpVal *v = avp_map_put(best, name.p ? name : AVP_S(""), &created);
        /* pack (local, addr) as one comparable pair: kept in a heap pair */
        i64 *pair = created ? NULL : v->p;
        if (created || local < pair[0] || (local == pair[0] && addr < pair[1])) {
            if (created) v->p = avp_alloc(a, 2 * sizeof(i64));
            ((i64 *)v->p)[0] = local;
            ((i64 *)v->p)[1] = addr;
        }
    }
    sqlite3_finalize(st);
    typedef struct {
        i64 owner, site;
    } LfsSite;
    AVP_VEC(LfsSite) lfs_sites = {0};
    AvpMap *local = avp_map_new();
    for (size_t k = 0; k < work.n; k++) {
        WiiPart *part = &parts[k];
        avp_map_clear(local);
        for (size_t j = 0; j < part->rows.n; j++) {
            bool created;
            avp_map_put(local, avp_str(part->rows.v[j].name), &created)->i = part->rows.v[j].addr;
        }
        for (size_t j = 0; j < part->rows.n; j++) {
            WiiRow *row = &part->rows.v[j];
            WtFn *f = wt_fn_new(row->addr);
            f->size = row->size;
            f->n = row->nrows;
            avp_pyintset_free(f->ints);
            avp_pyintset_free(f->offs);
            f->ints = avp_pyintset_from(row->ints.v, row->ints.n);
            f->offs = avp_pyintset_from(row->offs.v, row->offs.n);
            f->frame = row->frame;
            f->back = row->back;
            f->name = row->name ? avp_strdup(a, row->name) : NULL;
            f->unit = work.v[k].unit;
            const char *lib = shsm_libraries_lookup(a, libs, row->addr);
            f->lib = lib ? avp_strdup(a, lib) : NULL;
            for (size_t q = 0; q < row->calls.n; q++) {
                AvpVal *v = avp_map_get(local, row->calls.v[q].sym);
                i64 callee = WT_NONE;
                if (v)
                    callee = v->i;
                else if ((v = avp_map_get(best, row->calls.v[q].sym)) != NULL)
                    callee = ((i64 *)v->p)[1];
                avp_vec_push(&f->calls, callee);
                avp_vec_push(&f->sites, row->calls.v[q].site);
            }
            wt_side_put(fns, f);
            for (size_t q = 0; q < row->lfs.n; q++) {
                LfsSite ls = {row->addr, row->lfs.v[q]};
                avp_vec_push(&lfs_sites, ls);
            }
        }
    }
    avp_map_free(local);
    avp_map_free(best);
    bool have_dol = false;
    Dol dol;
    AvpBytes dol_bytes = {0};
    if (r->retail_dol && avp_isfile(r->retail_dol)) {
        if (!avp_read_all(a, r->retail_dol, &dol_bytes)) shsm_raise_io();
        have_dol = dol_init(&dol, dol_bytes.v, dol_bytes.n);
    }
    if (have_dol) {
        /* every addressed C string, typed or not (dtk types only some literals as strings) */
        st = prep(db, "select distinct owner_addr, to_addr from refs where kind in ('ha', 'l', 'h', 'sda21')"
                      " and to_addr is not null");
        while (step(db, st) == SQLITE_ROW) {
            WtFn *f = get_fn(fns, col_int(st, 0));
            if (!f) continue;
            const char *t = dol_cstring(a, &dol, col_int(st, 1));
            if (t && wt_pystr_len(t) >= 2) avp_set_add(f->strs, avp_str(t));
        }
        sqlite3_finalize(st);
        AvpIMap *target = avp_imap_new();
        st = prep(db, "select from_addr, to_addr from refs where kind in ('l', 'sda21')");
        while (step(db, st) == SQLITE_ROW) {
            bool created;
            avp_imap_put(target, (u64)col_int(st, 0), &created)->i = col_int(st, 1);
        }
        sqlite3_finalize(st);
        for (size_t k = 0; k < lfs_sites.n; k++) {
            AvpVal *v = avp_imap_get(target, (u64)lfs_sites.v[k].site);
            if (!v || v->i == WT_NONE || v->i == 0) continue;
            u32 bits;
            if (dol_word(&dol, v->i, &bits)) avp_pyintset_add(wt_side_get(fns, lfs_sites.v[k].owner)->floats, bits);
        }
        avp_imap_free(target);
    }
    st = prep(db, "select r.owner_addr, s.text from refs r join strings s on s.address = r.to_addr"
                  " where r.kind in ('ha', 'l', 'h', 'sda21')");
    while (step(db, st) == SQLITE_ROW) {
        WtFn *f = get_fn(fns, col_int(st, 0));
        AvpStr t = col_text(st, 1);
        if (f && t.p && t.n) avp_set_add(f->strs, t);
    }
    sqlite3_finalize(st);
    if (!have_dol) {
        st = prep(db, "select r.owner_addr, f.value from refs r join floats f on f.address = r.to_addr"
                      " where r.kind in ('l', 'sda21') and f.kind = 'float'");
        while (step(db, st) == SQLITE_ROW) {
            WtFn *f = get_fn(fns, col_int(st, 0));
            if (!f) continue;
            float fv = (float)sqlite3_column_double(st, 1);
            u32 bits;
            memcpy(&bits, &fv, 4);
            avp_pyintset_add(f->floats, bits);
        }
        sqlite3_finalize(st);
    }
    AvpIMap *strings = avp_imap_new();
    st = prep(db, "select address, text from strings");
    while (step(db, st) == SQLITE_ROW) {
        AvpStr t = col_text(st, 1);
        bool created;
        avp_imap_put(strings, (u64)col_int(st, 0), &created)->p = t.p ? avp_cz(a, t) : NULL;
    }
    sqlite3_finalize(st);
    WtRun cur = {0};
    bool have_last = false;
    i64 last_owner = 0, last_from = 0;
    st = prep(db, "select owner_addr, from_addr, to_addr from refs where kind = 'ptr'"
                  " and section not like '.text%' and section not like '.ctors%'"
                  " and section not like '.dtors%' order by from_addr");
    while (step(db, st) == SQLITE_ROW) {
        i64 owner = col_int(st, 0), frm = col_int(st, 1), to = col_int(st, 2);
        if (!have_last || frm != last_from + 4 || owner != last_owner) {
            size_t live = 0;
            for (size_t k = 0; k < cur.n; k++) live += cur.v[k].kind != 'o';
            if (live >= 3)
                avp_vec_push(runs, cur);
            else
                avp_vec_free(&cur);
            memset(&cur, 0, sizeof cur);
        }
        WtRunItem it = {'o', 0, NULL};
        AvpVal *sv;
        if (get_fn(fns, to)) {
            it.kind = 'f';
            it.fn = to;
        } else if (to != WT_NONE && (sv = avp_imap_get(strings, (u64)to)) != NULL) {
            it.kind = 's';
            it.text = sv->p ? sv->p : "";
        }
        avp_vec_push(&cur, it);
        have_last = true;
        last_owner = owner;
        last_from = frm;
    }
    sqlite3_finalize(st);
    size_t live = 0;
    for (size_t k = 0; k < cur.n; k++) live += cur.v[k].kind != 'o';
    if (live >= 3)
        avp_vec_push(runs, cur);
    else
        avp_vec_free(&cur);
    avp_imap_free(strings);
    avp_vec_free(&lfs_sites);
    /* the parts' arenas hold only names already copied */
    for (size_t k = 0; k < work.n; k++) {
        for (size_t j = 0; j < parts[k].rows.n; j++) {
            WiiRow *row = &parts[k].rows.v[j];
            avp_vec_free(&row->calls);
            avp_vec_free(&row->ints);
            avp_vec_free(&row->offs);
            avp_vec_free(&row->lfs);
        }
        avp_vec_free(&parts[k].rows);
        avp_arena_free(parts[k].arena);
    }
    avp_xfree(parts);
    avp_vec_free(&work);
}
