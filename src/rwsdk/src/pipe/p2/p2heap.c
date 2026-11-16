// Unit name and boundary: Gormiti link map (RwCore.a p2heap.obj). Locals: Gormiti DWARF.
/*
 * p2heap.c: the pipeline heap (RxHeap), the temporary store of the clusters' data while a pipeline
 * executes. Only create, destroy, reset and free have copies in retail; the allocation side is
 * dead-stripped.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/pipe/p2/p2heap.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>

#define rxHEAPGROWBLOCKS 0x20       /* entries the free-block array grows by (name invented) */

/*
 * Hands out the next entry of the heap's array of free blocks, growing the array first when it is
 * full. A grown array may have moved, so every entry in use is pointed back at by its block's
 * header. Returns NULL when the array can't grow.
 */
static rxHeapFreeBlock* HeapFreeBlocksNewEntry(RxHeap* heap) {
    rxHeapFreeBlock* freeBlocks = heap->freeBlocks;
    RwUInt32 entriesUsed = heap->entriesUsed;

    if (heap->entriesAlloced <= entriesUsed) {
        freeBlocks = (rxHeapFreeBlock*)RwRealloc(
            freeBlocks, (heap->entriesAlloced += rxHEAPGROWBLOCKS) * sizeof(rxHeapFreeBlock),
            rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_PIPEMODULE);
        if (!freeBlocks) {
            RWERROR((E_RW_NOMEM, heap->entriesAlloced * sizeof(rxHeapFreeBlock)));
            heap->entriesAlloced -= rxHEAPGROWBLOCKS;
        } else {
            if (freeBlocks != heap->freeBlocks && entriesUsed) {
                rxHeapFreeBlock* iter = freeBlocks;

                do {
                    iter->ptr->freeEntry = iter;
                    iter++;
                } while (--entriesUsed);
            }
            heap->freeBlocks = freeBlocks;
        }
    }

    if (freeBlocks) {
        freeBlocks += heap->entriesUsed++;
    }
    return freeBlocks;
} // 0x8029E368

/*
 * Gives back an entry of the heap's array of free blocks: the last entry in use moves into its
 * place (its block's header pointed back at it), and the array shrinks by one.
 */
static void HeapFreeBlocksDeleteEntry(RxHeap* heap, rxHeapFreeBlock* freeEntry) {
    RWASSERT(heap);

    if (&heap->freeBlocks[heap->entriesUsed - 1] != freeEntry) {
        *freeEntry = heap->freeBlocks[heap->entriesUsed - 1];
        freeEntry->ptr->freeEntry = freeEntry;
    }
    heap->entriesUsed--;
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2heap.obj), absent from the Final map

/*
 * Allocates a super block of size bytes (at least 128) behind its descriptor, with room to align
 * its start to 128 bytes. Returns NULL if the memory is not to be had.
 */
static rxHeapSuperBlockDescriptor* HeapSuperBlockCreate(RwUInt32 size) {
    rxHeapSuperBlockDescriptor* superBlock;

    RWASSERT(size >= 128);

    superBlock = (rxHeapSuperBlockDescriptor*)RwMalloc(
        size + sizeof(rxHeapSuperBlockDescriptor) + 127,
        rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_PIPEMODULE);
    if (superBlock) {
        superBlock->start = (void*)(((RwUInt32)(superBlock + 1) + 127) & ~127);
        superBlock->size = size;
        superBlock->next = NULL;
    }
    return superBlock;
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2heap.obj), absent from the Final map

/*
 * Frees a super block. A NULL super block is ignored.
 */
static void HeapSuperBlockDestroy(rxHeapSuperBlockDescriptor* superBlock) {
    if (superBlock) {
        RwFree(superBlock);
    }
} // no copy in retail (inlined): a static in the Gormiti Debug map (p2heap.obj), absent from the Final map

/*
 * Lays a super block out as an end header, one free block that fills the rest, and a start header,
 * and links its end header behind the previous super block's. The free block gets an entry in the
 * heap's array. Returns FALSE when no entry is left.
 */
static int HeapSuperBlockReset(rxHeapSuperBlockDescriptor* superBlock,
                               rxHeapSuperBlockDescriptor* attach2SuperBlock, RxHeap* heap) {
    rxHeapFreeBlock* freeEntry = HeapFreeBlocksNewEntry(heap);

    if (freeEntry) {
        rxHeapBlockHeader* blockHdrBeg = (rxHeapBlockHeader*)superBlock->start;
        rxHeapBlockHeader* blockHdrPrincipal = blockHdrBeg + 1;
        rxHeapBlockHeader* blockHdrEnd =
            (rxHeapBlockHeader*)((RwUInt8*)blockHdrBeg + superBlock->size) - 1;

        blockHdrBeg->prev = NULL;
        blockHdrBeg->next = NULL;
        blockHdrBeg->size = 0;
        blockHdrBeg->freeEntry = NULL;
        *blockHdrEnd = *blockHdrBeg;

        blockHdrBeg->next = blockHdrPrincipal;
        blockHdrPrincipal->prev = blockHdrBeg;
        blockHdrPrincipal->next = blockHdrEnd;
        blockHdrEnd->prev = blockHdrPrincipal;
        blockHdrPrincipal->size = (RwUInt8*)blockHdrEnd - (RwUInt8*)(blockHdrPrincipal + 1);
        blockHdrPrincipal->freeEntry = freeEntry;
        freeEntry->ptr = blockHdrPrincipal;
        freeEntry->size = blockHdrPrincipal->size;

        if (attach2SuperBlock) {
            rxHeapBlockHeader* blockHdrAttach2 =
                (rxHeapBlockHeader*)((RwUInt8*)attach2SuperBlock->start + attach2SuperBlock->size) - 1;

            blockHdrAttach2->next = blockHdrBeg;
            blockHdrBeg->prev = blockHdrAttach2;
        }
        return TRUE;
    }
    return FALSE;
} // 0x8029E474

