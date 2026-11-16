/* D.sym through lib/shsm_pe (the C port of tools/mwcc_trace/symbols.py), over the compiler the run uses. As the
 * Python, a table that fails to load is tried again at the next call (its hasattr test fails), and every failure
 * gives hex(addr). */
#include "wdbg_sym.h"
#include "shsm_pe.h"

static bool load_table(WdbgDebug *d, AvpArena *a) {
    if (d->syms) return true;
    const char *exe = d->exe ? d->exe
                             : avp_join(a, avp_path_normpath(a, avp_root()), "tools", "compilers", "Wii", "1.0", "mwcceppc.exe");
    AvpTry t;
    if (AVP_TRY(&t)) {
        d->syms = shsm_pe_symbols_load(avp_arena(), exe);
        avp_try_end(&t);
    } else {
        d->syms = NULL;
    }
    return d->syms != NULL;
}

AvpStr wdbg_sym(WdbgDebug *d, AvpArena *a, i64 addr) {
    AvpStr hex = avp_pyfmt(a, "%#x", (long long)addr);
    if (!load_table(d, a)) return hex;
    if (addr < 0 || addr > 0xffffffffLL) return hex;
    AvpStr name = {0};
    AvpTry t;
    if (AVP_TRY(&t)) {
        name = shsm_pe_symbols_describe(a, d->syms, (u32)addr);
        avp_try_end(&t);
    } else {
        return hex;
    }
    return name.p && name.n ? name : hex;
}

bool wdbg_sym_lookup(WdbgDebug *d, AvpArena *a, AvpStr name, i64 *addr) {
    if (!load_table(d, a)) return false;
    const ShsmPeSymbols *t = d->syms;
    for (size_t i = 0; i < t->nfuncs; i++)
        if (avp_str_eq(t->funcs[i].name, name)) {
            *addr = t->funcs[i].va;
            return true;
        }
    return false;
}
