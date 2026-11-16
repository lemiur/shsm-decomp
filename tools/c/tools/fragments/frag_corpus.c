/* fragments.py Corpus: token streams with per-token variable ids, the seeds, the extension of a seed to a maximal
 * repeat, repeats(), boilerplate() and drop_contained(). */
#include "frag.h"

#include "avp/sort.h"
#include "shsm_re.h"

#include <stdlib.h>
#include <string.h>

FragCorpus *frag_corpus_new(int nlevels) {
    FragCorpus *c = avp_xcalloc(1, sizeof *c);
    c->nlevels = nlevels;
    c->arena = avp_arena_new(0);
    for (int lv = 0; lv < nlevels; lv++) c->intern[lv] = avp_map_new();
    return c;
}

void frag_corpus_free(FragCorpus *c) {
    if (!c) return;
    for (int lv = 0; lv < c->nlevels; lv++) {
        avp_map_free(c->intern[lv]);
        avp_vec_free(&c->rev[lv]);
    }
    avp_vec_free(&c->fns);
    free(c->cmap);
    free(c->cgen);
    avp_arena_free(c->arena);
    free(c);
}

static i32 intern(FragCorpus *c, int lv, AvpStr t) {
    bool created;
    AvpVal *v = avp_map_put(c->intern[lv], t, &created);
    if (created) {
        v->i = (i64)c->rev[lv].n;
        avp_vec_push(&c->rev[lv], avp_str_dup(c->arena, t));
    }
    return (i32)v->i;
}

i32 frag_corpus_add(FragCorpus *c, const FragRowIn *rows, size_t n) {
    AvpArena *a = c->arena;
    FragFn f;
    memset(&f, 0, sizeof f);
    f.n = n;
    size_t nr = 0;
    for (size_t i = 0; i < n; i++)
        if (!rows[i].sep) nr += (size_t)rows[i].nregs;
    for (int lv = 0; lv < c->nlevels; lv++) f.tok[lv] = avp_alloc(a, (n ? n : 1) * sizeof(i32));
    f.segend = avp_alloc(a, (n ? n : 1) * sizeof(i32));
    f.roff = avp_alloc(a, (n + 1) * sizeof(u32));
    f.rpool = avp_alloc(a, (nr ? nr : 1) * sizeof(i32));
    f.disp = avp_alloc(a, (n ? n : 1) * sizeof(AvpStr));
    u32 off = 0;
    for (size_t i = 0; i < n; i++) {
        f.roff[i] = off;
        if (rows[i].sep) {
            for (int lv = 0; lv < c->nlevels; lv++) f.tok[lv][i] = FRAG_SEP;
            f.disp[i] = AVP_S("");
            continue;
        }
        for (int lv = 0; lv < c->nlevels; lv++) f.tok[lv][i] = intern(c, lv, rows[i].tok[lv]);
        for (int k = 0; k < rows[i].nregs; k++) {
            i32 v = rows[i].regs[k];
            f.rpool[off++] = v;
            if (v + 1 > c->nvars) c->nvars = v + 1;
        }
        f.disp[i] = avp_str_dup(a, rows[i].disp);
    }
    f.roff[n] = off;
    i32 end = (i32)n;
    for (i64 i = (i64)n - 1; i >= 0; i--) {
        if (rows[i].sep) {
            end = (i32)i;
            f.segend[i] = -1;
        } else {
            f.segend[i] = end;
        }
    }
    avp_vec_push(&c->fns, f);
    return (i32)c->fns.n - 1;
}

void frag_corpus_rows(const FragCorpus *c, i32 fi, i32 s, i32 len, FragRow *out) {
    const FragFn *f = &c->fns.v[fi];
    for (i32 k = 0; k < len; k++) {
        i32 i = s + k;
        out[k].regs = f->rpool + f->roff[i];
        out[k].nregs = (int)(f->roff[i + 1] - f->roff[i]);
        out[k].disp = f->disp[i];
    }
}

static void canon_reserve(FragCorpus *c) {
    if (c->ccap >= c->nvars) return;
    i32 cap = c->nvars + 64;
    c->cmap = realloc(c->cmap, (size_t)cap * sizeof *c->cmap);
    c->cgen = realloc(c->cgen, (size_t)cap * sizeof *c->cgen);
    if (!c->cmap || !c->cgen) avp_panic(__FILE__, __LINE__, "out of memory");
    memset(c->cgen + c->ccap, 0, (size_t)(cap - c->ccap) * sizeof *c->cgen);
    c->ccap = cap;
}

