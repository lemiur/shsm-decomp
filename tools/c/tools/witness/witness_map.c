/* The Wii <-> PS2 correspondence: tools/witness.py Correspondence, _hkey, _dedupe, _ev_strings, validate. */
#include "witness_map.h"

#include "avp/fmt.h"
#include "avp/hash.h"
#include "shsm_cli.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- small helpers ---- */

static WtFn *fn_req(const WtSide *s, i64 a) {
    WtFn *f = wt_side_get(s, a);
    if (!f) shsm_raise(SHSM_EXC_KEY, "%lld", (long long)a);
    return f;
}

/* len() of a Python str held as UTF-8. */
static size_t cp_len(AvpStr s) {
    size_t n = 0;
    for (size_t k = 0; k < s.n; k++)
        if (((u8)s.p[k] & 0xC0) != 0x80) n++;
    return n;
}

/* s[:n] in code points. */
static AvpStr cp_prefix(AvpStr s, size_t n) {
    size_t cps = 0;
    for (size_t k = 0; k < s.n; k++) {
        if (((u8)s.p[k] & 0xC0) != 0x80) {
            if (cps == n) return avp_strn(s.p, k);
            cps++;
        }
    }
    return s;
}

static u64 key2(i64 a, i64 b) { return ((u64)(u32)a << 32) | (u64)(u32)b; }

const WtPairEntry *wt_corr_pair(const WtCorr *c, i64 wii) {
    AvpVal *v = avp_imap_get(c->pair_idx, (u64)wii);
    return v ? &c->pairs.v[v->i] : NULL;
}

static bool in_pairs(const WtCorr *c, i64 w) { return w != WT_NONE && avp_imap_has(c->pair_idx, (u64)w); }
static bool in_rev(const WtCorr *c, i64 p) { return p != WT_NONE && avp_imap_has(c->rev, (u64)p); }
static i64 pair_ps2(const WtCorr *c, i64 w) { return c->pairs.v[avp_imap_get(c->pair_idx, (u64)w)->i].p.ps2; }
static i64 rev_of(const WtCorr *c, i64 p) { return avp_imap_get(c->rev, (u64)p)->i; }

bool wt_corr_rev(const WtCorr *c, i64 ps2, i64 *wii) {
    AvpVal *v = avp_imap_get(c->rev, (u64)ps2);
    if (!v) return false;
    if (wii) *wii = v->i;
    return true;
}

const AvpPyIntSet *wt_corr_pcallers(const WtCorr *c, i64 a) {
    AvpVal *v = avp_imap_get(c->pcallers, (u64)a);
    return v ? v->p : NULL;
}

const AvpPyIntSet *wt_corr_wcallers(const WtCorr *c, i64 a) {
    AvpVal *v = avp_imap_get(c->wcallers, (u64)a);
    return v ? v->p : NULL;
}

/* strs(side, a): NULL (the Python's ()) when a is hidden. */
static const AvpSet *strs_of(const WtCorr *c, const WtSide *side, i64 a) {
    if (c->hide && avp_iset_has(c->hide, (u64)a)) return NULL;
    return fn_req(side, a)->strs;
}

/* ---- construction ---- */

static void callers_of(AvpIMap *m, const WtSide *side) {
    for (size_t k = 0; k < side->v.n; k++) {
        const WtFn *f = side->v.v[k];
        for (size_t j = 0; j < f->calls.n; j++) {
            i64 callee = f->calls.v[j];
            if (callee == WT_NONE) continue;
            bool created;
            AvpVal *v = avp_imap_put(m, (u64)callee, &created);
            if (created) v->p = avp_pyintset_new();
            avp_pyintset_add(v->p, f->addr);
        }
    }
}

static void count_floats(AvpIMap *cnt, const WtSide *side) {
    for (size_t k = 0; k < side->v.n; k++) {
        size_t pos = 0;
        i64 v;
        while (avp_pyintset_next(side->v.v[k]->floats, &pos, &v)) {
            bool created;
            avp_imap_put(cnt, (u64)v, &created)->i += 1;
        }
    }
}

WtCorr *wt_corr_new(WtSide *wii, WtSide *ps2, const WtRuns *wruns, const WtRuns *pruns, AvpISet *hide) {
    WtCorr *c = avp_xcalloc(1, sizeof *c);
    c->arena = avp_arena_new(0);
    c->wii = wii;
    c->ps2 = ps2;
    if (wruns)
        for (size_t k = 0; k < wruns->n; k++) avp_vec_push(&c->wruns, wruns->v[k]);
    if (pruns)
        for (size_t k = 0; k < pruns->n; k++) avp_vec_push(&c->pruns, pruns->v[k]);
    c->hide = hide;
    c->pair_idx = avp_imap_new();
    c->rev = avp_imap_new();
    c->anchor_of = avp_imap_new();
    c->pcallers = avp_imap_new();
    c->wcallers = avp_imap_new();
    callers_of(c->pcallers, ps2);
    callers_of(c->wcallers, wii);
    AvpIMap *fw = avp_imap_new(), *fp = avp_imap_new();
    count_floats(fw, wii);
    count_floats(fp, ps2);
    c->common_floats = avp_iset_new();
    AvpIMapIter it = {0};
    while (avp_imap_next(fw, &it))
        if (it.val->i > 25) avp_iset_add(c->common_floats, it.key);
    memset(&it, 0, sizeof it);
    while (avp_imap_next(fp, &it))
        if (it.val->i > 25) avp_iset_add(c->common_floats, it.key);
    avp_iset_add(c->common_floats, 0);
    avp_imap_free(fw);
    avp_imap_free(fp);
    return c;
}

static void free_callers(AvpIMap *m) {
    AvpIMapIter it = {0};
    while (avp_imap_next(m, &it)) avp_pyintset_free(it.val->p);
    avp_imap_free(m);
}

void wt_corr_free(WtCorr *c) {
    if (!c) return;
    free_callers(c->pcallers);
    free_callers(c->wcallers);
    avp_imap_free(c->pair_idx);
    avp_imap_free(c->rev);
    avp_imap_free(c->anchor_of);
    avp_iset_free(c->common_floats);
    avp_vec_free(&c->pairs);
    avp_vec_free(&c->inl);
    avp_vec_free(&c->wruns);
    avp_vec_free(&c->pruns);
    avp_arena_free(c->arena);
    avp_xfree(c);
}

bool wt_corr_add(WtCorr *c, i64 w, i64 p, const char *tier, const char *ev) {
    if (in_pairs(c, w) || in_rev(c, p)) return false;
    bool created;
    avp_imap_put(c->pair_idx, (u64)w, &created)->i = (i64)c->pairs.n;
    WtPairEntry e = {w, {p, tier, avp_strdup(c->arena, ev)}};
    avp_vec_push(&c->pairs, e);
    avp_imap_put(c->rev, (u64)p, &created)->i = w;
    return true;
}

/* ---- compatible, reaches ---- */

