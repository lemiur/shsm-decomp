// Unit name and boundary: Gormiti link map (RpSkin2.a skingcn.obj)
// Names and layout: Gormiti link map and DWARF (RpSkin2.a skingcn.obj), Final and Debug builds.
/*
 * skingcn.c: GameCube/Wii vertex skinning and rendering callbacks for RpSkin2.
 * Handles resource-entry lifetime and caching for skinned vertex buffers, matrix palette blending,
 * paired-single vertex/normal skinning dispatch (via GQR5-7 and skingcnasm.c), geometry
 * instancing/reinstancing, matrix palette loading, and mesh rendering.
 */
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXManage.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/driver/wii/dldevice.h>
#include <rwsdk/driver/wii/dlraster.h>
#include <rwsdk/driver/wii/dlrendst.h>
#include <rwsdk/driver/wii/dltextur.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/plugin/hanim/rphanim.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/world/climaxcommon.h>
#include <rwsdk/world/climaxplatform.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/ibuffer.h>
#include <rwsdk/world/instancegeom.h>
#include <rwsdk/world/vtxfmt.h>

#define RPSKINGEOMETRYGETDATA(geometry) \
    (*RWPLUGINOFFSET(RpSkin*, (geometry), _rpSkinGlobals.geometryOffset))
#define RPSKINATOMICGETDATA(atomic) \
    (RWPLUGINOFFSET(SkinAtomicData, (atomic), _rpSkinGlobals.atomicOffset))

#define SKINRESENTRYGETARRAYBASE(resEntry) ((void*)(((unsigned int)(resEntry) + 0x3D) & ~0x1F))

/* GQR5 setting for 1.7 fixed-point unsigned 8-bit bone weights (LD_SCALE = 7, LD_TYPE = 4 / GX_U8). */
#define RPSKIN_WEIGHT_GQR 0x07040000

/*
 * ==================== _rpSkinMainResEntryCB ====================
 * Resource entry destroy callback for the main instanced geometry/atomic entry: waits for GX draw
 * sync on its vertex buffer, notifies the skin cache, and returns the unskinned vertex/normal
 * arrays back to the resource entry so the arena frees them with it.
 */
static void _rpSkinMainResEntryCB(RwResEntry* resEntry) {
    RxGameCubeVertexBuffer* vbHeader;
    unsigned int flags;

    vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);
    if (vbHeader->token == _RwDlTokenCurrent) {
        GXSetDrawSync(_RwDlTokenCurrent);
        _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % rwDLTOKENMODULUS;
    }
    while (!_rwDlTokenQueryDone(vbHeader->token)) {
    }

    _clSkinCache_RepEntryDestroyNotify(resEntry);

    flags = ((RpGeometry*)resEntry->owner)->flags;
    if (((RwObject*)resEntry->owner)->type == rpATOMIC) {
        SkinAtomicData* atomicData = RPSKINATOMICGETDATA(resEntry->owner);

        vbHeader->attr[0].array = atomicData->platformData.vertices;
        if (flags & rpGEOMETRYNORMALS) {
            vbHeader->attr[1].array = atomicData->platformData.normals;
        }
        atomicData->platformData.vertices = NULL;
        atomicData->platformData.normals = NULL;
    } else {
        RpSkin* skin = RPSKINGEOMETRYGETDATA(resEntry->owner);

        vbHeader->attr[0].array = skin->platformData.vertices;
        if (flags & rpGEOMETRYNORMALS) {
            vbHeader->attr[1].array = skin->platformData.normals;
        }
        skin->platformData.vertices = NULL;
        skin->platformData.normals = NULL;
    }
} // 0x802EB278

/*
 * ==================== _rpSkinResEntryWaitDone ====================
 * Resource entry destroy callback for a cached skinned vertex buffer: waits for GX draw sync on
 * its token and unregisters the entry from the skin cache.
 */
static void _rpSkinResEntryWaitDone(RwResEntry* resEntry) {
    RxGameCubeVertexBuffer* vbHeader;

    vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);
    if (vbHeader->token == _RwDlTokenCurrent) {
        GXSetDrawSync(_RwDlTokenCurrent);
        _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % rwDLTOKENMODULUS;
    }
    while (!_rwDlTokenQueryDone(vbHeader->token)) {
    }

    _clSkinCache_UnRegisterSkinRes(resEntry);
} // 0x802EB388

/*
 * ==================== _rpSkinVertexBuffersUpdate ====================
 * Acquires or reuses a skinned vertex buffer resource entry for the current frame, stashing the
 * unskinned arrays on the atomic or skin if needed and pointing the vertex buffer attributes at
 * the 32-byte aligned buffer.
 */
