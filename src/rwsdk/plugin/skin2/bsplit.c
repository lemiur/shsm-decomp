// Unit name and boundary: Gormiti link map (RpSkin2.a bsplit.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpSkin2.a bsplit.obj), Final and Debug builds.
/*
 * bsplit.c: RpSkin2's split data. A platform that holds only so many bones at once has each
 * skinned mesh split in parts; the skin keeps in one allocation the remap that takes a part's bones
 * to the skin's, each mesh's run counts and the runs. The unit makes and frees that block and
 * streams it with the skin.
 *
 * _rpSkinSplitDataCreate clears the old block through _rpSkinSplitDataDestroy (a call in Gormiti's
 * Debug build, one source line in its Final line table); the compiler inlines Destroy there, which
 * keeps Create small enough to be inlined into _rpSkinSplitDataStreamRead as retail has it.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/skin2/rpskin.h>

/*
 * ==================== _rpSkinGetMeshBoneRemapIndices ====================
 * The bytes that take a part's bones to the skin's.
 */
unsigned char* _rpSkinGetMeshBoneRemapIndices(const RpSkin* skin) {
    return skin->skinSplitData.matrixRemapIndices;
} // 0x802E9638

/*
 * ==================== _rpSkinGetMeshBoneRLECount ====================
 * The number of runs of each mesh.
 */
unsigned char* _rpSkinGetMeshBoneRLECount(const RpSkin* skin) {
    return skin->skinSplitData.meshRLECount;
} // 0x802E9640

/*
 * ==================== _rpSkinGetMeshBoneRLE ====================
 * The runs of all meshes.
 */
unsigned char* _rpSkinGetMeshBoneRLE(const RpSkin* skin) {
    return skin->skinSplitData.meshRLE;
} // 0x802E9648

/*
 * ==================== _rpSkinSplitDataCreate ====================
 * Replaces the skin's split data by a cleared block for the given counts. It returns NULL when
 * the block cannot be allocated, the skin otherwise.
 */
RpSkin* _rpSkinSplitDataCreate(RpSkin* skin, unsigned int boneLimit, unsigned int numMatrices,
                               unsigned int numMeshes, unsigned int numRLE) {
    unsigned int size;
    SkinSplitData* skinSplitData;

    skinSplitData = &skin->skinSplitData;

    _rpSkinSplitDataDestroy(skin);

    size = numMatrices + numMeshes * sizeof(RwUInt16) + numRLE * sizeof(RwUInt16);

    skinSplitData->matrixRemapIndices =
        (unsigned char*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
    if (!skinSplitData->matrixRemapIndices) {
        return NULL;
    }

    memset(skinSplitData->matrixRemapIndices, 0, size);

    skinSplitData->boneLimit = boneLimit;
    skinSplitData->numMeshes = numMeshes;
    skinSplitData->numRLE = numRLE;
    skinSplitData->meshRLECount = skinSplitData->matrixRemapIndices + numMatrices;
    skinSplitData->meshRLE = skinSplitData->meshRLECount + skinSplitData->numMeshes * sizeof(RwUInt16);

    return skin;
} // 0x802E9650

/*
 * ==================== _rpSkinSplitDataDestroy ====================
 * Frees the skin's split data and clears it.
 */
int _rpSkinSplitDataDestroy(RpSkin* skin) {
    SkinSplitData* skinSplitData;

    skinSplitData = &skin->skinSplitData;

    if (skinSplitData->matrixRemapIndices) {
        RwFree(skinSplitData->matrixRemapIndices);
    }

    skinSplitData->boneLimit = 0;
    skinSplitData->numMeshes = 0;
    skinSplitData->numRLE = 0;
    skinSplitData->matrixRemapIndices = NULL;
    skinSplitData->meshRLECount = NULL;
    skinSplitData->meshRLE = NULL;

    return TRUE;
} // 0x802E9738

/*
 * ==================== _rpSkinSplitDataStreamWrite ====================
 * Writes the skin's limit, mesh and run counts and, when it has meshes, the split block.
 */
RwStream* _rpSkinSplitDataStreamWrite(RwStream* stream, const RpSkin* skin) {
    const SkinSplitData* skinSplitData;

    skinSplitData = &skin->skinSplitData;

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&skinSplitData->boneLimit, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamWriteInt32(stream, (const RwInt32*)&skinSplitData->numMeshes, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamWriteInt32(stream, (const RwInt32*)&skinSplitData->numRLE, sizeof(RwInt32))) {
        return NULL;
    }

    if (skin->skinSplitData.numMeshes) {
        if (!RwStreamWrite(stream, skinSplitData->matrixRemapIndices,
                           skin->boneData.numBones + skinSplitData->numMeshes * sizeof(RwUInt16) +
                               skinSplitData->numRLE * sizeof(RwUInt16))) {
            return NULL;
        }
    }

    return stream;
} // 0x802E97A0

/*
 * ==================== _rpSkinSplitDataStreamRead ====================
 * Reads the counts and the split block into a new block made for them.
 */
RwStream* _rpSkinSplitDataStreamRead(RwStream* stream, RpSkin* skin) {
    int numMeshes, numRLE, boneLimit;
    SkinSplitData* skinSplitData;

    skinSplitData = &skin->skinSplitData;

    if (!RwStreamReadInt32(stream, &boneLimit, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &numMeshes, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &numRLE, sizeof(RwInt32))) {
        return NULL;
    }

    if (numMeshes > 0) {
        if (!_rpSkinSplitDataCreate(skin, boneLimit, skin->boneData.numBones, numMeshes, numRLE)) {
            return NULL;
        }

        if (!RwStreamRead(stream, skinSplitData->matrixRemapIndices,
                          skin->boneData.numBones + skinSplitData->numMeshes * sizeof(RwUInt16) +
                              skinSplitData->numRLE * sizeof(RwUInt16))) {
            /* Original bug, kept: frees the split data itself, an address inside the skin. */
            RwFree(skinSplitData);
            return NULL;
        }
    }

    return stream;
} // 0x802E9884

/*
 * ==================== _rpSkinSplitDataStreamGetSize ====================
 * The size of the stream data: the three counts and, when there are meshes, the split block.
 */
int _rpSkinSplitDataStreamGetSize(const RpSkin* skin) {
    unsigned int streamSize;

    streamSize = 3 * sizeof(RwInt32);

    if (skin->skinSplitData.numMeshes) {
        streamSize += skin->boneData.numBones + skin->skinSplitData.numMeshes * sizeof(RwUInt16) +
                      skin->skinSplitData.numRLE * sizeof(RwUInt16);
    }

    return streamSize;
} // 0x802E9A34
