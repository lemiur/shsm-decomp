/* sibling: proposed unit ranges. A sibling object's paired span leaves out the retail functions that did not pair
 * (statics, another revision's body, members the sibling's linker dropped); this file hands each unpaired retail
 * function between two neighbouring spans to the left object, the right one or neither, from evidence:
 *
 *   split    an already-split unit holds it with one of the spans (or holds neither: it is another unit)   weight 10
 *   pair     it is paired in a tier not selected with a member of one object                                 6
 *   name     its real name is an unpaired or UNUSED entry of one object                                       6
 *   size     its size equals (3) or is within 1/8 of (2) an unpaired or UNUSED entry of one side only
 *   callers  every caller lies in one span                                                                    3
 *   capacity each object takes at most as many functions as its map lists unpaired entries past its span
 *            (after the last paired member for the left object, before the first for the right one): +1 for
 *            each function within capacity, -2 for each beyond it; a function no object can hold is "neither".
 *   library  a function of another retail library (libraries.tsv) cannot belong to the object.
 *
 * Every split point (left takes [0,p), right takes [q,m), neither [p,q)) is scored; a function whose owner is the
 * same in every best-scoring split is assigned, any other is ambiguous. */
#include "sibling.h"

#include "shsm_symbols.h"

#include <string.h>

enum { SIDE_NONE = 0, SIDE_LEFT = 1, SIDE_RIGHT = 2, SIDE_NEITHER = 3 };

typedef struct {
    const char *name;
    i64 size;
    bool unused;
    i64 order; /* used entries: their map order; UNUSED entries: the order of the next used entry (before) */
    i64 peer;  /* used entries: retail index of their pair (any tier), -1 */
    int tier;
} Entry;
typedef AVP_VEC(Entry) EntryVec;

typedef struct {
    int side, weight;
    const char *why;
    bool strong; /* a split unit, a pair, a name or a call edge: what "assigned" needs; size and capacity only suggest */
} Evidence;

static bool tier_in(unsigned tiers, int tier) { return tier && (tiers & (1u << tier)); }

static const char *key_of(AvpArena *a, const char *lib, const char *object) {
    return avp_sprintf(a, "%s|%s", lib ? lib : "", object ? object : "");
}

/* lib|object -> EntryVec* of the sibling's map entries, used ones from g and UNUSED ones */
static AvpMap *object_entries(AvpArena *a, const SibFnVec *g, const SibMapSymVec *unused) {
    AvpMap *m = avp_map_new();
    for (size_t j = 0; j < g->n; j++) {
        const SibFn *f = &g->v[j];
        if (!f->object) continue;
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(key_of(a, f->lib, f->object)), &created);
        if (created) v->p = avp_alloc(a, sizeof(EntryVec));
        Entry e = {f->name, f->size, false, f->order, f->peer, f->tier};
        avp_vec_push((EntryVec *)v->p, e);
    }
    for (size_t k = 0; unused && k < unused->n; k++) {
        const SibMapSym *u = &unused->v[k];
        bool created;
        AvpVal *v = avp_map_put(m, avp_str(key_of(a, u->lib, u->object)), &created);
        if (created) v->p = avp_alloc(a, sizeof(EntryVec));
        Entry e = {u->name, u->size, true, u->before, -1, 0};
        avp_vec_push((EntryVec *)v->p, e);
    }
    return m;
}

/* The object's entries past its span: after the used entry `bound` (left object) or before it (right object).
 * An UNUSED entry of unknown position (before -1) counts on both sides. Entries paired in a selected tier are not
 * candidates: they belong to their own retail function. */
static EntryVec candidates(const EntryVec *all, i64 bound, bool after, unsigned tiers) {
    EntryVec out = {0};
    for (size_t k = 0; all && k < all->n; k++) {
        const Entry *e = &all->v[k];
        if (!e->unused && e->peer >= 0 && tier_in(tiers, e->tier)) continue;
        bool past;
        if (e->unused) past = e->order < 0 || (after ? e->order > bound : e->order <= bound);
        else past = after ? e->order > bound : e->order < bound;
        if (past) avp_vec_push(&out, *e);
    }
    return out;
}

static bool close_size(i64 x, i64 y) {
    i64 d = x > y ? x - y : y - x, big = x > y ? x : y;
    return d <= big / 8;
}

