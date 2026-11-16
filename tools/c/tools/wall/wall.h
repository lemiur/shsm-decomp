/* wall: internal declarations of the command-line half of tools/wall.py (compiling and comparing, the
 * prover, rendering, the single form, scan and notes). The public pieces other tools link are in wall_walls.h
 * (walls.json, notes) and wall_classify.h (the classifier).
 *
 * Files: wall_py.c (Python str()/repr()/%d of JSON values), wall_walls.c (walls.json, notes), wall_prove.c (the
 * eqcheck prover, wants_proof, apply_proof, proof_line), wall_cli.c (object_for, compare_all, _find, render, the
 * three forms, main). */
#pragma once
#include "avp/avp.h"
#include "cc_compare.h"
#include "eq_live.h"
#include "shsm_cli.h"
#include "shsm_repo.h"
#include "shsm_symbols.h"
#include "shsm_units.h"
#include "wall_classify.h"
#include "wall_walls.h"

#define WALL_ORDER_NOTE                                                                                              \
    "Verdict order: referent, call set, control flow, other, register/schedule; 'other' (a differing immediate or " \
    "offset) ranks above register/schedule, unlike the project's four-class order, because it is a source "         \
    "difference rather than an allocator decision."

/* ---- Python values (wall_py.c) --------------------------------------------------------------------------------- */
AvpStr wall_py_str(AvpArena *a, const AvpJson *v);  /* str(v) of a JSON-loaded value (NULL: None) */
AvpStr wall_py_repr(AvpArena *a, const AvpJson *v); /* repr(v) */
i64 wall_py_int(const AvpJson *v);                  /* the integer '%d' % v prints (bool 0/1, float truncated) */
AvpStr wall_py_hex(AvpArena *a, i64 v);             /* hex(v) */
bool wall_py_eq(const AvpJson *x, const AvpJson *y);  /* Python == (NULL is None) */
AvpStr wall_jstr(const AvpJson *obj, const char *key); /* a str member, "" when absent or not a str */
AvpJson *wall_jstrz(AvpArena *a, const char *s);     /* a str, null for NULL */

/* ---- compiling and comparing (wall_cli.c) ---------------------------------------------------------------------- */
const char *wall_work_dir(AvpArena *a, const ShsmRepo *r); /* <root>\build\tmp\wall */

/* object_for(unit, repo, built, source, tag): the object and match.state_inputs() of a compile of the unit's own
 * source (NULL for --built and --file). Compiles at below-normal priority into build/tmp/wall/obj<tag>/<path>/.
 * Raises SHSM_EXC_USAGE as the Python did. */
typedef struct {
    const char *obj;
    AvpJson *inputs; /* NULL: None */
} WallObject;
WallObject wall_object_for(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, bool built, const char *source,
                           const char *tag);
/* units.compile_source(unit, src, out_dir, repo=repo, low_priority=True, depfile=True): makes out_dir, compiles
 * with the unit's flags at below-normal priority (timeout 300 s); raises as cli.run when the compiler cannot start.
 * Also used by eqcheck's object_for. */
ShsmRun wall_compile_low(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *src, const char *out_dir);
/* cli.first_error(output): the compiler's first real error as one line; .p NULL for None. */
AvpStr wall_first_error(AvpArena *a, AvpStr output);
/* compare_all(unit, obj, repo): the object comparison (results, retail); raises as compare.compare_object. */
CcObjectComparison *wall_compare_all(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const char *obj);
/* _find(symbol, results, retail): the result; NULL when only the retail listing has the symbol; raises
 * SHSM_EXC_KEY when neither has it. */
const CcCompareResult *wall_find(AvpStr symbol, const AvpMap *results, const AvpMap *retail);

/* ---- the prover (wall_prove.c) ---------------------------------------------------------------------------------- */
/* prove(unit, res, repo, live): {verdict, reason, line} of eqcheck's prove_functions(res.retail_fn, res.compiled_fn,
 * compare.context(unit), res=res, live=live) (eq_prove.h), with a fresh context per proof as the Python. live NULL:
 * a CalleeLiveness made for this proof and freed after it (the single form); scan passes one per unit. Never raises:
 * a prover exception is a NOT PROVEN verdict "prover error: <type(e).__name__>[: <first line of str(e)>]". */
AvpJson *wall_prove(AvpArena *a, const ShsmRepo *r, const ShsmUnit *u, const CcCompareResult *res, EqLive *live);
bool wall_wants_proof(const AvpJson *d, bool prove_all);
void wall_apply_proof(AvpArena *a, AvpJson *d, AvpJson *p);
AvpStr wall_proof_line(AvpArena *a, const AvpJson *d); /* .p NULL for None */

/* ---- rendering and commands (wall_cli.c) ------------------------------------------------------------------------ */
AvpStrVec wall_render(AvpArena *a, const AvpJson *d, bool verbose);
AvpStr wall_phrase(const AvpJson *d);
int wall_main(int argc, char **argv);