bool wt_corr_compatible(const WtCorr *c, i64 w, i64 p) {
    const WtFn *wf = fn_req(c->wii, w), *pf = fn_req(c->ps2, p);
    i64 mx = wf->n > pf->n ? wf->n : pf->n, mn = wf->n < pf->n ? wf->n : pf->n;
    if (mx >= 16 && (double)mx > WT_MAX_RATIO * (double)(mn > 4 ? mn : 4)) return false;
    const AvpSet *ws = strs_of(c, c->wii, w), *ps = strs_of(c, c->ps2, p);
    bool any_w = false, any_p = false, meet = false;
    size_t pos = 0;
    AvpStr t;
    if (ws)
        while (avp_set_next(ws, &pos, &t)) {
            if (cp_len(t) < 4) continue;
            any_w = true;
            if (ps && avp_set_has(ps, t)) meet = true;
        }
    pos = 0;
    if (ps)
        while (avp_set_next(ps, &pos, &t))
            if (cp_len(t) >= 4) {
                any_p = true;
                break;
            }
    if (any_w && any_p && !meet) return false;
    size_t nw = 0, np = 0;
    bool fmeet = false;
    i64 v;
    pos = 0;
    while (avp_pyintset_next(wf->floats, &pos, &v)) {
        if (avp_iset_has(c->common_floats, (u64)v)) continue;
        nw++;
        if (avp_pyintset_has(pf->floats, v)) fmeet = true;
    }
    pos = 0;
    while (avp_pyintset_next(pf->floats, &pos, &v))
        if (!avp_iset_has(c->common_floats, (u64)v)) np++;
    if (nw >= 2 && np >= 2 && !fmeet) return false;
    return true;
}

bool wt_corr_reaches(const WtCorr *c, i64 src, i64 dst, int depth) {
    AvpISet *seen = avp_iset_new();
    avp_iset_add(seen, (u64)src);
    AvpI64Vec frontier = {0}, nxt = {0};
    avp_vec_push(&frontier, src);
    bool found = false;
    for (int d = 0; d < depth && !found; d++) {
        nxt.n = 0;
        for (size_t k = 0; k < frontier.n && !found; k++) {
            const WtFn *f = wt_side_get(c->ps2, frontier.v[k]);
            if (!f) continue;
            for (size_t j = 0; j < f->calls.n; j++) {
                i64 cl = f->calls.v[j];
                if (cl == dst) {
                    found = true;
                    break;
                }
                if (cl != WT_NONE && avp_iset_add(seen, (u64)cl)) avp_vec_push(&nxt, cl);
            }
        }
        AvpI64Vec tmp = frontier;
        frontier = nxt;
        nxt = tmp;
    }
    avp_vec_free(&frontier);
    avp_vec_free(&nxt);
    avp_iset_free(seen);
    return found;
}

/* ---- evidence texts ---- */

typedef struct {
    AvpStr s;
    size_t len, idx;
} LenItem;

static int by_len_desc(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const LenItem *a = x, *b = y;
    if (a->len != b->len) return a->len > b->len ? -1 : 1;
    return a->idx < b->idx ? -1 : a->idx > b->idx;
}

const char *wt_ev_strings(AvpArena *a, const AvpStr *ts, size_t n) {
    LenItem *it = avp_xcalloc(n ? n : 1, sizeof *it);
    for (size_t k = 0; k < n; k++) it[k] = (LenItem){ts[k], cp_len(ts[k]), k};
    avp_sort(it, n, sizeof *it, by_len_desc, NULL, false); /* sorted(ts, key=len, reverse=True): stable */
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "%zu unique string(s): ", n);
    for (size_t k = 0; k < n && k < 2; k++) {
        if (k) avp_buf_putz(&b, ", ");
        avp_json_put_string(&b, cp_prefix(it[k].s, 40), false);
    }
    avp_xfree(it);
    return avp_buf_str(&b).p;
}

/* ---- votes: an ordered {(w, p): value} ---- */

typedef struct {
    i64 w, p;
    i64 v;                  /* Counter value */
    AVP_VEC(AvpStr) ts;     /* string_anchors: the texts */
    AVP_VEC(WtRunItem) ks;  /* constant_candidates: (kind, value) keys (kind 'f' or 'i', value in .fn) */
} Vote;

typedef struct {
    AVP_VEC(Vote) v;
    AvpIMap *idx;
} Votes;

static void votes_init(Votes *vs) {
    memset(vs, 0, sizeof *vs);
    vs->idx = avp_imap_new();
}

static Vote *votes_get(Votes *vs, i64 w, i64 p) {
    bool created;
    AvpVal *x = avp_imap_put(vs->idx, key2(w, p), &created);
    if (created) {
        x->i = (i64)vs->v.n;
        Vote z = {0};
        z.w = w;
        z.p = p;
        avp_vec_push(&vs->v, z);
    }
    return &vs->v.v[x->i];
}

static void votes_free(Votes *vs) {
    for (size_t k = 0; k < vs->v.n; k++) {
        avp_vec_free(&vs->v.v[k].ts);
        avp_vec_free(&vs->v.v[k].ks);
    }
    avp_vec_free(&vs->v);
    avp_imap_free(vs->idx);
}

/* ---- string anchors ---- */

/* {text: [address]} of one side (texts of at least MIN_ANCHOR_TEXT code points). */
static AvpMap *texts_by(const WtCorr *c, const WtSide *side) {
    AvpMap *by = avp_map_new();
    for (size_t k = 0; k < side->v.n; k++) {
        i64 a = side->v.v[k]->addr;
        const AvpSet *ss = strs_of(c, side, a);
        if (!ss) continue;
        size_t pos = 0;
        AvpStr t;
        while (avp_set_next(ss, &pos, &t)) {
            if (cp_len(t) < WT_MIN_ANCHOR_TEXT) continue;
            bool created;
            AvpVal *v = avp_map_put(by, t, &created);
            if (created) v->p = avp_xcalloc(1, sizeof(AvpI64Vec));
            avp_vec_push((AvpI64Vec *)v->p, a);
        }
    }
    return by;
}

static void free_by(AvpMap *by) {
    AvpMapIter it = {0};
    while (avp_map_next(by, &it)) {
        avp_vec_free((AvpI64Vec *)it.val->p);
        avp_xfree(it.val->p);
    }
    avp_map_free(by);
}

static const AvpI64Vec *by_get(const AvpMap *by, AvpStr t) { return avp_map_get(by, t)->p; }

/* (text, int) keys: 8 bytes of the int, then the text. */
static AvpStr tv_key(AvpArena *a, AvpStr t, i64 v) {
    char *p = avp_alloc(a, t.n + 8);
    memcpy(p, &v, 8);
    memcpy(p + 8, t.p, t.n);
    return avp_strn(p, t.n + 8);
}

static void tv_split(AvpStr key, AvpStr *t, i64 *v) {
    memcpy(v, key.p, 8);
    *t = avp_strn(key.p + 8, key.n - 8);
}

static int tv_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    AvpStr ta, tb;
    i64 va, vb;
    tv_split(*(const AvpStr *)x, &ta, &va);
    tv_split(*(const AvpStr *)y, &tb, &vb);
    int r = avp_str_cmp(ta, tb);
    return r ? r : avp_cmp_i64(va, vb);
}

