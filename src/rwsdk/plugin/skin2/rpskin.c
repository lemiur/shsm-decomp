// Unit name and boundary: Gormiti link map (RpSkin2.a rpskin.obj; its source there is plugin/skin2/rpskin.c)
// Names and layout: Gormiti link map and DWARF (RpSkin2 rpskin.obj)
/*
 * rpskin.c: the RpSkin2 plugin. It registers the skin's extension of a geometry (the RpSkin
 * pointer) and of an atomic (the HAnim hierarchy that animates it), their stream callbacks and the
 * engine's open and close, which create the free list of RpSkin, the cache of skin matrices and
 * the platform's pipelines. The skin's stream is one chunk of the geometry's plugin data (the
 * native stream, for an instanced geometry, is skinstream.c's).
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/matfx/multitex.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/plugin/skin2/skin.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>

/* The toon plugin's ID: only the skin's choice of pipeline names it (name invented). */
#define rwID_TOONPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x2E)

/* The skin matrices the cache holds for one frame (name invented). */
#define SKINMATRIXCACHESIZE 256

/* The free list of RpSkin: its first block's space, and the size and count of the blocks. */
static RwFreeList _rpSkinFreeList;
static int _rpSkinFreeListBlockSize = 20;
static int _rpSkinFreeListPreallocBlocks = 1;

/*
 * ==================== MatfxPluginIsAttached ====================
 * Whether the material effects plugin is attached to the engine. (Gormiti Debug build: a function
 * of its own; retail inlined it.)
 */
static int MatfxPluginIsAttached(void) {
    return RwEngineGetPluginOffset(rwID_MATERIALEFFECTSPLUGIN) != -1;
}

/*
 * ==================== ToonPluginIsAttached ====================
 * Whether the toon plugin is attached to the engine. (Gormiti Debug build: a function of its own;
 * retail inlined it.)
 */
static int ToonPluginIsAttached(void) {
    return RwEngineGetPluginOffset(rwID_TOONPLUGIN) != -1;
}

/*
 * ==================== SkinAtomicAttachBestPipeForAttachedPlugins ====================
 * Attaches the atomic's pipeline of the type asked for, or the generic one when the plugin that
 * pipeline needs is not attached. (Gormiti Debug build: a function of its own; retail inlined it.)
 */
static RpAtomic* SkinAtomicAttachBestPipeForAttachedPlugins(RpAtomic* atomic, RpSkinType type) {
    if (!MatfxPluginIsAttached() && type == rpSKINTYPEMATFX) {
        type = rpSKINTYPEGENERIC;
    } else if (!ToonPluginIsAttached() && type == rpSKINTYPETOON) {
        type = rpSKINTYPEGENERIC;
    }

    return _rpSkinPipelinesAttach(atomic, type);
}

/*
 * ==================== SkinAtomicSetup ====================
 * Gives the atomic its skin's pipeline when its geometry has a skin. (Gormiti Debug build: a
 * function of its own; retail inlined it.)
 */
static RpAtomic* SkinAtomicSetup(RpAtomic* atomic, RpSkinType type) {
    RpGeometry* geometry;

    geometry = RpAtomicGetGeometry(atomic);
    if (geometry != NULL) {
        RpSkin* skin = RpSkinGeometryGetSkin(geometry);

        if (skin != NULL) {
            SkinAtomicAttachBestPipeForAttachedPlugins(atomic, type);
            /* Debug build: assertion at this point (Gormiti Debug): the pipeline was attached */
        }
    }

    return atomic;
}

/*
 * ==================== SkinFindMaxWeights ====================
 * The most weights any vertex has, from the first run of non-zero weights of each vertex. (Gormiti
 * Debug build: a function of its own; retail inlined it.)
 */
static void SkinFindMaxWeights(RpSkin* skin, RwMatrixWeights* matrixWeights, unsigned int numVertices) {
    unsigned int i;

    skin->vertexMaps.maxWeights = 1;

    for (i = 0; i < numVertices; i++) {
        unsigned int j;

        for (j = skin->vertexMaps.maxWeights; j < 4; j++) {
            if (((unsigned int*)&matrixWeights[i])[j] != 0) {
                skin->vertexMaps.maxWeights++;
                if (skin->vertexMaps.maxWeights == 4) {
                    return;
                }
            } else {
                break;
            }
        }
    }
}

