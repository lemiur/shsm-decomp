// Unit name and boundary: Gormiti link map (RpSkin2.a skinstream.obj; its source there is gcn/skinstream.c)
// Names, parameters, locals and statement order: Gormiti DWARF (RpSkin2.a skinstream.obj), Final and Debug builds.
/*
 * skinstream.c: RpSkin2's native stream for the GameCube. A skin streams as one chunk of the
 * geometry's native data: the platform ID, the counts packed into one word, then the skin's
 * arrays as they lie in memory (used bone list, per-vertex indices and weights, inverse bone
 * matrices) and the split data. Reading a skin whose geometry is already instanced hands the
 * instanced vertex buffer the skin's vertex and normal arrays.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/plugin/skin2/skin.h>
#include <rwsdk/world/gcpipe.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_SKINPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65), so the chunks this unit writes carry that stamp.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/*
 * ==================== _rpSkinGeometryNativeSize ====================
 * The bytes the skin's chunk takes in a stream, header included. A geometry that is already
 * instanced gives its vertex and normal arrays to the vertex buffer, which then owns them.
 */
int _rpSkinGeometryNativeSize(RpGeometry* geometry) {
    int size;
    RpSkin* skin;

    size = rwCHUNKHEADERSIZE + sizeof(RwPlatformID) + sizeof(unsigned int);
    skin = RPSKINGEOMETRYGETDATA(geometry);

    size += skin->boneData.numUsedBones;

    if (skin->vertexMaps.maxWeights > 1) {
        size += geometry->numVertices * (skin->vertexMaps.maxWeights * 2);
    }

    size += skin->boneData.numBones * sizeof(RwMatrix);

    if (skin->platformData.vertices != NULL) {
        RxGameCubeVertexBuffer* vbHeader = (RxGameCubeVertexBuffer*)(geometry->repEntry + 1);
        RwUInt32 flags = geometry->flags; // not in Gormiti DWARF (MWCC drops locals it eliminates); read before the stores below, as retail does

        // identity-TODO: retail clears the word at 0x10 of the block that array[-1] points to (an RwResEntry's ownerRef is the
        // field at that offset); nothing in this unit or its neighbours says what the instancer stores before the array.
        ((RwResEntry**)vbHeader->attr[0].array)[-1]->ownerRef = NULL;
        vbHeader->attr[0].array = skin->platformData.vertices;
        if (flags & rpGEOMETRYNORMALS) {
            vbHeader->attr[1].array = skin->platformData.normals;
        }
        geometry->repEntry->destroyNotify = _rxGCResEntryWaitDone;
        skin->platformData.vertices = NULL;
        skin->platformData.normals = NULL;
    }

    size += _rpSkinSplitDataStreamGetSize(skin);

    return size;
} // 0x802EDDC8

/*
 * ==================== _rpSkinGeometryNativeWrite ====================
 * Writes the skin's chunk: the platform ID, the bone and weight counts packed into one word, the
 * used bone list, the per-vertex indices and weights (when a vertex has more than one weight) and
 * the inverse bone matrices, then the split data.
 */
RwStream* _rpSkinGeometryNativeWrite(RwStream* stream, RpGeometry* geometry) {
    RwPlatformID id = rwID_GAMECUBE;
    unsigned int skinInfo;
    int maxWeights; // RULE-EXCEPTION: R30, declared before numVertices and size (Gormiti DWARF lists it last): retail numbers the registers of maxWeights and numVertices in this order
    int numVertices;
    int size;
    RpSkin* skin;

    size = _rpSkinGeometryNativeSize(geometry) - rwCHUNKHEADERSIZE;

    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, size)) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&id, sizeof(id))) {
        return NULL;
    }

    numVertices = geometry->numVertices;
    skin = RPSKINGEOMETRYGETDATA(geometry);
    maxWeights = skin->vertexMaps.maxWeights;

    skinInfo = ((maxWeights & 0xFF) << 16) | ((skin->boneData.numUsedBones & 0xFF) << 8) |
               (skin->boneData.numBones & 0xFF);
    if (!RwStreamWriteInt32(stream, (const RwInt32*)&skinInfo, sizeof(skinInfo))) {
        return NULL;
    }

    if (!RwStreamWrite(stream, skin->boneData.usedBoneList, skin->boneData.numUsedBones)) {
        return NULL;
    }

    if (skin->vertexMaps.maxWeights > 1) {
        size = maxWeights * numVertices;
        if (!RwStreamWrite(stream, skin->platformData.indices, size)) {
            return NULL;
        }
        if (!RwStreamWrite(stream, skin->platformData.weights, size)) {
            return NULL;
        }
    }

    if (!RwStreamWrite(stream, skin->boneData.invBoneToSkinMat, skin->boneData.numBones * sizeof(RwMatrix))) {
        return NULL;
    }

    if (!_rpSkinSplitDataStreamWrite(stream, skin)) {
        return NULL;
    }

    return stream;
} // 0x802EDE90

