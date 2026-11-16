// Unit name and boundary: Gormiti link map (RpDMorph.a dmphgeom.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpDMorph.a dmphgeom.obj), Final and Debug builds.
// RpDMorphGeometryDestroyDMorphTargets: Gormiti Debug build (a function there; inlined into DMorphGeometryDtor in retail).
/*
 * dmphgeom.c: the delta-morph extension of geometries. A geometry keeps a pointer to its delta-morph data (the
 * targets and the current weight of each); the data is created by RpDMorphGeometryCreateDMorphTargets, streamed
 * with the geometry, and freed with it.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_DMORPHPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/* The target as it lies in a stream, little endian. */
typedef struct DMorphTargetBinary {
    RwUInt32 flags;             /* 0x00 */
    RwUInt32 lockFlags;         /* 0x04 */
    RwUInt32 numCodeElements;   /* 0x08 */
    RwUInt32 numDataElements;   /* 0x0C */
} DMorphTargetBinary;

/* The bytes one data element of a target takes: a vertex and a normal are three reals, a pre-light colour four
   bytes and a texture coordinate pair two reals (name invented). */
#define DMORPHTARGETELEMENTSIZE(_flags)                                                                     \
    ((((_flags) & rpGEOMETRYPOSITIONS) ? sizeof(RwV3d) : 0) +                                               \
     (((_flags) & rpGEOMETRYNORMALS) ? sizeof(RwV3d) : 0) +                                                 \
     (((_flags) & rpGEOMETRYPRELIT) ? sizeof(RwRGBA) : 0) +                                                 \
     (((_flags) & rpGEOMETRYTEXTURED) ? sizeof(RwTexCoords) : 0))

/* The offset of the delta-morph data pointer in a geometry. */
RwInt32 rpDMorphGeometryDataOffset;

/*
 * ==================== DMorphGeometryCtor ====================
 * Plugin constructor: a new geometry has no delta-morph data.
 */
static void* DMorphGeometryCtor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RPDMORPHGEOMETRYGETDATA(object) = NULL;

    return object;
} // 0x802B95A0

/*
 * ==================== RpDMorphGeometryDestroyDMorphTargets ====================
 * Frees what each delta-morph target owns (its name and its code and data), then the targets and the geometry's
 * delta-morph data. Declared inline: retail inlines it into DMorphGeometryDtor, which the compiler does only for an
 * inline function of this size, and it keeps no copy of its own.
 */
inline RpGeometry* RpDMorphGeometryDestroyDMorphTargets(RpGeometry* geometry) {
    rpDMorphGeometryData** dataExtension;
    rpDMorphGeometryData* geometryData;
    RwUInt32 i;

    dataExtension = RWPLUGINOFFSET(rpDMorphGeometryData*, geometry, rpDMorphGeometryDataOffset);
    geometryData = *dataExtension;

    for (i = 0; i < geometryData->numDMorphTargets; i++) {
        RpDMorphTarget* dMorphTarget = &RPDMORPHGEOMETRYGETDATA(geometry)->dMorphTargets[i];

        if (dMorphTarget->name) {
            RwFree(dMorphTarget->name);
        }
        if (dMorphTarget->rle.code) {
            RwFree(dMorphTarget->rle.code);
        }
        if (dMorphTarget->rle.data) {
            RwFree(dMorphTarget->rle.data);
        }

        memset(dMorphTarget, 0, sizeof(RpDMorphTarget));
    }

    RwFree(geometryData->dMorphTargets);
    RwFree(geometryData);
    *dataExtension = NULL;

    return geometry;
} // no copy in retail (inlined)

/*
 * ==================== DMorphGeometryDtor ====================
 * Plugin destructor: frees the delta-morph targets and data, if the geometry has any.
 */