int _rpSkinVertexBuffersUpdate(RpSkin* skin, RpAtomic* atomic, RxGameCubeVertexBuffer* vbHeader,
                               RxGameCubePipeData* pipeData) {
    RwResEntry* childResEntry;
    RwResEntry* parentResEntry;
    RpGeometry* geometry;
    SkinAtomicData* atomicData;

    geometry = atomic->geometry;
    RwResourcesUseResEntry(pipeData->resEntry);
    pipeData->resEntry->destroyNotify = _rpSkinMainResEntryCB;

    if (((RwObject*)pipeData->resEntry->owner)->type == rpATOMIC) {
        SkinAtomicData* atomicData = RPSKINATOMICGETDATA(pipeData->resEntry->owner);

        if (atomicData->platformData.vertices == NULL) {
            atomicData->platformData.vertices = vbHeader->attr[0].array;
            vbHeader->attr[0].array = NULL;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                atomicData->platformData.normals = vbHeader->attr[1].array;
                vbHeader->attr[1].array = NULL;
            }
        }
    } else {
        if (skin->platformData.vertices == NULL) {
            skin->platformData.vertices = vbHeader->attr[0].array;
            vbHeader->attr[0].array = NULL;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                skin->platformData.normals = vbHeader->attr[1].array;
                vbHeader->attr[1].array = NULL;
            }
        }
    }

    if (vbHeader->attr[0].array != NULL) {
        if (!_clSkinCache_CheckResource(atomic, ((RwResEntry**)vbHeader->attr[0].array)[-1], vbHeader)) {
            vbHeader->attr[0].array = NULL;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                vbHeader->attr[1].array = NULL;
            }
        }
    }

    if (vbHeader->attr[0].array != NULL) {
        if (!_rwDlTokenQueryDone(((RxGameCubeVertexBuffer*)(((RwResEntry**)vbHeader->attr[0].array)[-1] + 1))->token)) {
            _clSkinCache_OrphanSkinRes((RwResEntry*)vbHeader->attr[0].array - 1);
            vbHeader->attr[0].array = NULL;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                vbHeader->attr[1].array = NULL;
            }
        }
    }

    if (vbHeader->attr[0].array == NULL) {
        unsigned int size;
        RpGameCubeVtxFmt* vtxFmt;

        vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
        if (vtxFmt != NULL) {
            unsigned int vtxSizeConvTable[5] = { 3, 3, 6, 6, 12 };

            size = (((geometry->numVertices * vtxSizeConvTable[vtxFmt->pos]) + 0x1F) & ~0x1F) + 6;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                size += ((geometry->numVertices * (vtxSizeConvTable[vtxFmt->norm] * (vtxFmt->nbt ? 3 : 1))) + 0x1F) &
                        ~0x1F;
            }
        } else {
            unsigned int posSize = ((geometry->numVertices * 12) + 0x1F) & ~0x1F;

            size = posSize + 6;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                size += posSize;
            }
        }

        childResEntry = _clSkinCache_GetUsableResource(atomic, (RwResEntry**)&vbHeader->attr[0].array, size,
                                                       _rpSkinResEntryWaitDone, vbHeader);
        parentResEntry = (geometry->numMorphTargets == 1) ? geometry->repEntry : atomic->repEntry;
        if (childResEntry == NULL || parentResEntry == NULL) {
            if (childResEntry != NULL) {
                RwResourcesFreeResEntry(childResEntry);
            }
            return 0;
        }

        vbHeader->attr[0].array = (void*)(((unsigned int)vbHeader->attr[0].array + 0x3D) & ~0x1F);
        if (geometry->flags & rpGEOMETRYNORMALS) {
            vbHeader->attr[1].array = (void*)((unsigned char*)vbHeader->attr[0].array +
                                              (((geometry->numVertices * vbHeader->attr[0].stride) + 0x1F) & ~0x1F));
        }
        ((RwResEntry**)vbHeader->attr[0].array)[-1] = childResEntry;
    } else {
        RwResourcesUseResEntry(((RwResEntry**)vbHeader->attr[0].array)[-1]);
    }

    atomicData = RPSKINATOMICGETDATA(atomic);
    atomicData->platformData.lastUpdatedFrameID = _RwSysFrameID;
    atomicData->platformData.skinResEntry = ((RwResEntry**)vbHeader->attr[0].array)[-1];
    ((RxGameCubeVertexBuffer*)(((RwResEntry**)vbHeader->attr[0].array)[-1] + 1))->token = _RwDlTokenCurrent;
    return 1;
} // 0x802EB418

/*
 * ==================== _rpSkinMatrixBlendUpdate ====================
 * Builds the bone matrix palette for the bones used by the skin, transforming through the
 * hierarchy's frame LTMs or matrix array and into view or atomic-local space.
 */
