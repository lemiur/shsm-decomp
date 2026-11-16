/* sibling: read-only lookups for other tools (brief): the sibling counterpart of a retail function and the sibling
 * object spans a retail address range covers. Nothing is created: without a database that holds pairs,
 * sibling_lookup_open returns NULL and callers print nothing. */
#pragma once
#include "sibling.h"

typedef struct SibLookup SibLookup;

/* The default sibling's database of the checkout (build/index/sibling/gormiti), or NULL when it is absent or holds no
 * pairs. Strings live in a. */
SibLookup *sibling_lookup_open(AvpArena *a, const ShsmRepo *repo);

typedef struct {
    const char *name, *demangled, *tier, *lib, *object; /* lib "" when the object is not from a library */
    i64 order;
} SibLookupFn;
/* The counterpart of the retail function at address; false when it has none. */
bool sibling_lookup_fn(SibLookup *lk, i64 address, SibLookupFn *out);

typedef struct {
    const char *name, *demangled, *lib, *object;
    i64 size, order;
    size_t count; /* sibling functions of that name (1: unambiguous) */
} SibLookupSame;
/* The sibling function that carries the retail function's real (mangled) name although its body is not an exact
 * pair (another revision); false for a placeholder name or no such function. */
bool sibling_lookup_same_named(SibLookup *lk, const char *name, SibLookupSame *out);

typedef struct {
    const char *lib, *object;
    i64 start, end;  /* the retail span of the object's paired members (tiers exact and exact-seq) */
    size_t paired;
    bool clean;      /* ordered, no other object's function inside, no overlap with a neighbour span */
    i64 unused;      /* members of the object the sibling's linker dead-stripped (UNUSED) */
    bool proposed;   /* `units` proposes a unit range for the span (it is clean) */
    i64 prop_start, prop_end, prop_pad_end; /* the proposed range; pad_end: with the alignment padding after it */
} SibLookupSpan;
typedef AVP_VEC(SibLookupSpan) SibLookupSpanVec;
/* The spans that overlap the retail range [start, end), in address order. */
SibLookupSpanVec sibling_lookup_range(SibLookup *lk, i64 start, i64 end);

/* All spans for the given pairing (what `units` prints); used by the lookup and by tests. */
SibLookupSpan *sibling_span_list(AvpArena *a, Sibling *sb, const SibFnVec *s, const SibFnVec *g, const SibArgs *args,
                                 size_t *n);
/* Link the peers of a pairing loaded from the database (peer addresses to indices). */
void sibling_link_peers(SibFnVec *s, SibFnVec *g);