static u32 canon_begin(FragCorpus *c) {
    canon_reserve(c);
    if (++c->gen == 0) {
        memset(c->cgen, 0, (size_t)c->ccap * sizeof *c->cgen);
        c->gen = 1;
    }
    return c->gen;
}

void frag_canon(FragCorpus *c, i32 fi, i32 a, i32 b, AvpI32Vec *out) {
    out->n = 0;
    const FragFn *f = &c->fns.v[fi];
    u32 g = canon_begin(c);
    i32 next = 0;
    for (i32 i = a; i < b; i++) {
        for (u32 k = f->roff[i]; k < f->roff[i + 1]; k++) {
            i32 r = f->rpool[k];
            i32 v;
            if (r < 0) {
                v = -2; /* not reachable: every register of a row has an id */
            } else if (c->cgen[r] == g) {
                v = c->cmap[r];
            } else {
                c->cgen[r] = g;
                v = c->cmap[r] = next++;
            }
            avp_vec_push(out, v);
        }
        avp_vec_push(out, -1);
    }
}

static bool canon_eq(const AvpI32Vec *x, const AvpI32Vec *y) {
    return x->n == y->n && (x->n == 0 || memcmp(x->v, y->v, x->n * sizeof(i32)) == 0);
}

static u64 hash_i32s(u64 h, const i32 *v, size_t n) {
    for (size_t i = 0; i < n; i++) {
        h ^= (u64)(u32)v[i];
        h *= 0x100000001B3ull;
        h ^= h >> 29;
    }
    return h;
}

/* ---- seeds ---- */

typedef struct {
    u64 h;
    i32 fi, p;      /* the first window (the key's representative) */
    i32 head, tail; /* window list */
    i32 count;
} Group;

static int cmp_seed(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const FragRepeat *a = x, *b = y;
    /* -len({fi}) is stored in length while sorting */
    if (a->length != b->length) return a->length < b->length ? -1 : 1;
    const FragPos *pa = &a->occ.v[0], *pb = &b->occ.v[0];
    if (pa->fi != pb->fi) return pa->fi < pb->fi ? -1 : 1;
    if (pa->p != pb->p) return pa->p < pb->p ? -1 : 1;
    return 0;
}

static i32 distinct_fi(const FragOcc *o) {
    i32 n = 0;
    for (size_t i = 0; i < o->n; i++)
        if (i == 0 || o->v[i].fi != o->v[i - 1].fi) n++;
    return n;
}