/*
 * ==================== SkinFindNumUsedBones ====================
 * Lists the bones the vertices use, each once, in usedBoneList and counts them in numUsedBones.
 * A vertex's bone indices are packed one byte per weight in its word.
 */
static void SkinFindNumUsedBones(RpSkin* skin, unsigned int* indices, RwMatrixWeights* weights,
                                 unsigned char* usedBoneList, unsigned int* numUsedBones,
                                 unsigned int numVertices) {
    unsigned int i;

    /* Debug build: assertion at this point (Gormiti Debug) */
    *numUsedBones = 0;

    for (i = 0; i < numVertices; i++) {
        unsigned int j;

        for (j = 0; j < skin->vertexMaps.maxWeights; j++) {
            if (((unsigned int*)&weights[i])[j] != 0) {
                unsigned int k;
                unsigned int index;
                int newBone;

                newBone = 1;
                index = (indices[i] >> (j * 8)) & 0xFF;

                for (k = 0; k < *numUsedBones; k++) {
                    if (index == usedBoneList[k]) {
                        newBone = 0;
                        break;
                    }
                }

                if (newBone) {
                    usedBoneList[*numUsedBones] = index;
                    (*numUsedBones)++;
                }
            }
        }
    }
} // 0x802E9A68

/*
 * ==================== SkinCreateSkinData ====================
 * Gives the skin one block for its used bone list, its inverse bone matrices (aligned to 16
 * bytes), its vertices' bone indices and its vertices' weights, and copies what is passed in.
 * Returns FALSE when the block cannot be allocated.
 */
static int SkinCreateSkinData(RpSkin* skin, unsigned int numBones, unsigned int numUsedBones,
                              unsigned int numVertices, unsigned char* usedBoneList,
                              RwMatrixWeights* vertexWeights, unsigned int* vertexIndices,
                              RwMatrix* inverseMatrices) {
    unsigned int size;

    size = numBones * sizeof(RwMatrix) + numUsedBones +
           numVertices * (sizeof(unsigned int) + sizeof(RwMatrixWeights)) + 15;

    skin->unaligned = (void**)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
    if (skin->unaligned == NULL) {
        return FALSE;
    }

    memset(skin->unaligned, 0, size);

    skin->boneData.numBones = numBones;
    skin->boneData.numUsedBones = numUsedBones;
    skin->boneData.usedBoneList = (unsigned char*)skin->unaligned;
    skin->boneData.invBoneToSkinMat =
        (RwMatrix*)(((unsigned int)(skin->boneData.usedBoneList + numUsedBones) + 15) & ~15);
    skin->vertexMaps.matrixIndices = (unsigned int*)(skin->boneData.invBoneToSkinMat + numBones);
    skin->vertexMaps.matrixWeights = (RwMatrixWeights*)(skin->vertexMaps.matrixIndices + numVertices);

    if (usedBoneList != NULL && numUsedBones != 0) {
        unsigned int size = numUsedBones * sizeof(unsigned char);

        memcpy(skin->boneData.usedBoneList, usedBoneList, size);
    }

    if (inverseMatrices != NULL) {
        unsigned int iBone;

        for (iBone = skin->boneData.numBones; iBone-- > 0;) {
            skin->boneData.invBoneToSkinMat[iBone] = inverseMatrices[iBone];
        }
    }

    if (vertexIndices != NULL) {
        unsigned int size = numVertices * sizeof(unsigned int);

        memcpy(skin->vertexMaps.matrixIndices, vertexIndices, size);
    }

    if (vertexWeights != NULL) {
        unsigned int size = numVertices * sizeof(RwMatrixWeights);

        memcpy(skin->vertexMaps.matrixWeights, vertexWeights, size);
    }

    return TRUE;
} // 0x802E9B34

/*
 * ==================== SkinOpen ====================
 * Engine open callback: on the first open, creates the platform's pipelines (the generic one, and
 * the material effects and toon ones when those plugins are attached), the free list of RpSkin
 * and the cache of skin matrices, then counts the open.
 */
