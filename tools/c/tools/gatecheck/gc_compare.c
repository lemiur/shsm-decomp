/* gatecheck layout, comparison (gatecheck.py Finding, content_equal, pair_score, align_objects, zero_tail,
 * short_name, masked_bytes, compose, compare_section, LayoutResult, unit_key, strip_ext, compare_unit,
 * compare_code_sizes, weak_dedupe_index, dedupe_for_unit, check_layout, format_layout). */
#include "gatecheck.h"

#include <ctype.h>

/* ---- offset sets (sorted, unique) --------------------------------------------------------------------------------- */

static void iset_norm(AvpI64Vec *v) {
    avp_sort_i64(v->v, v->n);
    size_t m = 0;
    for (size_t i = 0; i < v->n; i++)
        if (!m || v->v[m - 1] != v->v[i]) v->v[m++] = v->v[i];
    v->n = m;
}
static AvpI64Vec reloc_keys(const GcRelocVec *r, bool below, i64 n) {
    AvpI64Vec v = {0};
    for (size_t i = 0; i < r->n; i++)
        if (!below || r->v[i].off < n) avp_vec_push(&v, r->v[i].off);
    return v; /* already sorted and unique */
}
static AvpI64Vec iset_union(const AvpI64Vec *x, const AvpI64Vec *y) {
    AvpI64Vec v = {0};
    avp_vec_extend(&v, x->v, x->n);
    avp_vec_extend(&v, y->v, y->n);
    iset_norm(&v);
    return v;
}
static bool iset_eq(const AvpI64Vec *x, const AvpI64Vec *y) {
    return x->n == y->n && (x->n == 0 || memcmp(x->v, y->v, x->n * sizeof(i64)) == 0);
}
static bool iset_has(const AvpI64Vec *x, i64 v) {
    size_t i = avp_bisect_left_i64(x->v, x->n, v);
    return i < x->n && x->v[i] == v;
}

/* masked_bytes(data, n, relocs): data[:n] padded with zeros, the 4 bytes at each relocation cleared */
static u8 *masked_bytes(const u8 *data, i64 dlen, i64 n, const i64 *rel, size_t nrel) {
    u8 *buf = avp_xcalloc((size_t)n + 1, 1);
    if (dlen > 0) memcpy(buf, data, (size_t)(dlen < n ? dlen : n));
    for (size_t i = 0; i < nrel; i++) {
        i64 o = rel[i];
        for (i64 k = o > 0 ? o : 0; k < o + 4 && k < n; k++) buf[k] = 0;
    }
    return buf;
}

static const u8 *obj_masked(GcObj *o) {
    if (!o->mk) {
        u8 *b = avp_xmalloc((size_t)o->dlen + 1);
        if (o->dlen) memcpy(b, o->data, (size_t)o->dlen);
        for (size_t i = 0; i < o->relocs.n; i++) {
            i64 off = o->relocs.v[i].off;
            for (i64 k = off; k < off + 4 && k < o->dlen; k++)
                if (k >= 0) b[k] = 0;
        }
        o->mk = b;
    }
    return o->mk;
}

bool gc_content_equal(const GcObj *r, const GcObj *b) {
    if (!r->data || !b->data) return true;
    if (r->size != b->size) return false;
    if (r->dlen != b->dlen) return false;
    return memcmp(obj_masked((GcObj *)r), obj_masked((GcObj *)b), (size_t)r->dlen) == 0;
}

int gc_pair_score(const GcObj *r, const GcObj *b) {
    if (!r->anon && !b->anon) {
        if (strcmp(r->name, b->name) != 0) return 0;
        return r->size == b->size ? 4 : 2;
    }
    if (r->size != b->size) return 0;
    return gc_content_equal(r, b) ? 2 : 1;
}

/* "lbl_" and at least one hex digit, nothing else: the placeholder name symbols.txt gives an object by its address */
static bool is_addr_named(const char *name) {
    if (strncmp(name, "lbl_", 4) != 0 || !name[4]) return false;
    for (const char *p = name + 4; *p; p++)
        if (!isxdigit((unsigned char)*p)) return false;
    return true;
}

/* score * 2^40 minus the offset distance (capped at 2^20, so a section's worth of pairs stays below one score point) */
static i64 align_pair_value(i32 score, const GcObj *r, const GcObj *b) {
    i64 d = b->off - r->off;
    if (d < 0) d = -d;
    if (d > ((i64)1 << 20)) d = (i64)1 << 20;
    return ((i64)score << 40) - d;
}

GcPairVec gc_align_objects(GcObj *const *R, size_t n, GcObj *const *B, size_t m) {
    GcPairVec pairs = {0};
    size_t lo = 0;
    while (lo < n && lo < m && gc_pair_score(R[lo], B[lo]) >= 2) {
        avp_vec_push(&pairs, ((GcPair){(i64)lo, (i64)lo}));
        lo++;
    }
    size_t hi_r = n, hi_b = m;
    GcPairVec tail = {0};
    while (hi_r > lo && hi_b > lo && gc_pair_score(R[hi_r - 1], B[hi_b - 1]) >= 2) {
        hi_r--;
        hi_b--;
        avp_vec_push(&tail, ((GcPair){(i64)hi_r, (i64)hi_b}));
    }
    size_t nn = hi_r - lo, mm = hi_b - lo;
    if (nn > 0 && mm > 0) {
        if ((u64)nn * mm > 6000000) {
            size_t k = nn < mm ? nn : mm;
            for (size_t q = 0; q < k; q++)
                if (gc_pair_score(R[lo + q], B[lo + q]) > 0) avp_vec_push(&pairs, ((GcPair){(i64)(lo + q), (i64)(lo + q)}));
        } else {
            /* A pair is worth its score first and, among alignments of equal total score, the nearer its two offsets:
               a run of objects with equal content (the same __FILE__ literal repeated) is paired in place, not with
               the first copy on the other side. */
            size_t W = mm + 1;
            i64 *f = avp_xcalloc((nn + 1) * W, sizeof(i64));
            i8 *sc = avp_xmalloc(nn * mm + 1);
            for (size_t i = 0; i < nn; i++)
                for (size_t j = 0; j < mm; j++) sc[i * mm + j] = (i8)gc_pair_score(R[lo + i], B[lo + j]);
            for (size_t i = nn; i-- > 0;) {
                i64 *fi = f + i * W, *fn = f + (i + 1) * W;
                for (size_t j = mm; j-- > 0;) {
                    i64 best = fn[j] >= fi[j + 1] ? fn[j] : fi[j + 1];
                    i32 s = sc[i * mm + j];
                    if (s && fn[j + 1] + align_pair_value(s, R[lo + i], B[lo + j]) > best)
                        best = fn[j + 1] + align_pair_value(s, R[lo + i], B[lo + j]);
                    fi[j] = best;
                }
            }
            size_t i = 0, j = 0;
            while (i < nn && j < mm) {
                i32 s = sc[i * mm + j];
                if (s && f[i * W + j] == f[(i + 1) * W + j + 1] + align_pair_value(s, R[lo + i], B[lo + j])) {
                    avp_vec_push(&pairs, ((GcPair){(i64)(lo + i), (i64)(lo + j)}));
                    i++;
                    j++;
                } else if (f[i * W + j] == f[(i + 1) * W + j]) {
                    i++;
                } else {
                    j++;
                }
            }
            avp_xfree(f);
            avp_xfree(sc);
        }
    }
    for (size_t q = tail.n; q-- > 0;) avp_vec_push(&pairs, tail.v[q]);
    avp_vec_free(&tail);
    return pairs;
}

