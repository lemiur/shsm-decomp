// Unit name and boundary: Gormiti link map (RwCore.a p2resort.obj). Locals: Gormiti DWARF.
#include <rwsdk/pipe/p2/p2resort.h>

/*
 * Exchanges the elements at l and r one 32-bit word at a time, elementSize bytes each. The
 * macro name is invented; Gormiti's line table puts the whole exchange on one source line
 * at every use. Its DWARF lists the block's locals _l, _r, _elementSize, t0 and t1; they are
 * declared in that order (the DWARF lists a block's locals in reverse declaration order).
 */
#define RX_SWAP_ELEMENTS(l, r, elementSize)                 \
{                                                           \
    RwUInt8* _l = (l);                                      \
    RwUInt8* _r = (r);                                      \
    RwUInt32 _elementSize = (elementSize);                  \
    while (_elementSize >= 4) {                             \
        RwUInt32 t1 = *(RwUInt32*)_r;                       \
        RwUInt32 t0 = *(RwUInt32*)_l;                       \
        *(RwUInt32*)_r = t0;                                \
        *(RwUInt32*)_l = t1;                                \
        _l += 4;                                            \
        _r += 4;                                            \
        _elementSize -= 4;                                  \
    }                                                       \
}

/*
 * Radix partition of the elements in [l, r] on partitioningBit, with an explicit stack of the
 * ranges still to split. A range of five elements or fewer is left for _insertionsort.
 */
static void _repartition(RwUInt8* l, RwUInt8* r, RwUInt32 elementSize, RwUInt32 elementKeyOffset,
                         RwUInt32 partitioningBit) {
    tagStackEntry stack[32];
    tagStackEntry* stackptr;

    stackptr = stack;
    stackptr->l = l;
    stackptr->r = r;
    stackptr->bit = partitioningBit;
    stackptr++;
    do {
        RwUInt8* savel;
        RwUInt8* saver;

        stackptr--;
        l = stackptr->l;
        r = stackptr->r;
        partitioningBit = stackptr->bit;
        do {
            savel = l;
            saver = r;

            do {
                while (!(*(RwUInt32*)(l + elementKeyOffset) & partitioningBit)) {
                    l += elementSize;
                    if (l > r) {
                        goto partitioned;
                    }
                }
                while (*(RwUInt32*)(r + elementKeyOffset) & partitioningBit) {
                    r -= elementSize;
                    if (l > r) {
                        goto partitioned;
                    }
                }
                RX_SWAP_ELEMENTS(l, r, elementSize);
                l += elementSize;
                r -= elementSize;
            } while (l <= r);

        partitioned:
            partitioningBit >>= 1;
            if (partitioningBit == 0) {
                break;
            }
            {
                RwUInt8* defer_l = r + elementSize;
                RwUInt8* defer_r = saver;

                if (defer_r >= defer_l + 5 * elementSize) {
                    stackptr->l = defer_l;
                    stackptr->r = defer_r;
                    stackptr->bit = partitioningBit;
                    stackptr++;
                }
            }
            r = l - elementSize;
            l = savel;
        } while (r >= savel + 5 * elementSize);
    } while (stackptr != stack);
} // 0x8029EB3C

/*
 * Insertion sort of the elements, which are nearly sorted by the radix partition: each element
 * moves left past every larger key. The first element holds the least key (the caller moves it there).
 *
 * Each loop steps its pointer in its own test, a comma expression: the Gormiti Debug build
 * evaluates both steps in the test blocks and keeps each test's value in a temporary.
 */
static void _insertionsort(RwUInt8* elements, RwUInt32 numElements, RwUInt32 elementSize,
                           RwUInt32 elementKeyOffset) {
    while (elements += elementSize, --numElements) {
        RwUInt32 keyToPlace = *(RwUInt32*)(elements + elementKeyOffset);
        RwUInt8* p = elements;

        while (p -= elementSize, *(RwUInt32*)(p + elementKeyOffset) > keyToPlace) {
            RX_SWAP_ELEMENTS(p, p + elementSize, elementSize);
        }
    }
} // 0x8029ED2C

/*
 * Sorts the elements by their 32-bit key. The partition runs only for more than five elements;
 * the least key among the first five is then moved to element 0 and the insertion sort finishes.
 */
void _rx_rxRadixExchangeSort(void* elements, RwUInt32 numElements, RwUInt32 elementSize,
                             RwUInt32 elementKeyOffset, RwUInt32 keyLo, RwUInt32 keyHi) {
    if (elements != NULL && elementKeyOffset + 4 <= elementSize && keyLo < keyHi) {
        if (numElements > 5) {
            RwInt32 bit;

            if (keyHi != 0) {
                bit = 0;
                while (keyHi >>= 1) {
                    bit++;
                }
            } else {
                bit = -1;
            }
            _repartition((RwUInt8*)elements,
                         (RwUInt8*)elements + (numElements - 1) * elementSize,
                         elementSize, elementKeyOffset, 1U << bit);
        }
        if (numElements > 1) {
            RwUInt32 i = 4;
            RwUInt32 minKeyIndex, minKey;

            if (numElements - 1 < i) {
                i = numElements - 1;
            }
            minKey = *(RwUInt32*)((RwUInt8*)elements + i * elementSize + elementKeyOffset);
            minKeyIndex = i;
            i--;
            do {
                RwUInt32 key = *(RwUInt32*)((RwUInt8*)elements + i * elementSize + elementKeyOffset);

                if (key < minKey) {
                    minKey = key;
                    minKeyIndex = i;
                }
            } while (i--);
            if (minKeyIndex != 0) {
                RX_SWAP_ELEMENTS(elements, (RwUInt8*)elements + minKeyIndex * elementSize, elementSize);
            }
            _insertionsort(elements, numElements, elementSize, elementKeyOffset);
        }
    }
} // 0x8029EEB0