static void keys_of(AvpArena *a, AvpMap *keys, const WtSide *side, const AvpMap *by, const AvpStr *multi,
                    size_t nmulti) {
    for (size_t k = 0; k < nmulti; k++) {
        const AvpI64Vec *addrs = by_get(by, multi[k]);
        for (size_t j = 0; j < addrs->n; j++) {
            const WtFn *f = fn_req(side, addrs->v[j]);
            size_t pos = 0;
            i64 v;
            while (avp_pyintset_next(f->ints, &pos, &v)) {
                if (v >= 0x10000) continue;
                bool created;
                AvpVal *x = avp_map_put(keys, tv_key(a, multi[k], v), &created);
                if (created) x->p = avp_xcalloc(1, sizeof(AvpI64Vec));
                avp_vec_push((AvpI64Vec *)x->p, addrs->v[j]);
            }
        }
    }
}

typedef struct {
    i64 key;
    AvpI64Vec votes; /* indexes into Votes.v, in vote order */
} Group;

typedef struct {
    AVP_VEC(Group) g;
    AvpIMap *idx;
} Groups;

static void groups_build(Groups *gs, const Votes *vs, bool by_wii) {
    memset(gs, 0, sizeof *gs);
    gs->idx = avp_imap_new();
    for (size_t k = 0; k < vs->v.n; k++) {
        i64 key = by_wii ? vs->v.v[k].w : vs->v.v[k].p;
        bool created;
        AvpVal *x = avp_imap_put(gs->idx, (u64)key, &created);
        if (created) {
            x->i = (i64)gs->g.n;
            Group z = {0};
            z.key = key;
            avp_vec_push(&gs->g, z);
        }
        avp_vec_push(&gs->g.v[x->i].votes, (i64)k);
    }
}

static const Group *groups_get(const Groups *gs, i64 key) {
    AvpVal *x = avp_imap_get(gs->idx, (u64)key);
    return x ? &gs->g.v[x->i] : NULL;
}

static int group_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_cmp_i64(((const Group *)x)->key, ((const Group *)y)->key);
}

static void groups_free(Groups *gs) {
    for (size_t k = 0; k < gs->g.n; k++) avp_vec_free(&gs->g.v[k].votes);
    avp_vec_free(&gs->g);
    avp_imap_free(gs->idx);
}

static const char *ev_of(WtCorr *c, const Vote *v) { return wt_ev_strings(c->arena, v->ts.v, v->ts.n); }

/* The candidate with strictly the most texts (ranked[0] when it beats ranked[1]), or -1. */
static i64 unique_best(const Votes *vs, const i64 *cand, size_t n) {
    i64 best = -1;
    size_t bl = 0;
    bool tie = false;
    for (size_t k = 0; k < n; k++) {
        size_t l = vs->v.v[cand[k]].ts.n;
        if (best < 0 || l > bl) {
            best = cand[k];
            bl = l;
            tie = false;
        } else if (l == bl) {
            tie = true;
        }
    }
    return tie ? -1 : best;
}

static void string_anchors(WtCorr *c) {
    AvpArena *s = avp_arena_new(0);
    AvpMap *wby = texts_by(c, c->wii), *pby = texts_by(c, c->ps2);
    Votes votes;
    votes_init(&votes);
    /* shared = wby.keys() & pby.keys(): iterated sorted (D-witness-3) */
    AVP_VEC(AvpStr) shared = {0};
    AvpMapIter it = {0};
    while (avp_map_next(wby, &it))
        if (avp_map_has(pby, it.key)) avp_vec_push(&shared, avp_strn(avp_memdup(s, it.key.p, it.key.n), it.key.n));
    avp_sort_strs(shared.v, shared.n);
    AVP_VEC(AvpStr) multi = {0};
    for (size_t k = 0; k < shared.n; k++) {
        const AvpI64Vec *wa = by_get(wby, shared.v[k]), *pa = by_get(pby, shared.v[k]);
        if (wa->n == 1 && pa->n == 1) avp_vec_push(&votes_get(&votes, wa->v[0], pa->v[0])->ts, shared.v[k]);
        if (wa->n > 1 || pa->n > 1) avp_vec_push(&multi, shared.v[k]);
    }
    AvpMap *kw = avp_map_new(), *kp = avp_map_new();
    keys_of(s, kw, c->wii, wby, multi.v, multi.n);
    keys_of(s, kp, c->ps2, pby, multi.v, multi.n);
    AVP_VEC(AvpStr) both = {0};
    memset(&it, 0, sizeof it);
    while (avp_map_next(kw, &it))
        if (avp_map_has(kp, it.key)) avp_vec_push(&both, avp_strn(avp_memdup(s, it.key.p, it.key.n), it.key.n));
    avp_sort(both.v, both.n, sizeof *both.v, tv_cmp, NULL, false);
    for (size_t k = 0; k < both.n; k++) {
        const AvpI64Vec *wa = avp_map_get(kw, both.v[k])->p, *pa = avp_map_get(kp, both.v[k])->p;
        if (wa->n == 1 && pa->n == 1) {
            AvpStr t;
            i64 v;
            tv_split(both.v[k], &t, &v);
            avp_vec_push(&votes_get(&votes, wa->v[0], pa->v[0])->ts, avp_pyfmt(s, "%S+%d", &t, v));
        }
    }
    Groups byw, byp;
    groups_build(&byw, &votes, true);
    groups_build(&byp, &votes, false);
    /* sorted(byw.items()): a sorted copy (byw itself keeps its index for lookups) */
    Group *sw = avp_xcalloc(byw.g.n ? byw.g.n : 1, sizeof *sw), *sp = avp_xcalloc(byp.g.n ? byp.g.n : 1, sizeof *sp);
    memcpy(sw, byw.g.v, byw.g.n * sizeof *sw);
    memcpy(sp, byp.g.v, byp.g.n * sizeof *sp);
    avp_sort(sw, byw.g.n, sizeof *sw, group_cmp, NULL, false);
    avp_sort(sp, byp.g.n, sizeof *sp, group_cmp, NULL, false);
    for (size_t k = 0; k < byw.g.n; k++) {
        const Group *g = &sw[k];
        if (g->votes.n != 1) continue;
        const Vote *v = &votes.v.v[g->votes.v[0]];
        if (groups_get(&byp, v->p)->votes.n == 1) {
            wt_corr_add(c, g->key, v->p, "string", ev_of(c, v));
            bool created;
            avp_imap_put(c->anchor_of, (u64)g->key, &created)->i = v->p;
        }
    }
    AvpI64Vec free_ = {0}, parents = {0};
    for (size_t k = 0; k < byw.g.n; k++) {
        const Group *g = &sw[k];
        i64 w = g->key;
        if (in_pairs(c, w) || g->votes.n < 2) continue;
        free_.n = parents.n = 0;
        for (size_t j = 0; j < g->votes.n; j++)
            if (!in_rev(c, votes.v.v[g->votes.v[j]].p)) avp_vec_push(&free_, g->votes.v[j]);
        for (size_t j = 0; j < free_.n; j++) {
            i64 p = votes.v.v[free_.v[j]].p;
            bool all = true;
            for (size_t q = 0; q < g->votes.n && all; q++) {
                i64 qa = votes.v.v[g->votes.v[q]].p;
                if (qa != p && !wt_corr_reaches(c, p, qa, 3)) all = false;
            }
            if (all) avp_vec_push(&parents, free_.v[j]);
        }
        if (parents.n == 1) {
            const Vote *v0 = &votes.v.v[parents.v[0]];
            const char *ev = avp_pyfmt(c->arena, "%s (+%d inlined)", ev_of(c, v0), (i64)g->votes.n - 1).p;
            if (wt_corr_add(c, w, v0->p, "string", ev)) {
                for (size_t q = 0; q < g->votes.n; q++) {
                    const Vote *vq = &votes.v.v[g->votes.v[q]];
                    if (vq->p == v0->p) continue;
                    WtInl r = {w, vq->p, v0->p, avp_pyfmt(c->arena, "string %s", ev_of(c, vq)).p};
                    avp_vec_push(&c->inl, r);
                }
            }
            continue;
        }
        i64 b = unique_best(&votes, free_.v, free_.n);
        if (b >= 0) {
            const Vote *vb = &votes.v.v[b];
            wt_corr_add(c, w, vb->p, "string", avp_pyfmt(c->arena, "%s (contested)", ev_of(c, vb)).p);
        }
    }
    for (size_t k = 0; k < byp.g.n; k++) {
        const Group *g = &sp[k];
        if (in_rev(c, g->key) || g->votes.n < 2) continue;
        free_.n = 0;
        for (size_t j = 0; j < g->votes.n; j++)
            if (!in_pairs(c, votes.v.v[g->votes.v[j]].w)) avp_vec_push(&free_, g->votes.v[j]);
        i64 b = unique_best(&votes, free_.v, free_.n);
        if (b >= 0) {
            const Vote *vb = &votes.v.v[b];
            wt_corr_add(c, vb->w, g->key, "string", avp_pyfmt(c->arena, "%s (contested)", ev_of(c, vb)).p);
        }
    }
    avp_vec_free(&free_);
    avp_vec_free(&parents);
    avp_xfree(sw);
    avp_xfree(sp);
    groups_free(&byw);
    groups_free(&byp);
    free_by(kw);
    free_by(kp);
    avp_vec_free(&both);
    avp_vec_free(&shared);
    avp_vec_free(&multi);
    votes_free(&votes);
    free_by(wby);
    free_by(pby);
    avp_arena_free(s);
}

