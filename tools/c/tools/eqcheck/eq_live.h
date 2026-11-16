/* eq_live: callee argument liveness and the proof's graph helpers of tools/eqcheck.py: ARG_REGS, _ABIT, ALL_ARGS,
 * STD_ARGS, _REGTOK, _DEF_FIRST, _mask, CalleeLiveness (L1405-1584), _rpo, diff_path, first_diff (L1591-1630); plus
 * the two listing patterns _BRANCH and _HELPER (L536-537), which the Side uses too.
 *
 * Register sets are bitmasks over ARG_REGS (bit k = EQ_ARG_REGS[k]: r0, r3..r12, f0..f13; 25 bits). The Python's
 * CalleeLiveness.__call__ returns two sets of names; the Side only tests membership (`r in summ[0]`), so
 * eq_live_call gives the two masks and eq_abit(name) the bit to test (0 for a name outside ARG_REGS).
 *
 * Errors: none raised. CalleeLiveness swallows every index failure as the Python does (an index that cannot be
 * opened makes every call "None"; a function or symbol lookup that fails is "unknown"). */
#pragma once
#include "avp/avp.h"
#include "eq_terms.h"
#include "shsm_asm.h"
#include "shsm_repo.h"
#include "wall_cfg.h"

/* ---- listing patterns (L536-537) ------------------------------------------------------------------------------- */
/* _BRANCH.match(mn): ^b(?P<ctr>dnz|dz)?(?P<cc>eq|ne|lt|ge|gt|le|so|ns|un|nu|nl|ng)?(?P<lr>lr|ctr)?(?P<link>l)?$.
 * Groups that did not take part have p == NULL. */
typedef struct {
    AvpStr ctr, cc, lr, link;
} EqBranch;
bool eq_branch_match(AvpStr mnemonic, EqBranch *out);
/* _HELPER.match(name): ^_(save|rest)(gpr|fpr)_(\d+)$; groups 1-3 into the out parameters (any may be NULL). */
bool eq_helper_match(AvpStr name, AvpStr *kind, AvpStr *cls, i64 *n);

/* ---- argument registers (L1405-1418) --------------------------------------------------------------------------- */
#define EQ_NARG_REGS 25
extern const char *const EQ_ARG_REGS[EQ_NARG_REGS]; /* ARG_REGS: r0 r3..r12 f0..f13 */
#define EQ_ALL_ARGS ((u32)((1u << EQ_NARG_REGS) - 1))
extern const u32 EQ_STD_ARGS; /* r3..r10 and f1..f8 */
/* _ABIT.get(name, 0). */
u32 eq_abit(AvpStr name);
static inline u32 eq_abitz(const char *name) { return eq_abit(avp_str(name)); }
/* _mask(names). */
u32 eq_reg_mask(const AvpStr *names, size_t n);

/* ---- CalleeLiveness (L1422-1584) ------------------------------------------------------------------------------ */
/* What a retail function does with the caller's volatile registers, from its listing: live (the registers it may
 * read before writing them) and writes (the registers it writes on every path to its return). Calls inside it are
 * followed to `depth` levels; deeper, recursive, indirect or unknown calls read everything and write nothing.
 *
 * The index is the retail asm index (AsmIndex.open_readonly(repo, refresh=False)), opened on first use; a failure
 * to open it is remembered (every later call answers None). The memo is keyed by address only, as in the Python:
 * a summary first computed deep in a call chain (with its calls cut off at the depth limit) is what a later
 * top-level query returns, so results depend on the order of queries; keep one EqLive per run where the Python
 * keeps one CalleeLiveness. Not thread-safe (one per thread, as its sqlite connection). */
typedef struct EqLive EqLive;
/* CalleeLiveness(repo=None, depth=4): repo NULL = the checkout of the current directory. */
EqLive *eq_live_new(const ShsmRepo *repo, int depth);
void eq_live_free(EqLive *lv);
/* Sets the index to "unavailable" (the Python's ix = False): every summary is None, every callee unknown. For
 * tests of eq_live_analyse without an index. */
void eq_live_disable_index(EqLive *lv);
/* __call__(addr): false for None (the address starts no function the index knows); else the live and the
 * always-written masks. */
bool eq_live_call(EqLive *lv, i64 addr, u32 *live, u32 *writes);
/* summary(addr, depth): false for None. */
bool eq_live_summary(EqLive *lv, i64 addr, int depth, u32 *live, u32 *writes);
/* analyse(rows, depth): the summary of a function's rows (mnemonic, operands, sym, label and address are read). */
void eq_live_analyse(EqLive *lv, const ShsmAsmRow *rows, size_t n, int depth, u32 *live, u32 *writes);

/* ---- graph helpers (L1591-1630) --------------------------------------------------------------------------------- */
/* _rpo(cfg): reverse post-order of the blocks reachable from block 0 (iterative DFS, successors in order). Returns
 * the number of blocks written to *out (allocated in a). */
size_t eq_rpo(AvpArena *a, const WallCfg *cfg, int **out);

typedef struct {
    EqArg a, b;
} EqArgPair;
/* diff_path(a, b): the chain of differing subterm pairs of two terms, outermost first, down to where their shapes
 * part (the first pair is (a, b) itself). Returns the count, the pairs in *out (allocated in a). */
size_t eq_diff_path(AvpArena *a, EqArg x, EqArg y, EqArgPair **out);
/* first_diff(a, b): the innermost differing pair of subterms of two terms of the same shape. */
EqArgPair eq_first_diff(EqArg x, EqArg y);