/* 2: an entry of that size; 1: an entry within 1/8; 0: none. *name: the entry */
static int size_hit(const EntryVec *c, i64 size, const char **name) {
    int best = 0;
    for (size_t k = 0; k < c->n; k++) {
        int h = c->v[k].size == size ? 2 : close_size(c->v[k].size, size) ? 1 : 0;
        if (h > best) best = h, *name = c->v[k].name;
    }
    return best;
}

static bool name_hit(const EntryVec *c, const char *name) {
    if (!name || sibling_is_placeholder(name)) return false;
    for (size_t k = 0; k < c->n; k++)
        if (c->v[k].name && strcmp(c->v[k].name, name) == 0) return true;
    return false;
}

typedef struct {
    AvpArena *a;
    const SibFnVec *s, *g;
    const ShsmSplits *splits;
    AvpMap *callers; /* retail name -> AvpI32Vec* of caller indices */
    unsigned tiers;
    AvpMap *index;   /* retail name -> index + the .i of its entry */
} Ctx;

static bool same_lib(const SibFn *x, const SibFn *y) {
    return strcmp(x->lib ? x->lib : "", y->lib ? y->lib : "") == 0;
}

static bool split_holds(const ShsmSpan *u, const SibFnVec *s, const SibSpanRef *sp) {
    return sp && u->start <= (u64)s->v[sp->first].address && (u64)s->v[sp->last].address < u->end;
}

/* The evidence for one gap function between spans L and R (either NULL). */
static Evidence evidence_of(const Ctx *c, size_t i, const SibSpanRef *L, const SibSpanRef *R, const EntryVec *cl,
                            const EntryVec *cr) {
    AvpArena *a = c->a;
    const SibFn *f = &c->s->v[i];
    Evidence ev = {SIDE_NONE, 0, NULL};
    if (c->splits) {
        const ShsmSpan *u = shsm_splits_range_of(c->splits, (u64)f->address);
        if (u && u->unit && u->section && strncmp(u->section, ".text", 5) == 0) {
            bool l = split_holds(u, c->s, L), r = split_holds(u, c->s, R);
            if (l != r) return (Evidence){l ? SIDE_LEFT : SIDE_RIGHT, 10, avp_sprintf(a, "split unit %s", u->unit), true};
            if (!l) return (Evidence){SIDE_NEITHER, 10, avp_sprintf(a, "split unit %s holds neither span", u->unit), true};
        }
    }
    if (f->peer >= 0) {
        const SibFn *p = &c->g->v[f->peer];
        const SibSpanRef *sides[2] = {L, R};
        for (int k = 0; k < 2; k++)
            if (sides[k] && p->object && strcmp(p->object, sides[k]->object) == 0 &&
                strcmp(p->lib ? p->lib : "", sides[k]->lib ? sides[k]->lib : "") == 0)
                return (Evidence){k ? SIDE_RIGHT : SIDE_LEFT, 6,
                                  avp_sprintf(a, "paired (%s) with %s #%lld", SIBLING_TIERS[f->tier], p->name,
                                              (long long)p->order), true};
    }
    bool nl = L && name_hit(cl, f->name), nr = R && name_hit(cr, f->name);
    if (nl != nr) return (Evidence){nl ? SIDE_LEFT : SIDE_RIGHT, 6, avp_sprintf(a, "name %s in the map", f->name), true};
    const char *nml = NULL, *nmr = NULL;
    int hl = L ? size_hit(cl, f->size, &nml) : 0, hr = R ? size_hit(cr, f->size, &nmr) : 0;
    Evidence sz = ev; /* weak: used only when a call edge does not decide */
    if (hl != hr) {
        bool left = hl > hr;
        int h = left ? hl : hr;
        sz = (Evidence){left ? SIDE_LEFT : SIDE_RIGHT, h == 2 ? 3 : 2,
                          avp_sprintf(a, "size 0x%llX %s %s", (long long)f->size, h == 2 ? "=" : "~",
                                      left ? nml : nmr), false};
    }
    AvpVal *cv = f->name ? avp_map_get(c->callers, avp_str(f->name)) : NULL;
    if (cv) {
        const AvpI32Vec *cs = cv->p;
        size_t inl = 0, inr = 0;
        for (size_t k = 0; k < cs->n; k++) {
            size_t x = (size_t)cs->v[k];
            if (L && x >= L->first && x <= L->last) inl++;
            else if (R && x >= R->first && x <= R->last) inr++;
        }
        if (inl == cs->n || inr == cs->n)
            return (Evidence){inl ? SIDE_LEFT : SIDE_RIGHT, 3,
                              avp_sprintf(a, "called only from the %s span (%zu)", inl ? "left" : "right", cs->n), true};
    }
    /* a call edge out of the function into a paired function of one object */
    size_t outl = 0, outr = 0;
    const char *nl_callee = NULL, *nr_callee = NULL;
    for (size_t k = 0; k < f->ncallees; k++) {
        AvpVal *tv = avp_map_get(c->index, avp_str(f->callees[k]));
        if (!tv) continue;
        size_t x = (size_t)tv->i;
        if (c->s->v[x].peer < 0 || !tier_in(c->tiers, c->s->v[x].tier)) continue;
        if (L && x >= L->first && x <= L->last) outl++, nl_callee = f->callees[k];
        else if (R && x >= R->first && x <= R->last) outr++, nr_callee = f->callees[k];
    }
    if ((outl > 0) != (outr > 0))
        return (Evidence){outl ? SIDE_LEFT : SIDE_RIGHT, 3,
                          avp_sprintf(a, "calls %s, a paired function of the %s object", outl ? nl_callee : nr_callee,
                                      outl ? "left" : "right"), true};
    return sz;
}