void _rpSkinMatrixBlendUpdate(RwMatrix* matDst, RpSkin* skin, RwMatrix* ltm, RpHAnimHierarchy* hierarchy) {
    if (hierarchy) {
        if (hierarchy->flags & rpHANIMHIERARCHYNOMATRICES) {
            unsigned int i;
            RwMatrix* lastMatrix;
            RwMatrix inverseLTM;

            if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
                rwMatrixSetFlags(&inverseLTM, 0);
                RwMatrixInvert(&inverseLTM, ltm);
                lastMatrix = &inverseLTM;
            } else {
                lastMatrix = &_RwDlInvCamLTM;
            }

            for (i = 0; i < skin->boneData.numUsedBones; i++) {
                RwMatrix tmpMatrix;
                RwMatrix* hierLTM;

                rwMatrixSetFlags(&tmpMatrix, 0);
                hierLTM = RwFrameGetLTM(hierarchy->pNodeInfo[skin->boneData.usedBoneList[i]].pFrame);
                RwMatrixMultiply(&tmpMatrix, &skin->boneData.invBoneToSkinMat[skin->boneData.usedBoneList[i]],
                                 hierLTM);
                RwMatrixMultiply(&matDst[skin->boneData.usedBoneList[i]], &tmpMatrix, lastMatrix);
            }
        } else if (hierarchy->flags & rpHANIMHIERARCHYLOCALSPACEMATRICES) {
            if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
                unsigned int i;

                for (i = 0; i < skin->boneData.numUsedBones; i++) {
                    RwMatrixMultiply(&matDst[skin->boneData.usedBoneList[i]],
                                     &skin->boneData.invBoneToSkinMat[skin->boneData.usedBoneList[i]],
                                     &hierarchy->pMatrixArray[skin->boneData.usedBoneList[i]]);
                }
            } else {
                RwMatrix transMatrix;

                rwMatrixSetFlags(&transMatrix, 0);
                RwMatrixMultiply(&transMatrix, ltm, &_RwDlInvCamLTM);
                _rpSkinMatrixBlendUpdateASM(matDst, skin->boneData.invBoneToSkinMat, hierarchy->pMatrixArray,
                                            &transMatrix, skin->boneData.usedBoneList, skin->boneData.numUsedBones);
            }
        } else {
            RwMatrix* lastMatrix;
            RwMatrix inverseLTM;

            if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
                rwMatrixSetFlags(&inverseLTM, 0);
                RwMatrixInvert(&inverseLTM, ltm);
                lastMatrix = &inverseLTM;
            } else {
                lastMatrix = &_RwDlInvCamLTM;
            }

            _rpSkinMatrixBlendUpdateASM(matDst, skin->boneData.invBoneToSkinMat, hierarchy->pMatrixArray, lastMatrix,
                                        skin->boneData.usedBoneList, skin->boneData.numUsedBones);
        }
    }
} // 0x802EB850

/*
 * ==================== _rpSkinBlendBodyP ====================
 * Skins vertex positions into dstVertices using GQR5/GQR6 for quantised loads and stores, then
 * flushes the destination buffer and invalidates the GX vertex cache.
 */
// ASM-HELPER: C function whose GQR5/GQR6 mfspr/mtspr statements have no C spelling in MWCC; matches Gormiti DWARF locals
void _rpSkinBlendBodyP(RpSkin* skin, RwMatrix* matrixCache, void* srcVertices, void* dstVertices,
                       RpGameCubeVtxFmt* vtxFmt, int numVertices) {
    register unsigned int tmp;
    register unsigned int weightGQR;
    register unsigned int posGQR;
    unsigned int size;
    SkinAsmData asmData;

    weightGQR = RPSKIN_WEIGHT_GQR;
    posGQR = 0;

    asmData.dstVertices = dstVertices;
    asmData.srcVertices = srcVertices;
    asmData.numVerts = numVertices;

    if (vtxFmt != NULL) {
        unsigned char vtxFmtTypeConvTable[5] = { 4, 6, 5, 7, 0 };
        unsigned char vtxFmtSizeConvTable[5] = { 1, 1, 2, 2, 4 };

        posGQR = (vtxFmt->posFrac << 8) | vtxFmtTypeConvTable[vtxFmt->pos];
        posGQR |= posGQR << 16;
        asmData.posSize = vtxFmtSizeConvTable[vtxFmt->pos];
    } else {
        asmData.posSize = 4;
    }

    asm {
        mfspr tmp, GQR5
        mtspr GQR5, weightGQR
        mtspr GQR6, posGQR
    }

    switch (skin->vertexMaps.maxWeights) {
    case 1: {
        int i;

        for (i = 0; i < numVertices; i++) {
            int index = skin->vertexMaps.matrixIndices[i] & 0xFF;

            RwV3dTransformPoint(&((RwV3d*)dstVertices)[i], &((RwV3d*)skin->platformData.vertices)[i],
                                &matrixCache[index]);
        }
        break;
    }
    case 2:
        _rwDlSkinUpdate2WeightsP(matrixCache, skin, &asmData);
        break;
    case 3:
        _rwDlSkinUpdate3WeightsP(matrixCache, skin, &asmData);
        break;
    case 4:
        _rwDlSkinUpdate4WeightsP(matrixCache, skin, &asmData);
        break;
    }

    asm {
        mtspr GQR5, tmp
    }

    size = numVertices * 3;
    size *= asmData.posSize;
    DCFlushRange(dstVertices, (size + 0x1F) & ~0x1F);
    GXInvalidateVtxCache();
} // 0x802EBA40

