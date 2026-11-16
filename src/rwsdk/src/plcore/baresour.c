// unit name unconfirmed: tgsm/pbg lists a baresour.c here in the plcore link order (no source); no
// string evidence in this binary names the file.
// Reference: tgsm/pbg config/GPLE9G/symbols.txt (function and module names)
/*
 * baresour.c: the resources arena. Objects keep data they can rebuild (instanced geometry, say) in
 * resource entries; when the arena is full the oldest entries are freed, and their owners are told
 * through ownerRef and the destroy callback.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/resmem.h>
#include <rwsdk/plcore/baresour.h>

RwModuleInfo resourcesModule;

/* Sets up the resources globals with an arena of size bytes (none for 0). */
static rwResourcesGlobals* ResourcesInit(rwResourcesGlobals* globals, RwUInt32 size) {
    if (size != 0) {
        globals->res.memHeap = RwMalloc(size, rwMEMHINTDUR_GLOBAL | rwID_RESOURCESMODULE);
        if (globals->res.memHeap == NULL) {
            RWERROR((E_RW_NOMEM, size));
            return NULL;
        }

        if (!_rwResHeapInit(globals->res.memHeap, size)) {
            RwFree(globals->res.memHeap);
            RWERROR((E_RW_INSUFFICIENTRESOURCES, 0));
            return NULL;
        }
    } else {
        globals->res.memHeap = NULL;
    }

    rwLinkListInitialize(&globals->res.entriesA);
    rwLinkListInitialize(&globals->res.entriesB);
    globals->res.usedEntries = &globals->res.entriesA;
    globals->res.freeEntries = &globals->res.entriesB;

    globals->res.maxSize = size;
    globals->res.currentSize = 0;
    globals->res.reusageSize = 0;
    globals->res.requestedSize = 0;

    return globals;
} // 0x802A40A4

/* Opens the resources module: creates the arena with the size RwEngineInit was given. */
void* _rwResourcesOpen(void* instance, RwInt32 offset, RwInt32 size) {
    resourcesModule.globalsOffset = offset;

    if (ResourcesInit(RWPLUGINOFFSET(rwResourcesGlobals, RwEngineInstance, offset), RWSRCGLOBAL(resArenaInitSize)) ==
        NULL) {
        return NULL;
    }

    resourcesModule.numInstances++;

    return instance;
} // 0x802A41C8

/* Closes the resources module: frees every entry and the arena. */
void* _rwResourcesClose(void* instance, RwInt32 offset, RwInt32 size) {
    RwResourcesEmptyArena();

    if (RWRESOURCESGLOBAL(res.memHeap) != NULL) {
        _rwResHeapClose(RWRESOURCESGLOBAL(res.memHeap));
        RwFree(RWRESOURCESGLOBAL(res.memHeap));
        RWRESOURCESGLOBAL(res.memHeap) = NULL;
    }

    resourcesModule.numInstances--;

    return instance;
} // 0x802A4228

/* Frees a resource entry: tells its owner, then gives its memory back. */
RwBool RwResourcesFreeResEntry(RwResEntry* entry) {
    if (entry->destroyNotify != NULL) {
        entry->destroyNotify(entry);
    }

    if (entry->ownerRef != NULL) {
        *entry->ownerRef = NULL;
    }

    if (entry->link.next != NULL) {
        rwLinkListRemoveLLLink(&entry->link);
        RWRESOURCESGLOBAL(res.currentSize) -= entry->size;
        _rwResHeapFree(entry);
    } else {
        RwFree(entry);
    }

    return TRUE;
} // 0x802A42B4

/*
 * Starts a new period of use: every entry becomes free to reuse. The free entries join the end
 * of the used list, which becomes the free list, and the emptied list takes new entries.
 */
void _rwResourcesPurge(void) {
    RwLinkList* usedList;
    RwLinkList* freeList;
    RwLLLink* freeFirst;
    RwLLLink* freeLast;

    freeList = RWRESOURCESGLOBAL(res.freeEntries);
    usedList = RWRESOURCESGLOBAL(res.usedEntries);
    freeFirst = rwLinkListGetFirstLLLink(freeList);

    if (freeFirst != rwLinkListGetTerminator(freeList)) {
        if (rwLinkListEmpty(usedList)) {
            usedList->link.next = freeFirst;
            freeFirst->prev = &usedList->link;
            freeLast = rwLinkListGetLastLLLink(freeList);
            usedList->link.prev = freeLast;
            freeLast->next = &usedList->link;
            rwLinkListInitialize(freeList);
        } else {
            RwLLLink* usedLast = rwLinkListGetLastLLLink(usedList);

            usedLast->next = freeFirst;
            freeFirst->prev = usedLast;
            freeLast = rwLinkListGetLastLLLink(freeList);
            freeLast->next = &usedList->link;
            usedList->link.prev = freeLast;
            rwLinkListInitialize(freeList);
        }
    }

    RWRESOURCESGLOBAL(res.usedEntries) = freeList;
    RWRESOURCESGLOBAL(res.freeEntries) = usedList;
    RWRESOURCESGLOBAL(res.reusageSize) = 0;
    RWRESOURCESGLOBAL(res.requestedSize) = 0;
} // 0x802A4364


