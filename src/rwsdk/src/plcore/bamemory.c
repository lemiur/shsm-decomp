// Reference: tgsm/pbg include/rwsdk/plcore/bamemory.h (API names), src/rwsdk/src/badevice.c (callers)
// Unit name and boundary: Gormiti link map (RwCore.a bamemory)
// Name and locals of the free-list functions: Gormiti DWARF and Debug build.
/*
 * bamemory.c: the engine's memory interface and free lists. A free list hands out fixed-size
 * entries from blocks it allocates through RwMalloc; every block carries a bitmap of the entries
 * in use. Every free list is kept in one list so that _rwMemoryClose can destroy those still
 * alive; the RwFreeList structures themselves come from a free list of their own.
 */
#include <stdlib.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>

/* Entries per block of the free list of free lists. Name invented. */
#define rwFREELISTFREELISTBLOCKSIZE 16

/* The alignment of entries of a free list created with an alignment of 0. Name invented. */
#define rwFREELISTDEFAULTALIGN 32

/* Whether free lists preallocate blocks (cleared when the engine runs without free lists). */
static RwBool freeListsEnabled = TRUE;

/* Every free list except freeListFreeList. */
static RwLinkList freeListList;

/* The free list the RwFreeList structures are allocated from. */
static RwFreeList* freeListFreeList;

/* Set while the free-list module is open. */
static RwBool freeListModuleOpen;

/* The space of freeListFreeList itself. */
static RwFreeList freeListFreeListSpace;

/* Turns the preallocation of free-list blocks on or off. */
void _rwFreeListEnable(RwBool enable) {
    freeListsEnabled = enable;
} // 0x802A34A4

/* Frees a free list's blocks, then the free list itself unless it is the caller's space. Name invented. */
static void FreeListRelease(RwFreeList* freeList) {
    RwLLLink* block = rwLinkListGetFirstLLLink(&freeList->blockList);
    RwLLLink* end = rwLinkListGetTerminator(&freeList->blockList);

    while (block != end) {
        rwLinkListRemoveLLLink(block);
        RwFree(block);
        block = rwLinkListGetFirstLLLink(&freeList->blockList);
        end = rwLinkListGetTerminator(&freeList->blockList);
    }

    if (!(freeList->flags & rwFREELISTFLAG_STATIC)) {
        if (freeListFreeList == freeList || freeListFreeList == NULL) {
            RwFree(freeList);
        } else {
            RwFreeListFree(freeListFreeList, freeList);
        }
    }
}

/*
 * Creates a free list in freeList, the caller's space, or when that is NULL in a structure from
 * freeListFreeList. Name invented.
 */
static RwFreeList* FreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment,
                                  RwInt32 numBlocksToPreallocate, RwFreeList* freeList,
                                  RwUInt32 hint) {
    RwUInt32 heapSize;
    RwUInt32 alignedSize;

    if (!freeListsEnabled) {
        numBlocksToPreallocate = 0;
    }

    if (alignment == 0) {
        alignment = rwFREELISTDEFAULTALIGN;
    }

    if (freeList == NULL) {
        if (freeListFreeList != NULL) {
            freeList = (RwFreeList*)RwFreeListAlloc(freeListFreeList, hint & rwMEMHINTDUR_MASK);
        } else {
            freeList = (RwFreeList*)RwMalloc(sizeof(RwFreeList), hint & rwMEMHINTDUR_MASK);
        }

        if (freeList == NULL) {
            return NULL;
        }

        freeList->flags = rwFREELISTFLAG_FREEBLOCKS;
    } else {
        freeList->flags = rwFREELISTFLAG_STATIC | rwFREELISTFLAG_FREEBLOCKS;
    }

    alignedSize = (entrySize + alignment - 1) & ~(alignment - 1);
    freeList->entrySize = alignedSize;
    freeList->entriesPerBlock = entriesPerBlock;
    heapSize = (freeList->entriesPerBlock + 7) >> 3;
    freeList->alignment = alignment;
    freeList->heapSize = heapSize;
    rwLinkListInitialize(&freeList->blockList);

    while (numBlocksToPreallocate != 0) {
        /* The link, the bitmap, room to align the entries, then the entries. */
        RwLLLink* block = (RwLLLink*)RwMalloc(
            sizeof(RwLLLink) + heapSize + entriesPerBlock * alignedSize + alignment - 1, hint);

        if (block == NULL) {
            FreeListRelease(freeList);
            return NULL;
        }

        rwLLLinkInitialize(block);
        rwLinkListAddLLLink(&freeList->blockList, block);
        memset(block + 1, 0, heapSize);
        numBlocksToPreallocate--;
    }

    rwLinkListAddLLLink(&freeListList, &freeList->link);

    return freeList;
} // 0x802A34AC