/* ---- constants ---- */

static bool int_far(i64 v, i64 lim) {
    i64 x = (v & 0x80000000LL) ? v - (1LL << 32) : v;
    return llabs(x) >= lim;
}

/* unique(side): {key: address} of the (kind, value) keys exactly one function has; val.i = address, or WT_NONE
 * when several functions have it. */
static void unique_keys(const WtCorr *c, const WtSide *side, AvpIMap *f, AvpIMap *i) {
    for (size_t k = 0; k < side->v.n; k++) {
        const WtFn *fn = side->v.v[k];
        size_t pos = 0;
        i64 v;
        bool created;
        while (avp_pyintset_next(fn->floats, &pos, &v)) {
            if (avp_iset_has(c->common_floats, (u64)v)) continue;
            AvpVal *x = avp_imap_put(f, (u64)v, &created);
            x->i = created ? fn->addr : WT_NONE;
        }
        pos = 0;
        while (avp_pyintset_next(fn->ints, &pos, &v)) {
            if (!int_far(v, 0x100)) continue;
            AvpVal *x = avp_imap_put(i, (u64)v, &created);
            x->i = created ? fn->addr : WT_NONE;
        }
    }
}

static int i64_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return avp_cmp_i64(*(const i64 *)x, *(const i64 *)y);
}

static void shared_keys(const AvpIMap *uw, const AvpIMap *up, char kind, Votes *votes) {
    AvpI64Vec ks = {0};
    AvpIMapIter it = {0};
    while (avp_imap_next(uw, &it)) {
        if (it.val->i == WT_NONE) continue;
        AvpVal *o = avp_imap_get(up, it.key);
        if (o && o->i != WT_NONE) avp_vec_push(&ks, (i64)it.key);
    }
    avp_sort(ks.v, ks.n, sizeof *ks.v, i64_cmp, NULL, false);
    for (size_t k = 0; k < ks.n; k++) {
        i64 w = avp_imap_get(uw, (u64)ks.v[k])->i, p = avp_imap_get(up, (u64)ks.v[k])->i;
        WtRunItem key = {kind, ks.v[k], NULL};
        avp_vec_push(&votes_get(votes, w, p)->ks, key);
    }
    avp_vec_free(&ks);
}

/* constant_candidates(): the shared keys iterated sorted, ('f', v) before ('i', v) (D-witness-3). */
static void constant_candidates(const WtCorr *c, Votes *votes) {
    AvpIMap *wf = avp_imap_new(), *wi = avp_imap_new(), *pf = avp_imap_new(), *pi = avp_imap_new();
    unique_keys(c, c->wii, wf, wi);
    unique_keys(c, c->ps2, pf, pi);
    votes_init(votes);
    shared_keys(wf, pf, 'f', votes);
    shared_keys(wi, pi, 'i', votes);
    avp_imap_free(wf);
    avp_imap_free(wi);
    avp_imap_free(pf);
    avp_imap_free(pi);
}

static i64 shared_callees(const WtCorr *c, i64 w, i64 p) {
    const WtFn *pf = fn_req(c->ps2, p), *wf = fn_req(c->wii, w);
    AvpISet *pc = avp_iset_new(), *seen = avp_iset_new();
    for (size_t k = 0; k < pf->calls.n; k++) avp_iset_add(pc, (u64)pf->calls.v[k]);
    i64 n = 0;
    for (size_t k = 0; k < wf->calls.n; k++) {
        i64 cl = wf->calls.v[k];
        if (!avp_iset_add(seen, (u64)cl)) continue;
        if (in_pairs(c, cl) && avp_iset_has(pc, (u64)pair_ps2(c, cl))) n++;
    }
    avp_iset_free(pc);
    avp_iset_free(seen);
    return n;
}

typedef struct {
    size_t idx, len;
} IdxLen;

static int idxlen_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    const IdxLen *a = x, *b = y;
    if (a->len != b->len) return a->len > b->len ? -1 : 1;
    return a->idx < b->idx ? -1 : a->idx > b->idx;
}

static void hex_of(AvpBuf *b, i64 v) {
    if (v < 0)
        avp_buf_printf(b, "-0x%llx", (unsigned long long)(-(v + 1)) + 1ULL);
    else
        avp_buf_printf(b, "0x%llx", (unsigned long long)v);
}

