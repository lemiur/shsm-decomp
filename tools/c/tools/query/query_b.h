/* query.py addr, unit, vtable: declarations shared between their files (the notes search is query_notes.h). */
#pragma once
#include "query.h"

/* mangle.parse(name) (query_mangle.c): false when name does not parse; else its base name and scope parts. */
bool query_mangle_parse_scope(AvpArena *a, AvpStr name, AvpStr *base, const AvpStr **scope, size_t *nscope);

/* section_of(ctx, address): the section of the symbol holding address, else of the last symbol before it; p NULL:
 * None. */
AvpStr query_section_of(QueryCtx *ctx, i64 address);

/* load_classes(ctx): the "classes" list of build/<ver>/classes.json, NULL when it cannot be read or parsed (None). */
const AvpJson *query_load_classes(QueryCtx *ctx);

/* mangle.mangle_scope(parts) (the list form, parts already split): '<len><name>' or 'Q<n>...'. Raises
 * SHSM_EXC_VALUE where the Python raises ValueError (a scope deeper than 9, an unsupported template argument type). */
AvpStr query_mangle_scope(AvpArena *a, const AvpStr *parts, size_t n);

/* ctx.syms with splits.txt attached (load_symbols(repo)), cached in ctx->extra[6]. query.h's query_syms loads it
 * without the splits, so its unit_of is always None; addr and notes use this one. */
ShsmSymbolTable *query_syms_split(QueryCtx *ctx);
