// Reference: zeldaret/tp libs/revolution/src/mem/mem_expHeap.c
// Reference: doldecomp/ogws src/revolution/MEM/mem_expHeap.c (MEMDestroyExpHeap, MEMResizeForMBlockExpHeap shape)
#include <revolution/mem.h>

#define FillFreeMemory(heap, address, size) ((void)0)
#define FillNoUseMemory(heap, address, size) ((void)0)

#define RoundUp(value, alignment)                                              \
    (((value) + ((alignment)-1)) & ~((alignment)-1))
#define RoundUpPtr(ptr, alignment) ((void*)RoundUp(GetUIntPtr(ptr), (alignment)))
#define RoundDown(value, alignment) ((value) & ~((alignment)-1))
#define RoundDownPtr(ptr, alignment)                                           \
    ((void*)RoundDown(GetUIntPtr(ptr), (alignment)))

#define MBLOCK_SIZE_NONE 0xffffffff // no fitting block found yet

typedef struct MemRegion {
    void* start;
    void* end;
} MemRegion;

/*
 * GetMemPtrForMBlock_
 *
 * Payload address of a block (just past its head).
 */
static inline void* GetMemPtrForMBlock_(MEMiExpHeapMBlockHead* mblock) {
    return AddU32ToPtr(mblock, sizeof(MEMiExpHeapMBlockHead));
}

/*
 * GetMBlockEndAddr_
 *
 * First byte after a block payload.
 */
static inline void* GetMBlockEndAddr_(MEMiExpHeapMBlockHead* mblock) {
    return AddU32ToPtr(GetMemPtrForMBlock_(mblock), mblock->blockSize);
}

/*
 * GetAllocMode_
 *
 * Allocation mode bit of the heap: 0 takes the first fitting free block.
 */
static inline u16 GetAllocMode_(MEMiExpHeapHead* expHeap) {
    return expHeap->feature.fields.allocMode;
}

/*
 * InitMBlock_
 *
 * Writes a block head at the start of region, sized to reach its end.
 */
static MEMiExpHeapMBlockHead* InitMBlock_(MemRegion* region, u16 signature) {
    MEMiExpHeapMBlockHead* block = (MEMiExpHeapMBlockHead*)region->start;

    block->signature = signature;
    block->attribute.val = 0;
    block->blockSize =
        GetOffsetFromPtr(GetMemPtrForMBlock_(block), region->end);

    block->prev = NULL;
    block->next = NULL;

    return block;
}

/*
 * SetAllocDirForMBlock_
 *
 * Records whether the block was cut from the head (0) or the tail (1).
 */
static inline void SetAllocDirForMBlock_(MEMiExpHeapMBlockHead* mblock,
                                         u16 mode) {
    mblock->attribute.fields.allocDir = mode;
}

/*
 * GetAlignmentForMBlock_
 *
 * Alignment padding in front of the block head.
 */
static inline u16 GetAlignmentForMBlock_(const MEMiExpHeapMBlockHead* mblock) {
    return mblock->attribute.fields.alignment;
}

/*
 * SetAlignmentForMBlock_
 *
 * Stores the alignment padding in front of the block head.
 */
static inline void SetAlignmentForMBlock_(MEMiExpHeapMBlockHead* mblock,
                                          u16 alignment) {
    mblock->attribute.fields.alignment = alignment;
}

/*
 * SetGroupIDForMBlock_
 *
 * Stores the allocation group id.
 */
static inline void SetGroupIDForMBlock_(MEMiExpHeapMBlockHead* mblock, u16 id) {
    mblock->attribute.fields.groupID = (u8)id;
}

/*
 * GetRegionOfMBlock_
 *
 * Full extent of a block: leading padding, head and payload.
 */
static void GetRegionOfMBlock_(MemRegion* region,
                               MEMiExpHeapMBlockHead* block) {
    region->start = SubU32ToPtr(block, GetAlignmentForMBlock_(block));
    region->end = GetMBlockEndAddr_(block);
}

/*
 * GetHeapHeadPtrFromExpHeapHead_
 *
 * The common heap head sits right before the expanded heap head.
 */
static inline MEMiHeapHead*
GetHeapHeadPtrFromExpHeapHead_(MEMiExpHeapHead* expHeap) {
    return (MEMiHeapHead*)SubU32ToPtr(expHeap, sizeof(MEMiHeapHead));
}

