/* query.py's uses of tools/common/mangle.py (parse for is_placeholder and notes, mangle_scope for vtable), on the
 * shared demangler and mangler (lib/shsm_mangle.h). */
#include "query_b.h"

#include "shsm_mangle.h"

/* mangle.parse(name).name: the base name as written in source, or p NULL when name does not parse. Raises as
 * mangle.parse does (ValueError, RecursionError). */
AvpStr query_mangle_base_name(AvpArena *a, AvpStr name) {
    const ShsmDemangled *d = shsm_mangle_parse(a, avp_cz(a, name));
    return d ? avp_str(d->name) : (AvpStr){NULL, 0};
}

/* mangle.parse(name) for notes: false when name does not parse; else the base name and the scope parts (as
 * Demangled.scope). Raises as mangle.parse does. */
bool query_mangle_parse_scope(AvpArena *a, AvpStr name, AvpStr *base, const AvpStr **scope, size_t *nscope) {
    const ShsmDemangled *d = shsm_mangle_parse(a, avp_cz(a, name));
    if (!d) return false;
    AvpStr *parts = AVP_NEWN(a, AvpStr, d->nscope + 1);
    for (size_t i = 0; i < d->nscope; i++) parts[i] = avp_str(d->scope[i]);
    *base = avp_str(d->name);
    *scope = parts;
    *nscope = d->nscope;
    return true;
}

/* mangle.mangle_scope(parts) (the list form). */
AvpStr query_mangle_scope(AvpArena *a, const AvpStr *parts, size_t n) {
    const char **cz = AVP_NEWN(a, const char *, n + 1);
    for (size_t i = 0; i < n; i++) cz[i] = avp_cz(a, parts[i]);
    return avp_str(shsm_mangle_scope_parts(a, cz, n));
}
