/* Function-level comparison of a compiled object with the retail asm: the facade tools/common/compare.py over
 * cc_codecmp (codecmp decides everything; this packages its result as an aligned listing).
 *
 *     CcRepo repo = {root, "R5WEA4"};
 *     CcError err;
 *     AvpMap *compiled = cc_load_compiled(a, &repo, obj, work_dir, NULL, NULL, &err);   // runs dtk elf disasm
 *     CcObjectComparison *oc = cc_compare_object(a, &repo, AVP_S("main/game/PadRumble"), obj, NULL, true, NULL,
 *                                                NULL, compiled, NULL, NULL, &err);
 *     AvpMapIter it = {0};
 *     while (avp_map_next(oc->results, &it)) { CcCompareResult *r = it.val->p; r->exact, r->counts, r->pairs ... }
 *
 * Every function returning a pointer returns NULL with *err set where the Python raised: kind CC_ERR_USAGE
 * (cli.UsageError: dtk failed or could not start), CC_ERR_KEY (KeyError; msg is the message without the quotes
 * str(KeyError) adds), CC_ERR_OS (OSError: msg is str(exc)), CC_ERR_VALUE (ValueError: not an ELF object). */
#pragma once
#include "cc_codecmp.h"

typedef enum { CC_OK = 0, CC_ERR_USAGE, CC_ERR_KEY, CC_ERR_OS, CC_ERR_VALUE } CcErrKind;
typedef struct {
    CcErrKind kind;
    AvpStr msg;
} CcError;

/* The checkout: root (absolute, either slash) and the game version ("R5WEA4"). Paths are built as the Python's
 * os.path.join does (backslashes). */
typedef struct {
    const char *root;
    const char *version;
} CcRepo;

/* repo.unit_path(unit): "main/game/PadRumble" or "game/PadRumble.cpp" -> "game/PadRumble". */
AvpStr cc_unit_path(AvpArena *a, AvpStr unit);
/* repo.asm_path(unit): <root>\build\<ver>\asm\<unit path>.s */
char *cc_repo_asm_path(AvpArena *a, const CcRepo *r, AvpStr unit);
/* repo.src_obj_dir and repo.tool("dtk") */
char *cc_repo_src_obj_dir(AvpArena *a, const CcRepo *r);
char *cc_repo_dtk(AvpArena *a, const CcRepo *r);

/* ---- rows and pairs ---------------------------------------------------------------------------------------------- */

typedef enum { CC_EQUAL, CC_REPLACE, CC_DELETE, CC_INSERT } CcPairStatus;
const char *cc_pair_status_name(CcPairStatus s); /* "equal", "replace", "delete", "insert" */

/* Side: index (row number in its function), address, text (as listed), norm (codecmp's normalised text). */
typedef struct {
    size_t index;
    i64 address;
    AvpStr text, norm;
} CcSide;

/* Pair: status; retail / compiled (NULL for a missing side); target NULL, "mismatch" or "unresolved" (an equal
 * pair whose relocation target differs or is undecidable); detail (.p NULL is None) codecmp's explanation. */
typedef struct {
    CcPairStatus status;
    const CcSide *retail, *compiled;
    const char *target;
    AvpStr detail;
} CcPair;

/* Pair.differs: status not equal, or a target mismatch. */
bool cc_pair_differs(const CcPair *p);

/* align(rfn, cfn): the pairs of the two row lists aligned on their norms (difflib, autojunk off). */
CcPair *cc_align(AvpArena *a, const CcFn *rfn, const CcFn *cfn, size_t *npairs);

typedef struct {
    size_t equal, replace, delete_, insert, retail_rows, compiled_rows, target_mismatches, unresolved;
} CcCounts;

/* CompareResult. target_mismatches / unresolved are raw's lists (the Python's dicts i, j, retail, compiled, detail,
 * kind carry the same fields); value proofs are raw->notes. */
typedef struct CcCompareResult {
    AvpStr name, retail_name;
    const CcFn *retail_fn, *compiled_fn;
    const CcFnResult *raw;
    bool exact;
    size_t edit;
    size_t checked_targets;
    const CcPositional *positional;
    size_t npositional;
    const CcTdList *target_mismatches, *unresolved;
    CcPair *pairs;
    size_t npairs;
    CcCounts counts;
} CcCompareResult;

