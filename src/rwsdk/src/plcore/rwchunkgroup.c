// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
/*
 * rwchunkgroup.c: the chunk group module (module ID rwID_CHUNKGROUPMODULE). Its globals hold the
 * free list of chunk groups; nothing in this build allocates from it.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/rwchunkgroup.h>

/* The free list of chunk groups; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rwChunkGroupFreeList;

/* The chunk group module's registration (offset of its globals in the engine instance). */
static RwModuleInfo chunkGroupModule;

/* Chunk groups per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwChunkGroupFreeListBlockSize = 16;
static RwInt32 _rwChunkGroupFreeListPreallocBlocks = 1;

/* The chunk group module's globals (RWCHUNKGROUPGLOBAL invented). */
typedef struct rwChunkGroupGlobals {
    RwFreeList* groupFList;  /* 0x0 */
} rwChunkGroupGlobals;

#define RWCHUNKGROUPGLOBAL(var) \
    (RWPLUGINOFFSET(rwChunkGroupGlobals, RwEngineInstance, chunkGroupModule.globalsOffset)->var)

/* Opens the chunk group module: creates the chunk group free list. */
void* _rwChunkGroupOpen(void* instance, RwInt32 offset, RwInt32 size) {
    chunkGroupModule.globalsOffset = offset;

    RWCHUNKGROUPGLOBAL(groupFList) = RwFreeListCreateAndPreallocateSpace(
        sizeof(RwChunkGroup), _rwChunkGroupFreeListBlockSize, sizeof(RwUInt32),
        _rwChunkGroupFreeListPreallocBlocks, &_rwChunkGroupFreeList,
        rwMEMHINTDUR_GLOBAL | rwID_CHUNKGROUPMODULE);
    if (RWCHUNKGROUPGLOBAL(groupFList) == NULL) {
        return NULL;
    }

    chunkGroupModule.numInstances++;

    return instance;
} // 0x802A6CE4

/* Closes the chunk group module: destroys the chunk group free list. */
void* _rwChunkGroupClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWCHUNKGROUPGLOBAL(groupFList) != NULL) {
        RwFreeListDestroy(RWCHUNKGROUPGLOBAL(groupFList));
    }

    chunkGroupModule.numInstances--;

    return instance;
} // 0x802A6D70
