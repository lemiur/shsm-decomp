// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/include/PowerPC_EABI_Support/MetroTRK/ppc_mem.h

#ifndef TRK_PPC_MEM_H
#define TRK_PPC_MEM_H

#include <revolution/types.h>

/* Byte access through the aligned word that holds the byte, for memory that takes only word accesses
 * (big-endian lanes: byte 0 of a word is the most significant). */

/* Reads the byte at ptr. */
static u8 ppc_readbyte1(const u8* ptr) {
    u32* alignedPtr = (u32*)((u32)ptr & ~3);
    return (u8)(*alignedPtr >> ((3 - ((u32)ptr - (u32)alignedPtr)) << 3));
}

/* Writes val to ptr by merging it into the word that holds ptr. */
static void ppc_writebyte1(u8* ptr, u8 val) {
    u32* alignedPtr = (u32*)((u32)ptr & ~3);
    u32 word = *alignedPtr;
    u32 mask = 0xff << ((3 - ((u32)ptr - (u32)alignedPtr)) << 3);
    u32 shift = (3 - ((u32)ptr - (u32)alignedPtr)) << 3;

    *alignedPtr = (word & ~mask) | (mask & (val << shift));
}

#endif /* TRK_PPC_MEM_H */
