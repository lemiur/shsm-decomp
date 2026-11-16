#ifndef REVOLUTION_MEM_EXPHEAP_H
#define REVOLUTION_MEM_EXPHEAP_H

#include <revolution/mem/heapCommon.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiExpHeapMBlockHead MEMiExpHeapMBlockHead;

struct MEMiExpHeapMBlockHead {
    u16 signature; // 0x00: 'FR' free, 'UD' used

    union {
        u16 val;

        struct {
            u16 allocDir : 1;
            u16 alignment : 7;
            u16 groupID : 8;
        } fields;
    } attribute; // 0x02

    u32 blockSize;               // 0x04
    MEMiExpHeapMBlockHead* prev; // 0x08
    MEMiExpHeapMBlockHead* next; // 0x0C
};

typedef struct MEMiExpMBlockList {
    MEMiExpHeapMBlockHead* head; // 0x00
    MEMiExpHeapMBlockHead* tail; // 0x04
} MEMiExpMBlockList;

/* Follows the MEMiHeapHead of an expanded heap. */
typedef struct MEMiExpHeapHead {
    MEMiExpMBlockList mbFreeList; // 0x00
    MEMiExpMBlockList mbUsedList; // 0x08
    u16 groupID;                  // 0x10

    union {
        u16 val;

        struct {
            u16 reserved : 14;
            u16 useMarginOfAlign : 1;
            u16 allocMode : 1;
        } fields;
    } feature; // 0x12
} MEMiExpHeapHead;

MEMHeapHandle MEMCreateExpHeapEx(void* startAddress, u32 size, u16 optFlag);
void* MEMDestroyExpHeap(MEMHeapHandle heap);
void* MEMAllocFromExpHeapEx(MEMHeapHandle heap, u32 size, int alignment);
u32 MEMResizeForMBlockExpHeap(MEMHeapHandle heap, void* memBlock, u32 size);
void MEMFreeToExpHeap(MEMHeapHandle heap, void* memBlock);
u32 MEMGetSizeForMBlockExpHeap(const void* memBlock);

/*
 * MEMCreateExpHeap
 *
 * MEMCreateExpHeapEx with no option flags.
 */
static inline MEMHeapHandle MEMCreateExpHeap(void* startAddress, u32 size) {
    return MEMCreateExpHeapEx(startAddress, size, 0);
}

/*
 * MEMAllocFromExpHeap
 *
 * MEMAllocFromExpHeapEx with 4-byte alignment.
 */
static inline void* MEMAllocFromExpHeap(MEMHeapHandle heap, u32 size) {
    return MEMAllocFromExpHeapEx(heap, size, 4);
}

#ifdef __cplusplus
}
#endif

#endif
