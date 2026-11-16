// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h (RwResEntry, the resources API)
#ifndef RWSDK_PLCORE_BARESOUR_H
#define RWSDK_PLCORE_BARESOUR_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The core error the resources arena raises when it cannot hold a block. Belongs in baerr.h's
 * RwErrorCodePlugin_errcore; name invented (the code sorts between E_RW_ENDOFSTREAM and
 * E_RW_INVSTREAMACCESSTYPE in that alphabetical list).
 */
#define E_RW_INSUFFICIENTRESOURCES 12

typedef struct RwResEntry RwResEntry;

/* Called before a resource entry is freed. */
typedef void (*RwResEntryDestroyNotify)(RwResEntry* resEntry);

/* RwResEntry: a block of the resources arena, owned by an object that may lose it at any time. */
struct RwResEntry {
    RwLLLink link;                          /* 0x0 */
    RwInt32 size;                           /* 0x8: bytes after the header */
    void* owner;                            /* 0xC */
    RwResEntry** ownerRef;                  /* 0x10: cleared when the entry is freed */
    RwResEntryDestroyNotify destroyNotify;  /* 0x14 */
};

/*
 * The resource arena (the SDK's
 * rwResources ends at usedEntries; requestedSize is this build's). Entries allocated since the last
 * _rwResourcesPurge are on the used list; older ones are on the free list, the first to be freed
 * when the arena runs out of room. The two lists swap roles at every purge.
 */
typedef struct rwResources {
    RwUInt32 maxSize;           /* 0x0: the arena's size */
    RwUInt32 currentSize;       /* 0x4: bytes in entries */
    RwUInt32 reusageSize;       /* 0x8: bytes of used entries freed to make room since the last purge */
    void* memHeap;              /* 0xC: the arena's resource heap; NULL without an arena */
    RwLinkList entriesA;        /* 0x10 */
    RwLinkList entriesB;        /* 0x18 */
    RwLinkList* freeEntries;    /* 0x20: entriesA or entriesB */
    RwLinkList* usedEntries;    /* 0x24: the other one */
    RwUInt32 requestedSize;     /* 0x28: bytes requested since the last purge */
} rwResources;

/* The resources module's globals. */
typedef struct rwResourcesGlobals {
    rwResources res;            /* 0x0 */
} rwResourcesGlobals;

/* The resources module's registration (offset of its globals in the engine instance). */
extern RwModuleInfo resourcesModule;

#define RWRESOURCESGLOBAL(var) \
    (RWPLUGINOFFSET(rwResourcesGlobals, RwEngineInstance, resourcesModule.globalsOffset)->var)

/* Marks an entry as used again: one that is on a list moves to the front of the used list. */
#define RwResourcesUseResEntry(entry)                                           \
    MACRO_START {                                                               \
        if ((entry)->link.next) {                                               \
            rwLinkListRemoveLLLink(&(entry)->link);                             \
            rwLinkListAddLLLink(RWRESOURCESGLOBAL(res.usedEntries), &(entry)->link); \
        }                                                                       \
    } MACRO_STOP

void* _rwResourcesOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwResourcesClose(void* instance, RwInt32 offset, RwInt32 size);
RwBool RwResourcesFreeResEntry(RwResEntry* entry);
void _rwResourcesPurge(void);
RwResEntry* RwResourcesAllocateResEntry(void* owner, RwResEntry** ownerRef, RwInt32 size,
                                        RwResEntryDestroyNotify destroyNotify);
RwBool RwResourcesEmptyArena(void);

#ifdef __cplusplus
}
#endif

#endif