static bool zero_tail(const GcObj *r, i64 from) {
    if (!r->data) return true;
    for (i64 k = from; k < r->dlen; k++)
        if (r->data[k]) return false;
    return true;
}

static const char *short_name(AvpArena *a, const GcObj *r, const GcObj *b) {
    return !r->anon ? r->name : avp_sprintf(a, "%s/%s", r->name, b->name);
}

/* The link's re-alignment puts built object b at its retail offset r->off: the pair directly follows the previous
   one (rp, bp) on both sides, and the largest alignment b's built offset allows (at most the section's) moves it from
   the end of bp, at rp's retail place, to exactly r->off. The object files carry no per-object alignment, so a built
   object packed by chance at a larger boundary than its own can still be excused; the DOL hash covers that case. */
static bool realigned(const GcObj *rp, const GcObj *bp, const GcObj *r, const GcObj *b, i64 base, i64 balign) {
    if (balign <= 0) return false;
    i64 al = balign;
    while (al > 1 && avp_mod_i64(b->off, al)) al /= 2;
    i64 end = base + rp->off + bp->size;
    return base + r->off == avp_floordiv_i64(end + al - 1, al) * al;
}

/* ---- compare_section ------------------------------------------------------------------------------------------- */

typedef struct {
    AvpArena *a;
    GcFindingVec *F;
    const char *sec;
} Ctx;

static void add(Ctx *c, const char *kind, AvpStr text, bool diff) {
    GcFinding f = {kind, c->sec, avp_cz(c->a, text), diff};
    avp_vec_push(c->F, f);
}

static size_t block_of(i64 idx, const AvpI64Vec *positions) {
    return avp_bisect_left_i64(positions->v, positions->n, idx);
}

/* the names joined with ", ", cut to 200 code points */
static const char *names_joined(AvpArena *a, GcObj *const *objs, size_t n) {
    AvpBuf b;
    avp_buf_init(&b, a);
    for (size_t i = 0; i < n; i++) {
        if (i) avp_buf_putz(&b, ", ");
        avp_buf_putz(&b, objs[i]->name);
    }
    return avp_cz(a, gc_cp_prefix(avp_buf_str(&b), 200));
}

typedef AVP_VEC(GcObj *) ObjList;

static void try_cover(Ctx *c, const char *big_side, const AvpI64Vec *bigs, u8 *big_paired, const AvpI64Vec *smalls,
                      u8 *small_paired, GcObj *const *big_list, GcObj *const *small_list) {
    AvpArena *a = c->a;
    const char *other = strcmp(big_side, "retail") == 0 ? "built" : "retail";
    size_t cursor = 0;
    for (size_t bi_ = 0; bi_ < bigs->n; bi_++) {
        i64 bi = bigs->v[bi_];
        GcObj *big = big_list[bi];
        bool found = false;
        size_t f0 = 0, f1 = 0;
        for (size_t start = cursor; start < smalls->n && !found; start++) {
            i64 covered = 0;
            bool best = false;
            size_t b0 = 0, b1 = 0;
            const GcObj *first = small_list[smalls->v[start]];
            for (size_t k = start; k < smalls->n; k++) {
                const GcObj *s = small_list[smalls->v[k]];
                covered += s->size;
                if (covered > big->size) break;
                /* the run must fit in the big object's span, padding included: two 1-byte "" literals 4 bytes apart
                 * are not one 4-byte object (D-gatecheck-post-2) */
                if (gc_obj_end(s) - first->off > big->size) break;
                i64 n = (i64)(k - start + 1);
                if (n >= 2 && big->size - covered <= 4 * n + 16) {
                    best = true;
                    b0 = start;
                    b1 = k + 1;
                }
            }
            if (best) {
                found = true;
                f0 = b0;
                f1 = b1;
            }
        }
        if (!found) continue;
        cursor = f1;
        big_paired[bi] = 1;
        size_t nch = f1 - f0;
        GcObj **objs = AVP_NEWN(a, GcObj *, nch);
        bool all_data = true;
        for (size_t q = 0; q < nch; q++) {
            small_paired[smalls->v[f0 + q]] = 1;
            objs[q] = small_list[smalls->v[f0 + q]];
            all_data &= objs[q]->data != NULL;
        }
        bool bad = false;
        i64 bk = 0;
        int bra = 0, bba = 0;
        if (big->data && all_data) {
            for (i64 lead = 0; lead < 8; lead++) {
                /* compose(objs, big.size, lead) */
                i64 total = big->size, base = objs[0]->off;
                u8 *buf = avp_xcalloc((size_t)total + 1, 1);
                AvpI64Vec rel = {0};
                bool ok = true;
                for (size_t q = 0; q < nch && ok; q++) {
                    i64 at = objs[q]->off - base + lead;
                    if (at < 0 || at + objs[q]->size > total) {
                        ok = false;
                        break;
                    }
                    memcpy(buf + at, objs[q]->data, (size_t)objs[q]->dlen);
                    for (size_t z = 0; z < objs[q]->relocs.n; z++) avp_vec_push(&rel, at + objs[q]->relocs.v[z].off);
                }
                if (!ok) {
                    avp_xfree(buf);
                    avp_vec_free(&rel);
                    continue;
                }
                for (size_t z = 0; z < big->relocs.n; z++) avp_vec_push(&rel, big->relocs.v[z].off);
                iset_norm(&rel);
                u8 *ra = masked_bytes(big->data, big->dlen, big->size, rel.v, rel.n);
                u8 *ba = masked_bytes(buf, total, big->size, rel.v, rel.n);
                bool same = memcmp(ra, ba, (size_t)big->size) == 0;
                if (!same && !bad) {
                    i64 k = 0;
                    while (ra[k] == ba[k]) k++;
                    bad = true;
                    bk = k;
                    bra = ra[k];
                    bba = ba[k];
                }
                avp_xfree(ra);
                avp_xfree(ba);
                avp_xfree(buf);
                avp_vec_free(&rel);
                if (same) {
                    bad = false;
                    break;
                }
            }
        }
        if (bad) {
            add(c, "CONTENT",
                avp_pyfmt(a,
                          "%s object %s +0x%X size 0x%X (covers %d %s objects): bytes differ, first at +0x%X "
                          "(%02X vs %02X)",
                          big_side, big->name, big->off, big->size, nch, other, bk, bra, bba),
                true);
            continue;
        }
        add(c, "COARSE",
            avp_pyfmt(a, "%s object %s +0x%X size 0x%X covers %d %s objects", big_side, big->name, big->off, big->size,
                      nch, other),
            false);
    }
}

