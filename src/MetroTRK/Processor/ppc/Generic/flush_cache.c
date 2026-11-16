// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Processor/ppc/Generic/flush_cache.c

#include "TRK/flush_cache.h"

// ORIGINAL-ASM: cache-line loop (dcbst/dcbf/sync/icbi); an asm function with nofralloc in the public tp reconstruction
/* Rounds addr down (bits 1-3 cleared), widens size by the amount dropped and flushes the range 8 bytes at a time. */
asm void TRK_flush_cache(u32 addr, int size) {
    nofralloc
    lis r5, 0xFFFF
    ori r5, r5, 0xFFF1
    and r5, r5, r3
    subf r3, r5, r3
    add r4, r4, r3
loop:
    dcbst 0, r5
    dcbf 0, r5
    sync
    icbi 0, r5
    addic r5, r5, 8
    addic. r4, r4, -8
    bge loop
    isync
    blr
} // 0x801910FC
