// Unit name and boundary: Gormiti link map (RPCollis.a colldata.obj)

#include <rwsdk/badevice.h>
#include <rwsdk/babbox.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plugin/collis/colldata.h>

/* The open counter and the plugin offsets (names: Gormiti link map and DWARF, colldata.obj). The explicit zero is what
   the data layout needs: as tentative definitions the linker places them in reverse order in .sbss. */
RwInt32 _rpCollisionNumInstances = 0;
RwInt32 _rpCollisionGlobalRefOffset = 0;
RwInt32 _rpCollisionAtomicRefOffset = 0;
RwInt32 _rpCollisionGeometryDataOffset = 0;
RwInt32 _rpCollisionWorldSectorDataOffset = 0;

/* Engine plugin open: counts the open, then clears the engine's collision slot. 0x802B39F4 */
static void *CollisionOpen(void *instance, RwInt32 offset, RwInt32 size)
{
    _rpCollisionNumInstances++;
    *RWPLUGINOFFSET(RwInt32, RwEngineInstance, _rpCollisionGlobalRefOffset) = 0;
    return instance;
}

/* Engine plugin close: drops the open count. 0x802B3A14 */
static void *CollisionClose(void *instance, RwInt32 offset, RwInt32 size)
{
    _rpCollisionNumInstances--;
    return instance;
}

/* World sector and geometry plugin destructor: frees the tree, if any. 0x802B3A24 */
static void *CollisionDataDestructor(void *object, RwInt32 offset, RwInt32 size)
{
    RpCollisionData *collData;

    collData = RWPLUGINOFFSET(RpCollisionData, object, offset);
    if (collData->tree != NULL) {
        _rpCollTreeDestroy(collData->tree);
        collData->tree = NULL;
    }
    return object;
}

/* World sector and geometry plugin constructor: no tree yet. 0x802B3A78 */
static void *CollisionDataConstructor(void *object, RwInt32 offset, RwInt32 size)
{
    RWPLUGINOFFSET(RpCollisionData, object, offset)->tree = NULL;
    return object;
}

/* Stream size: the tree's size plus its chunk header and version word, or 0 without a tree. 0x802B3A84 */
static RwInt32 CollisionDataStreamGetSize(const void *object, RwInt32 offsetInObject, RwInt32 sizeInObject)
{
    RpCollisionData *collData;
    RwUInt32 binarySize;

    collData = RWPLUGINOFFSET(RpCollisionData, object, offsetInObject);
    if (collData->tree != NULL) {
        binarySize = _rpCollTreeStreamGetSize(collData->tree) + 0x10;
    } else {
        binarySize = 0;
    }
    return binarySize;
}

/* Stream write: the version word, then the tree. 0x802B3ABC */
static RwStream *CollisionDataStreamWrite(RwStream *stream, RwInt32 binaryLength, const void *object, RwInt32 offset,
                                          RwInt32 size)
{
    RpCollisionData *collData;
    RwUInt32 version;

    collData = RWPLUGINOFFSET(RpCollisionData, object, offset);
    if (collData->tree != NULL) {
        version = RwEngineGetVersion();
        if (RwStreamWriteInt32(stream, (RwInt32 *)&version, 4) == NULL) {
            return NULL;
        }
        if (!_rpCollTreeStreamWrite(collData->tree, stream)) {
            return NULL;
        }
    }
    return stream;
}

/* Stream read: the chunk is either the old format (no version word, a split count plus one first, read by the
   converter) or the current one (a collision tree chunk). The old format has no bounds of its own, so the owner's
   are handed to the converter. Returns the stream, or NULL on a failed read. 0x802B3B50 */