/* Creates a free list with one block preallocated. */
RwFreeList* RwFreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment,
                             RwUInt32 hint) {
    return FreeListCreate(entrySize, entriesPerBlock, alignment, 1, NULL, hint);
} // 0x802A36D4

/* Creates a free list with numBlocksToPreallocate blocks, in inPlaceSpace when that is not NULL. */
RwFreeList* RwFreeListCreateAndPreallocateSpace(RwInt32 entrySize, RwInt32 entriesPerBlock,
                                                RwInt32 alignment, RwInt32 numBlocksToPreallocate,
                                                RwFreeList* inPlaceSpace, RwUInt32 hint) {
    return FreeListCreate(entrySize, entriesPerBlock, alignment, numBlocksToPreallocate, inPlaceSpace,
                          hint);
} // 0x802A36E4

/* Destroys a free list and every block it holds. */
RwBool RwFreeListDestroy(RwFreeList* freeList) {
    rwLinkListRemoveLLLink(&freeList->link);
    FreeListRelease(freeList);

    return TRUE;
} // 0x802A36E8

/*
 * RwGlobals memoryAlloc with free lists: the first unused entry, in a new block when all are used.
 * Locals and their scopes: Gormiti DWARF.
 */
void* _rwFreeListAllocReal(RwFreeList* freeList, RwUInt32 hint) {
    RwUInt8 heapElement;
    RwUInt8 mask;
    void* freeEntry;
    RwUInt32 i;
    RwUInt32 j;
    RwUInt32 heapEntries;
    RwUInt8* heap;
    RwLLLink* link;
    RwLLLink* lastLink;
    RwUInt32 checkEntries;

    freeEntry = NULL;
    heapEntries = freeList->heapSize;
    link = rwLinkListGetFirstLLLink(&freeList->blockList);
    lastLink = rwLinkListGetTerminator(&freeList->blockList);

    while (link != lastLink && freeEntry == NULL) {
        heap = (RwUInt8*)(link + 1);
        checkEntries = freeList->entriesPerBlock;

        for (i = 0; i < heapEntries; i++) {
            heapElement = heap[i];

            if (heapElement != 0xFF) {
                for (j = 0; j < 8 && checkEntries != 0; j++, checkEntries--) {
                    mask = (RwUInt8)(0x80 >> j);

                    if (!(heapElement & mask)) {
                        RwUInt8* aligned;

                        heap[i] = mask | heap[i];
                        aligned = (RwUInt8*)link + sizeof(RwLLLink) + heapEntries + freeList->alignment - 1;
                        aligned = (RwUInt8*)((RwUInt32)aligned & ~(freeList->alignment - 1));
                        freeEntry = aligned + freeList->entrySize * ((i << 3) + j);
                        break;
                    }
                }
            } else {
                checkEntries -= 8;
            }

            if (freeEntry != NULL) {
                break;
            }
        }

        link = rwLLLinkGetNext(link);
    }

    if (freeEntry == NULL) {
        void* dataBlock;
        RwLLLink* link;

        dataBlock = RwMalloc(sizeof(RwLLLink) + heapEntries + freeList->alignment - 1 +
                                 freeList->entriesPerBlock * freeList->entrySize,
                             hint);
        if (dataBlock == NULL) {
            return NULL;
        }

        heap = (RwUInt8*)dataBlock + sizeof(RwLLLink);
        memset(heap, 0, heapEntries);
        link = (RwLLLink*)dataBlock;
        rwLinkListAddLLLink(&freeList->blockList, link);
        *heap = 0x80;

        {
            RwUInt8* aligned;

            aligned = (RwUInt8*)dataBlock + sizeof(RwLLLink) + heapEntries + freeList->alignment - 1;
            freeEntry = (RwUInt8*)((RwUInt32)aligned & ~(freeList->alignment - 1));
        }
    }

    return freeEntry;
} // 0x802A37BC

/*
 * Whether a block's bitmap has no entry in use: the sum of its bytes is zero.
 * Name and locals: Gormiti link map and DWARF (static there; inlined in retail).
 */
