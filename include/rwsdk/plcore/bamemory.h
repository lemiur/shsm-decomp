// Reference: tgsm/pbg include/rwsdk/plcore/bamemory.h; bfbbdecomp/bfbb include/rwsdk/rwplcore.h (link-list macros)
#ifndef RWSDK_PLCORE_BAMEMORY_H
#define RWSDK_PLCORE_BAMEMORY_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwLLLink: a link of a doubly linked list. */
typedef struct RwLLLink {
    struct RwLLLink* next;  /* 0x0 */
    struct RwLLLink* prev;  /* 0x4 */
} RwLLLink;

#define rwLLLinkGetData(linkvar, type, entry)     ((type*)(((RwUInt8*)(linkvar)) - offsetof(type, entry)))
#define rwLLLinkGetConstData(linkvar, type, entry)     ((const type*)(((const RwUInt8*)(linkvar)) - offsetof(type, entry)))
#define rwLLLinkGetNext(linkvar) ((linkvar)->next)
#define rwLLLinkInitialize(linkvar)     ((linkvar)->prev = (RwLLLink*)NULL, (linkvar)->next = (RwLLLink*)NULL)

/* RwLinkList: a doubly linked list, as its sentinel link. */
typedef struct RwLinkList {
    RwLLLink link;          /* 0x0 */
} RwLinkList;

#define rwLinkListInitialize(list)     ((list)->link.next = ((RwLLLink*)(list)), (list)->link.prev = ((RwLLLink*)(list)))
#define rwLinkListAddLLLink(list, linkvar)     ((linkvar)->next = (list)->link.next, (linkvar)->prev = (&(list)->link),      ((list)->link.next)->prev = (linkvar), (list)->link.next = (linkvar))
#define rwLinkListRemoveLLLink(linkvar)     (((linkvar)->prev)->next = (linkvar)->next, ((linkvar)->next)->prev = (linkvar)->prev)
#define rwLinkListEmpty(list) (((list)->link.next) == (&(list)->link))
#define rwLinkListGetFirstLLLink(list) ((list)->link.next)
#define rwLinkListGetLastLLLink(list) ((list)->link.prev)
#define rwLinkListGetTerminator(list) (&((list)->link))

/*
 * Memory hints (RenderWare 3.7): every allocation states how long the block lives, whether it
 * may be resized, and the module that asks for it in the low 16 bits. RwMemoryHintDuration and its
 * members are the RW 3.7 user guide's (electronicarts/RenderWare3Docs).
 */
typedef enum RwMemoryHintDuration {
    rwMEMHINTDUR_NADURATION = 0x00000000,
    rwMEMHINTDUR_FUNCTION = 0x00010000,
    rwMEMHINTDUR_FRAME = 0x00020000,
    rwMEMHINTDUR_EVENT = 0x00030000,
    rwMEMHINTDUR_GLOBAL = 0x00040000,
    rwMEMHINTDUR_MASK = 0x00FF0000,
    rwMEMHINTDURFORCEENUMSIZEINT = 0x7FFFFFFF
} RwMemoryHintDuration;

/* Memory hint flags: the block may be passed to RwRealloc. */
typedef enum RwMemoryHintFlag {
    rwMEMHINTFLAG_RESIZABLE = 0x01000000,
    rwMEMHINTFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwMemoryHintFlag;

/* RwMemoryFunctions: the allocator the engine was initialised with. */
typedef struct RwMemoryFunctions {
    void* (*rwmalloc)(size_t size, RwUInt32 hint);                  /* 0x0 */
    void (*rwfree)(void* mem);                                      /* 0x4 */
    void* (*rwrealloc)(void* mem, size_t newSize, RwUInt32 hint);   /* 0x8 */
    void* (*rwcalloc)(size_t numObj, size_t sizeObj, RwUInt32 hint); /* 0xC */
} RwMemoryFunctions;

/*
 * RwFreeList: a pool of fixed-size entries, allocated in blocks. Each block is an RwLLLink in
 * blockList, then a bitmap of heapSize bytes (one bit per entry, set while the entry is in use,
 * most significant bit first), then the entries, starting at the next multiple of alignment.
 */
typedef struct RwFreeList {
    RwUInt32 entrySize;         /* 0x0: rounded up to a multiple of alignment */
    RwUInt32 entriesPerBlock;   /* 0x4 */
    RwUInt32 heapSize;          /* 0x8: bytes of a block's bitmap */
    RwUInt32 alignment;         /* 0xC */
    RwLinkList blockList;       /* 0x10 */
    RwUInt32 flags;             /* 0x18: rwFREELISTFLAG_* */
    RwLLLink link;              /* 0x1C: in the list of every free list */
} RwFreeList; /* size 0x24 */

/* RwFreeList flags. */
#define rwFREELISTFLAG_STATIC 0x1       /* the RwFreeList is the caller's space: never freed */
#define rwFREELISTFLAG_FREEBLOCKS 0x2   /* a block is freed when its last entry is */

/* The engine's free-list entry allocator and release hooks (RwGlobals memoryAlloc, memoryFree). */
typedef void* (*RwMemoryAllocFn)(RwFreeList* freeList, RwUInt32 hint);
typedef RwFreeList* (*RwMemoryFreeFn)(RwFreeList* freeList, void* mem);

/* RwFreeListCallBack: called by RwFreeListForAllUsed for each entry in use, with the caller's data. */
typedef void (*RwFreeListCallBack)(void* mem, void* data);

void _rwFreeListEnable(RwBool enable);
RwFreeList* RwFreeListCreate(RwInt32 entrySize, RwInt32 entriesPerBlock, RwInt32 alignment,
                             RwUInt32 hint);
RwFreeList* RwFreeListCreateAndPreallocateSpace(RwInt32 entrySize, RwInt32 entriesPerBlock,
                                                RwInt32 alignment, RwInt32 numBlocksToPreallocate,
                                                RwFreeList* inPlaceSpace, RwUInt32 hint);
RwBool RwFreeListDestroy(RwFreeList* freeList);
void* _rwFreeListAllocReal(RwFreeList* freeList, RwUInt32 hint);
RwFreeList* _rwFreeListFreeReal(RwFreeList* freeList, void* mem);
RwInt32 RwFreeListPurge(RwFreeList* freeList);
RwFreeList* RwFreeListForAllUsed(RwFreeList* freeList, RwFreeListCallBack fpCallBack, void* data);
RwInt32 RwFreeListPurgeAllFreeLists(void);
RwBool _rwMemoryOpen(const RwMemoryFunctions* memoryFuncs);
void _rwMemoryClose(void);
RwMemoryFunctions* RwOsGetMemoryInterface(void);

#ifdef __cplusplus
}
#endif

#endif
