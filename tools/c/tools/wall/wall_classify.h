/* wall_classify: the per-function classifier of tools/wall.py (L376-950, L979-993): _hex, Resolver, _calls,
 * analyse_calls, _align, _stack, _fallback_targets, classify, _cfg_differences, _loops, _evidence_sentence,
 * describer and classify_result. Every result is the Python's dict as an AvpJson object with the same keys in the
 * same order and the same value types (wall_classify.c: the classifier; wall_describe.c: the referent describer and
 * classify_result).
 *
 * Memory: results are allocated from the arena given (strings copied); the working memory of one call is a private
 * arena freed before it returns. */
#pragma once
#include "avp/avp.h"
#include "cc_compare.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"
#include "wall_rows.h"

/* The class names (the same spellings as wall_rows.h). */
#define WALL_REFERENT "referent"
#define WALL_CALL "call set"
#define WALL_CONTROL "control flow"
#define WALL_OTHER "other"
#define WALL_REGISTER "register/schedule"

/* _hex(a): "0x%X" % a. */
AvpStr wall_hex(AvpArena *a, i64 addr);

/* One _calls() entry (i, name, kind): "<self>" for a self-recursive call, "<indirect>" for a bctrl. */
typedef struct {
    size_t i;
    AvpStr name;
    WallKind kind;
} WallCall;
/* _calls(rows): the calls of a function in row order. Returns the count; *out is an arena array. */
size_t wall_calls(AvpArena *a, const CcRow *rows, size_t nrows, WallCall **out);

/* analyse_calls(rfn, cfn, Resolver(project)): {retail, compiled, retail_only, compiled_only, size_diff, readings,
 * helpers, findings, order_differs, differs, flags_only}. project NULL is Resolver(None) (callee identity by
 * embedded address or name only). */
AvpJson *wall_analyse_calls(AvpArena *a, const CcFn *rfn, const CcFn *cfn, CcProject *project);

/* _fallback_targets(rfn, cfn): target mismatches without a codecmp Context (equal rows whose symbol names differ,
 * or whose local branch targets differ), kind "target" or "branch". */
CcTdList wall_fallback_targets(AvpArena *a, const CcFn *rfn, const CcFn *cfn);

/* describe(i, j) (the Python's callable): the {retail, compiled, same} dict of a referent row pair. */
typedef AvpJson *(*WallDescribeFn)(void *ctx, AvpArena *a, size_t i, size_t j);

/* classify(rfn, cfn, targets, describe, project, unresolved) (wall.py L523-791). targets NULL is the Python's None
 * (derived with wall_fallback_targets); describe NULL is None; project NULL is None. */
AvpJson *wall_classify(AvpArena *a, const CcFn *rfn, const CcFn *cfn, const CcTdList *targets,
                       WallDescribeFn describe, void *describe_ctx, CcProject *project, size_t unresolved);

/* describer(rfn, cfn, ctx, symtab) (wall.py L877-930): the describe callback's state; pass wall_describe with a
 * pointer to it. symtab may be NULL (the Python's None: no symbols.txt data for the retail side). */
typedef struct {
    const CcFn *rfn, *cfn;
    CcContext *ctx;
    const ShsmSymbolTable *symtab;
} WallDescriber;
AvpJson *wall_describe(void *describer, AvpArena *a, size_t i, size_t j);

/* classify_result(unit, res, repo, symtab=None) (wall.py L979-993): the classification of one compared function,
 * with name, retail_name and exact_codecmp added. symtab may be NULL (the Python's None). */
AvpJson *wall_classify_result(AvpArena *a, const char *unit, const CcCompareResult *res, const ShsmRepo *r,
                              const ShsmSymbolTable *symtab);
