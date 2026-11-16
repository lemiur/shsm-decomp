/* fragments (tools/fragments.py) known helpers for other tools: KNOWN and known_sites, the independent line-pattern
 * scan of every retail listing for the three helpers AGENTS.md records (the inlined string hash, the display-height
 * pair, the save-data key check). Link `tool_fragments` (shsm_add_tool(<name> LIBS tool_fragments)). This is the API
 * that witness's private copy (tools/c/tools/witness/witness_known.c, wt_known_sites) folds onto: its
 * wt_known_sites(a, r, ix, out) is fragments_known_functions(a, r, ix, out). Documented in fragments.md. */
#pragma once
#include "avp/arena.h"
#include "avp/map.h"
#include "avp/str.h"
#include "avp/vec.h"
#include "shsm_repo.h"

#define FRAGMENTS_NKNOWN 3

typedef struct {
    const char *name;  /* "string hash", "display-height pair", "save-data key check" */
    long long stated;  /* the count the docs state, or -1 (None) */
    const char *note;
} FragmentsKnown;
extern const FragmentsKnown FRAGMENTS_KNOWN[FRAGMENTS_NKNOWN]; /* KNOWN, in order */

typedef struct {
    AvpStr fn;          /* the function (the listing's .fn name, quotes stripped) */
    long long address;  /* the anchor row's address */
    long long offset;   /* its offset from the function's first row */
} FragmentsKnownSite;
typedef AVP_VEC(FragmentsKnownSite) FragmentsKnownSites;

struct ShsmAsmIndex;
/* known_sites(r, ix): out[k] lists the sites of FRAGMENTS_KNOWN[k] in the Python's order (listing order of
 * `select distinct path from files`, then function, then row). Strings live in the arena a; free out[k].v with
 * avp_vec_free. Raises SHSM_EXC_SQLITE on an index error, the fs.h exception when a listing cannot be read. */
void fragments_known_sites(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix,
                           FragmentsKnownSites out[FRAGMENTS_NKNOWN]);
/* The same reduced to the set of function names with a site of each helper (witness's wt_known_sites). */
void fragments_known_functions(AvpArena *a, const ShsmRepo *r, struct ShsmAsmIndex *ix,
                               AvpSet *out[FRAGMENTS_NKNOWN]);