static void* DMorphGeometryDtor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpDMorphGeometryData* geometryData;

    geometryData = RPDMORPHGEOMETRYGETDATA(object);
    if (geometryData != NULL) {
        if (geometryData->numDMorphTargets) {
            RpDMorphGeometryDestroyDMorphTargets((RpGeometry*)object);
        }
    }

    RPDMORPHGEOMETRYGETDATA(object) = NULL;

    return object;
} // 0x802B95B0

/*
 * ==================== DMorphGeometryCopy ====================
 * Plugin copy: a copied geometry gets no delta-morph data.
 */
static void* DMorphGeometryCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject,
                                RwInt32 sizeInObject) {
    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(dstObject);
    RWASSERT(srcObject);

    return dstObject;
} // 0x802B96C8

/*
 * ==================== DMorphGeometryStreamRead ====================
 * Reads a geometry's delta-morph targets: their number, then for each the name, the header, the run-length code
 * and the data arrays its flags name.
 */
static RwStream* DMorphGeometryStreamRead(RwStream* stream, RwInt32 binaryLength, void* object,
                                          RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpDMorphGeometryData* geometryData;
    RpGeometry* geometry;
    RwUInt32 numTargets;
    RwUInt32 i;

    geometry = (RpGeometry*)object;

    if (!RwStreamReadInt32(stream, (RwInt32*)&numTargets, sizeof(numTargets))) {
        return NULL;
    }

    RpDMorphGeometryCreateDMorphTargets(geometry, numTargets);

    geometryData = RPDMORPHGEOMETRYGETDATA(geometry);
    for (i = 0; i < geometryData->numDMorphTargets; i++) {
        RpDMorphTarget* target = &geometryData->dMorphTargets[i];
        DMorphTargetBinary binary;
        RwUInt32 length;
        RwUInt32 size;
        RwUInt8* memory;

        if (!RwStreamReadInt32(stream, (RwInt32*)&length, sizeof(length))) {
            return NULL;
        }

        if (length != 0) {
            target->name = (char*)RwMalloc(length, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
            if (target->name == NULL) {
                RWERROR((E_RW_NOMEM, length));
                return NULL;
            }

            if (!RwStreamRead(stream, target->name, length)) {
                return NULL;
            }
        } else {
            target->name = NULL;
        }

        if (!RwStreamReadInt32(stream, (RwInt32*)&binary, sizeof(binary))) {
            return NULL;
        }

        target->flags = binary.flags;
        target->lockFlags = binary.lockFlags;
        target->rle.numCodeElements = binary.numCodeElements;
        target->rle.numDataElements = binary.numDataElements;

        size = target->rle.numCodeElements;
        target->rle.code = (RwUInt8*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
        if (target->rle.code == NULL) {
            RWERROR((E_RW_NOMEM, size));
            return NULL;
        }

        if (!RwStreamRead(stream, target->rle.code, size)) {
            return NULL;
        }

        if (target->rle.numDataElements == 0) {
            target->rle.data = NULL;
        } else {
            size = DMORPHTARGETELEMENTSIZE(target->flags);
            size *= target->rle.numDataElements;
            memory = (RwUInt8*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
            target->rle.data = memory;
            if (memory == NULL) {
                RWERROR((E_RW_NOMEM, size));
                return NULL;
            }

            if (target->flags & rpGEOMETRYPOSITIONS) {
                size = target->rle.numDataElements * sizeof(RwV3d);
                if (!RwStreamReadReal(stream, (RwReal*)memory, size)) {
                    return NULL;
                }
                target->vertices = (RwV3d*)memory;
                memory += size;
            }

            if (target->flags & rpGEOMETRYNORMALS) {
                size = target->rle.numDataElements * sizeof(RwV3d);
                if (!RwStreamReadReal(stream, (RwReal*)memory, size)) {
                    return NULL;
                }
                target->normals = (RwV3d*)memory;
                memory += size;
            }

            if (target->flags & rpGEOMETRYPRELIT) {
                size = target->rle.numDataElements * sizeof(RwRGBA);
                if (!RwStreamRead(stream, memory, size)) {
                    return NULL;
                }
                target->preLightColors = (RwRGBA*)memory;
                memory += size;
            }

            if (target->flags & rpGEOMETRYTEXTURED) {
                size = target->rle.numDataElements * sizeof(RwTexCoords);
                if (!RwStreamReadReal(stream, (RwReal*)memory, size)) {
                    return NULL;
                }
                target->texCoords = (RwTexCoords*)memory;
            }
        }

        if (!RwStreamReadReal(stream, (RwReal*)&target->boundingSphere, sizeof(RwSphere))) {
            return NULL;
        }

        target->geometry = geometry;
    }

    return stream;
} // 0x802B96CC

/*
 * ==================== DMorphGeometryStreamWrite ====================
 * Writes what DMorphGeometryStreamRead reads: the number of targets, then for each the name, the header, the
 * run-length code and the data arrays its flags name. Each write's byte count goes through the size local first,
 * as in Gormiti's Debug build.
 */
static RwStream* DMorphGeometryStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object,
                                           RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpDMorphGeometryData* geometryData;
    RwUInt32 i;

    geometryData = RPDMORPHGEOMETRYGETDATA(object);

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&geometryData->numDMorphTargets, sizeof(RwUInt32))) {
        return NULL;
    }

    for (i = 0; i < geometryData->numDMorphTargets; i++) {
        RpDMorphTarget* target = &geometryData->dMorphTargets[i];
        DMorphTargetBinary binary;
        RwInt32 nameLength;
        RwUInt32 size;

        if (target->name) {
            nameLength = rwstrlen(target->name) + 1;
        } else {
            nameLength = 0;
        }

        if (!RwStreamWriteInt32(stream, &nameLength, sizeof(nameLength))) {
            return NULL;
        }

        if (target->name) {
            if (!RwStreamWrite(stream, target->name, nameLength)) {
                return NULL;
            }
        }

        binary.flags = target->flags;
        binary.lockFlags = target->lockFlags;
        binary.numCodeElements = target->rle.numCodeElements;
        binary.numDataElements = target->rle.numDataElements;
        if (!RwStreamWriteInt32(stream, (const RwInt32*)&binary, sizeof(binary))) {
            return NULL;
        }

        size = target->rle.numCodeElements;
        if (!RwStreamWrite(stream, target->rle.code, size)) {
            return NULL;
        }

        if (target->rle.numDataElements) {
            if (target->flags & rpGEOMETRYPOSITIONS) {
                size = target->rle.numDataElements * sizeof(RwV3d);
                if (!RwStreamWriteReal(stream, (const RwReal*)target->vertices, size)) {
                    return NULL;
                }
            }

            if (target->flags & rpGEOMETRYNORMALS) {
                size = target->rle.numDataElements * sizeof(RwV3d);
                if (!RwStreamWriteReal(stream, (const RwReal*)target->normals, size)) {
                    return NULL;
                }
            }

            if (target->flags & rpGEOMETRYPRELIT) {
                size = target->rle.numDataElements * sizeof(RwRGBA);
                if (!RwStreamWrite(stream, target->preLightColors, size)) {
                    return NULL;
                }
            }

            if (target->flags & rpGEOMETRYTEXTURED) {
                size = target->rle.numDataElements * sizeof(RwTexCoords);
                if (!RwStreamWriteReal(stream, (const RwReal*)target->texCoords, size)) {
                    return NULL;
                }
            }
        }

        if (!RwStreamWriteReal(stream, (const RwReal*)&target->boundingSphere, sizeof(RwSphere))) {
            return NULL;
        }
    }

    return stream;
} // 0x802B9A70

