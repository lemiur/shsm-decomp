#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>
#include <PowerPC_EABI_Support/Runtime/NMWException.h>

#pragma exceptions off
#pragma internal on

extern "C" {
void __init_cpp_exceptions(void);
void __fini_cpp_exceptions(void);
}

static int fragmentID = -2;

/* Reads the small-data base (TOC) register r2, which C cannot name. */
static inline char* GetR2(void) {
    register char* toc;

    asm { mr toc, r2 }
    return toc;
}

/*
 * __init_cpp_exceptions
 *
 * Registers this module's exception table index with the runtime.
 */
extern void __init_cpp_exceptions(void) {
    char* R2;

    if (fragmentID == -2) {
        R2 = GetR2();
        fragmentID = __register_fragment(_eti_init_info, R2);
    }
} // 0x801A6FD4

/*
 * __fini_cpp_exceptions
 */
extern void __fini_cpp_exceptions(void) {
    if (fragmentID != -2) {
        __unregister_fragment(fragmentID);
        fragmentID = -2;
    }
} // 0x801A7010

#pragma section ".ctors$10"
__declspec(section ".ctors$10") extern void* const __init_cpp_exceptions_reference = __init_cpp_exceptions;
#pragma section ".dtors$10"
__declspec(section ".dtors$10") extern void* const __destroy_global_chain_reference = __destroy_global_chain;
#pragma section ".dtors$15"
__declspec(section ".dtors$15") extern void* const __fini_cpp_exceptions_reference = __fini_cpp_exceptions;
