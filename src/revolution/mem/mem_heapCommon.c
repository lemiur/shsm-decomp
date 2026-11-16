// Reference: zeldaret/tp libs/revolution/src/mem/mem_heapCommon.c
// Reference: doldecomp/ogws src/revolution/MEM/mem_heapCommon.c (MEMiFinalizeHeap, MEMFindContainHeap)
#include <revolution/mem.h>
#include <revolution/os.h>

static MEMList sRootList;
static BOOL sRootListInitialized = FALSE;
static OSMutex sRootMutex;

/*
 * FindContainHeap_
 *
 * Searches the list recursively for the innermost heap whose range contains memBlock.
 */
static MEMiHeapHead* FindContainHeap_(MEMList* list, const void* memBlock) {
    MEMiHeapHead* heap = NULL;

    while (NULL != (heap = (MEMiHeapHead*)MEMGetNextListObject(list, heap))) {
        if (GetUIntPtr(heap->heapStart) <= GetUIntPtr(memBlock) &&
            GetUIntPtr(memBlock) < GetUIntPtr(heap->heapEnd)) {
            MEMiHeapHead* childHeap =
                FindContainHeap_(&heap->childList, memBlock);
            if (childHeap) {
                return childHeap;
            }
            return heap;
        }
    }
    return NULL;
} // 0x8023F0E0

/*
 * FindListContainHeap_
 *
 * The child list of the heap containing heap, or the root list when none does.
 */
static MEMList* FindListContainHeap_(const MEMiHeapHead* heap) {
    MEMList* list = &sRootList;

    MEMiHeapHead* containHeap = FindContainHeap_(&sRootList, heap);

    if (containHeap) {
        list = &containHeap->childList;
    }

    return list;
}

/*
 * MEMiInitHeapHead
 *
 * Fills in the common heap head and registers the heap with its parent
 * (the smallest heap containing it, else the root list).
 */
void MEMiInitHeapHead(MEMiHeapHead* heap, u32 signature, void* heapStart,
                      void* heapEnd, u16 optFlag) {
    heap->signature = signature;
    heap->heapStart = heapStart;
    heap->heapEnd = heapEnd;
    heap->attribute.val = 0;

    SetOptForHeap(heap, optFlag);

    MEM_INIT_LIST(&heap->childList, MEMiHeapHead, link);

    if (!sRootListInitialized) {
        MEM_INIT_LIST(&sRootList, MEMiHeapHead, link);
        OSInitMutex(&sRootMutex);
        sRootListInitialized = TRUE;
    }

    OSInitMutex(&heap->mutex);
    OSLockMutex(&sRootMutex);
    MEMAppendListObject(FindListContainHeap_(heap), heap);
    OSUnlockMutex(&sRootMutex);
} // 0x8023F220

/*
 * MEMiFinalizeHeap
 *
 * Removes the heap from its parent list and clears its signature.
 */
void MEMiFinalizeHeap(MEMiHeapHead* heap) {
    OSLockMutex(&sRootMutex);
    MEMRemoveListObject(FindListContainHeap_(heap), heap);
    OSUnlockMutex(&sRootMutex);
    heap->signature = 0;
} // 0x8023F3E0

/*
 * MEMFindContainHeap
 *
 * Returns the innermost heap whose range contains memBlock, or NULL.
 */
MEMiHeapHead* MEMFindContainHeap(const void* memBlock) {
    return FindContainHeap_(&sRootList, memBlock);
} // 0x8023F550
