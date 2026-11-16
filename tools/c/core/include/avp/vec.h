/* Typed dynamic arrays (heap-backed, grow by doubling). A vector is a struct { T *v; size_t n, cap; }; a zeroed
 * one is empty and valid. The macros take a pointer to the vector.
 *     AvpStrVec names = {0};
 *     avp_vec_push(&names, avp_str("x"));
 *     for (size_t i = 0; i < names.n; i++) use(names.v[i]);
 * Vectors are not thread-safe; free them with avp_vec_free (or never, in a short-lived tool). */
#pragma once
#include "base.h"

#define AVP_VEC(T) \
    struct {       \
        T *v;      \
        size_t n;  \
        size_t cap; \
    }

/* Makes room for need elements of size elem (internal). */
void avp_vec__grow(void **v, size_t *cap, size_t need, size_t elem);

#define avp_vec_reserve(vp, count) \
    avp_vec__grow((void **)&(vp)->v, &(vp)->cap, (count), sizeof(*(vp)->v))
#define avp_vec_push(vp, x) \
    (avp_vec_reserve((vp), (vp)->n + 1), (vp)->v[(vp)->n++] = (x))
/* Appends a zeroed element and returns a pointer to it. */
#define avp_vec_push_zero(vp) \
    (avp_vec_reserve((vp), (vp)->n + 1), memset(&(vp)->v[(vp)->n], 0, sizeof(*(vp)->v)), &(vp)->v[(vp)->n++])
#define avp_vec_pop(vp) ((vp)->v[--(vp)->n])
#define avp_vec_last(vp) ((vp)->v[(vp)->n - 1])
#define avp_vec_clear(vp) ((vp)->n = 0)
#define avp_vec_free(vp) (avp_xfree((vp)->v), (vp)->v = NULL, (vp)->n = (vp)->cap = 0)
/* Python list.insert(i, x) for 0 <= i <= n, and del list[i]. */
#define avp_vec_insert(vp, i, x)                                                                   \
    (avp_vec_reserve((vp), (vp)->n + 1),                                                            \
     memmove(&(vp)->v[(i) + 1], &(vp)->v[(i)], ((vp)->n - (size_t)(i)) * sizeof(*(vp)->v)),          \
     (vp)->v[(i)] = (x), (vp)->n++)
#define avp_vec_remove(vp, i)                                                                       \
    (memmove(&(vp)->v[(i)], &(vp)->v[(i) + 1], ((vp)->n - (size_t)(i) - 1) * sizeof(*(vp)->v)), (vp)->n--)
/* Appends count elements from the array src. */
#define avp_vec_extend(vp, src, count)                                                              \
    (avp_vec_reserve((vp), (vp)->n + (count)),                                                      \
     memcpy(&(vp)->v[(vp)->n], (src), (size_t)(count) * sizeof(*(vp)->v)), (vp)->n += (count))

typedef AVP_VEC(u8) AvpBytes;
typedef AVP_VEC(i32) AvpI32Vec;
typedef AVP_VEC(u32) AvpU32Vec;
typedef AVP_VEC(i64) AvpI64Vec;
typedef AVP_VEC(u64) AvpU64Vec;
typedef AVP_VEC(double) AvpF64Vec;
typedef AVP_VEC(void *) AvpPtrVec;
typedef AVP_VEC(char *) AvpCzVec; /* NUL-terminated strings */
/* AvpStrVec (a vector of AvpStr views) is in str.h. */