typedef struct {
    size_t key;
    AvpI64Vec r, b;
} Block;
typedef AVP_VEC(Block) BlockVec;

static Block *block_get(BlockVec *bv, size_t key) {
    for (size_t i = 0; i < bv->n; i++)
        if (bv->v[i].key == key) return &bv->v[i];
    Block *b = avp_vec_push_zero(bv);
    b->key = key;
    return b;
}
static int cmp_block(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    size_t a = ((const Block *)x)->key, b = ((const Block *)y)->key;
    return a < b ? -1 : a > b;
}

static BlockVec leftover_blocks(const AvpI64Vec *left_r, const u8 *paired_r, const AvpI64Vec *pos_r,
                                const AvpI64Vec *left_b, const u8 *paired_b, const AvpI64Vec *pos_b) {
    BlockVec bv = {0};
    for (size_t q = 0; q < left_r->n; q++)
        if (!paired_r[left_r->v[q]]) avp_vec_push(&block_get(&bv, block_of(left_r->v[q], pos_r))->r, left_r->v[q]);
    for (size_t q = 0; q < left_b->n; q++)
        if (!paired_b[left_b->v[q]]) avp_vec_push(&block_get(&bv, block_of(left_b->v[q], pos_b))->b, left_b->v[q]);
    avp_sort(bv.v, bv.n, sizeof(Block), cmp_block, NULL, false);
    return bv;
}