/*
 * ==================== _rpSkinBlendBodyPN ====================
 * Skins vertex positions and normals into dstVertices/dstNormals using GQR5/GQR6/GQR7, then
 * flushes the destination buffers and invalidates the GX vertex cache.
 */
// ASM-HELPER: C function whose GQR5/GQR6/GQR7 mfspr/mtspr statements have no C spelling in MWCC; matches Gormiti DWARF locals
void _rpSkinBlendBodyPN(RpSkin* skin, RwMatrix* matrixCache, void* srcVertices, void* srcNormals,
                        void* dstVertices, void* dstNormals, RpGameCubeVtxFmt* vtxFmt, int numVertices) {
    register unsigned int tmp;
    register unsigned int weightGQR;
    register unsigned int posGQR;
    register unsigned int normGQR;
    unsigned int size;
    SkinAsmData asmData;

    weightGQR = RPSKIN_WEIGHT_GQR;
    posGQR = 0;
    normGQR = 0;

    asmData.dstVertices = dstVertices;
    asmData.dstNormals = dstNormals;
    asmData.srcVertices = srcVertices;
    asmData.srcNormals = srcNormals;
    asmData.numVerts = numVertices;

    if (vtxFmt != NULL) {
        unsigned char vtxFmtTypeConvTable[5] = { 4, 6, 5, 7, 0 };
        unsigned char vtxFmtSizeConvTable[5] = { 1, 1, 2, 2, 4 };
        unsigned char vtxFmtNormConvTable[5] = { 0, 6, 0, 14, 0 };

        posGQR = (vtxFmt->posFrac << 8) | vtxFmtTypeConvTable[vtxFmt->pos];
        posGQR |= posGQR << 16;

        asmData.posSize = vtxFmtSizeConvTable[vtxFmt->pos];

        normGQR = (vtxFmtNormConvTable[vtxFmt->norm] << 8) | vtxFmtTypeConvTable[vtxFmt->norm];
        normGQR |= normGQR << 16;

        asmData.nrmSize = vtxFmtSizeConvTable[vtxFmt->norm];
        asmData.nrmSkip = (vtxFmt->nbt != 0) ? (asmData.nrmSize * 6) : 0;
    } else {
        asmData.posSize = 4;
        asmData.nrmSize = 4;
        asmData.nrmSkip = 0;
    }

    asm {
        mfspr tmp, GQR5
        mtspr GQR5, weightGQR
        mtspr GQR6, posGQR
        mtspr GQR7, normGQR
    }

    switch (skin->vertexMaps.maxWeights) {
    case 1: {
        int i;

        for (i = 0; i < numVertices; i++) {
            int index = skin->vertexMaps.matrixIndices[i] & 0xFF;

            RwV3dTransformPoint(&((RwV3d*)dstVertices)[i], &((RwV3d*)srcVertices)[i], &matrixCache[index]);
            RwV3dTransformVector(&((RwV3d*)dstNormals)[i], &((RwV3d*)srcNormals)[i], &matrixCache[index]);
        }
        break;
    }
    case 2:
        _rwDlSkinUpdate2WeightsPN(matrixCache, skin, &asmData);
        break;
    case 3:
        _rwDlSkinUpdate3WeightsPN(matrixCache, skin, &asmData);
        break;
    case 4:
        _rwDlSkinUpdate4WeightsPN(matrixCache, skin, &asmData);
        break;
    }

    asm {
        mtspr GQR5, tmp
    }

    size = numVertices * 3;
    size = ((size * asmData.posSize) + 0x1F) & ~0x1F;
    size += numVertices * (asmData.nrmSize * 3 + asmData.nrmSkip);
    DCFlushRange(dstVertices, size);
    GXInvalidateVtxCache();
} // 0x802EBBC4

/*
 * ==================== _rpSkinInstanceCallback ====================
 * Instances a skinned atomic's geometry into a display-list resource entry.
 */