static i64 value(const Evidence *e, int side) {
    if (e->side == SIDE_NONE) return 0;
    return e->side == side ? e->weight : -e->weight;
}

typedef struct {
    SibGapFnVec *after;  /* L's list */
    SibGapFnVec *before; /* R's list */
} Lists;

/* Decide the gap fns[lo, hi) between L and R; append to L's after list and R's before list; return p and q. */
static void split_gap(const Ctx *c, size_t lo, size_t hi, const SibSpanRef *L, const SibSpanRef *R,
                      const EntryVec *cl, const EntryVec *cr, Lists out, size_t *pp, size_t *qq) {
    AvpArena *a = c->a;
    size_t m = hi - lo;
    *pp = 0, *qq = m;
    if (!m) return;
    Evidence *ev = avp_alloc(a, m * sizeof *ev);
    size_t maxp = 0, minq = m; /* the library constraint */
    if (L)
        while (maxp < m && same_lib(&c->s->v[lo + maxp], &c->s->v[L->first])) maxp++;
    if (R)
        while (minq > 0 && same_lib(&c->s->v[lo + minq - 1], &c->s->v[R->first])) minq--;
    for (size_t i = 0; i < m; i++) ev[i] = evidence_of(c, lo + i, L, R, cl, cr);
    i64 capl = L ? (i64)cl->n : 0, capr = R ? (i64)cr->n : 0;
    /* prefix sums of each side's evidence value */
    i64 *sl = avp_alloc(a, (m + 1) * sizeof *sl), *sr = avp_alloc(a, (m + 1) * sizeof *sr),
        *sn = avp_alloc(a, (m + 1) * sizeof *sn);
    for (size_t i = 0; i < m; i++) {
        sl[i + 1] = sl[i] + value(&ev[i], SIDE_LEFT);
        sr[i + 1] = sr[i] + value(&ev[i], SIDE_RIGHT);
        sn[i + 1] = sn[i] + value(&ev[i], SIDE_NEITHER);
    }
    i64 best = INT64_MIN;
    size_t pmin = 0, pmax = 0, qmin = m, qmax = m, bp = 0, bq = m;
    for (size_t p = 0; p <= maxp; p++)
        for (size_t q = p > minq ? p : minq; q <= m; q++) {
            i64 tl = (i64)p, tr = (i64)(m - q);
            i64 sc = sl[p] + (sr[m] - sr[q]) + (sn[q] - sn[p]);
            sc += (tl < capl ? tl : capl) - 2 * (tl > capl ? tl - capl : 0);
            sc += (tr < capr ? tr : capr) - 2 * (tr > capr ? tr - capr : 0);
            if (sc > best) {
                best = sc, pmin = pmax = bp = p, qmin = qmax = bq = q;
            } else if (sc == best) {
                if (p < pmin) pmin = p;
                if (p > pmax) pmax = p;
                if (q < qmin) qmin = q;
                if (q > qmax) qmax = q;
                if (q - p < bq - bp) bp = p, bq = q; /* the canonical split: the fewest left to neither */
            }
        }
    /* A function is assigned to an object only on evidence of its own (a split unit, a pair, a name, a call edge)
     * or between two such functions; one the best split hands to an object on size or map capacity alone is a
     * candidate and stays out of the proposed range, which ends at the last (starts at the first) function the
     * stronger evidence decides. */
    int *sides = avp_alloc(a, m * sizeof *sides);
    size_t last_l = 0, first_r = m; /* the proposed prefix [0, last_l) and suffix [first_r, m) */
    for (size_t i = 0; i < m; i++) {
        sides[i] = i < pmin ? SIDE_LEFT : i >= qmax ? SIDE_RIGHT : (i >= pmax && i < qmin) ? SIDE_NEITHER : SIDE_NONE;
        if (sides[i] == SIDE_LEFT && ev[i].side == SIDE_LEFT && ev[i].strong) last_l = i + 1;
        if (sides[i] == SIDE_RIGHT && ev[i].side == SIDE_RIGHT && ev[i].strong && first_r == m) first_r = i;
    }
    if (last_l > bp) last_l = bp;
    if (first_r < bq) first_r = bq;
    *pp = last_l, *qq = first_r;
    for (size_t i = 0; i < m; i++) {
        const SibFn *f = &c->s->v[lo + i];
        SibGapFn x = {f->address, f->size, f->name, false, false, false, NULL};
        int side = sides[i];
        x.assigned = side != SIDE_NONE;
        bool candidate = (side == SIDE_LEFT && i >= last_l) || (side == SIDE_RIGHT && i < first_r);
        if (candidate) {
            /* the neighbouring object of the gap that has an unpaired entry of this size as well is the alternative */
            const EntryVec *other = side == SIDE_LEFT ? cr : cl;
            const SibSpanRef *oref = side == SIDE_LEFT ? R : L;
            const char *onm = NULL;
            int oh = oref ? size_hit(other, f->size, &onm) : 0;
            x.assigned = false;
            x.candidate = true;
            x.why = avp_sprintf(a, "candidate (size only): %s%s%s%s%s", ev[i].side == side ? ev[i].why : "map capacity",
                                oh ? "; alternative: " : "", oh ? oref->object : "", oh ? ", its unpaired " : "",
                                oh ? onm : "");
        } else if (side == SIDE_NONE) {
            x.why = ev[i].why ? avp_sprintf(a, "ambiguous (%s, but the best splits differ)", ev[i].why)
                              : "ambiguous (no evidence; the best splits differ)";
        } else if (ev[i].side == side) {
            x.why = ev[i].why;
        } else {
            const char *near = NULL;
            if (side == SIDE_LEFT)
                for (size_t j = i + 1; j < pmin && !near; j++)
                    if (ev[j].side == SIDE_LEFT) near = avp_sprintf(a, "before %s (%s)", c->s->v[lo + j].name, ev[j].why);
            if (side == SIDE_RIGHT)
                for (size_t j = i; j-- > qmax && !near;)
                    if (ev[j].side == SIDE_RIGHT) near = avp_sprintf(a, "after %s (%s)", c->s->v[lo + j].name, ev[j].why);
            x.why = near ? near
                         : avp_sprintf(a, "map capacity: %lld unpaired entries left, %lld right%s", (long long)capl,
                                       (long long)capr, ev[i].why ? avp_sprintf(a, "; against %s", ev[i].why) : "");
        }
        /* each object lists what it takes and the ambiguous functions it could take */
        if (L && (i < bp || (side == SIDE_NONE && i < maxp))) {
            x.mine = i < last_l;
            avp_vec_push(out.after, x);
        }
        if (R && (i >= bq || (side == SIDE_NONE && i >= minq))) {
            x.mine = i >= first_r;
            avp_vec_push(out.before, x);
        }
    }
}