/* CompareResult(name, retail_name, rfn, cfn, res): aligns and marks target results on the equal pairs. */
CcCompareResult *cc_compare_result_new(AvpArena *a, AvpStr name, AvpStr retail_name, const CcFn *rfn, const CcFn *cfn,
                                       const CcFnResult *res);
/* to_dict(pairs): the same keys in the same order (dump with avp_json_dumps as json.dumps would). */
AvpJson *cc_compare_result_to_json(AvpArena *a, const CcCompareResult *r, bool pairs);
/* render(only_diff, context, width): the side-by-side text, lines joined with "\n" (no final newline). Python's
 * defaults: false, 2, 40. */
AvpStr cc_compare_result_render(AvpArena *a, const CcCompareResult *r, bool only_diff, int context, int width);

/* ---- loading ----------------------------------------------------------------------------------------------------- */

/* is_gap(name): dtk's padding block name gap_NN_<address>_<section>. */
bool cc_is_gap(AvpStr name);

/* load_retail(unit, repo, asm_path, include_gaps): {name: CcFn*} of the unit's retail listing, cached per file
 * (size, mtime) for the process (thread-safe; the CcFn values are shared: do not change them). asm_path NULL: the
 * unit's build/<ver>/asm listing. */
AvpMap *cc_load_retail(const CcRepo *r, AvpStr unit, const char *asm_path, bool include_gaps, CcError *err);

/* The listing cache seam of load_compiled (the Python's cache=Cache: keyed on the object's bytes and the dtk
 * binary; key computation is the caller's). get: true with *listing on a hit. put: store a fresh listing. */
typedef struct {
    void *ctx;
    bool (*get)(void *ctx, AvpStr obj_bytes, AvpStr *listing);
    void (*put)(void *ctx, AvpStr obj_bytes, AvpStr listing);
} CcListingCache;

/* load_compiled(obj_path, repo, out_dir, cache): {name: CcFn*} of a compiled object. dtk runs in the listing's
 * directory with bare file names (timeout 120 s); the listing goes to <out_dir>/<obj name>.s (the object copied
 * there and the copy removed), else beside the object, except for an object under build/<ver>/src whose listing
 * goes to a temporary directory removed afterwards. cache NULL: always run dtk. *listing (may be NULL) receives the
 * listing text. */
AvpMap *cc_load_compiled(AvpArena *a, const CcRepo *r, const char *obj_path, const char *out_dir,
                         const CcListingCache *cache, AvpStr *listing, CcError *err);

/* context(unit, repo): a Context on the checkout's shared Project (one per root), for unit_path(unit). */
CcContext *cc_repo_context(const CcRepo *r, AvpStr unit);

/* ---- comparing --------------------------------------------------------------------------------------------------- */

/* compare_function(unit, obj_path, symbol, retail_name, repo, targets, compiled, asm_path). retail_name empty: the
 * codecmp rule (cc_retail_name_for). compiled NULL: load_compiled(obj_path) without out_dir or cache. */
CcCompareResult *cc_compare_function_unit(AvpArena *a, const CcRepo *r, AvpStr unit, const char *obj_path,
                                          AvpStr symbol, AvpStr retail_name, bool targets, const AvpMap *compiled,
                                          const char *asm_path, CcError *err);

/* ObjectComparison: results {compiled name: CcCompareResult*} (compiled order); compiled_only (compiled functions
 * with no retail counterpart) and retail_only (retail functions nothing was paired with, gap_ excluded), both
 * sorted; retail / compiled the two parsed sides. */
typedef struct {
    AvpStr *v;
    size_t n;
} CcNames;

typedef struct CcObjectComparison {
    AvpMap *results;
    CcNames compiled_only, retail_only;
    const AvpMap *retail, *compiled;
} CcObjectComparison;

/* compare_object(unit, obj_path, symbols, repo, targets, names, asm_path, compiled, out_dir, cache).
 * symbols: NULL or an empty set compares every function (the Python tested its truth); names: compiled name ->
 * AvpStr* retail name (val.p) or NULL. compiled NULL: cc_load_compiled(obj_path, out_dir, cache). */
CcObjectComparison *cc_compare_object(AvpArena *a, const CcRepo *r, AvpStr unit, const char *obj_path,
                                      const AvpSet *symbols, bool targets, const AvpMap *names, const char *asm_path,
                                      const AvpMap *compiled, const char *out_dir, const CcListingCache *cache,
                                      CcError *err);