void* _rpSkinInstanceCallback(void* object, RxGameCubePipeData* pipeData) {
    void* owner;
    RpAtomic* atomic;
    RpGeometry* geometry;
    RwResEntry** resEntryOwner;
    RpSkin* skin;

    atomic = (RpAtomic*)object;
    geometry = atomic->geometry;
    skin = RpSkinGeometryGetSkin(geometry);

    if (geometry->numMorphTargets != 1) {
        owner = atomic;
        resEntryOwner = &atomic->repEntry;
    } else {
        owner = geometry;
        resEntryOwner = &geometry->repEntry;
    }

    if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
        if (geometry->flags & rpGEOMETRYNATIVEINSTANCE) {
            if (_RwDlPreInstanceOptimize == 1) {
                pipeData->resEntry = _rwDlGeometrySkinInstanceOptimized(geometry, owner, resEntryOwner);
            } else {
                pipeData->resEntry = _rwDlGeometryInstanceFast(geometry, owner, resEntryOwner);
            }
        } else {
            pipeData->resEntry = _rwDlGeometryInstanceFast(geometry, owner, resEntryOwner);
        }
    } else {
        if (geometry->flags & rpGEOMETRYNATIVEINSTANCE) {
            if (_RwDlPreInstanceOptimize == 1) {
                pipeData->resEntry = _rwDlGeometrySkinInstanceOptimized(geometry, owner, resEntryOwner);
            } else {
                pipeData->resEntry = _rwDlGeometrySkinInstanceFast(geometry, owner, resEntryOwner);
            }
        } else {
            pipeData->resEntry = _rwDlGeometrySkinInstanceFast(geometry, owner, resEntryOwner);
        }
    }

    geometry->lockedSinceLastInst = 0;
    return object;
} // 0x802EBDEC

/*
 * ==================== _rpSkinAtomicReinstanceCallBack ====================
 * Reinstances/updates a skinned atomic for the current frame: reacquires cached skinned vertex
 * data when possible, otherwise updates buffers, blends bone matrices, and runs CPU skinning.
 */
void* _rpSkinAtomicReinstanceCallBack(void* object, RxGameCubePipeData* pipeData) {
    RxGameCubeVertexBuffer* vbHeader;
    RpAtomic* atomic;
    RpGeometry* geometry;
    RpSkin* skin;
    SkinAtomicData* skinAtomicData;

    atomic = (RpAtomic*)object;
    skinAtomicData = RPSKINATOMICGETDATA(atomic);
    vbHeader = (RxGameCubeVertexBuffer*)(pipeData->resEntry + 1);
    geometry = atomic->geometry;
    skin = RPSKINGEOMETRYGETDATA(geometry);

    if (skinAtomicData->platformData.lastUpdatedFrameID == _RwSysFrameID &&
        skinAtomicData->platformData.skinResEntry != NULL) {
        skinAtomicData->platformData.skinResEntry =
            _clSkinCache_ReAquireResource(atomic, skinAtomicData->platformData.skinResEntry, vbHeader);
        if (skinAtomicData->platformData.skinResEntry != NULL) {
            RpGeometry* geom = atomic->geometry;

            RwResourcesUseResEntry(pipeData->resEntry);
            vbHeader->attr[0].array = SKINRESENTRYGETARRAYBASE(skinAtomicData->platformData.skinResEntry);
            if (geom->flags & rpGEOMETRYNORMALS) {
                vbHeader->attr[1].array = (void*)((unsigned char*)vbHeader->attr[0].array +
                    (((geom->numVertices * vbHeader->attr[0].stride) + 0x1F) & ~0x1F));
            }
            ((RwResEntry**)vbHeader->attr[0].array)[-1] = skinAtomicData->platformData.skinResEntry;
            RwResourcesUseResEntry(skinAtomicData->platformData.skinResEntry);
            ((RxGameCubeVertexBuffer*)(((RwResEntry**)vbHeader->attr[0].array)[-1] + 1))->token = _RwDlTokenCurrent;
            return object;
        }

        vbHeader->attr[0].array = NULL;
        if (geometry->flags & rpGEOMETRYNORMALS) {
            vbHeader->attr[1].array = NULL;
        }
    }

    if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
        RwResEntry* resEntry = pipeData->resEntry;
        void** positions;
        void** normals;

        vbHeader = (RxGameCubeVertexBuffer*)(resEntry + 1);
        if (((RwObject*)resEntry->owner)->type == rpATOMIC) {
            SkinAtomicData* atomicData = RPSKINATOMICGETDATA(resEntry->owner);

            positions = &atomicData->platformData.vertices;
            normals = &atomicData->platformData.normals;
        } else {
            positions = &skin->platformData.vertices;
            normals = &skin->platformData.normals;
        }

        if (*positions == NULL) {
            if (!(geometry->flags & rpGEOMETRYNATIVE)) {
                _rxGCAtomicDefaultReinstanceCallback(object, pipeData);
            }
            *positions = vbHeader->attr[0].array;
            vbHeader->attr[0].array = NULL;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                *normals = vbHeader->attr[1].array;
                vbHeader->attr[1].array = NULL;
            }
        } else if (!(geometry->flags & rpGEOMETRYNATIVE)) {
            void* tmpPositions = vbHeader->attr[0].array;

            vbHeader->attr[0].array = *positions;
            if (geometry->flags & rpGEOMETRYNORMALS) {
                void* tmpNormals = vbHeader->attr[1].array;

                vbHeader->attr[1].array = *normals;
                _rxGCAtomicDefaultReinstanceCallback(object, pipeData);
                vbHeader->attr[1].array = tmpNormals;
            } else {
                _rxGCAtomicDefaultReinstanceCallback(object, pipeData);
            }
            vbHeader->attr[0].array = tmpPositions;
        }

        if (!_rpSkinVertexBuffersUpdate(skin, atomic, vbHeader, pipeData)) {
            return NULL;
        }

        _rpSkinMatrixBlendUpdate(_rpSkinGlobals.matrixCache.aligned, skin, RwFrameGetLTM(RpAtomicGetFrame(atomic)),
                                 RPSKINATOMICGETDATA(atomic)->hierarchy);

        if (geometry->flags & rpGEOMETRYNORMALS) {
            _rpSkinBlendBodyPN(skin, _rpSkinGlobals.matrixCache.aligned, *positions, *normals,
                               vbHeader->attr[0].array, vbHeader->attr[1].array,
                               *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset), geometry->numVertices);
        } else {
            _rpSkinBlendBodyP(skin, _rpSkinGlobals.matrixCache.aligned, *positions, vbHeader->attr[0].array,
                              *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset), geometry->numVertices);
        }
    } else {
        _rpSkinMatrixBlendUpdate(_rpSkinGlobals.matrixCache.aligned, skin, RwFrameGetLTM(RpAtomicGetFrame(atomic)),
                                 RPSKINATOMICGETDATA(atomic)->hierarchy);
    }

    return object;
} // 0x802EBF04