SibProposal *sibling_propose_units(AvpArena *a, const SibFnVec *s, const SibFnVec *g, const SibMapSymVec *unused,
                                   const ShsmSplits *splits, const SibSpanRef *spans, size_t n, unsigned tiers) {
    SibProposal *out = avp_alloc(a, (n + 1) * sizeof *out);
    AvpMap *index = avp_map_new();
    Ctx c = {a, s, g, splits, avp_map_new(), tiers, index};
    for (size_t i = 0; i < s->n; i++)
        if (s->v[i].name) avp_map_put(index, avp_str(s->v[i].name), NULL)->i = (i64)i;
    for (size_t i = 0; i < s->n; i++)
        for (size_t k = 0; k < s->v[i].ncallees; k++) {
            const char *t = s->v[i].callees[k];
            AvpVal *tv = avp_map_get(index, avp_str(t));
            if (!tv || (size_t)tv->i == i) continue;
            bool created;
            AvpVal *v = avp_map_put(c.callers, avp_str(t), &created);
            if (created) v->p = avp_alloc(a, sizeof(AvpI32Vec));
            AvpI32Vec *cs = v->p;
            if (!cs->n || cs->v[cs->n - 1] != (i32)i) avp_vec_push(cs, (i32)i);
        }
    AvpMap *objs = object_entries(a, g, unused);
    EntryVec *head = avp_alloc(a, (n + 1) * sizeof *head), *tail = avp_alloc(a, (n + 1) * sizeof *tail);
    for (size_t k = 0; k < n; k++) {
        const SibSpanRef *sp = &spans[k];
        out[k].start = s->v[sp->first].address;
        out[k].end = s->v[sp->last].address + s->v[sp->last].size;
        out[k].proposed = sp->clean;
        i64 omin = INT64_MAX, omax = -1;
        for (size_t i = sp->first; i <= sp->last; i++) {
            const SibFn *f = &s->v[i];
            if (f->peer < 0 || !tier_in(tiers, f->tier)) continue;
            const SibFn *p = &g->v[f->peer];
            if (!p->object || strcmp(p->object, sp->object) || strcmp(p->lib ? p->lib : "", sp->lib ? sp->lib : ""))
                continue;
            if (p->order < omin) omin = p->order;
            if (p->order > omax) omax = p->order;
        }
        AvpVal *ev = avp_map_get(objs, avp_str(key_of(a, sp->lib, sp->object)));
        head[k] = candidates(ev ? ev->p : NULL, omin, false, tiers);
        tail[k] = candidates(ev ? ev->p : NULL, omax, true, tiers);
    }
    /* each gap: between span k-1 (left) and span k (right); k = 0 and k = n are the open ends of the list */
    for (size_t k = 0; k <= n; k++) {
        const SibSpanRef *L = k > 0 ? &spans[k - 1] : NULL, *R = k < n ? &spans[k] : NULL;
        size_t lo = L ? L->last + 1 : 0, hi = R ? R->first : s->n;
        if (L && R && lo > hi) continue; /* overlapping spans */
        if (!L && R) /* open start: only the right object's library, as far back as its capacity could reach */
            for (lo = hi; lo > 0 && same_lib(&s->v[lo - 1], &s->v[R->first]) && hi - lo < 4096;) lo--;
        if (L && !R)
            for (hi = lo; hi < s->n && same_lib(&s->v[hi], &s->v[L->first]) && hi - lo < 4096;) hi++;
        const SibSpanRef *Lc = L && L->clean ? L : NULL, *Rc = R && R->clean ? R : NULL;
        if (!Lc && !Rc) continue;
        static const EntryVec none = {0};
        Lists lists = {Lc ? &out[k - 1].after : NULL, Rc ? &out[k].before : NULL};
        size_t p, q;
        split_gap(&c, lo, hi, Lc, Rc, Lc ? &tail[k - 1] : &none, Rc ? &head[k] : &none, lists, &p, &q);
        if (Lc && p) out[k - 1].end = s->v[lo + p - 1].address + s->v[lo + p - 1].size;
        if (Rc && q < hi - lo) out[k].start = s->v[lo + q].address;
    }
    /* alignment padding after the last function: the split units of this project end either before it or at the
     * next unit's start, so it is reported, not decided */
    for (size_t k = 0; k < n; k++) {
        size_t i = spans[k].last;
        out[k].pad_end = out[k].end;
        while (i + 1 < s->n && s->v[i + 1].address < out[k].end) i++;
        if (i + 1 < s->n && s->v[i + 1].address > out[k].end && s->v[i + 1].address - out[k].end < 0x20)
            out[k].pad_end = s->v[i + 1].address;
    }
    avp_map_free(index);
    avp_map_free(objs);
    avp_map_free(c.callers);
    return out;
}