/*
 * InsertMBlock_
 *
 * Links target into the list after prev (at the head when prev is NULL).
 */
static MEMiExpHeapMBlockHead* InsertMBlock_(MEMiExpMBlockList* list,
                                            MEMiExpHeapMBlockHead* target,
                                            MEMiExpHeapMBlockHead* prev) {
    MEMiExpHeapMBlockHead* next;

    target->prev = prev;
    if (prev) {
        next = prev->next;
        prev->next = target;
    } else {
        next = list->head;
        list->head = target;
    }

    target->next = next;
    if (next) {
        next->prev = target;
    } else {
        list->tail = target;
    }

    return target;
}

/*
 * AppendMBlock_
 *
 * Links block at the tail of the list.
 */
static inline void AppendMBlock_(MEMiExpMBlockList* list,
                                 MEMiExpHeapMBlockHead* block) {
    (void)InsertMBlock_(list, block, list->tail);
}

/*
 * GetExpHeapHeadPtrFromHeapHead_
 *
 * The expanded heap head follows the common heap head.
 */
static inline MEMiExpHeapHead*
GetExpHeapHeadPtrFromHeapHead_(MEMiHeapHead* heap) {
    return (MEMiExpHeapHead*)AddU32ToPtr(heap, sizeof(MEMiHeapHead));
}

/*
 * RemoveMBlock_
 *
 * Unlinks a block; returns its former predecessor.
 */
static MEMiExpHeapMBlockHead* RemoveMBlock_(MEMiExpMBlockList* list,
                                            MEMiExpHeapMBlockHead* block) {
    MEMiExpHeapMBlockHead* const prev = block->prev;
    MEMiExpHeapMBlockHead* const next = block->next;

    if (prev) {
        prev->next = next;
    } else {
        list->head = next;
    }

    if (next) {
        next->prev = prev;
    } else {
        list->tail = prev;
    }

    return prev;
}

/*
 * InitFreeMBlock_
 *
 * Writes a free block head over region.
 */
static inline MEMiExpHeapMBlockHead* InitFreeMBlock_(MemRegion* region) {
    return InitMBlock_(region, 'FR');
}

/*
 * SetAllocMode_
 *
 * Selects first-fit (0) or best-fit allocation.
 */
static inline void SetAllocMode_(MEMiExpHeapHead* expHeap, u16 mode) {
    expHeap->feature.fields.allocMode = mode;
}

/*
 * InitExpHeap_
 *
 * Sets up the heap heads and one free block spanning the whole heap.
 */
static MEMiHeapHead* InitExpHeap_(void* startAddress, void* endAddress,
                                  u16 optFlag) {
    MEMiHeapHead* heap = (MEMiHeapHead*)startAddress;
    MEMiExpHeapHead* expHeap = GetExpHeapHeadPtrFromHeapHead_(heap);
    MEMiExpHeapMBlockHead* mblock;
    MemRegion region;

    MEMiInitHeapHead(heap, 'EXPH',
                     AddU32ToPtr(expHeap, sizeof(MEMiExpHeapHead)), endAddress,
                     optFlag);

    expHeap->groupID = 0;
    expHeap->feature.val = 0;
    SetAllocMode_(expHeap, 0);

    region.start = heap->heapStart;
    region.end = heap->heapEnd;
    mblock = InitFreeMBlock_(&region);
    expHeap->mbFreeList.head = mblock;
    expHeap->mbFreeList.tail = mblock;
    expHeap->mbUsedList.head = NULL;
    expHeap->mbUsedList.tail = NULL;

    return heap;
}

/*
 * AllocUsedBlockFromFreeBlock_
 *
 * Splits a free block around mblock: leftover regions go back to the free list, the middle becomes a used block.
 */
