/* sibling: the pairing tiers.
 *
 * exact      identical exact fingerprint, the only such function on both sides, not small.
 * exact-seq  identical exact fingerprint inside a gap: the functions between two consecutive pairs that are
 *            consecutive in both binaries (no other pair between them on either side). Within the gap a
 *            fingerprint pairs when it occurs the same number of times on both sides (k-th with k-th), and the
 *            resulting pairs do not cross each other.
 * calls      identical exact fingerprint (so the same call rows), ambiguous by hash, resolved by the callees: at
 *            least one call row whose target is already paired, every paired target agreeing, and the candidate
 *            the only one on both sides that passes. Data references are not checked. Not small.
 * seq-size   weak: in a gap whose two bounding pairs lie in the same sibling object, the remaining unpaired
 *            functions are the same number on both sides and their sizes agree one for one; the bodies differ.
 * Tiers 2 and 3 run to a fixed point (new pairs make new gaps and resolve new callees); tier 4 runs once at the end.
 * "small" is fewer than SIBLING_SMALL_ROWS instructions; small bodies pair only through gaps (tiers 2 and 4). */
#include "sibling.h"

#include <stdlib.h>
#include <string.h>

const char *const SIBLING_TIERS[SIB_NTIERS] = {"", "exact", "exact-seq", "calls", "seq-size"};

int sibling_tier_index(const char *name) {
    for (int i = 1; i < SIB_NTIERS; i++)
        if (name && strcmp(name, SIBLING_TIERS[i]) == 0) return i;
    return 0;
}

static bool small(const SibFn *f) { return f->nrows < SIBLING_SMALL_ROWS; }

static void link(SibFnVec *s, SibFnVec *g, size_t i, size_t j, int tier) {
    s->v[i].peer = (i64)j;
    s->v[i].tier = tier;
    g->v[j].peer = (i64)i;
    g->v[j].tier = tier;
}

typedef AVP_VEC(size_t) IdxVec;
typedef struct {
    IdxVec s, g;
} Group;

/* fingerprint -> Group of indices, both sides */
static AvpMap *group_by_exact(AvpArena *a, const SibFnVec *s, const SibFnVec *g) {
    AvpMap *m = avp_map_new();
    for (int side = 0; side < 2; side++) {
        const SibFnVec *v = side ? g : s;
        for (size_t i = 0; i < v->n; i++) {
            if (v->v[i].initialiser) continue;
            bool created;
            AvpVal *val = avp_map_put(m, avp_str(v->v[i].exact), &created);
            if (created) val->p = avp_alloc(a, sizeof(Group));
            Group *gr = val->p;
            avp_vec_push(side ? &gr->g : &gr->s, i);
        }
    }
    return m;
}

/* name -> index, -1 for names held by more than one function */
static AvpMap *names_of(const SibFnVec *v) {
    AvpMap *m = avp_map_new();
    for (size_t i = 0; i < v->n; i++) {
        if (!v->v[i].name) continue;
        bool created;
        AvpVal *val = avp_map_put(m, avp_str(v->v[i].name), &created);
        val->i = created ? (i64)i : -1;
    }
    return m;
}

static i64 lookup(const AvpMap *m, const char *name) {
    AvpVal *v = avp_map_get(m, avp_str(name));
    return v ? v->i : -1;
}

/* Callee evidence for s[i] ~ g[j]: 1 when at least one call target is paired and every paired target agrees,
 * 0 when nothing is known, -1 on a contradiction (both targets resolve to one function each, at least one of them
 * is paired, and not to the other). A target whose name is held by several functions tells nothing. */
static int callee_evidence(const SibFnVec *s, const SibFnVec *g, size_t i, size_t j, const AvpMap *sn,
                           const AvpMap *gn) {
    const SibFn *x = &s->v[i], *y = &g->v[j];
    if (x->ncallees != y->ncallees) return -1;
    int known = 0;
    for (size_t k = 0; k < x->ncallees; k++) {
        i64 cs = lookup(sn, x->callees[k]), cg = lookup(gn, y->callees[k]);
        if (cs < 0 || cg < 0) continue;
        if (cs == (i64)i && cg == (i64)j) continue; /* recursion */
        i64 ps = s->v[cs].peer, pg = g->v[cg].peer;
        if (ps < 0 && pg < 0) continue;
        if (ps != cg || pg != cs) return -1;
        known = 1;
    }
    return known;
}

/* Undo every pair (of the tiers in mask) whose callees contradict it. */
static i64 reject_contradicted(SibFnVec *s, SibFnVec *g, const AvpMap *sn, const AvpMap *gn, unsigned mask) {
    i64 n = 0;
    IdxVec bad = {0};
    for (size_t i = 0; i < s->n; i++)
        if (s->v[i].peer >= 0 && (mask & (1u << s->v[i].tier)) &&
            callee_evidence(s, g, i, (size_t)s->v[i].peer, sn, gn) < 0)
            avp_vec_push(&bad, i);
    for (size_t k = 0; k < bad.n; k++) {
        size_t i = bad.v[k], j = (size_t)s->v[i].peer;
        s->v[i].peer = -1, s->v[i].tier = SIB_NONE;
        g->v[j].peer = -1, g->v[j].tier = SIB_NONE;
        n++;
    }
    avp_vec_free(&bad);
    return n;
}