static i64 constants(WtCorr *c, const Votes *votes) {
    size_t nv = votes->v.n;
    IdxLen *order = avp_xcalloc(nv ? nv : 1, sizeof *order);
    for (size_t k = 0; k < nv; k++) order[k] = (IdxLen){k, votes->v.v[k].ks.n};
    avp_sort(order, nv, sizeof *order, idxlen_cmp, NULL, false);
    i64 n = 0;
    for (size_t k = 0; k < nv; k++) {
        const Vote *v = &votes->v.v[order[k].idx];
        if (in_pairs(c, v->w) || in_rev(c, v->p) || !wt_corr_compatible(c, v->w, v->p)) continue;
        i64 shared = shared_callees(c, v->w, v->p);
        if (v->ks.n >= 2 || shared) {
            AvpBuf b;
            avp_buf_init(&b, c->arena);
            avp_buf_printf(&b, "%zu unique constant(s): ", v->ks.n);
            for (size_t j = 0; j < v->ks.n && j < 3; j++) {
                if (j) avp_buf_putz(&b, ", ");
                const WtRunItem *kk = &v->ks.v[j];
                if (kk->kind == 'f') {
                    u32 bits = (u32)kk->fn;
                    float f;
                    memcpy(&f, &bits, 4);
                    avp_buf_put(&b, avp_pyfmt(c->arena, "%g", (double)f));
                } else {
                    hex_of(&b, kk->fn);
                }
            }
            if (shared) avp_buf_printf(&b, " + %lld shared callee(s)", (long long)shared);
            n += wt_corr_add(c, v->w, v->p, "constant", avp_buf_str(&b).p);
        }
    }
    avp_xfree(order);
    return n;
}

/* ---- tables ---- */

static void idx_add(AvpVal *x, bool created, i64 k) {
    if (created) x->p = avp_xcalloc(1, sizeof(AvpI64Vec));
    AvpI64Vec *v = x->p;
    if (!v->n || v->v[v->n - 1] != k) avp_vec_push(v, k);
}

static void union_into(AvpISet *cand, const AvpVal *x) {
    if (!x) return;
    const AvpI64Vec *v = x->p;
    for (size_t k = 0; k < v->n; k++) avp_iset_add(cand, (u64)v->v[k]);
}

static bool str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static i64 tables(WtCorr *c) {
    AvpMap *sidx = avp_map_new();
    AvpIMap *fidx = avp_imap_new();
    for (size_t k = 0; k < c->pruns.n; k++) {
        const WtRun *run = &c->pruns.v[k];
        for (size_t j = 0; j < run->n; j++) {
            bool created;
            if (run->v[j].kind == 's') {
                AvpVal *x = avp_map_put(sidx, avp_str(run->v[j].text), &created);
                idx_add(x, created, (i64)k);
            } else if (run->v[j].kind == 'f') {
                AvpVal *x = avp_imap_put(fidx, (u64)run->v[j].fn, &created);
                idx_add(x, created, (i64)k);
            }
        }
    }
    i64 n = 0;
    AvpISet *cand = avp_iset_new();
    for (size_t r = 0; r < c->wruns.n; r++) {
        const WtRun *run = &c->wruns.v[r];
        avp_iset_free(cand);
        cand = avp_iset_new();
        for (size_t j = 0; j < run->n; j++) {
            const WtRunItem *it = &run->v[j];
            if (it->kind == 's')
                union_into(cand, avp_map_get(sidx, avp_str(it->text)));
            else if (it->kind == 'f' && in_pairs(c, it->fn))
                union_into(cand, avp_imap_get(fidx, (u64)pair_ps2(c, it->fn)));
        }
        size_t ngood = 0;
        i64 good = -1;
        size_t pos = 0;
        u64 kk;
        while (avp_iset_next(cand, &pos, &kk)) {
            const WtRun *pr = &c->pruns.v[kk];
            if (pr->n != run->n) continue;
            int agree = 0;
            bool bad = false;
            for (size_t j = 0; j < run->n && !bad; j++) {
                const WtRunItem *a = &run->v[j], *b = &pr->v[j];
                if (a->kind == 'o' || b->kind == 'o') continue;
                if (a->kind != b->kind) {
                    bad = true;
                } else if (a->kind == 's') {
                    if (!str_eq(a->text, b->text))
                        bad = true;
                    else
                        agree++;
                } else if (in_pairs(c, a->fn) || in_rev(c, b->fn)) {
                    if (!in_pairs(c, a->fn) || pair_ps2(c, a->fn) != b->fn)
                        bad = true;
                    else
                        agree++;
                }
            }
            if (!bad && agree >= 2) {
                ngood++;
                good = (i64)kk;
            }
        }
        if (ngood != 1) continue;
        const WtRun *pr = &c->pruns.v[good];
        for (size_t j = 0; j < run->n; j++) {
            const WtRunItem *a = &run->v[j], *b = &pr->v[j];
            if (a->kind == 'f' && b->kind == 'f' && !in_pairs(c, a->fn) && !in_rev(c, b->fn) &&
                wt_corr_compatible(c, a->fn, b->fn))
                n += wt_corr_add(c, a->fn, b->fn, "table",
                                 avp_pyfmt(c->arena, "position %d of a %d-entry table", (i64)j, (i64)run->n).p);
        }
    }
    avp_iset_free(cand);
    AvpMapIter it = {0};
    while (avp_map_next(sidx, &it)) {
        avp_vec_free((AvpI64Vec *)it.val->p);
        avp_xfree(it.val->p);
    }
    avp_map_free(sidx);
    AvpIMapIter ii = {0};
    while (avp_imap_next(fidx, &ii)) {
        avp_vec_free((AvpI64Vec *)ii.val->p);
        avp_xfree(ii.val->p);
    }
    avp_imap_free(fidx);
    return n;
}

/* ---- align: the call sequences as tagged codes ---- */

enum { TAG_P = 1, TAG_W = 2, TAG_U = 3 };
#define CODE(t, x) (((u64)(t) << 40) | ((u64)(x) & 0xFFFFFFFFFFULL))
#define TAG(c) ((int)((c) >> 40))
#define VAL(c) ((i64)((c) & 0xFFFFFFFFFFULL))

typedef struct {
    AvpU64Vec A, B;
    const AvpOpcode *ops;
    size_t nops;
} Align;

static void align(const WtCorr *c, AvpArena *a, i64 w, i64 p, Align *al) {
    memset(al, 0, sizeof *al);
    const WtFn *wf = fn_req(c->wii, w), *pf = fn_req(c->ps2, p);
    for (size_t k = 0; k < wf->calls.n; k++) {
        i64 cl = wf->calls.v[k];
        if (cl == WT_NONE) continue;
        avp_vec_push(&al->A, in_pairs(c, cl) ? CODE(TAG_P, pair_ps2(c, cl)) : CODE(TAG_W, cl));
    }
    for (size_t k = 0; k < pf->calls.n; k++) {
        i64 cl = pf->calls.v[k];
        if (cl == WT_NONE) continue;
        avp_vec_push(&al->B, in_rev(c, cl) ? CODE(TAG_P, cl) : CODE(TAG_U, cl));
    }
    AvpSeqMatcher *m = avp_seqm_new_u64(a, al->A.v, al->A.n, al->B.v, al->B.n, &avp_seqm_no_autojunk);
    al->ops = avp_seqm_opcodes(m, &al->nops);
}

static void align_free(Align *al) {
    avp_vec_free(&al->A);
    avp_vec_free(&al->B);
}

/* ---- propagation ---- */

static Vote *count_vote(Votes *vs, i64 a, i64 b) { return votes_get(vs, a, b); }