static RwBool FreeListBlockIsEmpty(RwUInt8* heap, RwUInt32 heapEntries) {
    RwUInt32 i;
    RwUInt32 sum = 0;

    for (i = 0; i < heapEntries; i++) {
        sum += heap[i];
    }

    return (sum == 0);
}

/*
 * RwGlobals memoryFree with free lists: marks the entry unused, and frees its block when empty.
 * Locals and the FreeListBlockIsEmpty helper: Gormiti DWARF and Debug build. heap is declared at function
 * scope (DWARF: in the block): the register colouring needs it first.
 */
RwFreeList* _rwFreeListFreeReal(RwFreeList* freeList, void* entry) {
    /* RULE-EXCEPTION: R30, declaration placement: the DWARF has heap in the block; at function scope it gets
       retail's register colouring */
    RwUInt8* heap;
    RwUInt32 heapEntries;
    RwUInt8* dataBlock;
    RwLLLink* link;
    RwLLLink* lastLink;

    heapEntries = freeList->heapSize;
    link = rwLinkListGetFirstLLLink(&freeList->blockList);
    lastLink = rwLinkListGetTerminator(&freeList->blockList);

    while (link != lastLink) {
        heap = (RwUInt8*)(link + 1);
        dataBlock = heap + heapEntries;

        if ((RwUInt8*)entry >= dataBlock &&
            (RwUInt8*)entry <= dataBlock + freeList->entriesPerBlock * freeList->entrySize) {
            RwUInt32 entryIndex;
            RwUInt32 heapElement;
            RwUInt8 mask;

            entryIndex = ((RwUInt8*)entry - dataBlock) / freeList->entrySize;
            heapElement = entryIndex >> 3;
            mask = (RwUInt8)(0x80 >> (entryIndex - (heapElement << 3)));

            heap[heapElement] &= (RwUInt8)~mask;

            if (freeList->flags & rwFREELISTFLAG_FREEBLOCKS) {
                if (FreeListBlockIsEmpty(heap, heapEntries)) {
                    rwLinkListRemoveLLLink(link);
                    RwFree(link);
                }
            }

            return freeList;
        }

        link = rwLLLinkGetNext(link);
    }

    return NULL;
} // 0x802A3958
/*
 * Frees every block of the free list with no entry in use; returns their count times the entry size.
 * Locals: Gormiti DWARF.
 */
RwInt32 RwFreeListPurge(RwFreeList* freeList) {
    RwUInt32 heapEntries;
    RwLLLink* link;
    RwLLLink* nextLink;
    RwLLLink* lastLink;
    RwUInt32 blocksFreed = 0;

    heapEntries = freeList->heapSize;
    link = rwLinkListGetFirstLLLink(&freeList->blockList);
    lastLink = rwLinkListGetTerminator(&freeList->blockList);

    while (link != lastLink) {
        RwUInt8* heap = (RwUInt8*)(link + 1);

        rwLinkListRemoveLLLink(link);
        nextLink = rwLLLinkGetNext(link);

        if (FreeListBlockIsEmpty(heap, heapEntries)) {
            RwFree(link);
            blocksFreed++;
        } else {
            rwLinkListAddLLLink(&freeList->blockList, link);
        }

        link = nextLink;
    }

    return blocksFreed * freeList->entrySize;
} // 0x802A3AE4

/*
 * Calls fpCallBack for every entry in use, from a copy of each block's bitmap.
 */
RwFreeList* RwFreeListForAllUsed(RwFreeList* freeList, RwFreeListCallBack fpCallBack, void* data) {
    RwUInt32 heapSize = freeList->heapSize;
    RwLLLink* next;
    RwLLLink* end = rwLinkListGetTerminator(&freeList->blockList);
    RwLLLink* cur = rwLinkListGetFirstLLLink(&freeList->blockList);
    RwUInt8* used;
    RwUInt8 byte;
    RwUInt32 i;
    RwUInt32 bit;
    RwUInt8* bitmap;

    while (cur != end) {
        bitmap = (RwUInt8*)(cur + 1);
        used = (RwUInt8*)RwMalloc(heapSize, rwMEMHINTDUR_FUNCTION);
        if (used == NULL) {
            return NULL;
        }

        memcpy(used, bitmap, heapSize);
        next = rwLLLinkGetNext(cur);

        for (i = 0; i < heapSize; i++) {
            byte = used[i];

            if (byte != 0) {
                for (bit = 0; bit < 8; bit++) {
                    if (byte & (RwUInt8)(0x80 >> bit)) {
                        RwUInt8* aligned = (RwUInt8*)((RwUInt8*)cur + sizeof(RwLLLink) + heapSize +
                                                       freeList->alignment - 1);

                        aligned = (RwUInt8*)((RwUInt32)aligned & ~(freeList->alignment - 1));
                        fpCallBack(aligned + freeList->entrySize * ((i << 3) + bit), data);
                    }
                }
            }
        }

        RwFree(used);
        cur = next;
    }

    return freeList;
} // 0x802A3C38

