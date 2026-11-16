// Layouts: Gormiti DWARF (p2heap.c).
#ifndef RWSDK_PIPE_P2_P2HEAP_H
#define RWSDK_PIPE_P2_P2HEAP_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The pipeline heap is a chain of super blocks. Each super block is carved into blocks, every one
 * behind a 32-byte header; the headers form a doubly linked list in address order, and a block
 * that is free is also listed in the heap's array of free blocks.
 */
typedef struct rxHeapFreeBlock rxHeapFreeBlock;
typedef struct rxHeapBlockHeader rxHeapBlockHeader;
typedef struct rxHeapSuperBlockDescriptor rxHeapSuperBlockDescriptor;

/* One free block: its size in bytes and its header. */
struct rxHeapFreeBlock {
    RwUInt32 size;                          /* 0x00 */
    rxHeapBlockHeader* ptr;                 /* 0x04 */
};

/* The header in front of every block; freeEntry is NULL while the block is in use. Size 0x20. */
struct rxHeapBlockHeader {
    rxHeapBlockHeader* prev;                /* 0x00 */
    rxHeapBlockHeader* next;                /* 0x04 */
    RwUInt32 size;                          /* 0x08: bytes after the header */
    rxHeapFreeBlock* freeEntry;             /* 0x0C */
    RwUInt32 pad[4];                        /* 0x10 */ // pad: Gormiti DWARF rxHeapBlockHeader member
};

/* A super block: the block of memory the heap hands out blocks from, and the next super block. */
struct rxHeapSuperBlockDescriptor {
    void* start;                            /* 0x00: 128-byte aligned, inside the allocation */
    RwUInt32 size;                          /* 0x04 */
    rxHeapSuperBlockDescriptor* next;       /* 0x08 */
};

/* RxHeap: temporary execution heap for pipeline clusters. Size 0x1C. */
typedef struct RxHeap {
    RwUInt32 superBlockSize;                /* 0x00 */
    rxHeapSuperBlockDescriptor* head;       /* 0x04 */
    rxHeapBlockHeader* headBlock;           /* 0x08 */
    rxHeapFreeBlock* freeBlocks;            /* 0x0C */
    RwUInt32 entriesAlloced;                /* 0x10 */
    RwUInt32 entriesUsed;                   /* 0x14 */
    RwBool dirty;                           /* 0x18 */
} RxHeap;

RxHeap* RxHeapCreate(RwUInt32 size);
void RxHeapDestroy(RxHeap* heap);
RwBool _rxHeapReset(RxHeap* heap);

/*
 * Makes every block of the heap free again, unless no block has been handed out since the last
 * reset (Gormiti Debug: RxHeapCreate tests dirty around the _rxHeapReset call).
 */
#define RxHeapReset(heap) (!((heap)->dirty) ? TRUE : _rxHeapReset(heap))

/* Returns a block to the pipeline heap. */
void RxHeapFree(RxHeap* heap, void* block);

#ifdef __cplusplus
}
#endif

#endif