/*
 * Allocates a resource entry of size bytes for owner, freeing the oldest entries while the arena
 * has no room; *ownerRef is set to the entry (NULL on failure).
 */
RwResEntry* RwResourcesAllocateResEntry(void* owner, RwResEntry** ownerRef, RwInt32 size,
                                        RwResEntryDestroyNotify destroyNotify) {
    RwUInt32 entrySize = size + sizeof(RwResEntry);
    RwBool outOfEntries = FALSE;

    RWRESOURCESGLOBAL(res.requestedSize) += size;

    while (!outOfEntries) {
        RwResEntry* entry = (RwResEntry*)_rwResHeapAlloc(RWRESOURCESGLOBAL(res.memHeap), entrySize);
        RwLLLink* oldest;

        if (entry != NULL) {
            rwLinkListAddLLLink(RWRESOURCESGLOBAL(res.usedEntries), &entry->link);
            entry->owner = owner;
            entry->size = size;
            entry->ownerRef = ownerRef;
            entry->destroyNotify = destroyNotify;
            RWRESOURCESGLOBAL(res.currentSize) += size;

            if (ownerRef != NULL) {
                *ownerRef = entry;
            }

            return entry;
        }

        oldest = rwLinkListGetLastLLLink(RWRESOURCESGLOBAL(res.freeEntries));
        if (oldest != rwLinkListGetTerminator(RWRESOURCESGLOBAL(res.freeEntries))) {
            RwResourcesFreeResEntry((RwResEntry*)oldest);
        } else {
            oldest = rwLinkListGetLastLLLink(RWRESOURCESGLOBAL(res.usedEntries));
            if (oldest != rwLinkListGetTerminator(RWRESOURCESGLOBAL(res.usedEntries))) {
                RWRESOURCESGLOBAL(res.reusageSize) += ((RwResEntry*)oldest)->size;
                RwResourcesFreeResEntry((RwResEntry*)oldest);
            } else {
                outOfEntries = TRUE;
            }
        }
    }

    if (ownerRef != NULL) {
        *ownerRef = NULL;
    }

    RWERROR((E_RW_INSUFFICIENTRESOURCES, size));
    return NULL;
} // 0x802A4418

/* Frees every resource entry. */
RwBool RwResourcesEmptyArena(void) {
    RwLLLink* cur;
    RwLLLink* end;

    /* Freeing an entry unlinks it and calls back its owner: each pass starts again at the list's head. */

    cur = rwLinkListGetFirstLLLink(&RWRESOURCESGLOBAL(res.entriesA));
    end = rwLinkListGetTerminator(&RWRESOURCESGLOBAL(res.entriesA));
    while (cur != end) {
        RwResourcesFreeResEntry((RwResEntry*)cur);
        cur = rwLinkListGetFirstLLLink(&RWRESOURCESGLOBAL(res.entriesA));
        end = rwLinkListGetTerminator(&RWRESOURCESGLOBAL(res.entriesA));
    }

    cur = rwLinkListGetFirstLLLink(&RWRESOURCESGLOBAL(res.entriesB));
    end = rwLinkListGetTerminator(&RWRESOURCESGLOBAL(res.entriesB));
    while (cur != end) {
        RwResourcesFreeResEntry((RwResEntry*)cur);
        cur = rwLinkListGetFirstLLLink(&RWRESOURCESGLOBAL(res.entriesB));
        end = rwLinkListGetTerminator(&RWRESOURCESGLOBAL(res.entriesB));
    }

    rwLinkListInitialize(&RWRESOURCESGLOBAL(res.entriesA));
    rwLinkListInitialize(&RWRESOURCESGLOBAL(res.entriesB));
    RWRESOURCESGLOBAL(res.reusageSize) = 0;

    return TRUE;
} // 0x802A46B8