static i64 tier_calls(AvpArena *a, SibFnVec *s, SibFnVec *g, AvpMap *groups, const AvpMap *sn, const AvpMap *gn) {
    (void)a;
    i64 made = 0;
    AvpMapIter it = {0};
    IdxVec props_s = {0}, props_g = {0};
    while (avp_map_next(groups, &it)) {
        Group *gr = it.val->p;
        if (gr->s.n < 1 || gr->g.n < 1 || (gr->s.n == 1 && gr->g.n == 1)) continue;
        for (size_t x = 0; x < gr->s.n; x++) {
            size_t i = gr->s.v[x];
            if (s->v[i].peer >= 0 || small(&s->v[i])) continue;
            i64 pass = -1;
            int npass = 0;
            for (size_t y = 0; y < gr->g.n; y++) {
                size_t j = gr->g.v[y];
                if (g->v[j].peer >= 0) continue;
                if (callee_evidence(s, g, i, j, sn, gn) == 1) {
                    npass++;
                    pass = (i64)j;
                }
            }
            if (npass != 1) continue;
            /* the sibling side must be unambiguous too */
            int back = 0;
            for (size_t x2 = 0; x2 < gr->s.n; x2++) {
                size_t i2 = gr->s.v[x2];
                if (s->v[i2].peer >= 0) continue;
                if (callee_evidence(s, g, i2, (size_t)pass, sn, gn) == 1) back++;
            }
            if (back != 1) continue;
            avp_vec_push(&props_s, i);
            avp_vec_push(&props_g, (size_t)pass);
        }
    }
    for (size_t k = 0; k < props_s.n; k++) {
        if (s->v[props_s.v[k]].peer >= 0 || g->v[props_g.v[k]].peer >= 0) continue;
        link(s, g, props_s.v[k], props_g.v[k], SIB_CALLS);
        made++;
    }
    avp_vec_free(&props_s);
    avp_vec_free(&props_g);
    return made;
}

/* Gaps between pairs consecutive in both binaries: callback per gap with [si0, si1) and [gj0, gj1). */
typedef i64 (*GapFn)(void *ctx, SibFnVec *s, SibFnVec *g, size_t a_s, size_t b_s, size_t si0, size_t si1,
                     size_t gj0, size_t gj1);

static i64 for_gaps(SibFnVec *s, SibFnVec *g, GapFn fn, void *ctx) {
    /* prefix count of paired sibling functions */
    i64 *pc = avp_xcalloc(g->n + 1, sizeof *pc);
    for (size_t j = 0; j < g->n; j++) pc[j + 1] = pc[j] + (g->v[j].peer >= 0);
    IdxVec anchors = {0};
    for (size_t i = 0; i < s->n; i++)
        if (s->v[i].peer >= 0) avp_vec_push(&anchors, i);
    i64 made = 0;
    for (size_t k = 0; k + 1 < anchors.n; k++) {
        size_t a_s = anchors.v[k], b_s = anchors.v[k + 1];
        if (b_s == a_s + 1) continue;
        size_t ga = (size_t)s->v[a_s].peer, gb = (size_t)s->v[b_s].peer;
        if (gb <= ga + 1) continue;
        if (pc[gb] - pc[ga + 1] != 0) continue; /* another pair sits between them in the sibling */
        made += fn(ctx, s, g, a_s, b_s, a_s + 1, b_s, ga + 1, gb);
    }
    avp_vec_free(&anchors);
    free(pc);
    return made;
}

static i64 gap_exact(void *ctx, SibFnVec *s, SibFnVec *g, size_t a_s, size_t b_s, size_t si0, size_t si1,
                     size_t gj0, size_t gj1) {
    (void)ctx;
    (void)a_s;
    (void)b_s;
    AvpMap *m = avp_map_new();
    AvpArena *ar = avp_scratch();
    AvpArenaMark mk = avp_arena_mark(ar);
    for (int side = 0; side < 2; side++) {
        SibFnVec *v = side ? g : s;
        size_t lo = side ? gj0 : si0, hi = side ? gj1 : si1;
        for (size_t i = lo; i < hi; i++) {
            if (v->v[i].peer >= 0 || v->v[i].initialiser) continue;
            bool created;
            AvpVal *val = avp_map_put(m, avp_str(v->v[i].exact), &created);
            if (created) val->p = avp_alloc(ar, sizeof(Group));
            Group *gr = val->p;
            avp_vec_push(side ? &gr->g : &gr->s, i);
        }
    }
    IdxVec cs = {0}, cg = {0};
    AvpMapIter it = {0};
    while (avp_map_next(m, &it)) {
        Group *gr = it.val->p;
        if (gr->s.n && gr->s.n == gr->g.n)
            for (size_t k = 0; k < gr->s.n; k++) {
                avp_vec_push(&cs, gr->s.v[k]);
                avp_vec_push(&cg, gr->g.v[k]);
            }
        avp_vec_free(&gr->s);
        avp_vec_free(&gr->g);
    }
    avp_map_free(m);
    i64 made = 0;
    for (size_t x = 0; x < cs.n; x++) {
        bool crosses = false;
        for (size_t y = 0; y < cs.n && !crosses; y++) {
            if (x == y) continue;
            if ((cs.v[x] < cs.v[y]) != (cg.v[x] < cg.v[y])) crosses = true;
        }
        if (crosses) continue;
        link(s, g, cs.v[x], cg.v[x], SIB_EXACT_SEQ);
        made++;
    }
    avp_vec_free(&cs);
    avp_vec_free(&cg);
    avp_arena_reset(ar, mk);
    return made;
}

