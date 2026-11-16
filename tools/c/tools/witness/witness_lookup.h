/* witness_lookup: the read-only query other tools (brief) make of the witness database (tools/witness.py lookup).
 *
 * This header stands alone: it needs only the core runtime and shsm_repo.h, and it declares nothing of the model.
 * Link the tool's library (`LIBS tool_witness`) and include "witness_lookup.h". wt_lookup opens
 * build/index/witness.sqlite read-only and reads only the rows of one Wii function; it never builds, refreshes or
 * checks the database for staleness (doctor reports that). */
#ifndef WITNESS_LOOKUP_H
#define WITNESS_LOOKUP_H

#include "avp/avp.h"
#include "shsm_repo.h"

#ifndef WT_NONE
#define WT_NONE INT64_MIN /* Python None in an int slot */
#endif

/* build/index/witness.sqlite of a checkout (db_path), in `a`. */
const char *wt_db_path(AvpArena *a, const ShsmRepo *r);

/* One PS2 helper inlined at the function: lookup()["helpers"][k]. */
typedef struct WtLookupHelper {
    i64 helper;
    i64 size;             /* WT_NONE = None */
    const char *wii_copy; /* NULL = None */
} WtLookupHelper;

/* lookup(r, address, limit): {ps2, tier, evidence, helpers, more}. */
typedef struct WtLookup {
    i64 ps2;              /* WT_NONE = None */
    const char *tier;     /* NULL = None */
    const char *evidence; /* NULL = None */
    AVP_VEC(WtLookupHelper) helpers; /* at most `limit`, largest first (heap: avp_vec_free) */
    i64 more;             /* helpers left out */
} WtLookup;

/* lookup(r, address, limit=3): false (Python None) when the database is missing, cannot be read, or knows nothing
 * of the address. Strings are allocated from `a`. */
bool wt_lookup(AvpArena *a, const ShsmRepo *r, i64 address, int limit, WtLookup *out);

/* One mapped Wii function and the PS2 link-order rank of its counterpart: the index of the PS2 address among every
 * PS2 function sorted by address (the rank `order` prints). */
typedef struct WtOrderRank {
    i64 wii, rank;
} WtOrderRank;

/* Every pair's (wii, rank), in Wii address order, allocated from `a`. false (and *n 0) when the database is
 * missing or cannot be read: callers treat that as no evidence. */
bool wt_lookup_order(AvpArena *a, const ShsmRepo *r, WtOrderRank **out, size_t *n);

#endif