FragRepeats frag_seeds(FragCorpus *c, int level, i32 n, i32 k) {
    FragRepeats out = {0};
    /* every window inside a run, grouped by (tokens, canon) */
    size_t nwin = 0;
    for (size_t fi = 0; fi < c->fns.n; fi++) {
        const FragFn *f = &c->fns.v[fi];
        for (i64 i = 0; i + n <= (i64)f->n; i++)
            if (f->segend[i] >= i + n) nwin++;
    }
    if (!nwin) return out;
    size_t cap = 16;
    while (cap < nwin * 2) cap <<= 1;
    i32 *slot = avp_xmalloc(cap * sizeof *slot); /* group index or -1 */
    memset(slot, 0xFF, cap * sizeof *slot);
    AVP_VEC(Group) groups = {0};
    i32 *wfi = avp_xmalloc(nwin * sizeof *wfi), *wp = avp_xmalloc(nwin * sizeof *wp),
        *wnext = avp_xmalloc(nwin * sizeof *wnext);
    AvpI32Vec cv = {0}, cg = {0};
    size_t w = 0;
    for (size_t fi = 0; fi < c->fns.n; fi++) {
        const FragFn *f = &c->fns.v[fi];
        const i32 *ts = f->tok[level];
        for (i64 i = 0; i + n <= (i64)f->n; i++) {
            if (f->segend[i] < i + n) continue;
            frag_canon(c, (i32)fi, (i32)i, (i32)(i + n), &cv);
            u64 h = hash_i32s(0xCBF29CE484222325ull, ts + i, (size_t)n);
            h = hash_i32s(h, cv.v, cv.n);
            size_t pos = (size_t)(h & (cap - 1));
            i32 gi = -1;
            for (;;) {
                i32 s = slot[pos];
                if (s < 0) break;
                Group *g = &groups.v[s];
                if (g->h == h && memcmp(c->fns.v[g->fi].tok[level] + g->p, ts + i, (size_t)n * sizeof(i32)) == 0) {
                    frag_canon(c, g->fi, g->p, g->p + n, &cg);
                    if (canon_eq(&cv, &cg)) {
                        gi = s;
                        break;
                    }
                }
                pos = (pos + 1) & (cap - 1);
            }
            wfi[w] = (i32)fi;
            wp[w] = (i32)i;
            wnext[w] = -1;
            if (gi < 0) {
                gi = (i32)groups.n;
                avp_vec_push(&groups, ((Group){h, (i32)fi, (i32)i, (i32)w, (i32)w, 1}));
                slot[pos] = gi;
            } else {
                Group *g = &groups.v[gi];
                wnext[g->tail] = (i32)w;
                g->tail = (i32)w;
                g->count++;
            }
            w++;
        }
    }
    for (size_t gi = 0; gi < groups.n; gi++) {
        const Group *g = &groups.v[gi];
        if (g->count < k) continue;
        i32 nd = 0, last = -1;
        for (i32 x = g->head; x >= 0; x = wnext[x])
            if (wfi[x] != last) {
                nd++;
                last = wfi[x];
            }
        if (nd < k) continue;
        FragOcc kept = {0}; /* windows come in (fi, p) order already */
        for (i32 x = g->head; x >= 0; x = wnext[x]) {
            if (kept.n && kept.v[kept.n - 1].fi == wfi[x] && wp[x] < kept.v[kept.n - 1].p + n) continue;
            avp_vec_push(&kept, ((FragPos){wfi[x], wp[x]}));
        }
        if (distinct_fi(&kept) >= k) avp_vec_push(&out, ((FragRepeat){n, kept}));
        else avp_vec_free(&kept);
    }
    free(slot);
    free(wfi);
    free(wp);
    free(wnext);
    avp_vec_free(&groups);
    avp_vec_free(&cv);
    avp_vec_free(&cg);
    return out;
}

/* ---- extend ---- */

static bool agree(FragCorpus *c, int level, const FragOcc *occ, i32 a, i32 b, AvpI32Vec *ref, AvpI32Vec *cur) {
    const i32 *rt = NULL;
    for (size_t o = 0; o < occ->n; o++) {
        i32 fi = occ->v[o].fi, p = occ->v[o].p;
        i32 s = p + a, e = p + b;
        const FragFn *f = &c->fns.v[fi];
        if (s < 0 || f->segend[s] < e) return false;
        if (!rt) {
            rt = f->tok[level] + s;
            frag_canon(c, fi, s, e, ref);
        } else {
            if (memcmp(rt, f->tok[level] + s, (size_t)(e - s) * sizeof(i32)) != 0) return false;
            frag_canon(c, fi, s, e, cur);
            if (!canon_eq(ref, cur)) return false;
        }
    }
    return true;
}

static bool no_overlap(const FragOcc *occ, i32 a, i32 b) {
    for (size_t o = 1; o < occ->n; o++)
        if (occ->v[o - 1].fi == occ->v[o].fi && occ->v[o].p + a < occ->v[o - 1].p + b) return false;
    return true;
}