static void* SkinOpen(void* instance, RwInt32 offset, RwInt32 size) {
    if (_rpSkinGlobals.module.numInstances == 0) {
        unsigned int pipes = 1;

        if (MatfxPluginIsAttached()) {
            pipes |= 2;
        }

        if (ToonPluginIsAttached()) {
            pipes |= 4;
        }

        _rpSkinPipelinesCreate(pipes);

        _rpSkinGlobals.freeList = RwFreeListCreateAndPreallocateSpace(
            sizeof(RpSkin), _rpSkinFreeListBlockSize, 4, _rpSkinFreeListPreallocBlocks, &_rpSkinFreeList,
            rwMEMHINTDUR_GLOBAL | rwID_SKINPLUGIN);

        _rpSkinGlobals.matrixCache.unaligned =
            RwMalloc(SKINMATRIXCACHESIZE * sizeof(RwMatrix) + 15, rwMEMHINTDUR_GLOBAL | rwID_SKINPLUGIN);
        memset(_rpSkinGlobals.matrixCache.unaligned, 0, SKINMATRIXCACHESIZE * sizeof(RwMatrix) + 15);
        _rpSkinGlobals.matrixCache.aligned =
            (RwMatrix*)(((unsigned int)_rpSkinGlobals.matrixCache.unaligned + 15) & ~15);
    }

    _rpSkinGlobals.module.numInstances++;

    return instance;
} // 0x802E9D00

/*
 * ==================== SkinClose ====================
 * Engine close callback: on the last close, destroys the pipelines, the free list and the matrix
 * cache.
 */
static void* SkinClose(void* instance, RwInt32 offset, RwInt32 size) {
    _rpSkinGlobals.module.numInstances--;

    if (_rpSkinGlobals.module.numInstances == 0) {
        _rpSkinPipelinesDestroy();

        RwFreeListDestroy(_rpSkinGlobals.freeList);
        _rpSkinGlobals.freeList = NULL;

        RwFree(_rpSkinGlobals.matrixCache.unaligned);
        _rpSkinGlobals.matrixCache.unaligned = NULL;
    }

    return instance;
} // 0x802E9E18

/*
 * ==================== SkinGeometryConstructor ====================
 * A geometry starts without a skin.
 */
static void* SkinGeometryConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RPSKINGEOMETRYGETDATA(object) = NULL;

    return object;
} // 0x802E9E98

/*
 * ==================== SkinGeometryDestructor ====================
 * Takes the platform data, the skin's block and the skin itself from the geometry.
 */
static void* SkinGeometryDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpGeometry* geometry = (RpGeometry*)object;
    RpSkin* skin = RPSKINGEOMETRYGETDATA(geometry);

    if (skin != NULL) {
        _rpSkinDeinitialize(geometry);

        if (skin->unaligned != NULL) {
            RwFree(skin->unaligned);
        }

        _rpSkinSplitDataDestroy(skin);

        RwFreeListFree(_rpSkinGlobals.freeList, skin);

        RPSKINGEOMETRYGETDATA(geometry) = NULL;
    }

    return object;
} // 0x802E9EB0

/*
 * ==================== SkinGeometryCopy ====================
 * A copy of a geometry does not take the skin.
 */
static void* SkinGeometryCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject,
                              RwInt32 sizeInObject) {
    return dstObject;
} // 0x802E9F58

/*
 * ==================== SkinAtomicConstructor ====================
 * An atomic starts without a hierarchy or platform data.
 */
static void* SkinAtomicConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    memset(RPSKINATOMICGETDATA(object), 0, sizeof(SkinAtomicData));

    return object;
} // 0x802E9F5C

/*
 * ==================== SkinAtomicDestructor ====================
 * Gives up the atomic's skinned vertex data and forgets its hierarchy.
 */
static void* SkinAtomicDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* atomic = (RpAtomic*)object;
    SkinAtomicData* atomicData = RPSKINATOMICGETDATA(atomic);

    _clSkinCache_AtomicDestruct(atomic);

    if (atomicData->hierarchy != NULL) {
        atomicData->hierarchy = NULL;
    }

    return object;
} // 0x802E9FA4

/*
 * ==================== SkinAtomicCopy ====================
 * A copy of an atomic shares the hierarchy.
 */
