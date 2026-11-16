// Unit name and boundary: Gormiti link map (RpSkin2.a rescache.obj; its source there is gcn/rescache.c)
// Names, parameters, locals and statement order: Gormiti DWARF (RpSkin2.a rescache.obj), Final and Debug builds.
/*
 * rescache.c: RpSkin2's cache of skinned vertex data. A skinned atomic's vertices are written each
 * frame into a resource entry of the arena (the skin res entry) that the atomic's instanced vertex
 * buffer then draws from. The cache lists one entry per such resource, oldest use first: an atomic
 * asks for a resource that is free of the GX pipe (its draw sync token reached) and was not used
 * this frame, taking the one it already owns when it has one, and every use moves the entry to the
 * end of the list.
 */
#include <stdio.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dldevice.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/world/gcpipe.h>

/* The cache, least recently used entry first. */
static SkinResCacheEntry* sCacheList;

/* The skin data of an atomic. */
#define RPSKINATOMICGETDATA(atomic) \
    (RWPLUGINOFFSET(SkinAtomicData, (atomic), _rpSkinGlobals.atomicOffset))

/*
 * The vertex arrays of a skin res entry start on a 32 byte boundary a little way after its header
 * (the GX pipe reads them by DMA); the word before them points back to the entry, whose vertex
 * buffer header holds the draw sync token of the last draw.
 */
#define SKINRESENTRYGETARRAYBASE(resEntry) ((void*)(((unsigned int)(resEntry) + 0x3D) & ~0x1F))
#define SKINRESENTRYGETVBHEADER(resEntry) \
    ((RxGameCubeVertexBuffer*)(((RwResEntry**)SKINRESENTRYGETARRAYBASE(resEntry))[-1] + 1))

/*
 * ==================== _clSkinCache_FlagUsed ====================
 * Marks the entry used this frame: it leaves its place in the list for the end of it.
 */
static void _clSkinCache_FlagUsed(SkinResCacheEntry* CacheEntry) {
    SkinResCacheEntry* Current;

    if (sCacheList == CacheEntry) {
        sCacheList = sCacheList->Next;
    } else {
        Current = sCacheList;
        while (Current->Next != NULL && Current->Next != CacheEntry) {
            Current = Current->Next;
        }
        if (Current->Next != NULL && Current->Next == CacheEntry) {
            Current->Next = Current->Next->Next;
        }
    }

    CacheEntry->Next = NULL;

    Current = sCacheList;
    if (Current != NULL) {
        while (Current->Next != NULL) {
            Current = Current->Next;
        }
        Current->Next = CacheEntry;
    } else {
        sCacheList = CacheEntry;
    }

    CacheEntry->lastFrameID = _RwSysFrameID;
} // 0x802EAAFC

/*
 * ==================== _clSkinCache_UnRegisterSkinRes ====================
 * Drops a skin res entry from the cache and frees its cache entry, leaving the vertex buffer
 * and the atomic that used it without a resource. Gormiti's copy of this function is 0x18 bytes
 * shorter than retail's (another revision), so retail's bytes decide the body.
 */
void _clSkinCache_UnRegisterSkinRes(RwResEntry* SkinRes) {
    SkinResCacheEntry* CacheEntry;
    SkinResCacheEntry* Current;
    SkinAtomicData* skinAtomicData;

    CacheEntry = NULL;

    if (sCacheList != NULL) {
        if (sCacheList->skinResEntry == SkinRes) {
            CacheEntry = sCacheList;
            sCacheList = CacheEntry->Next;
        } else {
            Current = sCacheList;
            while (Current->Next != NULL && Current->Next->skinResEntry != SkinRes) {
                Current = Current->Next;
            }
            if (Current->Next != NULL && Current->Next->skinResEntry == SkinRes) {
                CacheEntry = Current->Next;
                Current->Next = Current->Next->Next;
            }
        }
    }

    if (CacheEntry != NULL) {
        if (CacheEntry->VBOwner != NULL) {
            // identity-TODO: retail clears the word at 0xC of the vertex buffer header, which is the first attribute array's pointer.
            CacheEntry->VBOwner->attr[0].array = NULL;
        }

        if (CacheEntry->Atomic != NULL) {
            skinAtomicData = RPSKINATOMICGETDATA(CacheEntry->Atomic);
            skinAtomicData->platformData.skinResEntry = NULL;
        }

        if (SkinRes->ownerRef == &CacheEntry->skinResEntry) {
            SkinRes->ownerRef = NULL;
        }

        RwFree(CacheEntry);
    }
} // 0x802EAB88