/*
 * ==================== DMorphGeometryStreamSize ====================
 * The size of what DMorphGeometryStreamWrite writes. A geometry without delta-morph data takes nothing.
 */
static RwInt32 DMorphGeometryStreamSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpDMorphGeometryData* geometryData;
    RwUInt32 size;
    RwUInt32 i;

    size = 0;

    geometryData = RPDMORPHGEOMETRYGETDATA(object);
    if (geometryData != NULL) {
        size += sizeof(RwUInt32);
        for (i = 0; i < geometryData->numDMorphTargets; i++) {
            RpDMorphTarget* dMorphTarget = &geometryData->dMorphTargets[i];
            RwUInt32 flags = dMorphTarget->flags;

            size += sizeof(RwInt32);
            if (dMorphTarget->name) {
                size += rwstrlen(dMorphTarget->name) + 1;
            }

            size += sizeof(DMorphTargetBinary);
            size += dMorphTarget->rle.numCodeElements;
            size += dMorphTarget->rle.numDataElements * DMORPHTARGETELEMENTSIZE(flags);
            size += sizeof(RwSphere);
        }
    }

    return size;
} // 0x802B9CA4

/*
 * ==================== _rpDMorphGeometryPluginAttach ====================
 * Registers the delta-morph extension of geometries and its stream chunk callbacks. Returns FALSE when the engine
 * has no room for either.
 */
