/* eq_prove: the prover of tools/eqcheck.py (L1607-2257): Verdict (the exception, eq_side.h), Prover (entry, prove,
 * share_arities, pairings, _result, _prove, edges_into, in_state, _partition, _sig, fixpoint, mismatch,
 * record_pairs, check, compare_effects, compare_stores, check_exit), _has_op, _locname, _rowspan, _eff_text, the
 * relocation resolver (make_resolver, _QUOTED, _compiled_address, target_hints) and prove_functions.
 *
 * ---- Entry point -------------------------------------------------------------------------------------------------
 *     AvpJson *r = eq_prove_functions(a, rfn, cfn, ctx, EQ_RET_DEFAULT, explain, res, live);
 * is prove_functions(rfn, cfn, ctx, ret, explain, res, live) (L2235-2247): the result dict as an AvpJson object in
 * arena a, with the Python's keys in the Python's order:
 *     verdict, reason, block, terms, unmodelled, assumptions, blocks, swapped, pairs, line[, explain][, ret_only],
 *     seconds
 * ("explain" only when the per-block list is not empty, "ret_only" only for the "equivalent except the return
 * register" verdict, "seconds" = round(elapsed, 3) as a float: timing, which differs from run to run). The values
 * are the Python's byte for byte (json.dumps of the C object equals json.dumps of the Python dict, "seconds" aside).
 *
 * What the caller supplies (as the Python's callers do):
 *   rfn, cfn  the retail and compiled functions (CcFn of cc_listing.h). For a proof of retail against itself (the
 *             selfcheck --identity mode) pass the same CcFn twice. cfn->elf is the compiled object (NULL for a
 *             retail listing): compiled relocations are resolved through it.
 *   ctx       the codecmp context of the unit (cc_repo_context(repo, unit) in cc_compare.h; NULL = None: relocation
 *             targets stay symbolic). It is used by one thread at a time (its CcProject may be shared).
 *   ret       the return registers compared: EQ_RET_DEFAULT (the Python's None: r3, r4, f1, and a differing one is
 *             only NOT PROVEN, with the "equivalent except the return register" retry), or an explicit list
 *             (EqRet{names, n}; n 0 is the CLI's `--ret void`). Every name must be in REG_LOCS (the CLI checks).
 *   explain   collect the per-block effect lists (the "explain" key).
 *   res       the compare result of the pair (CcCompareResult of cc_compare.h, NULL = None): relocation targets
 *             codecmp proved identical are taken from it (target_hints). Its compiled_fn should be cfn (the hints are
 *             keyed by row identity, as the Python's id(row)).
 *   live      callee argument liveness (eq_live.h, NULL = None). Its memo makes answers depend on the order of
 *             queries, so keep one EqLive per run where the Python keeps one CalleeLiveness (one per unit in a scan).
 *
 * Errors. Unsupported, NotProven and Verdict never escape: they become the result (as in the Python). Errors of
 * Python itself on a malformed listing (KeyError, IndexError, ValueError, TypeError: eq_side.h, DEVIATIONS
 * D-eqcheck-2) escape as the Python's do: thrown with avp_throw (an eqcheck exception record, eq_exc_caught) after
 * the prover's own memory is freed; a caller that must survive them (the scan's `except Exception`) wraps the call
 * in AVP_TRY.
 *
 * Threading and memory. One call is one proof on the calling thread; calls on different threads are independent
 * (each needs its own ctx and live). The terms, states and runs of a proof live in a private arena freed before
 * the call returns; the result (and nothing else) is allocated in a. Node ids are the Python's (allocation order in
 * the proof's Terms), so every show() text in the result is the Python's.
 *
 * The resolver is exposed for tests and for callers that build Sides themselves (eq_side.h's EqResolveFn). */
#pragma once
#include "avp/avp.h"
#include "cc_codecmp.h"
#include "cc_compare.h"
#include "eq_live.h"
#include "eq_side.h"

/* ---- return registers ----------------------------------------------------------------------------------------- */
typedef struct {
    const char *const *names; /* REG_LOCS names */
    int n;                    /* -1: the Python's None (the default r3, r4, f1, not explicit) */
} EqRet;
#define EQ_RET_DEFAULT ((EqRet){NULL, -1})

/* ---- relocation resolution (L2147-2232) -------------------------------------------------------------------------- */
/* target_hints(res, ctx): {id(compiled row): retail address} for the aligned row pairs whose relocation targets
 * codecmp proves identical. Rows are keyed by pointer (into res->compiled_fn->rows). Empty when res or ctx is NULL. */
typedef struct {
    const CcRow *row;
    i64 addr;
} EqHint;
size_t eq_target_hints(AvpArena *a, const CcCompareResult *res, CcContext *ctx, EqHint **out);

/* make_resolver(fn, holder, ctx, rfn, hints) (L2150-2183): a row -> address term resolver for fn's rows (the
 * EqResolveFn callback with the returned struct as its ctx). Results are cached per row (the Python's id(row)
 * cache): a row is resolved once per resolver, so nodes are made only on its first resolution. ctx NULL = None,
 * rfn NULL = None (no data check of compiled-local targets). Rows passed to it must be fn's. Allocated in a. */
typedef struct EqResolver EqResolver;
EqResolver *eq_resolver_new(AvpArena *a, const CcFn *fn, CcContext *ctx, const CcFn *rfn, const EqHint *hints,
                            size_t nhints);
EqNode *eq_resolve(void *resolver, EqTerms *t, const CcRow *row);

/* _QUOTED.search(text) (L2147): "([^"]+)"(?:\+(0x[0-9A-Fa-f]+))? -- false when there is no match; *name is
 * group 1, *off_hex group 2 (p NULL when the group did not take part). Views into text. */
bool eq_quoted_search(AvpStr text, AvpStr *name, AvpStr *off_hex);

/* ---- the prover ------------------------------------------------------------------------------------------------- */
/* prove_functions(rfn, cfn, ctx, ret, explain, res, live) (L2235-2247). See the top of this header. */
AvpJson *eq_prove_functions(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcContext *ctx, EqRet ret, bool explain,
                            const CcCompareResult *res, EqLive *live);
/* The same with an explicit max_rounds (Prover's default 60) and without the "seconds" key (for tests that
 * compare whole results). */
AvpJson *eq_prove_functions_ex(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcContext *ctx, EqRet ret, bool explain,
                               const CcCompareResult *res, EqLive *live, int max_rounds, bool seconds);

/* _locname(loc) (L2125-2126) of a slot: "r1%+#x/%d" (Python's %+#x: "+0x8", "-0x10", "+0x0"). In arena a. */
const char *eq_slot_locname(AvpArena *a, i64 off, i64 w);
