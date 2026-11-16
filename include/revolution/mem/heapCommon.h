#ifndef REVOLUTION_MEM_HEAPCOMMON_H
#define REVOLUTION_MEM_HEAPCOMMON_H

#include <revolution/mem/list.h>
#include <revolution/os.h>
#include <revolution/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_HEAP_OPT_CLEAR_ALLOC 0x01 // zero-fill every allocation
#define MEM_HEAP_OPT_CAN_LOCK 0x04    // serialise heap calls with the heap mutex

typedef struct MEMiHeapHead MEMiHeapHead;

struct MEMiHeapHead {
    u32 signature;     // 0x00: heap type tag, e.g. 'EXPH'
    MEMLink link;      // 0x04: link in the parent heap list
    MEMList childList; // 0x0C: heaps created inside this one
    void* heapStart;   // 0x18
    void* heapEnd;     // 0x1C
    OSMutex mutex;     // 0x20

    union {
        u32 val;

        struct {
            u32 reserved : 24;
            u32 optFlag : 8;
        } fields;
    } attribute; // 0x38
};

typedef MEMiHeapHead* MEMHeapHandle;

typedef u32 UIntPtr;

/*
 * GetUIntPtr
 *
 * Pointer as an integer.
 */
static inline UIntPtr GetUIntPtr(const void* ptr) {
    return (UIntPtr)ptr;
}

/*
 * GetOffsetFromPtr
 *
 * Byte distance from start to end.
 */
static inline u32 GetOffsetFromPtr(const void* start, const void* end) {
    return GetUIntPtr(end) - GetUIntPtr(start);
}

/*
 * SubU32ToPtr
 *
 * ptr moved down by val bytes.
 */
static inline void* SubU32ToPtr(void* ptr, u32 val) {
    return (void*)(GetUIntPtr(ptr) - val);
}

/*
 * AddU32ToPtr
 *
 * ptr moved up by val bytes.
 */
static inline void* AddU32ToPtr(void* ptr, u32 val) {
    return (void*)(GetUIntPtr(ptr) + val);
}

/*
 * SetOptForHeap
 *
 * Stores the option flags in the heap attribute.
 */
static inline void SetOptForHeap(MEMiHeapHead* heap, u16 optFlag) {
    heap->attribute.fields.optFlag = (u8)optFlag;
}

/*
 * GetOptForHeap
 *
 * Option flags of the heap.
 */
static inline u16 GetOptForHeap(const MEMiHeapHead* heap) {
    return (u16)heap->attribute.fields.optFlag;
}

/*
 * FillAllocMemory
 *
 * Zero-fills a fresh allocation when the heap has the clear-alloc option.
 */
static inline void FillAllocMemory(MEMiHeapHead* heap, void* address,
                                   u32 size) {
    if (GetOptForHeap(heap) & MEM_HEAP_OPT_CLEAR_ALLOC) {
        (void)memset(address, 0, size);
    }
}

/*
 * ComparePtr
 *
 * Signed byte difference a - b.
 */
static inline int ComparePtr(const void* a, const void* b) {
    const u8* wa = (const u8*)a;
    const u8* wb = (const u8*)b;

    return wa - wb;
}

/*
 * LockHeap
 *
 * Takes the heap mutex if the heap is lockable.
 */
static inline void LockHeap(MEMiHeapHead* heap) {
    if (GetOptForHeap(heap) & MEM_HEAP_OPT_CAN_LOCK) {
        OSLockMutex(&heap->mutex);
    }
}

/*
 * UnlockHeap
 *
 * Releases the heap mutex if the heap is lockable.
 */
static inline void UnlockHeap(MEMiHeapHead* heap) {
    if (GetOptForHeap(heap) & MEM_HEAP_OPT_CAN_LOCK) {
        OSUnlockMutex(&heap->mutex);
    }
}

void MEMiInitHeapHead(MEMiHeapHead* heap, u32 signature, void* heapStart,
                      void* heapEnd, u16 optFlag);
void MEMiFinalizeHeap(MEMiHeapHead* heap);
MEMiHeapHead* MEMFindContainHeap(const void* memBlock);

#ifdef __cplusplus
}
#endif

#endif