static void* AllocUsedBlockFromFreeBlock_(MEMiExpHeapHead* expHeap,
                                          MEMiExpHeapMBlockHead* freeBlock,
                                          void* mblock, u32 size,
                                          u16 direction) {
    MemRegion freeRgnT;
    MemRegion freeRgnB;
    MEMiExpHeapMBlockHead* freeBlockPrev;
    MEMiExpHeapMBlockHead* newUsed;
    MemRegion region;

    GetRegionOfMBlock_(&freeRgnT, freeBlock);
    freeRgnB.end = freeRgnT.end;
    freeRgnB.start = AddU32ToPtr(mblock, size);
    freeRgnT.end = SubU32ToPtr(mblock, sizeof(MEMiExpHeapMBlockHead));

    freeBlockPrev = RemoveMBlock_(&expHeap->mbFreeList, freeBlock);

    if ((GetOffsetFromPtr(freeRgnT.start, freeRgnT.end) <
         sizeof(MEMiExpHeapMBlockHead) + 4) ||
        (direction == 0 && !expHeap->feature.fields.useMarginOfAlign)) {
        freeRgnT.end = freeRgnT.start;
    } else {
        freeBlockPrev = InsertMBlock_(&expHeap->mbFreeList,
                                      InitFreeMBlock_(&freeRgnT), freeBlockPrev);
    }

    if ((GetOffsetFromPtr(freeRgnB.start, freeRgnB.end) <
         sizeof(MEMiExpHeapMBlockHead) + 4) ||
        (direction == 1 && !expHeap->feature.fields.useMarginOfAlign)) {
        freeRgnB.start = freeRgnB.end;
    } else {
        (void)InsertMBlock_(&expHeap->mbFreeList, InitFreeMBlock_(&freeRgnB),
                            freeBlockPrev);
    }

    FillAllocMemory(GetHeapHeadPtrFromExpHeapHead_(expHeap), freeRgnT.end,
                    GetOffsetFromPtr(freeRgnT.end, freeRgnB.start));

    region.start = SubU32ToPtr(mblock, sizeof(MEMiExpHeapMBlockHead));
    region.end = freeRgnB.start;

    newUsed = InitMBlock_(&region, 'UD');
    SetAllocDirForMBlock_(newUsed, direction);
    SetAlignmentForMBlock_(newUsed,
                           (u16)GetOffsetFromPtr(freeRgnT.end, newUsed));
    SetGroupIDForMBlock_(newUsed, expHeap->groupID);
    AppendMBlock_(&expHeap->mbUsedList, newUsed);

    return mblock;
} // 0x8023F690

/*
 * AllocFromHead_
 *
 * Allocates from the low end of the free list.
 */
static void* AllocFromHead_(MEMiHeapHead* heap, u32 size, int alignment) {
    MEMiExpHeapHead* expHeap = GetExpHeapHeadPtrFromHeapHead_(heap);

    const BOOL allocFirst = GetAllocMode_(expHeap) == 0;

    MEMiExpHeapMBlockHead* block = NULL;
    MEMiExpHeapMBlockHead* blockFound = NULL;
    u32 foundSize = MBLOCK_SIZE_NONE;
    void* foundMBlock = NULL;

    for (block = expHeap->mbFreeList.head; block; block = block->next) {
        void* const mblock = GetMemPtrForMBlock_(block);
        void* const reqMBlock = RoundUpPtr(mblock, alignment);
        const u32 offset = GetOffsetFromPtr(mblock, reqMBlock);

        if (block->blockSize >= size + offset && foundSize > block->blockSize) {
            blockFound = block;
            foundSize = block->blockSize;
            foundMBlock = reqMBlock;

            if (allocFirst || foundSize == size) {
                break;
            }
        }
    }

    if (!blockFound) {
        return NULL;
    }

    return AllocUsedBlockFromFreeBlock_(expHeap, blockFound, foundMBlock, size,
                                        0);
} // 0x8023F8C0

/*
 * AllocFromTail_
 *
 * Allocates from the high end of the free list.
 */
static void* AllocFromTail_(MEMiHeapHead* heap, u32 size, int alignment) {
    MEMiExpHeapHead* expHeap = GetExpHeapHeadPtrFromHeapHead_(heap);
    const BOOL allocFirst = GetAllocMode_(expHeap) == 0;
    MEMiExpHeapMBlockHead* block = NULL;
    MEMiExpHeapMBlockHead* blockFound = NULL;
    u32 foundSize = MBLOCK_SIZE_NONE;
    void* foundMBlock = NULL;

    for (block = expHeap->mbFreeList.tail; block; block = block->prev) {
        void* const mblock = GetMemPtrForMBlock_(block);
        void* const mblockEnd = AddU32ToPtr(mblock, block->blockSize);
        void* const reqMBlock =
            RoundDownPtr(SubU32ToPtr(mblockEnd, size), alignment);

        if (ComparePtr(reqMBlock, mblock) >= 0 && foundSize > block->blockSize) {
            blockFound = block;
            foundSize = block->blockSize;
            foundMBlock = reqMBlock;

            if (allocFirst || foundSize == size) {
                break;
            }
        }
    }

    if (!blockFound) {
        return NULL;
    }

    return AllocUsedBlockFromFreeBlock_(expHeap, blockFound, foundMBlock, size,
                                        1);
} // 0x8023F9A0