/*
 * Returns a block to the heap. A neighbour that is free is merged with it (three blocks into the
 * previous one when both are); a block with no free neighbour gets a free-block entry of its own.
 */
void RxHeapFree(RxHeap* heap, void* block) {
    rxHeapBlockHeader* blockHdr = (rxHeapBlockHeader*)block - 1;
    int mergeBack = blockHdr->prev && blockHdr->prev->freeEntry;
    int mergeFwd = blockHdr->next && blockHdr->next->freeEntry;

    if (mergeBack) {
        if (mergeFwd) {
            HeapFreeBlocksDeleteEntry(heap, blockHdr->next->freeEntry);

            blockHdr->prev->size +=
                blockHdr->size + blockHdr->next->size + 2 * sizeof(rxHeapBlockHeader);
            blockHdr->prev->freeEntry->size = blockHdr->prev->size;
            blockHdr->prev->next = blockHdr->next->next;
            if (blockHdr->next->next) {
                blockHdr->next->next->prev = blockHdr->prev;
            }
        } else {
            blockHdr->prev->size += blockHdr->size + sizeof(rxHeapBlockHeader);
            blockHdr->prev->freeEntry->size = blockHdr->prev->size;
            blockHdr->prev->next = blockHdr->next;
            if (blockHdr->next) {
                blockHdr->next->prev = blockHdr->prev;
            }
        }
    } else if (mergeFwd) {
        blockHdr->size += blockHdr->next->size + sizeof(rxHeapBlockHeader);
        blockHdr->freeEntry = blockHdr->next->freeEntry;
        blockHdr->next->freeEntry->ptr = blockHdr;
        blockHdr->next->freeEntry->size = blockHdr->size;
        blockHdr->next = blockHdr->next->next;
        if (blockHdr->next) {
            blockHdr->next->prev = blockHdr;
        }
    } else {
        rxHeapFreeBlock* freeEntry = HeapFreeBlocksNewEntry(heap);

        if (freeEntry) {
            freeEntry->ptr = blockHdr;
            freeEntry->size = blockHdr->size;
            blockHdr->freeEntry = freeEntry;
        }
    }
} // 0x8029E570

/*
 * Makes every block of the heap free again: each super block is laid out afresh, chained behind
 * the one before, and the free-block array starts empty. Returns FALSE if an entry can't be had.
 */
RwBool _rxHeapReset(RxHeap* heap) {
    rxHeapSuperBlockDescriptor* prev = NULL;
    rxHeapSuperBlockDescriptor* iter;

    heap->entriesUsed = 0;

    iter = heap->head->next;
    while (iter) {
        if (!HeapSuperBlockReset(iter, prev, heap)) {
            return FALSE;
        }
        if (!prev) {
            heap->headBlock = (rxHeapBlockHeader*)iter->start;
        }
        prev = iter;
        iter = iter->next;
    }

    iter = heap->head;
    if (!HeapSuperBlockReset(iter, prev, heap)) {
        return FALSE;
    }
    if (!prev) {
        heap->headBlock = (rxHeapBlockHeader*)iter->start;
    }

    heap->dirty = FALSE;
    return TRUE;
} // 0x8029E748

/*
 * Frees the heap, its free-block array and every super block. A NULL heap is ignored.
 */
void RxHeapDestroy(RxHeap* heap) {
    if (heap) {
        rxHeapSuperBlockDescriptor* superBlock;

        if (heap->freeBlocks) {
            RwFree(heap->freeBlocks);
            heap->freeBlocks = NULL;
        }

        superBlock = heap->head;
        while (superBlock) {
            rxHeapSuperBlockDescriptor* next = superBlock->next;

            HeapSuperBlockDestroy(superBlock);
            superBlock = next;
        }

        RwFree(heap);
        heap = NULL;
    }
} // 0x8029E814

/*
 * Creates a heap of one super block of at least size bytes (rounded up to 32, with a floor of
 * 128), already reset. Returns NULL if memory or a free-block entry is not to be had.
 */
RxHeap* RxHeapCreate(RwUInt32 size) {
    RxHeap* heap;

    if (size < 1024) {
        size = 1024;
    }

    heap = (RxHeap*)RwMalloc(sizeof(RxHeap), rwMEMHINTDUR_GLOBAL | rwID_PIPEMODULE);
    if (heap) {
        rxHeapSuperBlockDescriptor* superBlock;

        size = (size + 31) & ~31;
        if (size < 128) {
            size = 128;
        }

        superBlock = HeapSuperBlockCreate(size);
        if (superBlock) {
            heap->superBlockSize = size;
            heap->head = superBlock;
            heap->freeBlocks = NULL;
            heap->entriesAlloced = 0;
            heap->entriesUsed = 0;
            heap->dirty = TRUE;

            if (RxHeapReset(heap)) {
                return heap;
            }
            HeapSuperBlockDestroy(superBlock);
        }
        RwFree(heap);
    }
    return NULL;
} // 0x8029E8B4