/*
 * ==================== _rpSkinGeometryNativeRead ====================
 * Reads the skin's chunk written above and attaches the skin to the geometry. The skin's arrays
 * share one allocation, each aligned to four bytes: the weights, the indices, the inverse bone
 * matrices and the used bone list. A failed read returns without freeing the skin.
 */
RwStream* _rpSkinGeometryNativeRead(RwStream* stream, RpGeometry* geometry) {
    unsigned int version;
    unsigned int size;
    unsigned int skinInfo;
    int numVertices;
    RwPlatformID id;
    RpSkin* skin;

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

    skin = (RpSkin*)RwFreeListAlloc(_rpSkinGlobals.freeList, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
    memset(skin, 0, sizeof(RpSkin));

    if (!RwStreamReadInt32(stream, (RwInt32*)&skinInfo, sizeof(skinInfo))) {
        return NULL;
    }

    skin->boneData.numBones = skinInfo & 0xFF;
    skin->boneData.numUsedBones = (skinInfo >> 8) & 0xFF;
    skin->vertexMaps.maxWeights = (skinInfo >> 16) & 0xFF;

    numVertices = geometry->numVertices;
    size -= sizeof(id) + sizeof(skinInfo);

    if (skin->vertexMaps.maxWeights > 1) {
        skin->platformData.weights = (unsigned char*)RwMalloc(size + 5, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
        skin->platformData.indices = (unsigned char*)(((unsigned int)skin->platformData.weights +
                                                       skin->vertexMaps.maxWeights * numVertices + 3) & ~3);
        skin->boneData.invBoneToSkinMat = (RwMatrix*)(((unsigned int)skin->platformData.indices +
                                                       skin->vertexMaps.maxWeights * numVertices + 3) & ~3);
        skin->boneData.usedBoneList = (unsigned char*)(skin->boneData.invBoneToSkinMat + skin->boneData.numBones);

        size = skin->boneData.numUsedBones;
        if (size != RwStreamRead(stream, skin->boneData.usedBoneList, size)) {
            return NULL;
        }

        size = skin->vertexMaps.maxWeights * numVertices;
        if (size != RwStreamRead(stream, skin->platformData.indices, size)) {
            return NULL;
        }

        size = skin->vertexMaps.maxWeights * numVertices;
        if (size != RwStreamRead(stream, skin->platformData.weights, size)) {
            return NULL;
        }

        size = skin->boneData.numBones * sizeof(RwMatrix);
        if (size != RwStreamRead(stream, skin->boneData.invBoneToSkinMat, size)) {
            return NULL;
        }
    } else {
        skin->platformData.weights = (unsigned char*)RwMalloc(size + 3, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
        skin->boneData.invBoneToSkinMat = (RwMatrix*)(((unsigned int)skin->platformData.weights + 3) & ~3);
        skin->boneData.usedBoneList = (unsigned char*)(skin->boneData.invBoneToSkinMat + skin->boneData.numBones);

        size = skin->boneData.numUsedBones;
        if (size != RwStreamRead(stream, skin->boneData.usedBoneList, size)) {
            return NULL;
        }

        size = skin->boneData.numBones * sizeof(RwMatrix);
        if (size != RwStreamRead(stream, skin->boneData.invBoneToSkinMat, size)) {
            return NULL;
        }
    }

    if (!_rpSkinSplitDataStreamRead(stream, skin)) {
        return NULL;
    }

    RpSkinGeometrySetSkin(geometry, skin);
    return stream;
} // 0x802EE034