static int vote_rank(const void *x, const void *y, void *ctx) {
    const Votes *vs = ctx;
    const Vote *a = &vs->v.v[*(const size_t *)x], *b = &vs->v.v[*(const size_t *)y];
    if (a->v != b->v) return a->v > b->v ? -1 : 1;
    if (a->w != b->w) return a->w < b->w ? -1 : 1;
    return avp_cmp_i64(a->p, b->p);
}

static i64 accept(WtCorr *c, Votes *votes, const char *tier, const char *fmt) {
    Groups byw, byp;
    groups_build(&byw, votes, true);
    groups_build(&byp, votes, false);
    size_t nv = votes->v.n;
    size_t *order = avp_xcalloc(nv ? nv : 1, sizeof *order);
    for (size_t k = 0; k < nv; k++) order[k] = k;
    avp_sort(order, nv, sizeof *order, vote_rank, votes, false);
    i64 n = 0;
    for (size_t k = 0; k < nv; k++) {
        const Vote *v = &votes->v.v[order[k]];
        i64 a = v->w, b = v->p;
        if (in_pairs(c, a) || in_rev(c, b) || !wt_side_get(c->wii, a) || !wt_side_get(c->ps2, b)) continue;
        i64 ow = 0, op = 0;
        const Group *g = groups_get(&byw, a);
        for (size_t j = 0; j < g->votes.n; j++) {
            const Vote *o = &votes->v.v[g->votes.v[j]];
            if (o->p != b && !in_rev(c, o->p) && o->v > ow) ow = o->v;
        }
        g = groups_get(&byp, b);
        for (size_t j = 0; j < g->votes.n; j++) {
            const Vote *o = &votes->v.v[g->votes.v[j]];
            if (o->w != a && !in_pairs(c, o->w) && o->v > op) op = o->v;
        }
        if (v->v > 2 * ow && v->v > 2 * op && wt_corr_compatible(c, a, b))
            n += wt_corr_add(c, a, b, tier, avp_pyfmt(c->arena, fmt, v->v).p);
    }
    avp_xfree(order);
    groups_free(&byw);
    groups_free(&byp);
    return n;
}

static void dedupe_tagged(const AvpU64Vec *s, size_t lo, size_t hi, int tag, AvpI64Vec *out, AvpISet *seen) {
    out->n = 0;
    for (size_t k = lo; k < hi; k++) {
        if (TAG(s->v[k]) != tag) continue;
        if (avp_iset_add(seen, (u64)VAL(s->v[k]))) avp_vec_push(out, VAL(s->v[k]));
    }
}

static i64 calls_tier(WtCorr *c) {
    Votes votes;
    votes_init(&votes);
    AvpArena *s = avp_arena_new(0);
    AvpI64Vec ua = {0}, ub = {0};
    size_t npairs = c->pairs.n;
    for (size_t k = 0; k < npairs; k++) {
        i64 w = c->pairs.v[k].wii, p = c->pairs.v[k].p.ps2;
        AvpArenaMark mk = avp_arena_mark(s);
        Align al;
        align(c, s, w, p, &al);
        AvpISet *seen = avp_iset_new();
        for (size_t o = 0; o < al.nops; o++) {
            const AvpOpcode *op = &al.ops[o];
            if (op->tag != AVP_OP_REPLACE) continue;
            AvpISet *sa = avp_iset_new(), *sb = avp_iset_new();
            dedupe_tagged(&al.A, op->i1, op->i2, TAG_W, &ua, sa);
            dedupe_tagged(&al.B, op->j1, op->j2, TAG_U, &ub, sb);
            avp_iset_free(sa);
            avp_iset_free(sb);
            if (ua.n && ua.n == ub.n)
                for (size_t j = 0; j < ua.n; j++)
                    if (avp_iset_add(seen, key2(ua.v[j], ub.v[j]))) count_vote(&votes, ua.v[j], ub.v[j])->v += 1;
        }
        avp_iset_free(seen);
        align_free(&al);
        avp_arena_reset(s, mk);
    }
    avp_vec_free(&ua);
    avp_vec_free(&ub);
    avp_arena_free(s);
    i64 n = accept(c, &votes, "call", "call order in %d mapped caller(s)");
    votes_free(&votes);
    return n;
}

/* Counter: the highest count, its key, and whether another key has it too; the second highest count. */
typedef struct {
    i64 best, k, second;
    bool tie;
} Top;

static Top top_of(const AvpIMap *cnt) {
    Top t = {WT_NONE, 0, 0, false};
    bool any = false;
    AvpIMapIter it = {0};
    while (avp_imap_next(cnt, &it)) {
        i64 v = it.val->i;
        if (!any || v > t.k) {
            if (any) t.second = t.k;
            t.best = (i64)it.key;
            t.k = v;
            t.tie = false;
            any = true;
        } else if (v == t.k) {
            t.tie = true;
            t.second = v;
        } else if (v > t.second) {
            t.second = v;
        }
    }
    return t;
}

static i64 callees_tier(WtCorr *c) {
    Votes votes;
    votes_init(&votes);
    AvpI64Vec mc = {0};
    for (size_t k = 0; k < c->wii->v.n; k++) {
        const WtFn *wf = c->wii->v.v[k];
        i64 w = wf->addr;
        if (in_pairs(c, w)) continue;
        AvpISet *ms = avp_iset_new();
        mc.n = 0;
        for (size_t j = 0; j < wf->calls.n; j++)
            if (in_pairs(c, wf->calls.v[j])) {
                i64 pc = pair_ps2(c, wf->calls.v[j]);
                if (avp_iset_add(ms, (u64)pc)) avp_vec_push(&mc, pc);
            }
        avp_iset_free(ms);
        if (mc.n < 2) continue;
        AvpIMap *cnt = avp_imap_new();
        for (size_t j = 0; j < mc.n; j++) {
            const AvpPyIntSet *callers = wt_corr_pcallers(c, mc.v[j]);
            if (!callers) continue;
            size_t pos = 0;
            i64 q;
            while (avp_pyintset_next(callers, &pos, &q))
                if (!in_rev(c, q)) {
                    bool created;
                    avp_imap_put(cnt, (u64)q, &created)->i += 1;
                }
        }
        bool empty = avp_imap_len(cnt) == 0;
        Top t = top_of(cnt);
        avp_imap_free(cnt);
        if (empty || t.tie) continue;
        i64 kk = t.k;
        if (kk < 2 || kk * 10 < 6 * (i64)mc.n) continue;
        if (kk == 2) {
            bool busy = false;
            for (size_t j = 0; j < mc.n; j++) {
                const AvpPyIntSet *callers = wt_corr_pcallers(c, mc.v[j]);
                if (callers && avp_pyintset_len(callers) > 10) busy = true;
            }
            if (busy) continue;
        }
        const WtFn *bf = fn_req(c->ps2, t.best);
        AvpISet *pm = avp_iset_new();
        for (size_t j = 0; j < bf->calls.n; j++)
            if (in_rev(c, bf->calls.v[j])) avp_iset_add(pm, (u64)bf->calls.v[j]);
        size_t npm = avp_iset_len(pm);
        avp_iset_free(pm);
        if (kk * 10 < 6 * (i64)npm) continue;
        count_vote(&votes, w, t.best)->v = kk;
    }
    avp_vec_free(&mc);
    i64 n = accept(c, &votes, "callees", "calls %d mapped callee(s) on both sides");
    votes_free(&votes);
    return n;
}