/*
 * ==================== _clSkinCache_RepEntryDestroyNotify ====================
 * Called when the arena frees a geometry's resource entry: the cache entries that drew from its
 * vertex buffer forget it.
 */
void _clSkinCache_RepEntryDestroyNotify(RwResEntry* RepEntry) {
    SkinResCacheEntry* Current;
    RxGameCubeVertexBuffer* vbHeader;

    vbHeader = (RxGameCubeVertexBuffer*)(RepEntry + 1);

    for (Current = sCacheList; Current != NULL; Current = Current->Next) {
        if (Current->VBOwner == vbHeader) {
            Current->VBOwner = NULL;
        }
    }
} // 0x802EAC64

/*
 * ==================== _clSkinCache_OrphanSkinRes ====================
 * The skin res entry is being freed: the vertex buffers that drew from it forget it.
 */
void _clSkinCache_OrphanSkinRes(RwResEntry* SkinRes) {
    SkinResCacheEntry* Current;

    for (Current = sCacheList; Current != NULL; Current = Current->Next) {
        if (Current->skinResEntry == SkinRes) {
            Current->VBOwner = NULL;
        }
    }
} // 0x802EAC94

/*
 * ==================== _clSkinCache_GetUsableResource ====================
 * The skin res entry the atomic draws its skinned vertices from this frame. The atomic's own
 * entry is taken back when it is free of the GX pipe; otherwise a new one is allocated.
 */
RwResEntry* _clSkinCache_GetUsableResource(RpAtomic* Atomic, RwResEntry** ownerRef, int size,
                                           RwResEntryDestroyNotify destroyNotify,
                                           RxGameCubeVertexBuffer* vbHeader) {
    SkinResCacheEntry* Current;
    SkinResCacheEntry* CacheEntry;
    RwResEntry* SkinRes;
    int LeastRecentQueuedFrameDif;
    int frameDif;

    CacheEntry = NULL;

    LeastRecentQueuedFrameDif = -1;

    for (Current = sCacheList; Current != NULL && Current->lastFrameID != _RwSysFrameID;
         Current = Current->Next) {
        if (Current->Atomic == Atomic && Current->skinResEntry->size == size) {
            if (Current->skinResEntry != NULL ? _rwDlTokenQueryDone(SKINRESENTRYGETVBHEADER(Current->skinResEntry)->token) : 0) {
                CacheEntry = Current;
                break;
            }

            frameDif = _RwSysFrameID - Current->lastFrameID;
            if (frameDif > LeastRecentQueuedFrameDif) {
                LeastRecentQueuedFrameDif = frameDif;
            }

            if (frameDif < 3) {
                RwResourcesUseResEntry(Current->skinResEntry);
            }
        }
    }

    if (CacheEntry != NULL) {
        _clSkinCache_FlagUsed(CacheEntry);
        CacheEntry->Atomic = Atomic;
        CacheEntry->skinResEntry->owner = Atomic;
        CacheEntry->skinResEntry->destroyNotify = destroyNotify;
        CacheEntry->VBOwner = vbHeader;
        CacheEntry->lastFrameID = _RwSysFrameID;
        CacheEntry->Geometry = Atomic->geometry;
        if (ownerRef != NULL) {
            *ownerRef = CacheEntry->skinResEntry;
        }
        RwResourcesUseResEntry(CacheEntry->skinResEntry);
        return CacheEntry->skinResEntry;
    }

    SkinRes = RwResourcesAllocateResEntry(Atomic, ownerRef, size, destroyNotify);
    if (SkinRes != NULL) {
        CacheEntry = (SkinResCacheEntry*)RwMalloc(sizeof(SkinResCacheEntry), 0);
        CacheEntry->Atomic = Atomic;
        CacheEntry->Geometry = Atomic->geometry;
        CacheEntry->skinResEntry = SkinRes;
        CacheEntry->lastFrameID = _RwSysFrameID;
        CacheEntry->VBOwner = vbHeader;
        SkinRes->ownerRef = &CacheEntry->skinResEntry;
        CacheEntry->Next = NULL;

        if (sCacheList != NULL) {
            Current = sCacheList;
            while (Current->Next != NULL) {
                Current = Current->Next;
            }
            Current->Next = CacheEntry;
        } else {
            sCacheList = CacheEntry;
        }
    }

    return SkinRes;
} // 0x802EACC0