static bool same_object(const SibFn *x, const SibFn *y) {
    return x->object && y->object && strcmp(x->object, y->object) == 0 && strcmp(x->lib ? x->lib : "", y->lib ? y->lib : "") == 0;
}

static i64 gap_size(void *ctx, SibFnVec *s, SibFnVec *g, size_t a_s, size_t b_s, size_t si0, size_t si1,
                    size_t gj0, size_t gj1) {
    (void)ctx;
    if (!same_object(&g->v[s->v[a_s].peer], &g->v[s->v[b_s].peer])) return 0;
    IdxVec us = {0}, ug = {0};
    for (size_t i = si0; i < si1; i++)
        if (s->v[i].peer < 0 && !s->v[i].initialiser) avp_vec_push(&us, i);
    for (size_t j = gj0; j < gj1; j++)
        if (g->v[j].peer < 0 && !g->v[j].initialiser) avp_vec_push(&ug, j);
    i64 made = 0;
    bool ok = us.n && us.n == ug.n;
    for (size_t k = 0; ok && k < us.n; k++)
        if (s->v[us.v[k]].size != g->v[ug.v[k]].size) ok = false;
    if (ok)
        for (size_t k = 0; k < us.n; k++) {
            link(s, g, us.v[k], ug.v[k], SIB_SEQ_SIZE);
            made++;
        }
    avp_vec_free(&us);
    avp_vec_free(&ug);
    return made;
}

void sibling_pair(SibFnVec *s, SibFnVec *g, SibPairCounts *c) {
    AvpArena *a = avp_arena_new(0);
    memset(c, 0, sizeof *c);
    for (size_t i = 0; i < s->n; i++) s->v[i].initialiser |= sibling_is_initialiser(s->v[i].name);
    for (size_t j = 0; j < g->n; j++) g->v[j].initialiser |= sibling_is_initialiser(g->v[j].name);
    for (size_t i = 0; i < s->n; i++) s->v[i].peer = -1, s->v[i].tier = SIB_NONE;
    for (size_t j = 0; j < g->n; j++) g->v[j].peer = -1, g->v[j].tier = SIB_NONE;
    AvpMap *groups = group_by_exact(a, s, g);
    AvpMapIter it = {0};
    while (avp_map_next(groups, &it)) {
        Group *gr = it.val->p;
        if (gr->s.n != 1 || gr->g.n != 1) continue;
        size_t i = gr->s.v[0], j = gr->g.v[0];
        if (small(&s->v[i]) || small(&g->v[j])) continue;
        link(s, g, i, j, SIB_EXACT);
    }
    AvpMap *sn = names_of(s), *gn = names_of(g);
    const unsigned strong = 1u << SIB_EXACT | 1u << SIB_EXACT_SEQ | 1u << SIB_CALLS;
    c->rejected += reject_contradicted(s, g, sn, gn, strong);
    for (;;) {
        c->rounds++;
        i64 e = for_gaps(s, g, gap_exact, NULL);
        i64 k = tier_calls(a, s, g, groups, sn, gn);
        if (!e && !k) break;
    }
    c->rejected += reject_contradicted(s, g, sn, gn, strong);
    for_gaps(s, g, gap_size, NULL);
    c->rejected += reject_contradicted(s, g, sn, gn, 1u << SIB_SEQ_SIZE);
    for (size_t i = 0; i < s->n; i++) {
        switch (s->v[i].tier) {
        case SIB_EXACT: c->exact++; break;
        case SIB_EXACT_SEQ: c->exact_seq++; break;
        case SIB_CALLS: c->calls++; break;
        case SIB_SEQ_SIZE: c->seq_size++; break;
        default: break;
        }
    }
    it = (AvpMapIter){0};
    while (avp_map_next(groups, &it)) {
        Group *gr = it.val->p;
        avp_vec_free(&gr->s);
        avp_vec_free(&gr->g);
    }
    avp_map_free(groups);
    avp_map_free(sn);
    avp_map_free(gn);
    avp_arena_free(a);
}
