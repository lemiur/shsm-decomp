/* fragments (tools/fragments.py) for other tools: the fragments inside one retail function, as the `in` subcommand
 * selects them, read from build/index/fragments.sqlite as it stands (never built here). Link `tool_fragments`
 * (shsm_add_tool(<name> LIBS tool_fragments)). Documented in fragments.md. */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"
#include "avp/vec.h"

typedef struct {
    const char *id;      /* "S12", "L3" */
    const char *level;   /* "strict" or "loose" */
    long long rank, length, nfuncs, nunits, nocc, score;
    AvpStrVec pattern;   /* the rendered rows (registers renamed rA, rB...; varying operands as <varies: ...>) */
    long long offset;    /* the occurrence's byte offset in the function */
} FragmentInFunction;
typedef AVP_VEC(FragmentInFunction) FragmentsInFunction;

/* in_function(address, repo, min_functions, level): the fragments inside the function at `address`, in address
 * order, the best-scoring fragment of each stretch (overlaps under half), each found in at least min_functions
 * functions (the Python's default is 4, the `in` subcommand's 0), of one level or both (level NULL).
 * root: the checkout (NULL: the one around the cwd). Returns false (Python's None) when the database is absent or
 * unreadable; else true with *out filled (strings in the arena a; free each pattern and out with avp_vec_free). */
bool fragments_in_function(AvpArena *a, const char *root, long long address, long long min_functions,
                           const char *level, FragmentsInFunction *out);
