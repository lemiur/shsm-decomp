/* Hash maps and sets with Python dict iteration order: insertion order; updating an existing key keeps its
 * position; deleting a key and inserting it again puts it at the end. Keys are byte strings (AvpMap, AvpSet; the
 * map keeps its own copy of each key) or 64-bit integers (AvpIMap, AvpISet). Values are an AvpVal union, so a map
 * holds a pointer, an integer or a double without a cast dance.
 *
 *     AvpMap *m = avp_map_new();
 *     bool created;
 *     avp_map_put(m, key, &created)->i += 1;              // Counter / defaultdict(int)
 *     AvpVal *v = avp_map_get(m, key);                    // NULL when absent
 *     AvpMapIter it = {0};
 *     while (avp_map_next(m, &it)) use(it.key, it.val);   // insertion order
 *
 * Pointers returned by get/put stay valid until the next put or del on the same map. Not thread-safe.
 *
 * AvpPyIntSet reproduces CPython 3.14's set of ints, including its iteration order (the table size, the
 * linear probes and the perturbation of setobject.c): use it only where a Python set of ints is iterated into an
 * output (mark the site with a py: set order comment). Its hash is CPython's hash(int) (the value modulo 2**61-1,
 * -1 mapped to -2). */
#pragma once
#include "str.h"

typedef union {
    void *p;
    i64 i;
    u64 u;
    double f;
} AvpVal;

/* ---- string keys ---- */
typedef struct AvpMap AvpMap;
typedef struct {
    size_t pos; /* internal cursor: zero it to start */
    AvpStr key;
    AvpVal *val;
} AvpMapIter;

AvpMap *avp_map_new(void);
AvpMap *avp_map_new_cap(size_t expected);
void avp_map_free(AvpMap *m);
void avp_map_clear(AvpMap *m);
size_t avp_map_len(const AvpMap *m);
AvpVal *avp_map_get(const AvpMap *m, AvpStr key);
bool avp_map_has(const AvpMap *m, AvpStr key);
/* Returns the value slot of key, inserting it (value zeroed, *created = true) when absent. created may be NULL. */
AvpVal *avp_map_put(AvpMap *m, AvpStr key, bool *created);
bool avp_map_del(AvpMap *m, AvpStr key);
bool avp_map_next(const AvpMap *m, AvpMapIter *it);
/* The keys in insertion order (views into the map's own copies). */
AvpStrVec avp_map_keys(const AvpMap *m);

/* ---- integer keys ---- */
typedef struct AvpIMap AvpIMap;
typedef struct {
    size_t pos;
    u64 key;
    AvpVal *val;
} AvpIMapIter;

AvpIMap *avp_imap_new(void);
AvpIMap *avp_imap_new_cap(size_t expected);
void avp_imap_free(AvpIMap *m);
void avp_imap_clear(AvpIMap *m);
size_t avp_imap_len(const AvpIMap *m);
AvpVal *avp_imap_get(const AvpIMap *m, u64 key);
bool avp_imap_has(const AvpIMap *m, u64 key);
AvpVal *avp_imap_put(AvpIMap *m, u64 key, bool *created);
bool avp_imap_del(AvpIMap *m, u64 key);
bool avp_imap_next(const AvpIMap *m, AvpIMapIter *it);

/* ---- sets (insertion order) ---- */
typedef struct AvpSet AvpSet;   /* string members */
typedef struct AvpISet AvpISet; /* integer members */
AvpSet *avp_set_new(void);
void avp_set_free(AvpSet *s);
size_t avp_set_len(const AvpSet *s);
bool avp_set_add(AvpSet *s, AvpStr member); /* true when it was new */
bool avp_set_has(const AvpSet *s, AvpStr member);
bool avp_set_del(AvpSet *s, AvpStr member);
bool avp_set_next(const AvpSet *s, size_t *pos, AvpStr *member);
AvpStrVec avp_set_sorted(const AvpSet *s); /* sorted(s) */

AvpISet *avp_iset_new(void);
void avp_iset_free(AvpISet *s);
size_t avp_iset_len(const AvpISet *s);
bool avp_iset_add(AvpISet *s, u64 member);
bool avp_iset_has(const AvpISet *s, u64 member);
bool avp_iset_del(AvpISet *s, u64 member);
bool avp_iset_next(const AvpISet *s, size_t *pos, u64 *member);
AvpU64Vec avp_iset_sorted(const AvpISet *s);

/* ---- CPython's set of ints, iteration order included ---- */
typedef struct AvpPyIntSet AvpPyIntSet;
AvpPyIntSet *avp_pyintset_new(void);
void avp_pyintset_free(AvpPyIntSet *s);
size_t avp_pyintset_len(const AvpPyIntSet *s);
bool avp_pyintset_add(AvpPyIntSet *s, i64 member);
bool avp_pyintset_has(const AvpPyIntSet *s, i64 member);
bool avp_pyintset_discard(AvpPyIntSet *s, i64 member);
bool avp_pyintset_next(const AvpPyIntSet *s, size_t *pos, i64 *member); /* CPython's iteration order */
/* set(list) built from an array in order: for a list source CPython adds the items one by one, so this equals repeated
 * avp_pyintset_add (set(dict) and set(set) presize or copy differently and are not reproduced). */
AvpPyIntSet *avp_pyintset_from(const i64 *items, size_t count);
/* set(d) for a dict d with these keys in insertion order: CPython's set_update_internal presizes the table for a dict
 * source ((fill + len) * 5 >= mask * 3 -> one resize to (used + len) * 2), so the order differs from set(list) once the
 * dict passes the load limit. */
AvpPyIntSet *avp_pyintset_from_dict(const i64 *keys, size_t count);
/* set.copy(), set.update(other_set) and a | b for sets of ints, with CPython's set_merge: the result's table size and
 * therefore its iteration order are those of the Python set (the target is presized when the sum passes the load
 * limit, an empty target of the same size gets the table copied slot by slot). */
AvpPyIntSet *avp_pyintset_copy(const AvpPyIntSet *s);
void avp_pyintset_update(AvpPyIntSet *dst, const AvpPyIntSet *src);
AvpPyIntSet *avp_pyintset_or(const AvpPyIntSet *a, const AvpPyIntSet *b);