/*
 * RecycleRegion_
 *
 * Returns a region to the free list, merging with adjacent free blocks; FALSE if too small to hold a block head.
 */
static BOOL RecycleRegion_(MEMiExpHeapHead* expHeap, const MemRegion* region) {
    MEMiExpHeapMBlockHead* freePrev = NULL;
    MemRegion freeRgn = *region;
    MEMiExpHeapMBlockHead* block;

    for (block = expHeap->mbFreeList.head; block; block = block->next) {
        if (block < region->start) {
            freePrev = block;
            continue;
        }

        if (block == region->end) {
            freeRgn.end = GetMBlockEndAddr_(block);
            (void)RemoveMBlock_(&expHeap->mbFreeList, block);
            FillNoUseMemory(GetHeapHeadPtrFromExpHeapHead_(expHeap), block,
                            sizeof(MEMiExpHeapMBlockHead));
        }
        break;
    }

    if (freePrev && GetMBlockEndAddr_(freePrev) == region->start) {
        freeRgn.start = freePrev;
        freePrev = RemoveMBlock_(&expHeap->mbFreeList, freePrev);
    }

    if (GetOffsetFromPtr(freeRgn.start, freeRgn.end) <
        sizeof(MEMiExpHeapMBlockHead)) {
        return FALSE;
    }

    FillFreeMemory(GetHeapHeadPtrFromExpHeapHead_(expHeap), region->start,
                   GetOffsetFromPtr(region->start, region->end));

    (void)InsertMBlock_(&expHeap->mbFreeList, InitFreeMBlock_(&freeRgn),
                        freePrev);

    return TRUE;
} // 0x8023FA70

/*
 * MEMCreateExpHeapEx
 *
 * Builds an expanded heap in [startAddress, startAddress + size), 4-aligned.
 */
MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag) {
    void* endAddress;
    MEMiHeapHead* heap;

    endAddress = RoundDownPtr(AddU32ToPtr(startAddress, size), 4);
    startAddress = RoundUpPtr(startAddress, 4);

    if (GetUIntPtr(startAddress) > GetUIntPtr(endAddress) ||
        GetOffsetFromPtr(startAddress, endAddress) <
            sizeof(MEMiHeapHead) + sizeof(MEMiExpHeapHead) +
                sizeof(MEMiExpHeapMBlockHead) + 4) {
        return 0;
    }

    heap = InitExpHeap_(startAddress, endAddress, optFlag);
    return heap;
} // 0x8023FBE0

/*
 * MEMDestroyExpHeap
 *
 * Unregisters the heap; returns the heap start for the caller to release.
 */
void* MEMDestroyExpHeap(MEMHeapHandle heap) {
    MEMiFinalizeHeap(heap);
    return heap;
} // 0x8023FC90

/*
 * MEMAllocFromExpHeapEx
 *
 * Allocates size bytes; a negative alignment allocates from the heap tail.
 */
void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, u32 size, int alignment) {
    void* memory = NULL;

    if (size == 0) {
        size = 1;
    }

    size = RoundUp(size, 4);

    LockHeap(heap);

    if (alignment >= 0) {
        memory = AllocFromHead_(heap, size, alignment);
    } else {
        memory = AllocFromTail_(heap, size, -alignment);
    }

    UnlockHeap(heap);

    return memory;
} // 0x8023FCC0

/*
 * GetMBlockHeadPtr_
 *
 * Block head of a payload address.
 */
static inline MEMiExpHeapMBlockHead* GetMBlockHeadPtr_(void* memBlock) {
    return (MEMiExpHeapMBlockHead*)SubU32ToPtr(memBlock,
                                               sizeof(MEMiExpHeapMBlockHead));
}

/*
 * GetMBlockHeadCPtr_
 *
 * Block head of a payload address, for read-only callers.
 */
static inline const MEMiExpHeapMBlockHead*
GetMBlockHeadCPtr_(const void* memBlock) {
    return (const MEMiExpHeapMBlockHead*)(GetUIntPtr(memBlock) -
                                          sizeof(MEMiExpHeapMBlockHead));
}