static i64 callers_tier(WtCorr *c) {
    Votes votes;
    votes_init(&votes);
    AvpI64Vec mc = {0};
    for (size_t k = 0; k < c->wii->v.n; k++) {
        i64 w = c->wii->v.v[k]->addr;
        if (in_pairs(c, w)) continue;
        mc.n = 0;
        const AvpPyIntSet *wc = wt_corr_wcallers(c, w);
        if (wc) {
            size_t pos = 0;
            i64 cl;
            while (avp_pyintset_next(wc, &pos, &cl))
                if (in_pairs(c, cl)) avp_vec_push(&mc, pair_ps2(c, cl));
        }
        if (mc.n < 2) continue;
        AvpIMap *cnt = avp_imap_new();
        for (size_t j = 0; j < mc.n; j++) {
            const WtFn *pf = fn_req(c->ps2, mc.v[j]);
            AvpISet *seen = avp_iset_new();
            for (size_t q = 0; q < pf->calls.n; q++) {
                i64 cl = pf->calls.v[q];
                if (!avp_iset_add(seen, (u64)cl)) continue;
                if (cl != WT_NONE && !in_rev(c, cl) && wt_side_get(c->ps2, cl)) {
                    bool created;
                    avp_imap_put(cnt, (u64)cl, &created)->i += 1;
                }
            }
            avp_iset_free(seen);
        }
        bool empty = avp_imap_len(cnt) == 0;
        Top t = top_of(cnt);
        avp_imap_free(cnt);
        if (empty) continue;
        i64 kk = t.k, second = t.tie ? kk : t.second;
        if (kk >= 2 && kk * 10 >= 6 * (i64)mc.n && kk > second) {
            i64 nback = 0, hits = 0;
            const AvpPyIntSet *pcs = wt_corr_pcallers(c, t.best);
            if (pcs) {
                size_t pos = 0;
                i64 cl;
                while (avp_pyintset_next(pcs, &pos, &cl)) {
                    if (!in_rev(c, cl)) continue;
                    nback++;
                    const WtFn *bf = fn_req(c->wii, rev_of(c, cl));
                    for (size_t q = 0; q < bf->calls.n; q++)
                        if (bf->calls.v[q] == w) {
                            hits++;
                            break;
                        }
                }
            }
            if (hits * 10 >= 6 * (nback > 1 ? nback : 1)) count_vote(&votes, w, t.best)->v = kk;
        }
    }
    avp_vec_free(&mc);
    i64 n = accept(c, &votes, "callers", "called by %d mapped caller(s) on both sides");
    votes_free(&votes);
    return n;
}

WtCorr *wt_corr_run(WtCorr *c, int max_rounds) {
    string_anchors(c);
    Votes cvotes;
    constant_candidates(c, &cvotes);
    c->rounds = 0;
    for (int r = 0; r < max_rounds; r++) {
        c->rounds++;
        i64 n = tables(c);
        n += constants(c, &cvotes);
        n += calls_tier(c);
        n += callees_tier(c);
        n += callers_tier(c);
        if (!n) break;
    }
    votes_free(&cvotes);
    return c;
}

/* ---- call_diff, inlined_rows ---- */

static void dedupe_i64(AvpI64Vec *v) {
    AvpISet *seen = avp_iset_new();
    size_t o = 0;
    for (size_t k = 0; k < v->n; k++)
        if (avp_iset_add(seen, (u64)v->v[k])) v->v[o++] = v->v[k];
    v->n = o;
    avp_iset_free(seen);
}

void wt_call_diff(const WtCorr *c, i64 w, i64 p, WtCallDiff *out) {
    memset(out, 0, sizeof *out);
    AvpArena *s = avp_arena_new(0);
    Align al;
    align(c, s, w, p, &al);
    AvpISet *wset = avp_iset_new(), *pset = avp_iset_new(), *ap = avp_iset_new(), *bp = avp_iset_new();
    for (size_t k = 0; k < al.A.n; k++) {
        avp_iset_add(wset, (u64)VAL(al.A.v[k]));
        if (TAG(al.A.v[k]) == TAG_P) avp_iset_add(ap, (u64)VAL(al.A.v[k]));
    }
    for (size_t k = 0; k < al.B.n; k++) {
        i64 x = VAL(al.B.v[k]), wv;
        if (wt_corr_rev(c, x, &wv)) avp_iset_add(pset, (u64)wv);
        if (TAG(al.B.v[k]) == TAG_P) avp_iset_add(bp, (u64)x);
    }
    for (size_t o = 0; o < al.nops; o++) {
        const AvpOpcode *op = &al.ops[o];
        if (op->tag == AVP_OP_EQUAL) {
            for (size_t j = op->j1; j < op->j2; j++) {
                i64 x = VAL(al.B.v[j]);
                WtCallPair cp = {rev_of(c, x), x};
                avp_vec_push(&out->both, cp);
            }
            continue;
        }
        AvpISet *wu = avp_iset_new(), *wp = avp_iset_new(); /* wrapped: ints, ("p", x) tuples */
        for (size_t j = op->j1; j < op->j2; j++) {
            if (TAG(al.B.v[j]) != TAG_U) continue;
            i64 x = VAL(al.B.v[j]);
            for (size_t i = op->i1; i < op->i2; i++) {
                i64 xx = VAL(al.A.v[i]);
                if (TAG(al.A.v[i]) == TAG_P && !avp_iset_has(bp, (u64)xx) && wt_corr_reaches(c, x, xx, 2)) {
                    avp_iset_add(wu, (u64)x);
                    avp_iset_add(wp, (u64)xx);
                }
            }
        }
        bool subst = false;
        if (op->tag == AVP_OP_REPLACE) {
            bool b1 = false, b2 = false;
            for (size_t j = op->j1; j < op->j2 && !b1; j++)
                if (TAG(al.B.v[j]) == TAG_U && !avp_iset_has(wu, (u64)VAL(al.B.v[j]))) b1 = true;
            for (size_t i = op->i1; i < op->i2 && !b2; i++) {
                int t = TAG(al.A.v[i]);
                i64 x = VAL(al.A.v[i]);
                if ((t == TAG_W || !avp_iset_has(bp, (u64)x)) && !(t == TAG_P && avp_iset_has(wp, (u64)x))) b2 = true;
            }
            subst = b1 && b2;
        }
        for (size_t j = op->j1; j < op->j2; j++) {
            int t = TAG(al.B.v[j]);
            i64 x = VAL(al.B.v[j]), wv;
            if (wt_corr_rev(c, x, &wv) && (avp_iset_has(wset, (u64)wv) || avp_iset_has(ap, (u64)x))) continue;
            if (subst && t == TAG_U && !avp_iset_has(wu, (u64)x))
                avp_vec_push(&out->replaced, x);
            else
                avp_vec_push(&out->ps2_only, x);
        }
        for (size_t i = op->i1; i < op->i2; i++) {
            int t = TAG(al.A.v[i]);
            i64 x = VAL(al.A.v[i]);
            if (t == TAG_P && avp_iset_has(bp, (u64)x)) continue;
            if (t == TAG_P && avp_iset_has(wp, (u64)x)) continue;
            if (t == TAG_W) {
                avp_vec_push(&out->wii_only, x);
            } else {
                i64 wv = rev_of(c, x);
                if (!avp_iset_has(pset, (u64)wv)) avp_vec_push(&out->wii_only, wv);
            }
        }
        avp_iset_free(wu);
        avp_iset_free(wp);
    }
    /* _dedupe(both) on (wii, ps2) tuples */
    AvpISet *seen = avp_iset_new();
    size_t o = 0;
    for (size_t k = 0; k < out->both.n; k++)
        if (avp_iset_add(seen, key2(out->both.v[k].wii, out->both.v[k].ps2))) out->both.v[o++] = out->both.v[k];
    out->both.n = o;
    avp_iset_free(seen);
    dedupe_i64(&out->ps2_only);
    dedupe_i64(&out->wii_only);
    dedupe_i64(&out->replaced);
    avp_iset_free(wset);
    avp_iset_free(pset);
    avp_iset_free(ap);
    avp_iset_free(bp);
    align_free(&al);
    avp_arena_free(s);
}

