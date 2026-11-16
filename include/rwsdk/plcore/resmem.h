#ifndef RWSDK_PLCORE_RESMEM_H
#define RWSDK_PLCORE_RESMEM_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Blocks of a resource heap start on, and are sized in, steps of this many bytes. Name invented. */
#define rwRESHEAPALIGN 32

/* A block's flag: the block is allocated. Name invented. */
#define rwRESHEAPBLOCKUSED 0x1

struct rwResHeap;

/*
 * rwResHeapBlock: the header in front of every block of a resource heap; the blocks form a list
 * in address order. Name invented.
 */
typedef struct rwResHeapBlock {
    struct rwResHeap* heap;         /* 0x0: the heap the block belongs to */
    struct rwResHeapBlock* next;    /* 0x4: the block after this one */
    struct rwResHeapBlock* prev;    /* 0x8: the block before this one */
    RwUInt32 size;                  /* 0xC: bytes after the header */
    RwUInt32 flags;                 /* 0x10: rwRESHEAPBLOCKUSED */
    RwUInt32 field_0x14[3];         /* 0x14: not used here; the header fills rwRESHEAPALIGN bytes */
} rwResHeapBlock;

/* rwResHeap: a heap of blocks inside a caller's memory area, at its start. Name invented. */
typedef struct rwResHeap {
    rwResHeapBlock* firstBlock;     /* 0x0 */
    rwResHeapBlock* firstFree;      /* 0x4: no free block lies before it */
} rwResHeap;

RwBool _rwResHeapInit(void* ptr, RwUInt32 size);
RwBool _rwResHeapClose(void* ptr);
void _rwResHeapFree(void* ptr);
void* _rwResHeapAlloc(void* ptr, RwUInt32 size);

#ifdef __cplusplus
}
#endif

#endif
