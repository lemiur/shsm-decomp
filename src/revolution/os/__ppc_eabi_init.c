#include <revolution/base.h>
#include <revolution/os.h>
#include <revolution/os/__ppc_eabi_init.h>
#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>

static void __init_cpp(void);

/*
 * __init_hardware
 *
 * Called from __start: enables the FPU, then sets up paired singles, the
 * FPRs and the caches. Written in assembly because it runs before the
 * stack is usable (the return address is kept in r31).
 */
// ORIGINAL-ASM: an asm function in the SDK __ppc_eabi_init.c (public ogws reconstruction has the same body)
__declspec(section ".init") asm void __init_hardware(void) {
    nofralloc
    mfmsr r0
    ori r0, r0, 0x2000      // MSR[FP]
    mtmsr r0
    mflr r31
    bl __OSPSInit
    bl __OSFPRInit
    bl __OSCacheInit
    mtlr r31
    blr
} // 0x800065C0

/*
 * __flush_cache
 *
 * Writes back and invalidates the cache blocks covering a range of code
 * copied in by __start.
 */
// ORIGINAL-ASM: an asm function in the SDK __ppc_eabi_init.c (public ogws reconstruction has the same body)
__declspec(section ".init") asm void __flush_cache(register void* address, register unsigned int size) {
    nofralloc
    lis r5, 0xFFFF
    ori r5, r5, 0xFFF1
    and r5, r5, address
    subf r3, r5, address
    add size, size, r3
loop:
    dcbst r0, r5
    sync
    icbi r0, r5
    addic r5, r5, 8
    subic. size, size, 8
    bge loop
    isync
    blr
} // 0x800065F0

/*
 * __init_user
 *
 * Called from __start before main. An asm function in the SDK: it keeps a
 * frame and calls __init_cpp with bl, where a C body is compiled to a tail
 * branch.
 */
// ORIGINAL-ASM: an asm function in the SDK __ppc_eabi_init.c (public ogws reconstruction has the same body)
asm void __init_user(void) {
    fralloc
    bl __init_cpp
    frfree
    blr
} // 0x8021E8B0

/*
 * __init_cpp
 *
 * Runs the static constructors listed in .ctors.
 */
static void __init_cpp(void) {
    voidfunctionptr* constructor;

    for (constructor = _ctors; *constructor; constructor++) {
        (*constructor)();
    }
} // 0x8021E8D0

/*
 * exit
 *
 * Runs the static destructors listed in .dtors and halts; there is no
 * process to return to.
 */
__declspec(weak) void exit(int status) {
    voidfunctionptr* destructor;

    for (destructor = _dtors; *destructor; destructor++) {
        (*destructor)();
    }

    PPCHalt();
} // 0x8021E920