static void* SkinAtomicCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject,
                            RwInt32 sizeInObject) {
    RPSKINATOMICGETDATA(dstObject)->hierarchy = RPSKINATOMICGETDATA(srcObject)->hierarchy;

    return dstObject;
} // 0x802E9FFC

/*
 * ==================== SkinAtomicAlways ====================
 * Stream callback, always: sets the atomic's pipeline up, the material effects one when the
 * atomic uses them, else the generic one.
 */
static RwBool SkinAtomicAlways(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpSkinType type = rpSKINTYPEGENERIC;
    RpAtomic* atomic = (RpAtomic*)object;

    /* Debug build: assertion at this point (Gormiti Debug) */

    if (MatfxPluginIsAttached() &&
        *RWPLUGINOFFSET(RwUInt8, atomic, RpAtomicGetPluginOffset(rwID_MATERIALEFFECTSPLUGIN)) != 0) {
        type = rpSKINTYPEMATFX;
    }

    SkinAtomicSetup(atomic, type);
    /* Debug build: assertion at this point (Gormiti Debug): the atomic was set up */

    return TRUE;
} // 0x802EA014

/*
 * ==================== SkinAtomicRights ====================
 * Stream callback, rights: the extra data is the pipeline type the atomic was written with.
 */
static RwBool SkinAtomicRights(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject,
                               RwUInt32 extraData) {
    RpAtomic* atomic = (RpAtomic*)object;
    RpSkinType type = (RpSkinType)extraData;

    SkinAtomicSetup(atomic, type);
    /* Debug build: assertion at this point (Gormiti Debug): the atomic was set up */

    return TRUE;
} // 0x802EA104

/*
 * ==================== SkinGeometrySize ====================
 * The bytes the skin takes in a stream: the packed counts, the used bone list, the vertices'
 * indices and weights, the inverse bone matrices and the split data; a native geometry's skin is
 * sized by the native stream.
 */
static RwInt32 SkinGeometrySize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpGeometry* geometry = (RpGeometry*)object;
    RpSkin* skin;
    RwInt32 size = 0;

    skin = RPSKINGEOMETRYGETDATA(geometry);
    if (skin != NULL) {
        if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
            size = sizeof(unsigned int) + skin->boneData.numUsedBones;
            size += geometry->numVertices * sizeof(unsigned int);
            size += geometry->numVertices * sizeof(RwMatrixWeights);
            size += skin->boneData.numBones * sizeof(RwMatrix);
            size += _rpSkinSplitDataStreamGetSize(skin);
        } else {
            size = _rpSkinGeometryNativeSize(geometry);
        }
    }

    return size;
} // 0x802EA1C0

/*
 * ==================== SkinGeometryWrite ====================
 * Writes the skin's chunk: the counts packed into one word, the used bone list, the vertices'
 * indices and weights, the inverse bone matrices, then the split data. A native geometry's skin
 * goes through the native stream.
 */
static RwStream* SkinGeometryWrite(RwStream* stream, RwInt32 binaryLength, const void* object,
                                   RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpGeometry* geometry = (RpGeometry*)object;
    RpSkin* skin = RPSKINGEOMETRYGETDATA(geometry);

    if (skin != NULL) {
        if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
            unsigned int numVertices = geometry->numVertices;
            unsigned int skinInfo;

            skinInfo = ((skin->vertexMaps.maxWeights & 0xFF) << 16) | ((skin->boneData.numUsedBones & 0xFF) << 8) |
                       (skin->boneData.numBones & 0xFF);

            if (RwStreamWriteInt32(stream, (RwInt32*)&skinInfo, sizeof(skinInfo)) == NULL) {
                return NULL;
            }

            if (RwStreamWrite(stream, skin->boneData.usedBoneList, skin->boneData.numUsedBones) == NULL) {
                return NULL;
            }

            if (RwStreamWriteInt32(stream, (RwInt32*)skin->vertexMaps.matrixIndices,
                                   numVertices * sizeof(unsigned int)) == NULL) {
                return NULL;
            }

            if (RwStreamWriteReal(stream, (RwReal*)skin->vertexMaps.matrixWeights,
                                  numVertices * sizeof(RwMatrixWeights)) == NULL) {
                return NULL;
            }

            if (RwStreamWriteReal(stream, (RwReal*)skin->boneData.invBoneToSkinMat,
                                  skin->boneData.numBones * sizeof(RwMatrix)) == NULL) {
                return NULL;
            }

            if (_rpSkinSplitDataStreamWrite(stream, skin) == NULL) {
                return NULL;
            }
        } else {
            if (_rpSkinGeometryNativeWrite(stream, geometry) == NULL) {
                return NULL;
            }
        }
    }

    return stream;
} // 0x802EA250

