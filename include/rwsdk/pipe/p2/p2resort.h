#ifndef RWSDK_PIPE_P2_P2RESORT_H
#define RWSDK_PIPE_P2_P2RESORT_H

#include <rwsdk/plcore/batypes.h>

/*
 * One pending range of the radix partition's explicit stack: the first and last element of the
 * range and the partitioning bit to split it on next (12 bytes; the names are Gormiti's DWARF).
 */
typedef struct tagStackEntry {
    RwUInt8* l;
    RwUInt8* r;
    RwUInt32 bit;
} tagStackEntry;

/*
 * Sorts numElements elements of elementSize bytes by the 32-bit key at elementKeyOffset, given
 * the key range [keyLo, keyHi] of the elements (keyLo >= keyHi returns at once).
 */
void _rx_rxRadixExchangeSort(void* elements, RwUInt32 numElements, RwUInt32 elementSize,
                             RwUInt32 elementKeyOffset, RwUInt32 keyLo, RwUInt32 keyHi);

#endif
