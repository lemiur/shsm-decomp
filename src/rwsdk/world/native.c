// Unit name and boundary: Gormiti link map (RPworld.a native.obj; its source there is pipe/p2/gcn/native.c)
// Names, parameters and locals: Gormiti DWARF (Final and Debug builds)
/*
 * native.c: the GameCube native data stream of the world plugin. The instanced data of a geometry
 * or a world sector, which lives in a resource entry, streams as one chunk: the platform ID, the
 * sizes of the vertex buffer header and of the display lists, then the header (the vertex buffer,
 * its attribute records and the display list table) and the display lists. The pointers inside
 * them go to the stream as offsets from the first display list, and are rebased on reading.
 */
#include <revolution/gx/GXGeometry.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/world/rpplugin.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/ibuffer.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65), so the chunks this unit writes carry that stamp.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/*
 * ==================== _rpNativePointer2Offset ====================
 * Turns the attribute array pointers and the display list pointers of an instanced vertex
 * buffer into offsets from the first display list (the 32 byte aligned end of the display list table).
 * An attribute array's offset is stored as a word over its pointer member; that store, typed apart from the
 * load, takes its own induction pointer when the loop is inlined into _rpNativeWrite (retail and Gormiti Final).
 * Gormiti's Debug build also computes &vbHeader->attr[i] after the store (line 84) and two dList addresses in
 * the second loop (lines 95-96), without a load or store; release code shows no trace of them.
 */
static void _rpNativePointer2Offset(RxGameCubeVertexBuffer* vbHeader, RxGameCubeDisplayList* dList,
                                    unsigned int numMeshes) {
    unsigned int i;
    unsigned char* baseAddress;

    baseAddress = (unsigned char*)(((unsigned int)&dList[numMeshes] + 31) & ~31);

    for (i = 0; i < vbHeader->numAttrArrays; i++) {
        *(unsigned int*)&vbHeader->attr[i].array = (unsigned int)vbHeader->attr[i].array - (unsigned int)baseAddress;
    }

    for (i = 0; i < numMeshes; i++) {
        dList[i].displayList = (void*)((unsigned int)dList[i].displayList - (unsigned int)baseAddress);
    }
}

/* Turns the offsets _rpNativePointer2Offset made back into pointers. */
static void _rpNativeOffset2Pointer(RxGameCubeVertexBuffer* vbHeader, RxGameCubeDisplayList* dList,
                                    unsigned int numMeshes) {
    unsigned int i;
    unsigned char* baseAddress;

    baseAddress = (unsigned char*)(((unsigned int)&dList[numMeshes] + 31) & ~31);

    for (i = 0; i < vbHeader->numAttrArrays; i++) {
        vbHeader->attr[i].array = (void*)((unsigned int)vbHeader->attr[i].array + (unsigned int)baseAddress);
    }

    for (i = 0; i < numMeshes; i++) {
        dList[i].displayList = (void*)((unsigned int)dList[i].displayList + (unsigned int)baseAddress);
    }
}

/*
 * ==================== _rpNativeSize ====================
 * The bytes a native chunk of the resource entry takes in a stream, its chunk header included.
 * numMeshes is passed by every caller (Gormiti Debug) but the release body does not read it.
 */
static unsigned int _rpNativeSize(RwResEntry* resEntry, unsigned int numMeshes) {
    unsigned int size = 0;
    RxGameCubeVertexBuffer* vbHeader;
    RxGameCubeDisplayList* dList;

    vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);
    RWASSERT(vbHeader);

    dList = (RxGameCubeDisplayList*)(&vbHeader->attr[vbHeader->numAttrArrays - 1] + 1);
    RWASSERT(dList);

    size = resEntry->size - 7;

    return size;
} // no copy in retail (inlined): a static in the Gormiti Debug map (native.obj)

/*
 * ==================== _rpNativeRead ====================
 * Reads the native chunk of a geometry or world sector into a new resource entry and rebases the
 * pointers in it. Returns the owner, or NULL when the chunk is not this platform's or does not read.
 */
static void* _rpNativeRead(RwStream* stream, void* owner, RwResEntry** resEntryPointer, unsigned int numMeshes) {
    unsigned int version;
    unsigned int size;
    unsigned int headerSize;
    unsigned int dlistSize;
    unsigned int resSize;
    RwPlatformID id;
    RxGameCubeVertexBuffer* vbHeader;
    RxGameCubeDisplayList* dList;
    unsigned char* displayLists;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)&id, sizeof(id))) {
        return NULL;
    }

    if (id != rwID_GAMECUBE) {
        return NULL;
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)&headerSize, sizeof(headerSize))) {
        return NULL;
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)&dlistSize, sizeof(dlistSize))) {
        return NULL;
    }

    resSize = sizeof(RwResEntry) + headerSize + dlistSize + 31;
    *resEntryPointer = (RwResEntry*)RwMalloc(resSize, rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    vbHeader = (RxGameCubeVertexBuffer*)(*resEntryPointer + 1);

    if (RwStreamRead(stream, vbHeader, headerSize) != headerSize) {
        return NULL;
    }

    displayLists = (unsigned char*)(((unsigned int)vbHeader + headerSize + 31) & ~31);
    if (RwStreamRead(stream, displayLists, dlistSize) != dlistSize) {
        return NULL;
    }

    dList = (RxGameCubeDisplayList*)(&vbHeader->attr[vbHeader->numAttrArrays - 1] + 1);
    _rpNativeOffset2Pointer(vbHeader, dList, numMeshes);

    (*resEntryPointer)->link.next = NULL;
    (*resEntryPointer)->link.prev = NULL;
    (*resEntryPointer)->owner = owner;
    (*resEntryPointer)->size = size;
    (*resEntryPointer)->ownerRef = resEntryPointer;
    (*resEntryPointer)->destroyNotify = _rxGCResEntryWaitDone;

    vbHeader->token = _RwDlTokenCurrent;

    DCFlushRange(*resEntryPointer + 1, (*resEntryPointer)->size);
    GXInvalidateVtxCache();

    return owner;
} // 0x802DCAB0