/*
 * ==================== SkinGeometryRead ====================
 * Reads the skin's chunk into a new skin of the geometry. A stream that does not say how many
 * weights or used bones the skin has gets them counted from the data (the original counts them
 * before the data is read, from null arrays). A native geometry's skin goes through the native
 * stream.
 *
 * Not exact (99.71%): the inlined SkinFindMaxWeights takes its two loop counters in swapped
 * registers (register wall). Retail's two copies want opposite orders, which the helper alone cannot
 * give: SkinGeometryRead's inner counter is r5 and its outer r4, SkinAtomicRead's inner r4 and its
 * outer r5. Declaring j before i closes this function and opens SkinAtomicRead's, so neither form is
 * kept; closing both needs SkinAtomicRead's call-site context to differ from this one by one more live
 * value, which no evidenced spelling supplies.
 */
static RwStream* SkinGeometryRead(RwStream* stream, RwInt32 binaryLength, void* object,
                                  RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpSkin* skin;
    RpGeometry* geometry = (RpGeometry*)object;

    if ((geometry->flags & rpGEOMETRYNATIVE) == 0) {
        unsigned int skinInfo;
        unsigned int maxWeights;
        unsigned int numBones;
        unsigned int numUsedBones;
        unsigned int numVertices;
        unsigned int size;
        unsigned char usedBoneList[256];

        if (RwStreamReadInt32(stream, (RwInt32*)&skinInfo, sizeof(skinInfo)) == NULL) {
            return NULL;
        }

        numBones = skinInfo & 0xFF;
        numUsedBones = (skinInfo >> 8) & 0xFF;
        maxWeights = (skinInfo >> 16) & 0xFF;
        numVertices = geometry->numVertices;
        size = numUsedBones;

        skin = (RpSkin*)RwFreeListAlloc(_rpSkinGlobals.freeList, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
        memset(skin, 0, sizeof(RpSkin));

        if (maxWeights == 0) {
            SkinFindMaxWeights(skin, NULL, numVertices);
        }

        if (size == 0) {
            SkinFindNumUsedBones(skin, NULL, NULL, usedBoneList, &size, numVertices);
        }

        if (!SkinCreateSkinData(skin, numBones, size, numVertices, usedBoneList, NULL, NULL, NULL)) {
            RwFreeListFree(_rpSkinGlobals.freeList, skin);
            skin = NULL;
        }

        if (skin == NULL) {
            return NULL;
        }

        if (RwStreamRead(stream, skin->boneData.usedBoneList, numUsedBones) != numUsedBones) {
            return NULL;
        }

        if (RwStreamReadInt32(stream, (RwInt32*)skin->vertexMaps.matrixIndices,
                              numVertices * sizeof(unsigned int)) == NULL) {
            return NULL;
        }

        if (RwStreamReadReal(stream, (RwReal*)skin->vertexMaps.matrixWeights,
                             numVertices * sizeof(RwMatrixWeights)) == NULL) {
            return NULL;
        }

        skin->vertexMaps.maxWeights = maxWeights;

        if (RwStreamReadReal(stream, (RwReal*)skin->boneData.invBoneToSkinMat,
                             skin->boneData.numBones * sizeof(RwMatrix)) == NULL) {
            return NULL;
        }

        if (_rpSkinSplitDataStreamRead(stream, skin) == NULL) {
            return NULL;
        }

        if (skin != RPSKINGEOMETRYGETDATA(geometry)) {
            if (RPSKINGEOMETRYGETDATA(geometry) != NULL) {
                _rpSkinDeinitialize(geometry);
            }

            RPSKINGEOMETRYGETDATA(geometry) = skin;

            if (skin != NULL) {
                _rpSkinInitialize(geometry);
            }
        }
    } else {
        if (_rpSkinGeometryNativeRead(stream, geometry) == NULL) {
            return NULL;
        }
    }

    return stream;
} // 0x802EA3A4

/*
 * ==================== SkinAtomicRead ====================
 * Reads the skin of the atomic's geometry from an older stream, which keeps it in the atomic's
 * chunk: the used bone count, the vertices' indices and weights, then each bone's matrix after
 * 12 skipped bytes. The skin is read only when the geometry has none; otherwise the chunk is
 * skipped. The maximum weights and the used bones are counted from the data.
 */
static RwStream* SkinAtomicRead(RwStream* stream, RwInt32 binaryLength, void* object,
                                RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpAtomic* atomic = (RpAtomic*)object;
    RpSkin* skin;
    RpGeometry* geometry = atomic->geometry;

    if (RPSKINGEOMETRYGETDATA(geometry) == NULL) {
        unsigned int skinInfo;
        unsigned int numBones;
        unsigned int numVertices;
        unsigned int iBone;
        unsigned char usedBoneList[256];
        unsigned int numUsedBones;

        if (RwStreamReadInt32(stream, (RwInt32*)&skinInfo, sizeof(skinInfo)) == NULL) {
            return NULL;
        }

        numVertices = geometry->numVertices;
        numBones = skinInfo;
        numUsedBones = numBones;

        skin = (RpSkin*)RwFreeListAlloc(_rpSkinGlobals.freeList, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);
        memset(skin, 0, sizeof(RpSkin));

        if (numUsedBones == 0) {
            SkinFindNumUsedBones(skin, NULL, NULL, usedBoneList, &numUsedBones, numVertices);
        }

        if (!SkinCreateSkinData(skin, numBones, numUsedBones, numVertices, usedBoneList, NULL, NULL, NULL)) {
            RwFreeListFree(_rpSkinGlobals.freeList, skin);
            skin = NULL;
        }

        if (RwStreamSkip(stream, 4) == NULL) {
            return NULL;
        }

        if (RwStreamReadInt32(stream, (RwInt32*)skin->vertexMaps.matrixIndices,
                              numVertices * sizeof(unsigned int)) == NULL) {
            return NULL;
        }

        if (RwStreamReadReal(stream, (RwReal*)skin->vertexMaps.matrixWeights,
                             numVertices * sizeof(RwMatrixWeights)) == NULL) {
            return NULL;
        }

        for (iBone = 0; iBone < skin->boneData.numBones; iBone++) {
            if (RwStreamSkip(stream, 12) == NULL) {
                return NULL;
            }

            if (RwStreamReadReal(stream, (RwReal*)&skin->boneData.invBoneToSkinMat[iBone], sizeof(RwMatrix)) ==
                NULL) {
                return NULL;
            }
        }

        SkinFindMaxWeights(skin, skin->vertexMaps.matrixWeights, numVertices);
        SkinFindNumUsedBones(skin, skin->vertexMaps.matrixIndices, skin->vertexMaps.matrixWeights,
                             skin->boneData.usedBoneList, &skin->boneData.numUsedBones, numVertices);

        if (skin != RPSKINGEOMETRYGETDATA(geometry)) {
            if (RPSKINGEOMETRYGETDATA(geometry) != NULL) {
                _rpSkinDeinitialize(geometry);
            }

            RPSKINGEOMETRYGETDATA(geometry) = skin;

            if (skin != NULL) {
                _rpSkinInitialize(geometry);
            }
        }
    } else {
        if (RwStreamSkip(stream, sizeInObject) == NULL) {
            return NULL;
        }
    }

    return stream;
} // 0x802EA64C

/*
 * ==================== SkinAtomicWrite ====================
 * The atomic's chunk is never written; the skin goes with its geometry.
 */
static RwStream* SkinAtomicWrite(RwStream* stream, RwInt32 binaryLength, const void* object,
                                 RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return stream;
} // 0x802EA908

/*
 * ==================== SkinAtomicGetSize ====================
 * The atomic's chunk takes no bytes.
 */
static RwInt32 SkinAtomicGetSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return 0;
} // 0x802EA90C