static RwStream *CollisionDataStreamRead(RwStream *stream, RwInt32 binaryLength, void *object, RwInt32 offset,
                                         RwInt32 size)
{
    RwUInt32 version;
    RpCollisionData *collData;

    collData = RWPLUGINOFFSET(RpCollisionData, object, offset);
    if (!RwStreamReadInt32(stream, (RwInt32 *)&version, sizeof(RwUInt32))) {
        return NULL;
    }
    if (version < rwCOLLTREEVERSION) {
        RwUInt32 numSplits;
        RwUInt32 numTriangles;
        RwBool failConv;
        RwBBox bbox;

        if (!RwStreamReadInt32(stream, (RwInt32 *)&numTriangles, sizeof(RwUInt32))) {
            return NULL;
        }
        if (((RwObject *)object)->type == rpGEOMETRY) {
            RwSphere *sph;

            sph = &((RpGeometry *)object)->morphTarget->boundingSphere;
            bbox.inf.x = sph->center.x - sph->radius;
            bbox.inf.y = sph->center.y - sph->radius;
            bbox.inf.z = sph->center.z - sph->radius;
            bbox.sup.x = sph->center.x + sph->radius;
            bbox.sup.y = sph->center.y + sph->radius;
            bbox.sup.z = sph->center.z + sph->radius;
        } else {
            bbox = ((RpWorldSector *)object)->tightBoundingBox;
        }
        failConv = FALSE;
        numSplits = version - 1;
        collData->tree = _rpCollTreeStreamReadPre36001(stream, numTriangles, numSplits, &bbox, &failConv);
        if (failConv) {
            return stream;
        }
    } else {
        if (!RwStreamFindChunk(stream, rwID_COLLTREE, NULL, NULL)) {
            return NULL;
        }
        collData->tree = _rpCollTreeStreamRead(stream);
    }
    return collData->tree != NULL ? stream : NULL;
}

/* Registers the geometry collision plugin and its stream callbacks (inlined into RpCollisionPluginAttach). */
static RwBool CollisionGeometryPluginAttach(void)
{
    RwBool result;
    RwInt32 offset;

    _rpCollisionGeometryDataOffset =
        RpGeometryRegisterPlugin(sizeof(RpCollisionData), rwID_COLLISPLUGIN,
                                 CollisionDataConstructor, CollisionDataDestructor, NULL);
    result = _rpCollisionGeometryDataOffset >= 0;
    if (result) {
        offset = RpGeometryRegisterPluginStream(rwID_COLLISPLUGIN, CollisionDataStreamRead,
                                                CollisionDataStreamWrite, CollisionDataStreamGetSize);
        result = offset >= 0;
    }
    return result;
} // no copy in retail (inlined): a static in the Gormiti Debug map (colldata.obj) and DWARF, absent from the Final map

/* Registers the world sector collision plugin and its stream callbacks (inlined into RpCollisionPluginAttach). */
static RwBool CollisionWorldSectorPluginAttach(void)
{
    RwBool result;
    RwInt32 offset;

    _rpCollisionWorldSectorDataOffset =
        RpWorldSectorRegisterPlugin(sizeof(RpCollisionData), rwID_COLLISPLUGIN,
                                    CollisionDataConstructor, CollisionDataDestructor, NULL);
    result = _rpCollisionWorldSectorDataOffset >= 0;
    if (result) {
        offset = RpWorldSectorRegisterPluginStream(rwID_COLLISPLUGIN, CollisionDataStreamRead,
                                                   CollisionDataStreamWrite, CollisionDataStreamGetSize);
        result = offset >= 0;
    }
    return result;
} // no copy in retail (inlined): a static in the Gormiti Debug map (colldata.obj) and DWARF, absent from the Final map

/* Atomic plugin constructor: no tree yet, in the atomic's own slot. 0x802B3D08 */
static void *CollisionAtomicInit(void *object, RwInt32 offset, RwInt32 size)
{
    RWPLUGINOFFSET(RpCollisionData, object, _rpCollisionAtomicRefOffset)->tree = NULL;
    return object;
}

/*
 * Registers the collision plugin with the engine, atomics, world sectors and geometries. The plugin's
 * stream callbacks go to world sectors and geometries only. 0x802B3D18
 */
RwBool RpCollisionPluginAttach(void)
{
    RwBool result;

    _rpCollisionGlobalRefOffset =
        RwEngineRegisterPlugin(sizeof(RwInt32), rwID_COLLISPLUGIN, CollisionOpen, CollisionClose);
    result = _rpCollisionGlobalRefOffset > 0;
    if (result) {
        _rpCollisionAtomicRefOffset = RpAtomicRegisterPlugin(sizeof(RpCollisionData), rwID_COLLISPLUGIN,
                                                             CollisionAtomicInit, NULL, NULL);
        result = (_rpCollisionAtomicRefOffset > 0)
            && CollisionWorldSectorPluginAttach()
            && CollisionGeometryPluginAttach();
    }
    return result;
}