/*
 * ==================== _rpNativeWrite ====================
 * Writes the native chunk of a resource entry. The pointers in the entry are turned into offsets
 * from the first display list while the header goes out and are put back afterwards.
 * Retail revision differs from Gormiti (Gormiti is a reading aid here: _rpNativeWrite pairs only by size).
 * The opening (id initialised, vbHeader and dList before the inlined _rpNativeSize) follows the
 * Gormiti Debug line table. The inlined _rpNativePointer2Offset's attribute loop takes two induction
 * pointers (lwzu on the load, a separate store pointer) because its store writes the offset as a word
 * over the pointer member (see there).
 */
static RwStream* _rpNativeWrite(RwStream* stream, RwResEntry* resEntry, unsigned int numMeshes) {
    RwPlatformID id = rwID_GAMECUBE;
    unsigned int chunkSize;
    unsigned int headerSize;
    unsigned int dListSize;
    RxGameCubeVertexBuffer* vbHeader;
    RxGameCubeDisplayList* dList;

    vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);
    RWASSERT(vbHeader);

    dList = (RxGameCubeDisplayList*)(&vbHeader->attr[vbHeader->numAttrArrays - 1] + 1);
    RWASSERT(dList);

    chunkSize = _rpNativeSize(resEntry, numMeshes) - rwCHUNKHEADERSIZE;

    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, chunkSize)) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&id, sizeof(id))) {
        return NULL;
    }

    headerSize = sizeof(RxGameCubeVertexBuffer) + (vbHeader->numAttrArrays - 1) * sizeof(RxGameCubeVertexAttr) +
                 numMeshes * sizeof(RxGameCubeDisplayList);
    if (!RwStreamWriteInt32(stream, (const RwInt32*)&headerSize, sizeof(headerSize))) {
        return NULL;
    }

    dListSize = resEntry->size - headerSize - 31;
    if (!RwStreamWriteInt32(stream, (const RwInt32*)&dListSize, sizeof(dListSize))) {
        return NULL;
    }

    _rpNativePointer2Offset(vbHeader, dList, numMeshes);

    if (!RwStreamWrite(stream, vbHeader, headerSize)) {
        return NULL;
    }

    _rpNativeOffset2Pointer(vbHeader, dList, numMeshes);

    if (!RwStreamWrite(stream, dList->displayList, dListSize)) {
        return NULL;
    }

    DCFlushRange(resEntry + 1, resEntry->size);
    GXInvalidateVtxCache();

    return stream;
} // 0x802DCDAC

/*
 * ==================== _rpGeometryNativeSize ====================
 * The bytes the geometry's native chunk takes in a stream, header included.
 */
RwInt32 _rpGeometryNativeSize(const RpGeometry* geometry) {
    if ((geometry->flags & rpGEOMETRYNATIVE) && geometry->repEntry != NULL) {
        return _rpNativeSize(geometry->repEntry, geometry->mesh->numMeshes);
    }

    return 0;
} // 0x802DD100

/* Writes the geometry's native chunk when it is instanced natively. */
RwStream* _rpGeometryNativeWrite(RwStream* stream, const RpGeometry* geometry) {
    if (geometry->flags & rpGEOMETRYNATIVE) {
        _rpNativeWrite(stream, geometry->repEntry, geometry->mesh->numMeshes);
    }

    return stream;
} // 0x802DD12C

/* Reads the geometry's native chunk into its resource entry. */
RpGeometry* _rpGeometryNativeRead(RwStream* stream, RpGeometry* geometry) {
    RpGeometry* result;

    result = _rpNativeRead(stream, geometry, &geometry->repEntry, geometry->mesh->numMeshes);

    return result;
} // 0x802DD174

/* The bytes the sector's native chunk takes in a stream, header included. */
RwInt32 _rpWorldSectorNativeSize(const RpWorldSector* sector) {
    if ((RpWorldSectorGetWorld(sector)->flags & rpWORLDNATIVE) && sector->repEntry != NULL) {
        return _rpNativeSize(sector->repEntry, sector->mesh->numMeshes);
    }

    return 0;
} // 0x802DD184

/* Writes the sector's native chunk when its world is instanced natively. */
RwStream* _rpWorldSectorNativeWrite(RwStream* stream, const RpWorldSector* sector) {
    RpWorld* world;

    world = RpWorldSectorGetWorld(sector);
    if (world->flags & rpWORLDNATIVE) {
        _rpNativeWrite(stream, sector->repEntry, sector->mesh->numMeshes);
    }

    return stream;
} // 0x802DD1D8

/* Reads the sector's native chunk into its resource entry. */
RpWorldSector* _rpWorldSectorNativeRead(RwStream* stream, RpWorldSector* sector) {
    RpWorldSector* result;

    result = _rpNativeRead(stream, sector, &sector->repEntry, sector->mesh->numMeshes);

    return result;
} // 0x802DD238