FragRepeat frag_extend(FragCorpus *c, int level, const FragOcc *occ, i32 n) {
    i32 a = 0, b = n;
    AvpI32Vec ref = {0}, cur = {0};
    for (;;) { /* right */
        bool ok = true, have = false;
        i32 nxt = 0;
        for (size_t o = 0; o < occ->n && ok; o++) {
            const FragFn *f = &c->fns.v[occ->v[o].fi];
            i32 p = occ->v[o].p, q = p + b;
            if ((size_t)q >= f->n || f->segend[p] <= q) {
                ok = false;
            } else if (!have) {
                nxt = f->tok[level][q];
                have = true;
            } else if (f->tok[level][q] != nxt) {
                ok = false;
            }
        }
        if (!ok || !no_overlap(occ, a, b + 1) || !agree(c, level, occ, a, b + 1, &ref, &cur)) break;
        b++;
    }
    for (;;) { /* left */
        bool ok = true, have = false;
        i32 prev = 0;
        for (size_t o = 0; o < occ->n && ok; o++) {
            const FragFn *f = &c->fns.v[occ->v[o].fi];
            i32 q = occ->v[o].p + a - 1;
            if (q < 0 || f->tok[level][q] == FRAG_SEP) {
                ok = false;
            } else if (!have) {
                prev = f->tok[level][q];
                have = true;
            } else if (f->tok[level][q] != prev) {
                ok = false;
            }
        }
        if (!ok || !no_overlap(occ, a - 1, b) || !agree(c, level, occ, a - 1, b, &ref, &cur)) break;
        a--;
    }
    avp_vec_free(&ref);
    avp_vec_free(&cur);
    FragRepeat r = {b - a, {0}};
    avp_vec_reserve(&r.occ, occ->n);
    for (size_t o = 0; o < occ->n; o++) avp_vec_push(&r.occ, ((FragPos){occ->v[o].fi, occ->v[o].p + a}));
    return r;
}

/* ---- repeats ---- */

typedef struct {
    i32 count, idx, next;
} Cov;
typedef AVP_VEC(Cov) CovVec;

void frag_repeats_free(FragRepeats *r) {
    for (size_t i = 0; i < r->n; i++) avp_vec_free(&r->v[i].occ);
    avp_vec_free(r);
}

static u64 poskey(i32 fi, i32 p) { return ((u64)(u32)fi << 32) | (u32)p; }

/* covered.get((fi, p), {}).get(count): the fragment index or -1 */
static i32 cov_get(AvpIMap *covered, const CovVec *cov, i32 fi, i32 p, i32 count) {
    AvpVal *v = avp_imap_get(covered, poskey(fi, p));
    if (!v) return -1;
    for (i32 x = (i32)v->i; x >= 0; x = cov->v[x].next)
        if (cov->v[x].count == count) return cov->v[x].idx;
    return -1;
}

FragRepeats frag_repeats(FragCorpus *c, int level, i32 n, i32 k) {
    FragRepeats seeds = frag_seeds(c, level, n, k);
    for (size_t i = 0; i < seeds.n; i++) seeds.v[i].length = -distinct_fi(&seeds.v[i].occ);
    avp_sort(seeds.v, seeds.n, sizeof *seeds.v, cmp_seed, NULL, false);
    AvpIMap *covered = avp_imap_new();
    CovVec cov = {0};
    AvpSet *seen = avp_set_new();
    FragRepeats frags = {0};
    AvpBuf kb = {0};
    for (size_t si = 0; si < seeds.n; si++) {
        const FragOcc *occ = &seeds.v[si].occ;
        i32 cnt = (i32)occ->n;
        i32 first = cov_get(covered, &cov, occ->v[0].fi, occ->v[0].p, cnt);
        bool skip = first >= 0;
        for (size_t o = 1; o < occ->n && skip; o++)
            if (cov_get(covered, &cov, occ->v[o].fi, occ->v[o].p, cnt) != first) skip = false;
        if (skip) continue; /* an inner window of a fragment with the same sites */
        FragRepeat r = frag_extend(c, level, occ, n);
        avp_buf_clear(&kb);
        avp_buf_putn(&kb, (const char *)&r.length, sizeof r.length);
        avp_buf_putn(&kb, (const char *)r.occ.v, r.occ.n * sizeof *r.occ.v);
        if (!avp_set_add(seen, avp_buf_str(&kb))) {
            avp_vec_free(&r.occ);
            continue;
        }
        i32 idx = (i32)frags.n;
        avp_vec_push(&frags, r);
        i32 nocc = (i32)r.occ.n;
        for (size_t o = 0; o < r.occ.n; o++) {
            i32 fi = r.occ.v[o].fi, s = r.occ.v[o].p;
            for (i32 q = s; q < s + r.length - n + 1; q++) {
                bool created;
                AvpVal *v = avp_imap_put(covered, poskey(fi, q), &created);
                if (created) v->i = -1;
                bool have = false;
                for (i32 x = (i32)v->i; x >= 0 && !have; x = cov.v[x].next)
                    if (cov.v[x].count == nocc) have = true;
                if (!have) {
                    avp_vec_push(&cov, ((Cov){nocc, idx, (i32)v->i}));
                    v->i = (i64)cov.n - 1;
                }
            }
        }
    }
    avp_buf_free(&kb);
    avp_set_free(seen);
    avp_imap_free(covered);
    avp_vec_free(&cov);
    frag_repeats_free(&seeds);
    return frags;
}

