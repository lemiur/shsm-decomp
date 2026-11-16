// Reference: tgsm/pbg src/rwsdk/src/plcore/batkreg.c
/*
 * batkreg.c: plugin registries. A plugin registers its data with an object type's registry; the
 * registry lays the data out after the object's own and runs the plugins' constructors,
 * destructors and copy callbacks on every object. Registry entries come from one free list;
 * when the engine's free-list allocator is replaced, the registries are also listed in
 * toolkitNonFLRegList so that their entries can be released one by one on close.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>

/* The free list of registry entries; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList toolkitRegEntriesSpace;

/* The free list of registry entries. */
static RwFreeList* toolkitRegEntries;

/* The number of registries in toolkitNonFLRegList. */
static RwUInt32 numRegToolkits;

/* The registries that have plugins, kept only when the free-list allocator is replaced. */
static RwPluginRegistry** toolkitNonFLRegList;

/* Entries per block of the free list, and the number of blocks allocated at open. */
static RwInt32 _rwPluginRegFreeListBlockSize = 64;
static RwInt32 _rwPluginRegListPreallocBlocks = 1;

/* Creates the free list of registry entries. */
RwBool _rwPluginRegistryOpen(void) {
    toolkitRegEntries = RwFreeListCreateAndPreallocateSpace(
        sizeof(RwPluginRegEntry), _rwPluginRegFreeListBlockSize, 4, _rwPluginRegListPreallocBlocks,
        &toolkitRegEntriesSpace, rwMEMHINTDUR_GLOBAL);
    if (toolkitRegEntries == NULL) {
        return FALSE;
    }

    numRegToolkits = 0;

    return TRUE;
} // 0x802A58BC

/* RwFreeListForAllUsed callback: empties the entry's registry, then frees the entry. */
static void rwDestroyEntry(void* mem, void* data) {
    RwPluginRegEntry* entry = (RwPluginRegEntry*)mem;

    if (entry->parentRegistry->firstRegEntry != NULL) {
        entry->parentRegistry->sizeOfStruct = entry->parentRegistry->origSizeOfStruct;
        entry->parentRegistry->firstRegEntry = NULL;
        entry->parentRegistry->lastRegEntry = NULL;
    }

    RwFreeListFree((RwFreeList*)data, entry);
} // 0x802A5918

/* Frees every registry entry and empties every registry. */
RwBool _rwPluginRegistryClose(void) {
    if (toolkitRegEntries != NULL) {
        RwFreeListForAllUsed(toolkitRegEntries, rwDestroyEntry, toolkitRegEntries);

        if (RWSRCGLOBAL(memoryAlloc) != _rwFreeListAllocReal) {
            RwUInt32 i;

            for (i = 0; i < numRegToolkits; i++) {
                RwPluginRegistry* reg = NULL;
                RwPluginRegEntry* entry = toolkitNonFLRegList[i]->firstRegEntry;

                if (entry != NULL) {
                    reg = entry->parentRegistry;
                }

                while (entry != NULL) {
                    RwPluginRegEntry* next = entry->nextRegEntry;

                    RwFreeListFree(NULL, entry);
                    entry = next;
                }

                if (reg != NULL && reg->firstRegEntry != NULL) {
                    reg->sizeOfStruct = reg->origSizeOfStruct;
                    reg->firstRegEntry = NULL;
                    reg->lastRegEntry = NULL;
                }
            }

            if (toolkitNonFLRegList != NULL) {
                RwFree(toolkitNonFLRegList);
                toolkitNonFLRegList = NULL;
            }
        }

        RwFreeListDestroy(toolkitRegEntries);
        toolkitRegEntries = NULL;
    }

    return TRUE;
} // 0x802A5960

/* The constructor of a plugin registered without one: does nothing. */
static void* PluginDefaultConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
} // 0x802A5A7C

/* The destructor of a plugin registered without one: does nothing. */
static void* PluginDefaultDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
} // 0x802A5A80

/* The copy callback of a plugin registered without one: does nothing. */
static void* PluginDefaultCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject,
                               RwInt32 sizeInObject) {
    return dstObject;
} // 0x802A5A84

/* Returns the data offset of the plugin pluginID in reg's objects, or -1 if it is not registered. */
RwInt32 _rwPluginRegistryGetPluginOffset(const RwPluginRegistry* reg, RwUInt32 pluginID) {
    const RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->pluginID == pluginID) {
            return entry->offset;
        }
        entry = entry->nextRegEntry;
    }

    return -1;
} // 0x802A5A88

/*
 * Registers size bytes of plugin data with reg's objects; returns the data's offset in an object,
 * or -1. Plugins can only be registered while no engine instance is open.
 */
