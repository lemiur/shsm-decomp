// unit name unconfirmed: tgsm/pbg lists a resmem.c here in the plcore link order (no source); no
// string evidence in this binary names the file.
/*
 * resmem.c: the resource heap. A heap manages a caller's memory area as a list of blocks in
 * address order, each behind a 32-byte header; allocation takes the first free block that is
 * large enough and splits off what is left, and freeing merges a block with its free neighbours.
 */
#include <rwsdk/plcore/resmem.h>

/* Splits block after size bytes; the rest becomes a free block, merged with a free next block. */
static void ResHeapSplitBlock(rwResHeapBlock* block, RwUInt32 size) {
    rwResHeapBlock* next = block->next;
    rwResHeapBlock* rest = (rwResHeapBlock*)((RwUInt8*)block + size + sizeof(rwResHeapBlock));

    if (next != NULL && (~next->flags & rwRESHEAPBLOCKUSED)) {
        rest->next = next->next;
        rest->size = block->next->size + (block->size - size);
    } else {
        rest->next = next;
        rest->size = block->size - size - sizeof(rwResHeapBlock);
    }

    block->next = rest;
    rest->flags = 0;
    rest->prev = block;
    if (rest->next != NULL) {
        rest->next->prev = rest;
    }

    block->size = size;
    rest->heap = block->heap;
} // 0x802A6A74

/* Makes the memory area ptr (size bytes) a heap of one free block; FALSE if it is too small. */
RwBool _rwResHeapInit(void* ptr, RwUInt32 size) {
    rwResHeap* heap = (rwResHeap*)ptr;
    rwResHeapBlock* block;
    RwInt32 blockSize;

    block = (rwResHeapBlock*)(((RwUInt32)heap + sizeof(rwResHeap) + rwRESHEAPALIGN - 1) &
                              ~(rwRESHEAPALIGN - 1));
    blockSize = (((RwUInt32)heap + size) & ~(rwRESHEAPALIGN - 1)) - (RwUInt32)block -
                sizeof(rwResHeapBlock);
    if (blockSize < rwRESHEAPALIGN) {
        return FALSE;
    }

    block->heap = heap;
    block->next = NULL;
    block->prev = NULL;
    block->flags = 0;
    block->size = blockSize;

    heap->firstBlock = block;
    heap->firstFree = block;

    return TRUE;
} // 0x802A6B00

/* Closes a heap; the memory area stays the caller's. */
RwBool _rwResHeapClose(void* ptr) {
    return TRUE;
} // 0x802A6B50

/* Frees the block whose memory is ptr, merging it with its free neighbours. */
void _rwResHeapFree(void* ptr) {
    rwResHeapBlock* block = (rwResHeapBlock*)ptr - 1;
    rwResHeap* heap;
    rwResHeapBlock* prev;
    rwResHeapBlock* next;

    block->flags = 0;
    heap = block->heap;
    prev = block->prev;
    next = block->next;

    if (heap->firstFree == NULL || block < heap->firstFree) {
        heap->firstFree = block;
    }

    if (prev != NULL && (~prev->flags & rwRESHEAPBLOCKUSED)) {
        prev->next = next;
        if (next != NULL) {
            next->prev = prev;
        }
        prev->size += block->size + sizeof(rwResHeapBlock);
        block = prev;
    }

    if (next != NULL && (~next->flags & rwRESHEAPBLOCKUSED)) {
        block->next = next->next;
        if (next->next != NULL) {
            next->next->prev = block;
        }
        block->size += next->size + sizeof(rwResHeapBlock);
    }
} // 0x802A6B58

/* Allocates size bytes from a heap: the first free block large enough; NULL if there is none. */
void* _rwResHeapAlloc(void* ptr, RwUInt32 size) {
    rwResHeap* heap = (rwResHeap*)ptr;
    rwResHeapBlock* found = NULL;
    rwResHeapBlock* block;

    size += rwRESHEAPALIGN - 1;
    size &= ~(rwRESHEAPALIGN - 1);

    for (block = heap->firstFree; block != NULL && found == NULL; block = block->next) {
        if ((~block->flags & rwRESHEAPBLOCKUSED) && block->size >= size) {
            found = block;
        }
    }

    if (found == NULL) {
        return NULL;
    }

    /* Split only when the rest can hold a header and a block of its own. */
    if (found->size > size + sizeof(rwResHeapBlock) + rwRESHEAPALIGN) {
        ResHeapSplitBlock(found, size);
    }

    if (found == heap->firstFree) {
        do {
            heap->firstFree = heap->firstFree->next;
        } while (heap->firstFree != NULL && (heap->firstFree->flags & rwRESHEAPBLOCKUSED));
    }

    found->flags = rwRESHEAPBLOCKUSED;

    return found + 1;
} // 0x802A6C10