/* ---- boilerplate, drop_contained ---- */

const char *frag_boilerplate(FragCorpus *c, int level, const FragRepeat *f) {
    i32 fi = f->occ.v[0].fi, s = f->occ.v[0].p;
    const ShsmRe *sw = shsm_re("(?:cmp\\w*|b|b[a-z]{2}[+-]?|bdnz|bdz|crclr|cror|cmpwi\\.?)$", 0);
    bool all = true;
    for (i32 i = s; i < s + f->length && all; i++) {
        AvpStr t = c->rev[level].v[c->fns.v[fi].tok[level][i]];
        i64 sp = avp_str_find(t, AVP_S(" "), 0, AVP_END);
        AvpStr mn = sp >= 0 ? avp_strn(t.p, (size_t)sp) : t;
        if (!shsm_re_match(sw, mn, 0, AVP_END, NULL)) all = false;
    }
    if (all) return "switch";
    for (size_t o = 0; o < f->occ.n; o++) {
        const char *name = c->fns.v[f->occ.v[o].fi].name;
        if (!name || !strstr(name, "__dt__")) return NULL;
    }
    return "destructor";
}

typedef struct {
    i32 start, end, k;
} Span;
typedef AVP_VEC(Span) Spans;

FragRepeats frag_drop_contained(FragRepeats in) {
    FragRepeats kept = {0};
    AvpIMap *spans = avp_imap_new(); /* fi -> index into lists + 1 */
    AVP_VEC(Spans) lists = {0};
    AvpI32Vec common = {0}, next = {0};
    for (size_t i = 0; i < in.n; i++) {
        FragRepeat *fr = &in.v[i];
        bool have_common = false;
        for (size_t o = 0; o < fr->occ.n; o++) {
            i32 fi = fr->occ.v[o].fi, s = fr->occ.v[o].p;
            AvpVal *v = avp_imap_get(spans, (u64)(u32)fi);
            next.n = 0;
            if (v) {
                const Spans *sp = &lists.v[v->i];
                for (size_t x = 0; x < sp->n; x++)
                    if (sp->v[x].start <= s && s + fr->length <= sp->v[x].end) {
                        i32 kk = sp->v[x].k;
                        bool in_common = !have_common;
                        for (size_t y = 0; y < common.n && !in_common; y++)
                            if (common.v[y] == kk) in_common = true;
                        bool dup = false;
                        for (size_t y = 0; y < next.n && !dup; y++)
                            if (next.v[y] == kk) dup = true;
                        if (in_common && !dup) avp_vec_push(&next, kk);
                    }
            }
            common.n = 0;
            for (size_t y = 0; y < next.n; y++) avp_vec_push(&common, next.v[y]);
            have_common = true;
            if (!common.n) break;
        }
        if (have_common && common.n) {
            avp_vec_free(&fr->occ);
            continue;
        }
        i32 idx = (i32)kept.n;
        avp_vec_push(&kept, *fr);
        for (size_t o = 0; o < fr->occ.n; o++) {
            i32 fi = fr->occ.v[o].fi, s = fr->occ.v[o].p;
            bool created;
            AvpVal *v = avp_imap_put(spans, (u64)(u32)fi, &created);
            if (created) {
                v->i = (i64)lists.n;
                avp_vec_push_zero(&lists);
            }
            avp_vec_push(&lists.v[v->i], ((Span){s, s + fr->length, idx}));
        }
    }
    for (size_t i = 0; i < lists.n; i++) avp_vec_free(&lists.v[i]);
    avp_vec_free(&lists);
    avp_imap_free(spans);
    avp_vec_free(&common);
    avp_vec_free(&next);
    avp_vec_free(&in);
    return kept;
}
