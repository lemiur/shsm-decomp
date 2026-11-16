// Reference: doldecomp/ogws src/revolution/DB/db.c
#include <revolution/base.h>
#include <revolution/db.h>
#include <revolution/os.h>

BOOL DBVerbose;
DBInterface* __DBInterface;

/*
 * DBInit
 *
 * Points the debugger interface at its low-memory block and hooks the
 * debugger's exception entry.
 */
void DBInit(void) {
    __DBInterface = (DBInterface*)OSPhysicalToCached(DB_INTERFACE_PHYS_ADDR);
    __DBInterface->exceptionHook = (void (*)(void))OSCachedToPhysical(__DBExceptionDestination);
    DBVerbose = TRUE;
} // 0x8021E970

/*
 * __DBExceptionDestinationAux
 *
 * Reports the exception, dumps the interrupted context and halts.
 */
void __DBExceptionDestinationAux(void) {
    OSContext* context = (OSContext*)OSPhysicalToCached(*(u32*)DB_CURRENT_CONTEXT_PHYS_ADDR);

    OSReport("DBExceptionDestination\n");
    OSDumpContext(context);
    PPCHalt();
} // 0x8021E9A0

/*
 * __DBExceptionDestination
 *
 * Where the debugger's exception hook lands: turns address translation on,
 * then continues in __DBExceptionDestinationAux. The hook runs with the MMU
 * off and no stack, so it cannot be compiled C.
 */
// ORIGINAL-ASM: an asm function with nofralloc in the SDK db.c (the public ogws reconstruction has the same body)
asm void __DBExceptionDestination(void) {
    nofralloc
    mfmsr r3
    ori r3, r3, MSR_IR | MSR_DR
    mtmsr r3
    b __DBExceptionDestinationAux
} // 0x8021E9F0

BOOL __DBIsExceptionMarked(u8 exception) {
    return __DBInterface->exceptionMask & (1 << exception);
} // 0x8021EA00

/* The retail build has no debugger output: DBPrintf ignores its arguments. */
void DBPrintf(char* str, ...) {
} // 0x8021EA20