/*
 * ==================== RpSkinPluginAttach ====================
 * Registers the plugin with the engine, then its atomic and geometry extensions and their stream
 * callbacks.
 */
RwBool RpSkinPluginAttach(void) {
    _rpSkinGlobals.engineOffset = RwEngineRegisterPlugin(0, rwID_SKINPLUGIN, SkinOpen, SkinClose);

    _rpSkinGlobals.atomicOffset = RpAtomicRegisterPlugin(sizeof(SkinAtomicData), rwID_SKINPLUGIN,
                                                          SkinAtomicConstructor, SkinAtomicDestructor,
                                                          SkinAtomicCopy);

    RpAtomicRegisterPluginStream(rwID_SKINPLUGIN, SkinAtomicRead, SkinAtomicWrite, SkinAtomicGetSize);
    RpAtomicSetStreamAlwaysCallBack(rwID_SKINPLUGIN, SkinAtomicAlways);
    RpAtomicSetStreamRightsCallBack(rwID_SKINPLUGIN, SkinAtomicRights);

    _rpSkinGlobals.geometryOffset = RpGeometryRegisterPlugin(sizeof(RpSkin*), rwID_SKINPLUGIN,
                                                              SkinGeometryConstructor,
                                                              SkinGeometryDestructor, SkinGeometryCopy);

    RpGeometryRegisterPluginStream(rwID_SKINPLUGIN, SkinGeometryRead, SkinGeometryWrite, SkinGeometrySize);

    return TRUE;
} // 0x802EA914