static int cmp_cstr(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

void gc_compare_section(AvpArena *a, GcFindingVec *F, const char *secname, GcObj *const *R, size_t nr, bool has_rspan,
                        i64 rspan, bool has_rstart, i64 rstart, GcObj *const *B, size_t nb, i64 bsize, i64 balign,
                        const u8 *rbytes, i64 rlen, const u8 *bbytes, i64 blen, const GcRelocVec *rrelocs,
                        const GcRelocVec *brelocs) {
    Ctx c = {a, F, secname};
    size_t ndead = 0;
    ObjList stripped = {0};
    AVP_VEC(const char *) dunits = {0};
    for (size_t i = 0; i < nb; i++) {
        if (!B[i]->dead) continue;
        ndead++;
        if (!B[i]->dedup) {
            avp_vec_push(&stripped, B[i]);
        } else {
            bool seen = false;
            for (size_t q = 0; q < dunits.n; q++) seen |= strcmp(dunits.v[q], B[i]->dedup) == 0;
            if (!seen) avp_vec_push(&dunits, B[i]->dedup);
        }
    }
    if (stripped.n) {
        i64 sz = 0;
        for (size_t i = 0; i < stripped.n; i++) sz += stripped.v[i]->size;
        add(&c, "DEADSTRIP",
            avp_pyfmt(a, "%d built objects (0x%X bytes) are not kept by the link: %s", stripped.n, sz,
                      names_joined(a, stripped.v, stripped.n)),
            false);
    }
    avp_sort(dunits.v, dunits.n, sizeof(const char *), cmp_cstr, NULL, false);
    for (size_t u = 0; u < dunits.n; u++) {
        ObjList dups = {0};
        i64 sz = 0;
        for (size_t i = 0; i < nb; i++)
            if (B[i]->dead && B[i]->dedup && strcmp(B[i]->dedup, dunits.v[u]) == 0) {
                avp_vec_push(&dups, B[i]);
                sz += B[i]->size;
            }
        add(&c, "DEDUP",
            avp_pyfmt(a, "%d built objects (0x%X bytes) are deduplicated by the link (retail copy in %s): %s", dups.n,
                      sz, dunits.v[u], names_joined(a, dups.v, dups.n)),
            false);
        avp_vec_free(&dups);
    }
    ObjList Rl = {0}, Bl = {0};
    for (size_t i = 0; i < nr; i++) {
        const GcObj *o = R[i];
        bool nonzero = false;
        for (i64 k = 0; o->data && k < o->dlen && !nonzero; k++) nonzero = o->data[k] != 0;
        if (!o->gap || nonzero || o->relocs.n) avp_vec_push(&Rl, R[i]);
    }
    for (size_t i = 0; i < nb; i++)
        if (!B[i]->dead) avp_vec_push(&Bl, B[i]);
    GcPairVec pairs = gc_align_objects(Rl.v, Rl.n, Bl.v, Bl.n);
    u8 *used_r = AVP_NEWN(a, u8, Rl.n + 1), *used_b = AVP_NEWN(a, u8, Bl.n + 1);
    for (size_t q = 0; q < pairs.n; q++) {
        used_r[pairs.v[q].i] = 1;
        used_b[pairs.v[q].j] = 1;
    }

    /* ---- matched pairs: offsets, sizes, contents, relocations */
    i64 prev_delta = 0, prev_i = -1, prev_j = -1;
    /* deltas in force just before a change the removal of a dead object explains: when the link
       re-aligns a later object after the dropped one, the delta returns to one of these */
    AvpI64Vec pre_dead = {0};
    for (size_t q = 0; q < pairs.n; q++) {
        i64 i = pairs.v[q].i, j = pairs.v[q].j;
        GcObj *r = Rl.v[i], *b = Bl.v[j];
        const char *nm = short_name(a, r, b);
        i64 delta = b->off - r->off;
        bool skipped = false;
        for (i64 z = prev_j + 1; z <= j; z++) skipped |= Bl.v[z]->after_dead;
        if (delta != prev_delta) {
            bool returns = false;
            for (size_t z = 0; z < pre_dead.n; z++) returns |= pre_dead.v[z] == delta;
            returns = returns && prev_i >= 0 && i == prev_i + 1 && j == prev_j + 1 &&
                      realigned(Rl.v[prev_i], Bl.v[prev_j], r, b, has_rstart ? rstart : 0, balign);
            if (skipped) avp_vec_push(&pre_dead, prev_delta);
            if (!skipped && !returns)
                add(&c, "OFFSET",
                    avp_pyfmt(a,
                              "%s: retail +0x%X, built +0x%X (built minus retail goes from %+d to %+d: padding or "
                              "size before it differs)",
                              nm, r->off, b->off, prev_delta, delta),
                    true);
            prev_delta = delta;
        }
        prev_i = i;
        prev_j = j;
        i64 n = r->size < b->size ? r->size : b->size;
        if (r->size != b->size) {
            i64 extra = r->size - b->size;
            if (0 < extra && extra < 32 && zero_tail(r, b->size))
                add(&c, "PADSIZE",
                    avp_pyfmt(a, "%s: retail size 0x%X includes 0x%X bytes of trailing padding (built 0x%X)", nm,
                              r->size, extra, b->size),
                    false);
            else
                add(&c, "SIZE",
                    avp_pyfmt(a, "%s: retail size 0x%X, built size 0x%X (retail +0x%X)", nm, r->size, b->size, r->off),
                    true);
        }
        if (r->data && b->data && n) {
            AvpI64Vec rrel = reloc_keys(&r->relocs, true, n), brel = reloc_keys(&b->relocs, true, n);
            AvpI64Vec both = iset_union(&rrel, &brel);
            u8 *ra = masked_bytes(r->data, r->dlen, n, both.v, both.n);
            u8 *ba = masked_bytes(b->data, b->dlen, n, both.v, both.n);
            if (memcmp(ra, ba, (size_t)n) != 0) {
                i64 k = 0, nd = 0;
                while (ra[k] == ba[k]) k++;
                for (i64 z = 0; z < n; z++) nd += ra[z] != ba[z];
                add(&c, "CONTENT",
                    avp_pyfmt(a, "%s: %d bytes differ, first at +0x%X (retail %02X, built %02X)", nm, nd, k, ra[k],
                              ba[k]),
                    true);
            } else if (!iset_eq(&rrel, &brel)) {
                AvpBuf t;
                avp_buf_init(&t, a);
                int shown = 0;
                for (size_t z = 0; z < both.n && shown < 6; z++) {
                    i64 o = both.v[z];
                    if (iset_has(&rrel, o) != iset_has(&brel, o)) {
                        if (shown) avp_buf_putz(&t, ", ");
                        avp_buf_putz(&t, avp_sprintf(a, "+0x%s", gc_hex(a, o)));
                        shown++;
                    }
                }
                add(&c, "RELOC", avp_pyfmt(a, "%s: relocated words differ at %s", nm, avp_buf_cz(&t)), true);
            } else {
                for (size_t z = 0; z < rrel.n; z++) {
                    i64 o = rrel.v[z];
                    const char *rt = gc_relocs_get(&r->relocs, o)->target;
                    const GcReloc *bx = gc_relocs_get(&b->relocs, o);
                    const char *bt = bx ? bx->target : "";
                    if (rt[0] && bt[0] && !gc_is_anon(rt) && !gc_is_anon(bt) && strcmp(rt, bt) != 0)
                        add(&c, "RELOC",
                            avp_pyfmt(a, "%s +0x%X: retail points at %s, built at %s", nm, o, rt, bt), true);
                }
            }
            avp_xfree(ra);
            avp_xfree(ba);
            avp_vec_free(&rrel);
            avp_vec_free(&brel);
            avp_vec_free(&both);
        }
        /* Two address-named objects (lbl_<hex>, a name from symbols.txt that the source defines) paired by position
           must be the same symbol: their name is their place. MWCC lays uninitialised small data out in reverse
           order of definition, so two zero-filled .sbss/.bss counters defined in the wrong order are equal in size
           and content and differ only in which name sits at which address. */
        if (is_addr_named(r->name) && is_addr_named(b->name) && strcmp(r->name, b->name) != 0)
            add(&c, "ORDER",
                avp_pyfmt(a, "%s: retail +0x%X holds %s, built +0x%X holds %s (same size, defined in another order)",
                          r->name, r->off, r->name, b->off, b->name),
                true);
        if (!r->anon && !b->anon && strcmp(r->name, b->name) != 0 && r->size == b->size)
            add(&c, "RENAME", avp_pyfmt(a, "retail %s vs built %s (same slot, same size)", r->name, b->name), false);
    }

    /* ---- leftovers */
    AvpI64Vec left_r = {0}, left_b = {0}, pos_r = {0}, pos_b = {0};
    for (size_t i = 0; i < Rl.n; i++)
        if (!used_r[i]) avp_vec_push(&left_r, (i64)i);
    for (size_t j = 0; j < Bl.n; j++)
        if (!used_b[j]) avp_vec_push(&left_b, (i64)j);
    for (size_t q = 0; q < pairs.n; q++) {
        avp_vec_push(&pos_r, pairs.v[q].i);
        avp_vec_push(&pos_b, pairs.v[q].j);
    }
    avp_sort_i64(pos_r.v, pos_r.n);
    avp_sort_i64(pos_b.v, pos_b.n);
    u8 *paired_r = AVP_NEWN(a, u8, Rl.n + 1), *paired_b = AVP_NEWN(a, u8, Bl.n + 1);

    /* a real name present on both sides: by_name_b[name] is a queue of built indices */
    AvpMap *by_name_b = avp_map_new();
    for (size_t q = 0; q < left_b.n; q++) {
        GcObj *b = Bl.v[left_b.v[q]];
        if (b->anon) continue;
        bool created;
        AvpVal *v = avp_map_put(by_name_b, avp_str(b->name), &created);
        if (created) v->p = AVP_NEW(a, AvpI64Vec);
        avp_vec_push((AvpI64Vec *)v->p, left_b.v[q]);
    }
    AvpMap *heads = avp_map_new(); /* name -> next index into the queue */
    for (size_t q = 0; q < left_r.n; q++) {
        i64 i = left_r.v[q];
        GcObj *r = Rl.v[i];
        if (r->anon) continue;
        AvpVal *v = avp_map_get(by_name_b, avp_str(r->name));
        if (!v) continue;
        AvpI64Vec *queue = v->p;
        bool created;
        AvpVal *h = avp_map_put(heads, avp_str(r->name), &created);
        if (created) h->i = 0;
        if ((size_t)h->i >= queue->n) continue;
        i64 j = queue->v[h->i++];
        GcObj *b = Bl.v[j];
        paired_r[i] = 1;
        paired_b[j] = 1;
        bool moved = block_of(i, &pos_r) != block_of(j, &pos_b);
        if (r->size != b->size)
            add(&c, "SIZE",
                avp_pyfmt(a, "%s: retail size 0x%X, built size 0x%X (retail +0x%X, built +0x%X)", r->name, r->size,
                          b->size, r->off, b->off),
                true);
        if (moved || (r->size == b->size && !ndead))
            add(&c, "ORDER",
                avp_pyfmt(a, "%s: retail +0x%X, built +0x%X (%s)", r->name, r->off, b->off,
                          moved ? "moved within the section" : "same neighbours, different offset"),
                true);
    }
    avp_map_free(by_name_b);
    avp_map_free(heads);

    /* anonymous objects that moved */
    AvpI64Vec todo = {0};
    for (size_t q = 0; q < left_r.n; q++)
        if (!paired_r[left_r.v[q]]) avp_vec_push(&todo, left_r.v[q]);
    for (size_t q = 0; q < todo.n; q++) {
        i64 i = todo.v[q];
        GcObj *r = Rl.v[i];
        for (size_t z = 0; z < left_b.n; z++) {
            i64 j = left_b.v[z];
            if (paired_b[j]) continue;
            GcObj *b = Bl.v[j];
            if ((r->anon || b->anon) && r->size == b->size && gc_content_equal(r, b) &&
                block_of(i, &pos_r) != block_of(j, &pos_b) && (r->data || r->size >= 8)) {
                paired_r[i] = 1;
                paired_b[j] = 1;
                add(&c, "ORDER",
                    avp_pyfmt(a, "%s retail +0x%X moved to %s built +0x%X (size 0x%X)", r->name, r->off, b->name,
                              b->off, r->size),
                    true);
                break;
            }
        }
    }
    avp_vec_free(&todo);

    /* granularity: one object on one side covers several consecutive leftovers on the other */
    BlockVec bv = leftover_blocks(&left_r, paired_r, &pos_r, &left_b, paired_b, &pos_b);
    for (size_t k = 0; k < bv.n; k++) {
        AvpI64Vec *rr = &bv.v[k].r, *bb = &bv.v[k].b;
        try_cover(&c, "retail", rr, paired_r, bb, paired_b, Rl.v, Bl.v);
        AvpI64Vec bb2 = {0}, rr2 = {0};
        for (size_t z = 0; z < bb->n; z++)
            if (!paired_b[bb->v[z]]) avp_vec_push(&bb2, bb->v[z]);
        for (size_t z = 0; z < rr->n; z++)
            if (!paired_r[rr->v[z]]) avp_vec_push(&rr2, rr->v[z]);
        try_cover(&c, "built", &bb2, paired_b, &rr2, paired_r, Bl.v, Rl.v);
        avp_vec_free(&bb2);
        avp_vec_free(&rr2);
    }
    for (size_t k = 0; k < bv.n; k++) {
        avp_vec_free(&bv.v[k].r);
        avp_vec_free(&bv.v[k].b);
    }
    avp_vec_free(&bv);

    /* same number of leftovers between the same two matched neighbours */
    bv = leftover_blocks(&left_r, paired_r, &pos_r, &left_b, paired_b, &pos_b);
    for (size_t k = 0; k < bv.n; k++) {
        AvpI64Vec *rr = &bv.v[k].r, *bb = &bv.v[k].b;
        if (!rr->n || !bb->n || rr->n != bb->n) continue;
        for (size_t z = 0; z < rr->n; z++) {
            i64 i = rr->v[z], j = bb->v[z];
            GcObj *r = Rl.v[i], *b = Bl.v[j];
            paired_r[i] = 1;
            paired_b[j] = 1;
            if (r->size != b->size) {
                i64 extra = r->size - b->size;
                if (0 < extra && extra < 32 && zero_tail(r, b->size))
                    add(&c, "PADSIZE",
                        avp_pyfmt(a, "retail %s / built %s: retail size includes 0x%X bytes of padding", r->name,
                                  b->name, extra),
                        false);
                else
                    add(&c, "SIZE",
                        avp_pyfmt(a, "retail %s (+0x%X size 0x%X) vs built %s (+0x%X size 0x%X)", r->name, r->off,
                                  r->size, b->name, b->off, b->size),
                        true);
            } else if (!gc_content_equal(r, b)) {
                add(&c, "CONTENT",
                    avp_pyfmt(a, "retail %s vs built %s (+0x%X size 0x%X): bytes differ", r->name, b->name, r->off,
                              r->size),
                    true);
            } else {
                add(&c, "RENAME", avp_pyfmt(a, "retail %s vs built %s (same slot, same size)", r->name, b->name),
                    false);
            }
        }
    }
    for (size_t k = 0; k < bv.n; k++) {
        avp_vec_free(&bv.v[k].r);
        avp_vec_free(&bv.v[k].b);
    }
    avp_vec_free(&bv);
    for (size_t q = 0; q < left_r.n; q++)
        if (!paired_r[left_r.v[q]]) {
            GcObj *r = Rl.v[left_r.v[q]];
            add(&c, "MISSING",
                avp_pyfmt(a, "retail %s +0x%X size 0x%X has no built counterpart", r->name, r->off, r->size), true);
        }
    for (size_t q = 0; q < left_b.n; q++)
        if (!paired_b[left_b.v[q]]) {
            GcObj *b = Bl.v[left_b.v[q]];
            add(&c, "EXTRA",
                avp_pyfmt(a, "built %s +0x%X size 0x%X is kept by the link but has no retail counterpart", b->name,
                          b->off, b->size),
                true);
        }

    /* ---- totals and alignment */
    if (has_rspan) {
        if (ndead) {
            i64 live_sum = 0;
            for (size_t q = 0; q < Bl.n; q++) live_sum += Bl.v[q]->size;
            if (rspan < live_sum - 8 * (i64)Bl.n || rspan > bsize + 31)
                add(&c, "TOTAL",
                    avp_pyfmt(a, "retail span 0x%X, built live objects total 0x%X (section 0x%X before stripping)",
                              rspan, live_sum, bsize),
                    true);
        } else {
            i64 d = rspan - bsize;
            if (d < 0 || d >= (balign > 32 ? balign : 32))
                add(&c, "TOTAL", avp_pyfmt(a, "built section size 0x%X, retail span 0x%X", bsize, rspan), true);
        }
    }
    if (has_rstart && balign && avp_mod_i64(rstart, balign))
        add(&c, "ALIGN", avp_pyfmt(a, "retail section starts at %s, built alignment is %d", gc_hx(a, rstart, 8), balign),
            true);

    /* ---- whole-section bytes (catches what the object pairing cannot explain) */
    if (rbytes && bbytes && has_rspan && !ndead) {
        i64 n = rspan;
        AvpI64Vec both = {0};
        for (size_t q = 0; q < rrelocs->n; q++) avp_vec_push(&both, rrelocs->v[q].off);
        for (size_t q = 0; q < brelocs->n; q++) avp_vec_push(&both, brelocs->v[q].off);
        u8 *ra = masked_bytes(rbytes, rlen, n, both.v, both.n);
        u8 *ba = masked_bytes(bbytes, blen, n, both.v, both.n);
        bool explained = false;
        static const char *const kinds[] = {"ORDER", "SIZE", "CONTENT", "MISSING", "EXTRA", "OFFSET", "TOTAL"};
        for (size_t q = 0; q < F->n; q++)
            for (int z = 0; z < 7; z++)
                if (F->v[q].difference && strcmp(F->v[q].kind, kinds[z]) == 0) explained = true;
        if (n > 0 && memcmp(ra, ba, (size_t)n) != 0 && !explained) {
            i64 k = 0, nd = 0;
            while (ra[k] == ba[k]) k++;
            for (i64 z = 0; z < n; z++) nd += ra[z] != ba[z];
            const char *at_r = "?", *at_b = "?";
            for (size_t q = 0; q < nr; q++)
                if (R[q]->off <= k && k < gc_obj_end(R[q])) {
                    at_r = R[q]->name;
                    break;
                }
            for (size_t q = 0; q < nb; q++)
                if (B[q]->off <= k && k < gc_obj_end(B[q])) {
                    at_b = B[q]->name;
                    break;
                }
            add(&c, "BYTES",
                avp_pyfmt(a, "section bytes differ: %d bytes, first at +0x%X (retail %02X in %s, built %02X in %s)", nd,
                          k, ra[k], at_r, ba[k], at_b),
                true);
        }
        avp_xfree(ra);
        avp_xfree(ba);
        avp_vec_free(&both);
    }
    avp_vec_free(&pairs);
    avp_vec_free(&pre_dead);
    avp_vec_free(&Rl);
    avp_vec_free(&Bl);
    avp_vec_free(&left_r);
    avp_vec_free(&left_b);
    avp_vec_free(&pos_r);
    avp_vec_free(&pos_b);
    avp_vec_free(&stripped);
    avp_vec_free(&dunits);
}

/* ---- units ------------------------------------------------------------------------------------------------------- */

size_t gc_layout_ndiffs(const GcLayoutResult *r) {
    size_t n = 0;
    for (size_t i = 0; i < r->findings.n; i++) n += r->findings.v[i].difference;
    return n;
}

const char *gc_unit_key(AvpArena *a, const char *name) {
    char *s = avp_strdup(a, name);
    for (char *p = s; *p; p++)
        if (*p == '\\') *p = '/';
    return strncmp(s, "main/", 5) == 0 ? s + 5 : s;
}

const char *gc_strip_ext(AvpArena *a, const char *path) {
    AvpStr root, ext;
    avp_path_splitext(path, &root, &ext);
    static const char *const exts[] = {".c", ".cc", ".cp", ".cpp", ".cxx", ".s"};
    char *low = avp_cz(a, ext);
    for (char *p = low; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    for (int i = 0; i < 6; i++)
        if (strcmp(low, exts[i]) == 0) return avp_cz(a, root);
    return path;
}

static int cmp_obj_off(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return avp_cmp_i64((*(GcObj *const *)x)->off, (*(GcObj *const *)y)->off);
}

void gc_compare_unit(AvpArena *a, GcLayoutResult *res, const GcAsmUnit *u, const GcBuiltSections *bsecs,
                     const char *only_section) {
    for (int si = 0; si < 11; si++) {
        const char *name = gc_data_sections[si];
        if (only_section && only_section[0] && strcmp(name, only_section) != 0) continue;
        GcAsmSection *rs = gc_asm_section(u, name);
        GcBuiltSection *bs = gc_built_get(bsecs, name);
        if (!rs && !bs) continue;
        if (rs)
            for (size_t w = 0; w < rs->warnings.n; w++)
                avp_vec_push(&res->notes, avp_str(avp_sprintf(a, "%s: asm parse: %.*s", name, AVP_SF(rs->warnings.v[w]))));
        size_t nr = rs ? rs->objs.n : 0;
        GcObj **robjs = AVP_NEWN(a, GcObj *, nr + 1);
        if (nr) memcpy(robjs, rs->objs.v, nr * sizeof(GcObj *));
        avp_sort(robjs, nr, sizeof(GcObj *), cmp_obj_off, NULL, false);
        bool has_rstart = rs && rs->has_start;
        i64 rstart = has_rstart ? rs->start : 0;
        i64 rspan;
        if (rs && rs->has_start && rs->has_end) {
            rspan = rs->end - rs->start;
        } else {
            rspan = 0;
            for (size_t q = 0; q < nr; q++)
                if (q == 0 || gc_obj_end(robjs[q]) > rspan) rspan = gc_obj_end(robjs[q]);
        }
        bool nobits = gc_is_nobits_section(name);
        if (!bs) {
            bs = AVP_NEW(a, GcBuiltSection);
            bs->name = name;
            bs->nobits = nobits;
        }
        u8 *rbytes = NULL;
        i64 rlen = 0;
        GcRelocVec rrel = {0};
        if (!nobits && nr) {
            i64 len = rspan > 0 ? rspan : 0;
            u8 *buf = AVP_NEWN(a, u8, (size_t)len + 1);
            bool ok = true;
            for (size_t q = 0; q < nr; q++) {
                GcObj *o = robjs[q];
                if (!o->data || o->off + o->size > len) {
                    ok = false;
                    break;
                }
                if (o->off >= 0 && o->dlen) memcpy(buf + o->off, o->data, (size_t)o->dlen);
                for (size_t z = 0; z < o->relocs.n; z++) {
                    GcReloc rr = o->relocs.v[z];
                    rr.off += o->off;
                    avp_vec_push(&rrel, rr);
                }
            }
            gc_relocs_dict(&rrel);
            if (ok) {
                rbytes = buf;
                rlen = len;
            }
        }
        GcFindingVec fs = {0};
        gc_compare_section(a, &fs, name, robjs, nr, rs != NULL, rspan, has_rstart, rstart, bs->objs.v, bs->objs.n,
                           bs->size, bs->align, rbytes, rlen, bs->data, bs->dlen, &rrel, &bs->relocs);
        avp_vec_extend(&res->findings, fs.v, fs.n);
        size_t ndead = 0, ngap = 0;
        for (size_t q = 0; q < bs->objs.n; q++) ndead += bs->objs.v[q]->dead;
        for (size_t q = 0; q < nr; q++) ngap += robjs[q]->gap;
        AVP_VEC(const char *) kinds = {0};
        for (size_t q = 0; q < fs.n; q++) {
            if (!fs.v[q].difference) continue;
            bool seen = false;
            for (size_t z = 0; z < kinds.n; z++) seen |= strcmp(kinds.v[z], fs.v[q].kind) == 0;
            if (!seen) avp_vec_push(&kinds, fs.v[q].kind);
        }
        avp_sort(kinds.v, kinds.n, sizeof(const char *), cmp_cstr, NULL, false);
        AvpBuf kb;
        avp_buf_init(&kb, a);
        if (kinds.n) {
            avp_buf_putz(&kb, "  [");
            for (size_t z = 0; z < kinds.n; z++) {
                if (z) avp_buf_putz(&kb, ", ");
                avp_buf_putz(&kb, kinds.v[z]);
            }
            avp_buf_putz(&kb, "]");
        }
        AvpStr line = avp_pyfmt(a, "%s: retail %d objs (%d gaps) span 0x%X @%s | built %d objs%s size 0x%X align %d%s",
                                name, nr - ngap, ngap, rspan, has_rstart ? gc_hx(a, rstart, 8) : "-",
                                bs->objs.n - ndead, ndead ? avp_sprintf(a, " (+%zu stripped)", ndead) : "", bs->size,
                                bs->align, avp_buf_cz(&kb));
        avp_map_put(res->sections, avp_str(name), NULL)->p = avp_cz(a, line);
        avp_vec_free(&kinds);
        avp_vec_free(&fs);
        avp_vec_free(&rrel);
    }
}

static int cmp_sym_val(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    const GcSym *a = *(GcSym *const *)x, *b = *(GcSym *const *)y;
    int c = avp_cmp_i64(a->value, b->value);
    return c ? c : avp_cmp_i64(b->size, a->size);
}
static int cmp_secp_name(const void *x, const void *y, void *ctx) {
    AVP_UNUSED(ctx);
    return strcmp((*(GcSec *const *)x)->name, (*(GcSec *const *)y)->name);
}

/* A code section with no sized symbol (an assembly source unit's label-only section, user's ruling 2026-11-14) is one
 * whole object: its bytes, with each relocated word masked, against retail's at the same place in the split. */
static void compare_whole_section(AvpArena *a, GcLayoutResult *res, const char *name, GcElf *e, const GcSec *sec,
                                  const ShsmDol *retail, i64 addr) {
    AvpStr built = gc_elf_section_data(e, sec), got;
    if (!shsm_dol_read(a, retail, addr, sec->size, &got) || (i64)got.n != sec->size || (i64)built.n != sec->size) {
        avp_vec_push(&res->notes, avp_pyfmt(a, "%s: no retail bytes at %s for the whole section %s (0x%X bytes)", name,
                                            gc_hx(a, addr, 8), sec->name, sec->size));
        return;
    }
    GcRelVec rels = {0};
    gc_elf_relocs_for(e, sec->index, &rels);
    AVP_VEC(i64) offs = {0};
    for (size_t i = 0; i < rels.n; i++) avp_vec_push(&offs, rels.v[i].offset);
    u8 *ra = masked_bytes((const u8 *)got.p, (i64)got.n, sec->size, offs.v, offs.n);
    u8 *ba = masked_bytes((const u8 *)built.p, (i64)built.n, sec->size, offs.v, offs.n);
    i64 first = -1;
    for (i64 k = 0; k < sec->size && first < 0; k++)
        if (ra[k] != ba[k]) first = k;
    if (first >= 0) {
        GcFinding f = {"CONTENT", name,
                       avp_cz(a, avp_pyfmt(a, "whole section %s (no sized symbol, 0x%X bytes): first difference at +0x%X "
                                              "(%s)",
                                           sec->name, sec->size, first, gc_hx(a, addr + first, 8))),
                       true};
        avp_vec_push(&res->findings, f);
    } else {
        avp_vec_push(&res->notes, avp_pyfmt(a,
                                            "%s: whole section %s (no sized symbol) matches retail at %s: 0x%X bytes, %d "
                                            "relocated words masked",
                                            name, sec->name, gc_hx(a, addr, 8), sec->size, (i64)offs.n));
    }
    avp_vec_free(&offs);
    avp_vec_free(&rels);
}

/* The functions the unit keeps must be emitted in retail's order (a unit compiled with -inline deferred emits them
 * last to first: the sizes agree, the bytes do not). Compared over the functions both sides have; the finding
 * names the first function whose place differs. */
static void compare_function_order(AvpArena *a, GcLayoutResult *res, const char *section, const GcAsmCode *rc,
                                   const char *const *built, size_t nbuilt) {
    AvpSet *have = avp_set_new();
    for (size_t i = 0; i < nbuilt; i++) avp_set_add(have, avp_str(built[i]));
    AVP_VEC(const char *) want = {0};
    AvpSet *seen = avp_set_new();
    for (size_t q = 0; q < rc->fns.n; q++) {
        if (!avp_set_has(have, rc->fns.v[q]) || !avp_set_add(seen, rc->fns.v[q])) continue;
        avp_vec_push(&want, avp_cz(a, rc->fns.v[q]));
    }
    size_t n = nbuilt < want.n ? nbuilt : want.n, k = 0;
    while (k < n && strcmp(built[k], want.v[k]) == 0) k++;
    if (k < n) {
        size_t at = 0; /* where the built object puts retail's function k */
        while (at < nbuilt && strcmp(built[at], want.v[k]) != 0) at++;
        GcFinding f = {"FN-ORDER", section,
                       avp_sprintf(a, "function order differs from retail: first out-of-order function %s (retail "
                                      "position %zu of %zu, built position %zu); the object emits %s there",
                                   want.v[k], k + 1, want.n, at + 1, built[k]),
                       true};
        avp_vec_push(&res->findings, f);
    }
    avp_vec_free(&want);
    avp_set_free(seen);
    avp_set_free(have);
}

void gc_compare_code_sizes(AvpArena *a, GcLayoutResult *res, const GcAsmUnit *u, GcElf *e, const char *only_section,
                           const ShsmDol *retail) {
    static const char *const names[] = {".init", ".text"};
    gc_elf_symbols(a, e);
    for (int ni = 0; ni < 2; ni++) {
        const char *name = names[ni];
        if (only_section && only_section[0] && strcmp(name, only_section) != 0) continue;
        GcAsmCode *rc = gc_asm_code(u, name);
        AVP_VEC(GcSec *) parts = {0};
        for (size_t k = 0; k < e->nsecs; k++) {
            GcSec *s = &e->secs[k];
            if (s->type == 1 && s->size && avp_str_eqz(gc_base_section(avp_str(s->name)), name)) avp_vec_push(&parts, s);
        }
        if ((!rc || !rc->has_start) && !parts.n) continue;
        i64 rsize = rc && rc->has_start ? rc->end - rc->start : 0;
        AvpSet *keep = avp_set_new();
        if (rc)
            for (size_t q = 0; q < rc->fns.n; q++)
                if (!gc_is_gap_name(avp_cz(a, rc->fns.v[q]))) avp_set_add(keep, rc->fns.v[q]);
        i64 pos = 0, align = 1, kept = 0, dead = 0, whole = 0;
        AVP_VEC(const char *) order = {0}; /* the kept functions, in the order the object emits them */
        avp_sort(parts.v, parts.n, sizeof(GcSec *), cmp_secp_name, NULL, false);
        AVP_VEC(GcSym *) syms = {0};
        for (size_t pi = 0; pi < parts.n; pi++) {
            GcSec *sec = parts.v[pi];
            i64 al = sec->align > 1 ? sec->align : 1;
            if (al > align) align = al;
            avp_vec_clear(&syms);
            for (size_t k = 0; k < e->nsyms; k++) {
                GcSym *s = &e->syms[k];
                if (s->shndx == sec->index && s->size && s->name[0] && s->type != GC_STT_SECTION &&
                    s->type != GC_STT_FILE)
                    avp_vec_push(&syms, s);
            }
            avp_sort(syms.v, syms.n, sizeof(GcSym *), cmp_sym_val, NULL, false);
            if (!syms.n) {
                /* no sized symbol (a label-only assembler section): the whole section is one object */
                i64 at = (pos + al - 1) / al * al;
                if (retail && rc && rc->has_start) compare_whole_section(a, res, name, e, sec, retail, rc->start + at);
                pos = at + sec->size;
                whole++;
                continue;
            }
            i64 end = 0;
            for (size_t q = 0; q < syms.n; q++) {
                GcSym *s = syms.v[q];
                if (s->value < end) continue; /* a symbol inside a function (a local label with a size) */
                end = s->value + s->size;
                if (!avp_set_has(keep, avp_str(s->name))) {
                    dead++;
                    continue;
                }
                pos = (pos + al - 1) / al * al + s->size;
                kept++;
                avp_vec_push(&order, s->name);
            }
        }
        if (rc) compare_function_order(a, res, name, rc, order.v, order.n);
        avp_vec_free(&order);
        avp_vec_free(&syms);
        avp_vec_free(&parts);
        avp_set_free(keep);
        i64 padded = (pos + align - 1) / align * align;
        const char *wh = whole ? avp_sprintf(a, ", %lld whole section%s", (long long)whole, whole == 1 ? "" : "s") : "";
        AvpStr line = avp_pyfmt(a, "%s: retail span 0x%X | built 0x%X (%d functions kept, %d dropped%s) align %d", name,
                                rsize, pos, kept, dead, wh, align);
        if (!(pos <= rsize && rsize <= padded)) {
            const char *pad = padded != pos ? avp_sprintf(a, " (0x%s with padding)", gc_hex(a, padded)) : "";
            GcFinding f = {"TOTAL", name,
                           avp_cz(a, avp_pyfmt(a, "code size: retail split 0x%X, built 0x%X%s (%d functions kept, %d "
                                                  "dropped%s)",
                                               rsize, pos, pad, kept, dead, wh)),
                           true};
            avp_vec_push(&res->findings, f);
            line = avp_pyfmt(a, "%S  [TOTAL]", &line);
        }
        avp_map_put(res->sections, avp_str(name), NULL)->p = avp_cz(a, line);
    }
}

GcWeakVec gc_weak_dedupe_index(AvpArena *a, const GcSymTxtVec *symbols, const GcSplitVec *splits) {
    GcUnitMap *ranges = gc_unit_map(a, splits, NULL);
    GcWeakVec out = {0};
    AvpSet *seen = avp_set_new();
    for (size_t i = 0; i < symbols->n; i++) {
        const GcSymTxt *s = &symbols->v[i];
        if (strcmp(s->scope, "weak") != 0) continue;
        if (!avp_set_add(seen, avp_str(s->name))) continue;
        const char *unit, *section;
        gc_unit_lookup(ranges, s->addr, &unit, &section, NULL);
        avp_vec_push(&out, ((GcWeak){s->name, unit, s->addr}));
    }
    avp_set_free(seen);
    return out;
}

GcDedupeVec gc_dedupe_for_unit(AvpArena *a, const GcWeakVec *index, const char *unit) {
    const char *mine = gc_strip_ext(a, gc_unit_key(a, unit));
    GcDedupeVec out = {0};
    for (size_t i = 0; i < index->n; i++) {
        const GcWeak *w = &index->v[i];
        if (!w->unit)
            avp_vec_push(&out, ((GcDedupe){w->name, avp_sprintf(a, "an unsplit range (%s)", gc_hx(a, w->addr, 8))}));
        else if (strcmp(gc_strip_ext(a, w->unit), mine) != 0)
            avp_vec_push(&out, ((GcDedupe){w->name, w->unit}));
    }
    return out;
}

GcLayoutResult *gc_check_layout(AvpArena *a, const char *unit, const char *status, const char *obj_path,
                                const char *asm_path, const ShsmDol *retail, const char *only_section,
                                const GcWeakVec *dedupe) {
    GcLayoutResult *res = AVP_NEW(a, GcLayoutResult);
    res->unit = unit;
    res->status = status;
    res->sections = avp_map_new();
    GcElf *e = NULL;
    GcAsmUnit *u = NULL;
    GcBuiltSections *b = NULL;
    AvpTry t;
    if (AVP_TRY(&t)) {
        e = gc_elf_load(a, obj_path);
        u = gc_parse_asm_data(a, gc_read_text(a, asm_path), retail);
        GcWeakVec none = {0};
        GcDedupeVec dd = gc_dedupe_for_unit(a, dedupe ? dedupe : &none, unit);
        b = gc_built_sections(a, e, u->names, &dd);
        avp_try_end(&t);
    } else {
        if (t.code == GC_EXC_CRASH) avp_throw(t.code, "%s", t.msg);
        if (!(gc_is_gate(t.code) || t.code == GC_EXC_STRUCT || t.code == GC_EXC_VALUE || t.code == GC_EXC_INDEX))
            avp_throw(t.code, "%s", t.msg);
        avp_vec_push(&res->errors, avp_str(avp_sprintf(a, "cannot read %s / %s: %s", obj_path, asm_path, t.msg)));
        return res;
    }
    gc_compare_code_sizes(a, res, u, e, only_section, retail);
    gc_compare_unit(a, res, u, b, only_section);
    return res;
}

AvpStrVec gc_format_layout(AvpArena *a, const GcLayoutResult *res, bool verbose) {
    AvpStrVec out = {0};
    for (size_t i = 0; i < res->errors.n; i++) avp_vec_push(&out, avp_pyfmt(a, "  ERROR %S", &res->errors.v[i]));
    for (size_t i = 0; i < res->findings.n; i++) {
        const GcFinding *f = &res->findings.v[i];
        if (f->difference || verbose) avp_vec_push(&out, avp_pyfmt(a, "  %-8s %-8s %s", f->kind, f->section, f->text));
    }
    if (verbose) {
        AvpMapIter it = {0};
        while (avp_map_next(res->sections, &it)) avp_vec_push(&out, avp_pyfmt(a, "  . %s", (const char *)it.val->p));
        for (size_t i = 0; i < res->notes.n; i++) avp_vec_push(&out, avp_pyfmt(a, "  note: %S", &res->notes.v[i]));
    }
    return out;
}
