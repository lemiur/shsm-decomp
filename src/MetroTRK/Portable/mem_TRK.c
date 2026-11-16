// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/mem_TRK.c
// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/mem_TRK.c
// (the retail bodies are xenoblade's word-access version, not tp's byte and word stores)

#include "TRK/mem_TRK.h"
#include "TRK/ppc_mem.h"

/* Copies size bytes from src to dst, front to back. */
void* TRK_memcpy(void* dst, const void* src, int size) {
    const u8* srcTemp = (const u8*)src;
    u8* dstTemp = (u8*)dst;
    int i;

    for (i = 0; i != size; i++) {
        ppc_writebyte1(dstTemp, ppc_readbyte1(srcTemp));
        srcTemp++;
        dstTemp++;
    }

    return dst;
} // 0x8019125C

/* Fills size bytes at dst with the low byte of val. */
void TRK_fill_mem(u8* dst, int val, int size) {
    u8 b = val;
    int i;

    for (i = 0; i != size; i++) {
        ppc_writebyte1(dst, b);
        dst++;
    }
} // no copy in retail (dead-stripped or inlined)

/* Fills size bytes at dst with the low byte of val; returns dst. */
void* TRK_memset(void* dst, int val, int size) {
    TRK_fill_mem(dst, val, size);
    return dst;
} // 0x80191398
