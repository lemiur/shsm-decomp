/* witness_map: the Wii <-> PS2 correspondence (tools/witness.py Correspondence, _hkey, _dedupe, _ev_strings,
 * validate).
 *
 * Python's dicts are reproduced in insertion order (pairs, rev, anchor_of, pcallers, wcallers). Where the Python
 * iterates a set of str or of tuples (string_anchors' shared texts and (text, int) keys, constant_candidates'
 * shared (kind, value) keys), its order follows PYTHONHASHSEED; the C iterates them sorted (DEVIATIONS.md
 * D-witness-3): the pairs and the inlined rows are the same, only evidence texts (which strings or constants are
 * listed first) can differ. */
#ifndef WITNESS_MAP_H
#define WITNESS_MAP_H

#include "witness.h"

/* One entry of Correspondence.pairs: wii -> (ps2, tier, evidence). */
typedef struct WtPairEntry {
    i64 wii;
    WtPair p;
} WtPairEntry;

struct WtCorr {
    AvpArena *arena;                   /* evidence texts and the other allocations of this correspondence */
    WtSide *wii, *ps2;
    WtRuns wruns, pruns;
    AvpISet *hide;                     /* frozenset of addresses whose strings are ignored; NULL = empty */
    AVP_VEC(WtPairEntry) pairs;        /* self.pairs in insertion order */
    AvpIMap *pair_idx;                 /* wii -> index into pairs (AvpVal.i) */
    AvpIMap *rev;                      /* ps2 -> wii (AvpVal.i), insertion order */
    WtInls inl;                        /* self.inl: (wii, ps2 helper, ps2 parent, evidence) */
    AvpIMap *anchor_of;                /* wii -> ps2 (AvpVal.i), insertion order */
    AvpIMap *pcallers, *wcallers;      /* callee -> AvpPyIntSet* of callers (AvpVal.p), defaultdict(set) order */
    AvpISet *common_floats;
    int rounds;                        /* set by run(); 0 before */
};

/* Correspondence(wii, ps2, wruns, pruns, hide). The sides are borrowed (they must outlive the WtCorr); wruns and
 * pruns are copied (the run vectors themselves are shared). hide may be NULL. */
WtCorr *wt_corr_new(WtSide *wii, WtSide *ps2, const WtRuns *wruns, const WtRuns *pruns, AvpISet *hide);
void wt_corr_free(WtCorr *c);

/* self.pairs.get(w) / self.rev.get(p): NULL / false when absent. */
const WtPairEntry *wt_corr_pair(const WtCorr *c, i64 wii);
bool wt_corr_rev(const WtCorr *c, i64 ps2, i64 *wii);
/* self.pcallers.get(a) / self.wcallers.get(a): NULL when absent (the Python's ()). */
const AvpPyIntSet *wt_corr_pcallers(const WtCorr *c, i64 a);
const AvpPyIntSet *wt_corr_wcallers(const WtCorr *c, i64 a);

/* add(w, p, tier, ev): false when either side is already mapped. ev is copied. */
bool wt_corr_add(WtCorr *c, i64 w, i64 p, const char *tier, const char *ev);
bool wt_corr_compatible(const WtCorr *c, i64 w, i64 p);
bool wt_corr_reaches(const WtCorr *c, i64 src, i64 dst, int depth);

/* run(max_rounds=30): string anchors, constants, then propagation rounds. */
WtCorr *wt_corr_run(WtCorr *c, int max_rounds);

/* call_diff(w, p): callees in call order, each once. both holds (wii callee, ps2 callee) pairs. */
typedef struct WtCallPair {
    i64 wii, ps2;
} WtCallPair;
typedef struct WtCallDiff {
    AVP_VEC(WtCallPair) both;
    AvpI64Vec ps2_only, wii_only, replaced;
} WtCallDiff;
void wt_call_diff(const WtCorr *c, i64 w, i64 p, WtCallDiff *out);
void wt_call_diff_free(WtCallDiff *d);

/* inlined_rows(): self.inl, then (w, helper, p, "call set") per pair in pair order. A new vector (heap). */
WtInls wt_corr_inlined_rows(const WtCorr *c);

/* _hkey(addr): int(sha1(b"%d" % addr).hexdigest()[:8], 16). */
i64 wt_hkey(i64 addr);

/* _ev_strings(ts): "%d unique string(s): " + the two longest texts (json.dumps(t[:40])). Into `a`. */
const char *wt_ev_strings(AvpArena *a, const AvpStr *ts, size_t n);

/* validate(...): the meta["validation"] dict. */
typedef struct WtTierCount {
    const char *tier;
    i64 correct, wrong;
} WtTierCount;
typedef struct WtValidation {
    i64 held_out, correct, wrong, missed;
    bool has_precision, has_recall; /* false = None */
    double precision, recall;
    AVP_VEC(WtTierCount) by_tier; /* sorted by tier */
} WtValidation;
void wt_validate(WtSide *wii, WtSide *ps2, const WtRuns *wruns, const WtRuns *pruns, const AvpIMap *anchors,
                 WtValidation *out);
/* The dict as JSON (key order of the Python). */
AvpJson *wt_validation_json(AvpArena *a, const WtValidation *v);

#endif
