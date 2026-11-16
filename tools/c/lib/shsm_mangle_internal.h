/* Shared by shsm_mangle.c (demangling) and shsm_mangle_encode.c (mangling); not part of the module's API. */
#pragma once
#include "avp/str.h"

/* mangle._OPS as {code, "operator<spelling>"} rows, in the Python dict's order. */
extern const char *const SHSM_MANGLE_OPS[][2];
extern const size_t SHSM_MANGLE_NOPS;

/* re.fullmatch(r"-?\d+", s) */
bool shsm_mangle_is_int_literal(AvpStr s);
/* _split_targs: the pieces of s cut at commas outside <...> (heap vector: avp_vec_free it). */
AvpStrVec shsm_mangle_split_targs(AvpStr s);