void wt_call_diff_free(WtCallDiff *d) {
    avp_vec_free(&d->both);
    avp_vec_free(&d->ps2_only);
    avp_vec_free(&d->wii_only);
    avp_vec_free(&d->replaced);
}

WtInls wt_corr_inlined_rows(const WtCorr *c) {
    WtInls rows = {0};
    for (size_t k = 0; k < c->inl.n; k++) avp_vec_push(&rows, c->inl.v[k]);
    for (size_t k = 0; k < c->pairs.n; k++) {
        i64 w = c->pairs.v[k].wii, p = c->pairs.v[k].p.ps2;
        WtCallDiff d;
        wt_call_diff(c, w, p, &d);
        for (size_t j = 0; j < d.ps2_only.n; j++) {
            WtInl r = {w, d.ps2_only.v[j], p, "call set"};
            avp_vec_push(&rows, r);
        }
        wt_call_diff_free(&d);
    }
    return rows;
}

/* ---- validation ---- */

i64 wt_hkey(i64 addr) {
    char text[32];
    int n = snprintf(text, sizeof text, "%lld", (long long)addr);
    u8 d[20];
    avp_sha1(text, (size_t)n, d);
    return ((i64)d[0] << 24) | ((i64)d[1] << 16) | ((i64)d[2] << 8) | (i64)d[3];
}

static WtTierCount *tier_slot(WtValidation *v, const char *tier) {
    for (size_t k = 0; k < v->by_tier.n; k++)
        if (strcmp(v->by_tier.v[k].tier, tier) == 0) return &v->by_tier.v[k];
    WtTierCount z = {tier, 0, 0};
    avp_vec_push(&v->by_tier, z);
    return &v->by_tier.v[v->by_tier.n - 1];
}

static int tier_cmp(const void *x, const void *y, void *ctx) {
    (void)ctx;
    return strcmp(((const WtTierCount *)x)->tier, ((const WtTierCount *)y)->tier);
}

void wt_validate(WtSide *wii, WtSide *ps2, const WtRuns *wruns, const WtRuns *pruns, const AvpIMap *anchors,
                 WtValidation *out) {
    memset(out, 0, sizeof *out);
    AvpI64Vec hw = {0}, hp = {0};
    AvpISet *hide = avp_iset_new();
    AvpIMapIter it = {0};
    while (avp_imap_next(anchors, &it))
        if (wt_hkey((i64)it.key) % WT_HOLDOUT_MOD == 0) {
            avp_vec_push(&hw, (i64)it.key);
            avp_vec_push(&hp, it.val->i);
        }
    for (size_t k = 0; k < hw.n; k++) avp_iset_add(hide, (u64)hw.v[k]);
    for (size_t k = 0; k < hp.n; k++) avp_iset_add(hide, (u64)hp.v[k]);
    WtCorr *c = wt_corr_run(wt_corr_new(wii, ps2, wruns, pruns, hide), 30);
    for (size_t k = 0; k < hw.n; k++) {
        i64 w = hw.v[k], p = hp.v[k], other = WT_NONE;
        const WtPairEntry *got = wt_corr_pair(c, w);
        bool has_other = wt_corr_rev(c, p, &other);
        if (got && got->p.ps2 == p) {
            out->correct++;
            tier_slot(out, got->p.tier)->correct++;
        } else if (got || (has_other && other != w)) {
            out->wrong++;
            tier_slot(out, (got ? got : wt_corr_pair(c, other))->p.tier)->wrong++;
        }
    }
    i64 found = out->correct + out->wrong;
    out->held_out = (i64)hw.n;
    out->missed = out->held_out - found;
    if (found) {
        out->has_precision = true;
        out->precision = avp_py_round(100.0 * (double)out->correct / (double)found, 1);
    }
    if (hw.n) {
        out->has_recall = true;
        out->recall = avp_py_round(100.0 * (double)out->correct / (double)hw.n, 1);
    }
    /* the tiers are static strings (WT_TIERS) or the corr's arena: keep them static */
    for (size_t k = 0; k < out->by_tier.n; k++)
        for (int t = 0; t < WT_NTIERS; t++)
            if (strcmp(out->by_tier.v[k].tier, WT_TIERS[t]) == 0) out->by_tier.v[k].tier = WT_TIERS[t];
    avp_sort(out->by_tier.v, out->by_tier.n, sizeof *out->by_tier.v, tier_cmp, NULL, false);
    wt_corr_free(c);
    avp_iset_free(hide);
    avp_vec_free(&hw);
    avp_vec_free(&hp);
}

AvpJson *wt_validation_json(AvpArena *a, const WtValidation *v) {
    AvpJson *o = avp_json_obj(a);
    avp_json_set(a, o, AVP_S("held_out"), avp_json_int(a, v->held_out));
    avp_json_set(a, o, AVP_S("correct"), avp_json_int(a, v->correct));
    avp_json_set(a, o, AVP_S("wrong"), avp_json_int(a, v->wrong));
    avp_json_set(a, o, AVP_S("missed"), avp_json_int(a, v->missed));
    avp_json_set(a, o, AVP_S("precision"), v->has_precision ? avp_json_float(a, v->precision) : avp_json_null());
    avp_json_set(a, o, AVP_S("recall"), v->has_recall ? avp_json_float(a, v->recall) : avp_json_null());
    AvpJson *bt = avp_json_obj(a);
    for (size_t k = 0; k < v->by_tier.n; k++) {
        AvpJson *t = avp_json_obj(a);
        avp_json_set(a, t, AVP_S("correct"), avp_json_int(a, v->by_tier.v[k].correct));
        avp_json_set(a, t, AVP_S("wrong"), avp_json_int(a, v->by_tier.v[k].wrong));
        avp_json_set(a, bt, avp_str(v->by_tier.v[k].tier), t);
    }
    avp_json_set(a, o, AVP_S("by_tier"), bt);
    return o;
}
