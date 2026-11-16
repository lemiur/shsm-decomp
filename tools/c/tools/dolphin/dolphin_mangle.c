/* mangle.parse(name) reduced to what default_signature reads, on the shared demangler (lib/shsm_mangle.h). */
#include "dolphin_priv.h"

#include "shsm_mangle.h"

bool dolphin_mangle_parse(AvpArena *a, const char *mangled, DolphinParsed *out) {
    const ShsmDemangled *d = shsm_mangle_parse(a, mangled);
    if (!d) return false;
    out->function = !d->is_data;
    out->nscope = d->nscope;
    out->params = AVP_NEWN(a, AvpStr, d->nparams + 1);
    for (size_t i = 0; i < d->nparams; i++) out->params[i] = avp_str(d->params[i]);
    out->nparams = d->nparams;
    return true;
}