/*
 * ==================== RpSkinAtomicSetHAnimHierarchy ====================
 * Gives the atomic the hierarchy that animates its skin.
 */
RpAtomic* RpSkinAtomicSetHAnimHierarchy(RpAtomic* atomic, RpHAnimHierarchy* hierarchy) {
    RPSKINATOMICGETDATA(atomic)->hierarchy = hierarchy;

    return atomic;
} // 0x802EAA14

/*
 * ==================== RpSkinAtomicGetHAnimHierarchy ====================
 * The hierarchy that animates the atomic's skin.
 */
RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic) {
    return (RpHAnimHierarchy*)RPSKINATOMICGETDATA(atomic)->hierarchy;
} // 0x802EAA28

/*
 * ==================== RpSkinGeometryGetSkin ====================
 * The skin of the geometry, or NULL.
 */
RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry) {
    return RPSKINGEOMETRYGETDATA(geometry);
} // 0x802EAA3C

/*
 * ==================== RpSkinGeometrySetSkin ====================
 * Replaces the geometry's skin, and sets the new one up. Returns NULL when that fails.
 */
RpGeometry* RpSkinGeometrySetSkin(RpGeometry* geometry, RpSkin* skin) {
    if (skin != RPSKINGEOMETRYGETDATA(geometry)) {
        if (RPSKINGEOMETRYGETDATA(geometry) != NULL) {
            _rpSkinDeinitialize(geometry);
        }

        RPSKINGEOMETRYGETDATA(geometry) = skin;

        if (skin != NULL) {
            if (_rpSkinInitialize(geometry) == NULL) {
                return NULL;
            }
        }
    }

    return geometry;
} // 0x802EAA50

/*
 * ==================== RpSkinGetNumBones ====================
 * The number of bones of the skin's skeleton.
 */
unsigned int RpSkinGetNumBones(RpSkin* skin) {
    return skin->boneData.numBones;
} // 0x802EAADC

/*
 * ==================== RpSkinGetVertexBoneWeights ====================
 * The weights of the skin's vertices.
 */
RwMatrixWeights* RpSkinGetVertexBoneWeights(RpSkin* skin) {
    return skin->vertexMaps.matrixWeights;
} // 0x802EAAE4

/*
 * ==================== RpSkinGetVertexBoneIndices ====================
 * The bone indices of the skin's vertices.
 */
unsigned int* RpSkinGetVertexBoneIndices(RpSkin* skin) {
    return skin->vertexMaps.matrixIndices;
} // 0x802EAAEC

/*
 * ==================== RpSkinGetSkinToBoneMatrices ====================
 * The skin's inverse bone matrices.
 */
RwMatrix* RpSkinGetSkinToBoneMatrices(RpSkin* skin) {
    return skin->boneData.invBoneToSkinMat;
} // 0x802EAAF4
