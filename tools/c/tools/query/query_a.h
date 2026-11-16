/* query.py strings, find-string, find-float, find-const and data: the helpers other tools may want (strings_used,
 * _string_users, const_hits / const_line, struct_period, CTYPE). */
#pragma once
#include "query.h"

/* strings_used(ctx, target): the strings block `target` addresses directly (via p NULL) or through one pointer
 * object it addresses (via = that object, or "?"). */
typedef struct {
    AvpStr owner; /* p NULL: None */
    i64 address;
    AvpStr text;
    AvpStr via;   /* p NULL: None */
} QueryStrUse;
typedef AVP_VEC(QueryStrUse) QueryStrUses; /* heap vector: avp_vec_free */
QueryStrUses query_strings_used(QueryCtx *ctx, ShsmTarget target);

/* _string_users(ctx, owner, addr): (name or "@site", kind) in first-reference order; a pooled constant's users
 * through the pool's base get the kind "<kind> (pool)". */
typedef struct {
    AvpStr name, kind;
} QueryUser;
typedef AVP_VEC(QueryUser) QueryUsers; /* heap vector: avp_vec_free */
QueryUsers query_string_users(QueryCtx *ctx, i64 addr);

/* const_hits(ctx, value, data): where the 32-bit value occurs in the retail DOL (aligned data words when data, and
 * lis/addi|ori|load-store pairs). UsageError when there is no retail DOL. */
typedef struct {
    i64 address;
    const char *kind;  /* "data word" "lis" "lis/ori" "lis/<op>" */
    i64 pair;          /* SHSM_NONE: no "pair" key */
    AvpStr detail;
    AvpStr symbol;     /* p NULL: None */
    i64 offset;        /* SHSM_NONE: None */
    AvpStr unit;       /* p NULL: None */
    const char *status;
} QueryConstHit;
typedef AVP_VEC(QueryConstHit) QueryConstHits; /* heap vector: avp_vec_free */
QueryConstHits query_const_hits(QueryCtx *ctx, i64 value, bool data);
AvpJson *query_const_hit_json(AvpArena *a, const QueryConstHit *h);
AvpStr query_const_line(AvpArena *a, const QueryConstHit *h);

/* struct_period(items): the smallest period p (2..16) of the items' (kind, size) sequence when it repeats at least
 * twice, else 0 (None). */
int query_struct_period(const ShsmAsmItem *items, size_t n);
/* CTYPE.get(kind, kind). */
const char *query_ctype(const char *kind);