/*
 * ==================== _clSkinCache_CheckResource ====================
 * Whether the atomic still owns its skin res entry and the GX pipe is done with it; an entry
 * that passes is used again this frame.
 */
int _clSkinCache_CheckResource(RpAtomic* Atomic, RwResEntry* SkinRes, RxGameCubeVertexBuffer* vbHeader) {
    SkinResCacheEntry* Current;

    Current = sCacheList;
    if (Current == NULL) {
        return 0;
    }

    for (; Current != NULL && Current->skinResEntry != SkinRes; Current = Current->Next) {
    }

    if (Current == NULL) {
        return 0;
    }

    if (Atomic != Current->Atomic) {
        if (Current->VBOwner == vbHeader) {
            Current->VBOwner = NULL;
        }
        return 0;
    }

    if (Current->skinResEntry != NULL ? _rwDlTokenQueryDone(SKINRESENTRYGETVBHEADER(Current->skinResEntry)->token) : 0) {
        RwResourcesUseResEntry(SkinRes);
        Current->lastFrameID = _RwSysFrameID;
        return 1;
    }

    if (Current->VBOwner == vbHeader) {
        Current->VBOwner = NULL;
    }
    return 0;
} // 0x802EAF98

/*
 * ==================== _clSkinCache_TokenStampResource ====================
 * Stamps the skin res entry the vertex buffer draws from with the current draw sync token.
 */
void _clSkinCache_TokenStampResource(RxGameCubeVertexBuffer* vbHeader) {
    SkinResCacheEntry* Current;
    void* ArrayBase;
    unsigned short* Token;

    for (Current = sCacheList; Current != NULL && Current->VBOwner != vbHeader; Current = Current->Next) {
    }

    if (Current != NULL && Current->skinResEntry != NULL) {
        ArrayBase = SKINRESENTRYGETARRAYBASE(Current->skinResEntry);
        Token = &((RxGameCubeVertexBuffer*)(((RwResEntry**)ArrayBase)[-1] + 1))->token;
        *Token = _RwDlTokenCurrent;
    }
} // 0x802EB10C

/*
 * ==================== _clSkinCache_ReAquireResource ====================
 * The skin res entry the vertex buffer drew from last frame, when it still belongs to the atomic
 * and its geometry. (The name is the original's.)
 */
RwResEntry* _clSkinCache_ReAquireResource(RpAtomic* Atomic, RwResEntry* SkinRes, RxGameCubeVertexBuffer* vbHeader) {
    SkinResCacheEntry* Current;

    Current = sCacheList;
    if (Current == NULL) {
        return NULL;
    }

    for (; Current != NULL && Current->skinResEntry != SkinRes; Current = Current->Next) {
    }

    if (Current == NULL) {
        return NULL;
    }

    if (Atomic != Current->Atomic) {
        if (Current->VBOwner == vbHeader) {
            Current->VBOwner = NULL;
        }
        return NULL;
    }

    if (Atomic->geometry != Current->Geometry) {
        if (Current->VBOwner == vbHeader) {
            Current->VBOwner = NULL;
        }
        return NULL;
    }

    if (Current->lastFrameID != _RwSysFrameID) {
        printf("Error,  reaquire in error?\n");
    }

    Current->VBOwner = vbHeader;
    _clSkinCache_FlagUsed(Current);

    return Current->skinResEntry;
} // 0x802EB158

/*
 * ==================== _clSkinCache_AtomicDestruct ====================
 * The atomic is being destroyed: the cache entries forget it.
 */
void _clSkinCache_AtomicDestruct(RpAtomic* Atomic) {
    SkinResCacheEntry* Current;

    for (Current = sCacheList; Current != NULL; Current = Current->Next) {
        if (Current->Atomic == Atomic) {
            Current->Atomic = NULL;
        }
    }
} // 0x802EB24C