/* Purges every free list; returns the sum of RwFreeListPurge's results. */
RwInt32 RwFreeListPurgeAllFreeLists(void) {
    RwInt32 total = 0;
    RwLLLink* cur = rwLinkListGetFirstLLLink(&freeListList);
    RwLLLink* end = rwLinkListGetTerminator(&freeListList);

    while (cur != end) {
        RwInt32 freed = RwFreeListPurge(rwLLLinkGetData(cur, RwFreeList, link));

        if (freed > 0) {
            total += freed;
        }

        cur = rwLLLinkGetNext(cur);
    }

    return total;
} // 0x802A3D64

/* The default RwMemoryFunctions: the C library's, ignoring the hint. Names invented. */

/* rwmalloc: malloc. */
static void* DefaultMalloc(size_t size, RwUInt32 hint) {
    return malloc(size);
} // 0x802A3DCC

/* rwrealloc: realloc. */
static void* DefaultRealloc(void* mem, size_t newSize, RwUInt32 hint) {
    return realloc(mem, newSize);
} // 0x802A3DD0

/* rwcalloc: calloc. */
static void* DefaultCalloc(size_t numObj, size_t sizeObj, RwUInt32 hint) {
    return calloc(numObj, sizeObj);
} // 0x802A3DD4

/* Opens the free-list module: creates the free list of free lists. Name invented. */
static RwBool FreeListModuleOpen(void) {
    rwLinkListInitialize(&freeListList);
    freeListModuleOpen = TRUE;

    freeListFreeList = RwFreeListCreateAndPreallocateSpace(
        sizeof(RwFreeList), rwFREELISTFREELISTBLOCKSIZE, rwFREELISTDEFAULTALIGN, 0,
        &freeListFreeListSpace, rwMEMHINTDUR_GLOBAL);
    if (freeListFreeList == NULL) {
        freeListModuleOpen = FALSE;
        return FALSE;
    }

    /* Keep it out of freeListList: purges and _rwMemoryClose's sweep pass it by. */
    rwLinkListRemoveLLLink(&freeListFreeList->link);

    return TRUE;
}

/* Opens the memory module with the caller's memory functions, or the C library's when NULL. */
RwBool _rwMemoryOpen(const RwMemoryFunctions* memoryFuncs) {
    if (!FreeListModuleOpen()) {
        return FALSE;
    }

    if (memoryFuncs != NULL) {
        RWSRCGLOBAL(memoryFuncs) = *memoryFuncs;
    } else {
        RWSRCGLOBAL(memoryFuncs).rwmalloc = DefaultMalloc;
        RWSRCGLOBAL(memoryFuncs).rwfree = free;
        RWSRCGLOBAL(memoryFuncs).rwrealloc = DefaultRealloc;
        RWSRCGLOBAL(memoryFuncs).rwcalloc = DefaultCalloc;
    }

    return TRUE;
} // 0x802A3DD8

/* Closes the memory module: destroys every free list still alive, then the free list of free lists. */
void _rwMemoryClose(void) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&freeListList);
    RwLLLink* end = rwLinkListGetTerminator(&freeListList);

    while (cur != end) {
        RwFreeList* freeList = rwLLLinkGetData(cur, RwFreeList, link);

        RwFreeListDestroy(freeList);
        cur = rwLinkListGetFirstLLLink(&freeListList);
        end = rwLinkListGetTerminator(&freeListList);
    }

    RwFreeListDestroy(freeListFreeList);
    freeListFreeList = NULL;
    freeListModuleOpen = FALSE;
} // 0x802A3EF4

/* The memory functions the engine allocates with. */
RwMemoryFunctions* RwOsGetMemoryInterface(void) {
    return &RWSRCGLOBAL(memoryFuncs);
} // 0x802A4098
