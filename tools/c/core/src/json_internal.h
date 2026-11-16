/* Private to json.c, json_read.c and json_write.c. */
#pragma once
#include "avp/json.h"

/* The member index of an object: an open-addressing table of member numbers + 1 (0 is empty). */
u32 avp_json__hash(AvpStr key);
void avp_json__index_put(u32 *tab, u32 cap, u32 hash, u32 member_number);
/* Looks key up in members[base .. base + n) through tab (cap slots); tab may be NULL for a linear scan.
 * Returns the member number (relative to base) or -1. */
i64 avp_json__find(const AvpJsonMember *members, size_t n, const u32 *tab, u32 cap, AvpStr key);
/* The number of members above which an object gets an index. */
#define AVP_JSON_LINEAR_MAX 8
u32 avp_json__index_cap_for(size_t n);

/* digits of an int ("-?digits"): true and the value when it fits an i64 */
bool avp_json__int_fits(AvpStr digits, i64 *out);

extern const AvpJson avp_json__null_node, avp_json__true_node, avp_json__false_node;

/* UTF-8 encoding of a code point, including lone surrogates (generalized UTF-8). */
size_t avp_json__put_cp(char out[4], u32 cp);