RwBool _rpDMorphGeometryPluginAttach(void) {
    RwInt32 offset;

    rpDMorphGeometryDataOffset = RpGeometryRegisterPlugin(sizeof(rpDMorphGeometryData*), rwID_DMORPHPLUGIN,
                                                          DMorphGeometryCtor, DMorphGeometryDtor, DMorphGeometryCopy);
    if (rpDMorphGeometryDataOffset < 0) {
        return FALSE;
    }

    offset = RpGeometryRegisterPluginStream(rwID_DMORPHPLUGIN, DMorphGeometryStreamRead, DMorphGeometryStreamWrite,
                                            DMorphGeometryStreamSize);

    return offset >= 0;
} // 0x802B9D90

/*
 * ==================== RpDMorphGeometryCreateDMorphTargets ====================
 * Gives a geometry room for number delta-morph targets and their weights, all cleared. Returns NULL when the
 * engine has no memory for them.
 */
RpGeometry* RpDMorphGeometryCreateDMorphTargets(RpGeometry* geometry, RwUInt32 number) {
    rpDMorphGeometryData** dataExtension;
    rpDMorphGeometryData* geometryData;
    RpDMorphTarget* dMorphTargets;
    RwUInt32 size;

    dataExtension = RWPLUGINOFFSET(rpDMorphGeometryData*, geometry, rpDMorphGeometryDataOffset);

    *dataExtension = (rpDMorphGeometryData*)RwMalloc(sizeof(rpDMorphGeometryData), rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
    geometryData = *dataExtension;
    if (geometryData == NULL) {
        return NULL;
    }

    memset(geometryData, 0, sizeof(rpDMorphGeometryData));
    geometryData->numDMorphTargets = number;

    size = number * (sizeof(RpDMorphTarget) + sizeof(RwReal));
    dMorphTargets = (RpDMorphTarget*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
    if (dMorphTargets == NULL) {
        RwFree(geometryData);
        return NULL;
    }

    memset(dMorphTargets, 0, size);
    geometryData->dMorphTargets = dMorphTargets;
    geometryData->currValues = (RwReal*)(dMorphTargets + number);

    return geometry;
} // 0x802B9E0C

/*
 * ==================== RpDMorphGeometryGetNumDMorphTargets ====================
 * The number of delta-morph targets of a geometry, 0 for one without delta-morph data.
 */
RwUInt32 RpDMorphGeometryGetNumDMorphTargets(const RpGeometry* geometry) {
    rpDMorphGeometryData* geometryData;

    geometryData = RPDMORPHGEOMETRYGETDATA(geometry);
    if (geometryData != NULL) {
        return geometryData->numDMorphTargets;
    }

    return 0;
} // 0x802B9EE4