RwInt32 _rwPluginRegistryAddPlugin(RwPluginRegistry* reg, RwInt32 size, RwUInt32 pluginID,
                                   RwPluginObjectConstructor constructCB,
                                   RwPluginObjectDestructor destructCB,
                                   RwPluginObjectCopy copyCB) {
    RwPluginRegEntry* entry;
    RwInt32 newSize;

    if (toolkitRegEntries == NULL) {
        return -1;
    }

    if (_rwGetNumEngineInstances() != 0) {
        RWERROR((E_RW_PLUGININIT));
        return -1;
    }

    if (RWSRCGLOBAL(memoryAlloc) != _rwFreeListAllocReal) {
        RwUInt32 i;

        for (i = 0; i < numRegToolkits; i++) {
            if (reg == toolkitNonFLRegList[i]) {
                break;
            }
        }

        if (numRegToolkits == i) {
            RwPluginRegistry** regList =
                (RwPluginRegistry**)RwMalloc((numRegToolkits + 1) * sizeof(RwPluginRegistry*),
                                             rwMEMHINTDUR_GLOBAL);
            RwUInt32 j = 0;

            if (toolkitNonFLRegList != NULL) {
                for (; j < numRegToolkits; j++) {
                    regList[j] = toolkitNonFLRegList[j];
                }
                RwFree(toolkitNonFLRegList);
            }

            regList[j] = reg;
            toolkitNonFLRegList = regList;
            numRegToolkits++;
        }
    }

    entry = reg->firstRegEntry;
    while (entry != NULL) {
        if (entry->pluginID == pluginID) {
            RWERROR((E_RW_PLUGININIT));
            return entry->offset;
        }
        entry = entry->nextRegEntry;
    }

    newSize = reg->sizeOfStruct + ((size + 3) & ~3);
    if (reg->maxSizeOfStruct != 0 && newSize > reg->maxSizeOfStruct) {
        return -1;
    }

    entry = (RwPluginRegEntry*)RwFreeListAlloc(toolkitRegEntries, rwMEMHINTDUR_GLOBAL);
    if (entry != NULL) {
        entry->offset = reg->sizeOfStruct;
        reg->sizeOfStruct = newSize;
        entry->size = size;
        entry->pluginID = pluginID;
        entry->readCB = NULL;
        entry->writeCB = NULL;
        entry->getSizeCB = NULL;
        entry->alwaysCB = NULL;
        entry->rightsCB = NULL;
        entry->constructCB = constructCB ? constructCB : PluginDefaultConstructor;
        entry->destructCB = destructCB ? destructCB : PluginDefaultDestructor;
        entry->copyCB = copyCB ? copyCB : PluginDefaultCopy;
        entry->errorStrCB = NULL;
        entry->nextRegEntry = NULL;
        entry->prevRegEntry = NULL;
        entry->parentRegistry = reg;

        if (reg->firstRegEntry == NULL) {
            reg->firstRegEntry = entry;
            reg->lastRegEntry = entry;
        } else {
            reg->lastRegEntry->nextRegEntry = entry;
            entry->prevRegEntry = reg->lastRegEntry;
            reg->lastRegEntry = entry;
        }

        return entry->offset;
    }

    return -1;
} // 0x802A5AB8

/*
 * Runs every plugin's constructor on object, in registration order. If one fails, the plugins
 * constructed before it are destroyed and NULL is returned.
 */
const RwPluginRegistry* _rwPluginRegistryInitObject(const RwPluginRegistry* reg, void* object) {
    const RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->constructCB(object, entry->offset, entry->size) == NULL) {
            entry = entry->prevRegEntry;
            while (entry != NULL) {
                entry->destructCB(object, entry->offset, entry->size);
                entry = entry->prevRegEntry;
            }

            return NULL;
        }
        entry = entry->nextRegEntry;
    }

    return reg;
} // 0x802A5D70

/* Runs every plugin's destructor on object, in reverse registration order. */
const RwPluginRegistry* _rwPluginRegistryDeInitObject(const RwPluginRegistry* reg, void* object) {
    const RwPluginRegEntry* entry = reg->lastRegEntry;

    while (entry != NULL) {
        entry->destructCB(object, entry->offset, entry->size);
        entry = entry->prevRegEntry;
    }

    return reg;
} // 0x802A5E18

/* Copies every plugin's data from srcObject to dstObject with the plugins' copy callbacks. */
const RwPluginRegistry* _rwPluginRegistryCopyObject(const RwPluginRegistry* reg, void* dstObject,
                                                    const void* srcObject) {
    const RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        entry->copyCB(dstObject, srcObject, entry->offset, entry->size);
        entry = entry->nextRegEntry;
    }

    return reg;
} // 0x802A5E84