/*
 * MEMResizeForMBlockExpHeap
 *
 * Grows a used block into the free block right after it, or shrinks it and
 * recycles the tail; returns the new size, or 0 when it cannot grow.
 */
u32 MEMResizeForMBlockExpHeap(MEMHeapHandle heap, void* memBlock, u32 size) {
    MEMiExpHeapHead* expHeap;
    MEMiExpHeapMBlockHead* block;
    MEMiExpHeapMBlockHead* freeBlock;
    MEMiExpHeapMBlockHead* freePrev;
    MemRegion newFreeRgn;
    MemRegion shrunkFreeRgn;
    void* blockEnd;
    void* oldFreeStart;
    u32 oldBlockSize;

    expHeap = GetExpHeapHeadPtrFromHeapHead_(heap);
    block = GetMBlockHeadPtr_(memBlock);
    size = RoundUp(size, 4);

    if (size == block->blockSize) {
        return size;
    }

    LockHeap(heap);

    if (size > block->blockSize) {
        blockEnd = GetMBlockEndAddr_(block);

        for (freeBlock = expHeap->mbFreeList.head; freeBlock;
             freeBlock = freeBlock->next) {
            if (freeBlock == blockEnd) {
                break;
            }
        }

        if (!freeBlock || size > block->blockSize +
                                     sizeof(MEMiExpHeapMBlockHead) +
                                     freeBlock->blockSize) {
            UnlockHeap(heap);
            return 0;
        }

        oldFreeStart =
            SubU32ToPtr(freeBlock, GetAlignmentForMBlock_(freeBlock));
        newFreeRgn.end = GetMBlockEndAddr_(freeBlock);

        freePrev = RemoveMBlock_(&expHeap->mbFreeList, freeBlock);

        newFreeRgn.start = AddU32ToPtr(memBlock, size);

        if (GetOffsetFromPtr(newFreeRgn.start, newFreeRgn.end) <
            sizeof(MEMiExpHeapMBlockHead)) {
            newFreeRgn.start = newFreeRgn.end;
        }

        block->blockSize = GetOffsetFromPtr(memBlock, newFreeRgn.start);

        if (GetOffsetFromPtr(newFreeRgn.start, newFreeRgn.end) >=
            sizeof(MEMiExpHeapMBlockHead)) {
            (void)InsertMBlock_(&expHeap->mbFreeList,
                                InitFreeMBlock_(&newFreeRgn), freePrev);
        }

        FillAllocMemory(heap, oldFreeStart,
                        GetOffsetFromPtr(oldFreeStart, newFreeRgn.start));
    } else {
        oldBlockSize = block->blockSize;

        shrunkFreeRgn.start = AddU32ToPtr(memBlock, size);
        shrunkFreeRgn.end = GetMBlockEndAddr_(block);
        block->blockSize = size;

        if (!RecycleRegion_(expHeap, &shrunkFreeRgn)) {
            block->blockSize = oldBlockSize;
        }
    }

    UnlockHeap(heap);

    return block->blockSize;
} // 0x8023FD70

/*
 * MEMFreeToExpHeap
 *
 * Returns a used block (and its alignment padding) to the free list.
 */
void MEMFreeToExpHeap(MEMHeapHandle heap, void* memBlock) {
    MEMiHeapHead* heapHead;
    MEMiExpHeapHead* expHeap;
    MEMiExpHeapMBlockHead* block;
    MemRegion region;

    if (memBlock == NULL) {
        return;
    }

    LockHeap(heap);

    heapHead = heap;
    expHeap = GetExpHeapHeadPtrFromHeapHead_(heapHead);
    block = GetMBlockHeadPtr_(memBlock);

    GetRegionOfMBlock_(&region, block);
    (void)RemoveMBlock_(&expHeap->mbUsedList, block);
    (void)RecycleRegion_(expHeap, &region);

    UnlockHeap(heap);
} // 0x8023FF70

/*
 * MEMGetSizeForMBlockExpHeap
 *
 * Returns the size of a used block's payload.
 */
u32 MEMGetSizeForMBlockExpHeap(const void* memBlock) {
    const MEMiExpHeapMBlockHead* block = GetMBlockHeadCPtr_(memBlock);

    return block->blockSize;
} // 0x80240040