/*
 * ==================== _rpSkinLoadMatrix ====================
 * Converts a RenderWare bone matrix into GX 3x4 column-major form (negating X and Z) and loads
 * it into GX position matrix memory (and normal matrix memory when normals is nonzero).
 */
void _rpSkinLoadMatrix(RwMatrix* matrix, unsigned int index, int normals) {
    float mtx[3][4];

    mtx[0][0] = -matrix->right.x;
    mtx[0][1] = -matrix->up.x;
    mtx[0][2] = -matrix->at.x;
    mtx[0][3] = -matrix->pos.x;

    mtx[1][0] = matrix->right.y;
    mtx[1][1] = matrix->up.y;
    mtx[1][2] = matrix->at.y;
    mtx[1][3] = matrix->pos.y;

    mtx[2][0] = -matrix->right.z;
    mtx[2][1] = -matrix->up.z;
    mtx[2][2] = -matrix->at.z;
    mtx[2][3] = -matrix->pos.z;

    GXLoadPosMtxImm(mtx, index);
    if (normals) {
        GXLoadNrmMtxImm(mtx, index);
    }
} // 0x802EC2E8

/*
 * ==================== _rpSkinLoadMatrixPalette ====================
 * Loads the split-skin bone matrix runs for one mesh into GX matrix memory.
 */
void _rpSkinLoadMatrixPalette(RpSkin* skin, unsigned int mesh, int normals) {
    unsigned char i;
    unsigned char index;
    unsigned char count;
    unsigned int matrixID;

    matrixID = 0;
    index = skin->skinSplitData.meshRLECount[mesh * 2] * 2;
    count = skin->skinSplitData.meshRLECount[mesh * 2 + 1];

    for (i = 0; i < count; i++) {
        unsigned int j;
        unsigned char start = skin->skinSplitData.meshRLE[index + i * 2];
        unsigned char run = skin->skinSplitData.meshRLE[index + i * 2 + 1];

        for (j = 0; j < run; j++) {
            _rpSkinLoadMatrix(&_rpSkinGlobals.matrixCache.aligned[start + j], matrixID, normals);
            matrixID += 3;
        }
    }
} // 0x802EC3B8

/*
 * ==================== _rpSkinRenderCallback ====================
 * Renders a skinned atomic: sets vertex format/transform/lighting state, loads bone matrices
 * for 1-weight skins, binds textures and Z-compare mode per mesh, and calls display lists.
 */
