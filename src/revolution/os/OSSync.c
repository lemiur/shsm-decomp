#include <revolution/os.h>
#include <string.h>

/* Low-memory system call exception vector. */
#define OS_SYSTEM_CALL_VECTOR_ADDR 0x0C00
#define OS_EXCEPTION_VECTOR_SIZE   0x100

/*
 * SystemCallVector
 *
 * System call exception handler, copied to the vector by
 * __OSInitSystemCall: synchronises the caches through HID0[ABE] and
 * returns.
 */
// ORIGINAL-ASM: exception vector body with entry labels; an asm function in the public RVL SDK OSSync.c
static asm void SystemCallVector(void) {
    nofralloc
entry __OSSystemCallVectorStart
    mfspr r9, HID0
    ori r10, r9, 0x8
    mtspr HID0, r10
    isync
    sync
    mtspr HID0, r9
    rfi
entry __OSSystemCallVectorEnd
    nop
} // 0x8021A120

/*
 * __OSInitSystemCall
 *
 * Installs SystemCallVector at the system call exception vector.
 */
void __OSInitSystemCall(void) {
    void* addr;

    addr = OSPhysicalToCached(OS_SYSTEM_CALL_VECTOR_ADDR);
    memcpy(addr, __OSSystemCallVectorStart, (u32)__OSSystemCallVectorEnd - (u32)__OSSystemCallVectorStart);
    DCFlushRangeNoSync(addr, OS_EXCEPTION_VECTOR_SIZE);
    __sync();
    ICInvalidateRange(addr, OS_EXCEPTION_VECTOR_SIZE);
} // 0x8021A140