void* _rpSkinRenderCallback(void* object, RxGameCubePipeData* pipeData) {
    RpAtomic* atomic;
    unsigned int numMeshes;
    RxGameCubeVertexBuffer* vbHeader;
    RxGameCubeDisplayList* dList;
    RpSkin* skin;
    RpMesh* mesh;
    RwMatrix* ltm;
    RpGameCubeVtxFmt* fmt;
    void (*matFunc)(RwRGBAReal*, RwRGBA*, float);

    atomic = (RpAtomic*)object;
    vbHeader = (RxGameCubeVertexBuffer*)(pipeData->resEntry + 1);
    dList = (RxGameCubeDisplayList*)(&vbHeader->attr[vbHeader->numAttrArrays - 1] + 1);
    vbHeader->token = _RwDlTokenCurrent;

    skin = RpSkinGeometryGetSkin(atomic->geometry);
    fmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, atomic->geometry, _rpDlGeomVtxFmtOffset);
    ltm = RwFrameGetLTM(RpAtomicGetFrame(atomic));

    if (_Current_RwClimaxVertexProcCB != NULL) {
        RpGameCubeVtxFmt* Fmt = fmt;

        if (Fmt == NULL) {
            Fmt = fn_802DF0DC();
        }
        if (_Current_RwClimaxVertexProcCB(ltm, Fmt, vbHeader, pipeData->flags,
                                          skin->vertexMaps.maxWeights == 1)) {
            fn_802DEE68(fmt, pipeData);
        }
    } else {
        fn_802DEE68(fmt, pipeData);
    }

    if (skin->vertexMaps.maxWeights > 1 || skin->platformData.skinType == rpSKINTYPETOON) {
        _rwDlTransformSetup(ltm, (pipeData->flags & rpGEOMETRYNORMALS) != 0);
    } else {
        GXSetVtxDesc(GX_VA_PNMTXIDX, GX_DIRECT);
    }

    if (fmt == NULL) {
        fn_802DF0DC();
    }

    matFunc = fn_802DE568(pipeData->flags, pipeData->lightMask, pipeData->ambientLight, vbHeader->flags & 1,
                          vbHeader, pipeData);

    numMeshes = pipeData->meshHeader->numMeshes;
    mesh = (RpMesh*)(pipeData->meshHeader + 1);

    if (skin->vertexMaps.maxWeights == 1 && skin->skinSplitData.numMeshes == 0) {
        unsigned int i;

        for (i = 0; i < skin->boneData.numUsedBones; i++) {
            _rpSkinLoadMatrix(&_rpSkinGlobals.matrixCache.aligned[skin->boneData.usedBoneList[i]], i * 3,
                              (pipeData->flags & rpGEOMETRYNORMALS) != 0);
        }
    }

    if (pipeData->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        while (numMeshes--) {
            if (mesh->material->pad & RwClimaxMaterialMask) {
                RwTexture* texture = mesh->material->texture;

                _rwDlTextureSet(texture, 0);
                if (texture != NULL && texture->raster != NULL) {
                    _rwDlRenderStateSetZCompLoc(
                        !(RASTEREXTFROMRASTER(texture->raster->parent)->flags & rwGAMECUBERASTERALPHA));
                } else {
                    _rwDlRenderStateSetZCompLoc(1);
                }

                if (matFunc != NULL) {
                    matFunc(&pipeData->ambientLightColor, &mesh->material->color,
                            mesh->material->surfaceProps.ambient);
                }

                if (skin->vertexMaps.maxWeights == 1 && skin->skinSplitData.numMeshes != 0) {
                    _rpSkinLoadMatrixPalette(skin, pipeData->meshHeader->numMeshes - (numMeshes + 1),
                                             (pipeData->flags & rpGEOMETRYNORMALS) != 0);
                }

                fn_802AEDC8(dList->displayList, dList->size);
            }
            dList++;
            mesh++;
        }
    } else {
        _rwDlRenderStateSetZCompLoc(1);
        while (numMeshes--) {
            if (mesh->material->pad & RwClimaxMaterialMask) {
                if (matFunc != NULL) {
                    matFunc(&pipeData->ambientLightColor, &mesh->material->color,
                            mesh->material->surfaceProps.ambient);
                }

                if (skin->vertexMaps.maxWeights == 1 && skin->skinSplitData.numMeshes != 0) {
                    _rpSkinLoadMatrixPalette(skin, pipeData->meshHeader->numMeshes - (numMeshes + 1),
                                             (pipeData->flags & rpGEOMETRYNORMALS) != 0);
                }

                fn_802AEDC8(dList->displayList, dList->size);
            }
            dList++;
            mesh++;
        }
    }

    vbHeader->token = _RwDlTokenCurrent;
    _clSkinCache_TokenStampResource(vbHeader);
    return object;
} // 0x802EC46C

/*
 * ==================== _rpSkinCreate ====================
 * Sorts each vertex's bone weights/indices in descending weight order and packs quantised u8
 * weights and bone indices for multi-weight skins.
 */
static RpSkin* _rpSkinCreate(RpSkin* skin, unsigned int numVertices) {
    unsigned int i;

    skin->platformData.vertices = NULL;
    skin->platformData.normals = NULL;
    skin->platformData.weights = NULL;
    skin->platformData.indices = NULL;

    for (i = 0; i < numVertices; i++) {
        unsigned int j;

        for (j = 0; j < 3; j++) {
            unsigned int k;

            for (k = 1; k < 4 - j; k++) {
                if ((&skin->vertexMaps.matrixWeights[i].w0 + j)[k] >
                    (&skin->vertexMaps.matrixWeights[i].w0)[j]) {
                    float tmp = (&skin->vertexMaps.matrixWeights[i].w0)[j];
                    unsigned int index1;
                    unsigned int index2;

                    (&skin->vertexMaps.matrixWeights[i].w0)[j] =
                        (&skin->vertexMaps.matrixWeights[i].w0 + j)[k];
                    (&skin->vertexMaps.matrixWeights[i].w0 + j)[k] = tmp;

                    index1 = (skin->vertexMaps.matrixIndices[i] >> (j * 8)) & 0xFF;
                    index2 = (skin->vertexMaps.matrixIndices[i] >> ((j + k) * 8)) & 0xFF;
                    skin->vertexMaps.matrixIndices[i] &= ~(0xFF << (j * 8));
                    skin->vertexMaps.matrixIndices[i] |= index2 << (j * 8);
                    skin->vertexMaps.matrixIndices[i] &= ~(0xFF << ((j + k) * 8));
                    skin->vertexMaps.matrixIndices[i] |= index1 << ((j + k) * 8);
                }
            }
        }
    }

    if (skin->vertexMaps.maxWeights > 1) {
        unsigned int i;
        unsigned char* weight;
        unsigned char* indices;
        unsigned int size = numVertices * (skin->vertexMaps.maxWeights * 2);

        skin->platformData.weights =
            (unsigned char*)RwMalloc(size + 3, rwMEMHINTDUR_EVENT | rwID_SKINPLUGIN);

        weight = skin->platformData.weights;
        for (i = 0; i < numVertices; i++) {
            unsigned char sum = 0;
            unsigned int j;

            for (j = 0; j < skin->vertexMaps.maxWeights; j++) {
                *weight = (unsigned char)(128.0f * ((float*)&skin->vertexMaps.matrixWeights[i])[j]);
                sum += *weight++;
            }

            if (sum < 128) {
                j = 0;
                while (j < skin->vertexMaps.maxWeights) {
                    (*(weight - (skin->vertexMaps.maxWeights - j)))++;
                    sum++;
                    if (sum == 128) {
                        break;
                    }
                    j++;
                }
            }
        }

        skin->platformData.indices =
            (unsigned char*)(((unsigned int)(skin->platformData.weights +
                                             skin->vertexMaps.maxWeights * numVertices) +
                              3) &
                             ~3);
        indices = skin->platformData.indices;
        for (i = 0; i < numVertices; i++) {
            unsigned int j;

            for (j = 0; j < skin->vertexMaps.maxWeights; j++) {
                *indices++ = (unsigned char)(skin->vertexMaps.matrixIndices[i] >> (j * 8));
            }
        }
    }

    return skin;
} // 0x802EC7CC

/*
 * ==================== _rpSkinInitialize ====================
 * Initialises the GameCube skin data for a geometry (marks native geometries locked, or sorts
 * and packs weights/indices via _rpSkinCreate).
 */
RpGeometry* _rpSkinInitialize(RpGeometry* geometry) {
    RpSkin* skin = RPSKINGEOMETRYGETDATA(geometry);

    if (skin != NULL) {
        if (geometry->flags & rpGEOMETRYNATIVE) {
            if (geometry->flags & rpGEOMETRYNORMALS) {
                geometry->lockedSinceLastInst |= rpGEOMETRYLOCKVERTICES | rpGEOMETRYLOCKNORMALS;
            } else {
                geometry->lockedSinceLastInst |= rpGEOMETRYLOCKVERTICES;
            }
            skin->platformData.vertices = NULL;
            skin->platformData.normals = NULL;
        } else {
            if (!_rpSkinCreate(skin, geometry->numVertices)) {
                return NULL;
            }
        }
    }

    return geometry;
} // 0x802ECA74

/*
 * ==================== _rpSkinDeinitialize ====================
 * Releases the packed weight/index buffer and clears platform vertex/normal pointers on a geometry's skin.
 */
RpGeometry* _rpSkinDeinitialize(RpGeometry* geometry) {
    RpSkin* skin = RPSKINGEOMETRYGETDATA(geometry);

    if (skin->platformData.weights != NULL) {
        RwFree(skin->platformData.weights);
        skin->platformData.weights = NULL;
        skin->platformData.indices = NULL;
    }

    skin->platformData.vertices = NULL;
    skin->platformData.normals = NULL;

    return geometry;
} // 0x802ECB14
