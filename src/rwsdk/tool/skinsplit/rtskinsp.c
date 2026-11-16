// Unit name and boundary: Gormiti link map (RtSkinSplit.a rtskinsp.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RtSkinSplit.a rtskinsp.obj), Final and Debug builds.
/*
 * rtskinsp.c: the skin split toolkit's driver. RtSkinSplitAtomicSplitGeometry sorts an atomic's
 * triangles by material into lists, splits every list that needs more bones than the platform
 * holds, and then rewrites the geometry: each triangle takes the material of its final list and the
 * skin gets the remap of each list's bones and its runs of bone indices.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/plugin/skin2/rpskin.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/tool/skinsplit/rtskinsplit.h>

/*
 * ==================== _rtSkinSplitMeshBoneRLEValidate ====================
 * Checks that the runs written for a mesh agree with the bones its vertices are skinned to: the
 * runs set a flag per bone, and every bone a vertex weighs must be flagged both there and in the
 * list's own flags. The checks are RWASSERTs of the Debug build (it reports each as "FALSE" with
 * the Debug build's source line), so a release build keeps only the return of matTriList and the
 * function stays out of line. The Debug build's per-function name probe (line 103) is also gone.
 * Names, locals and statement order from Gormiti DWARF and its Debug build's code.
 */
static rtSkinSplitMatTriList* _rtSkinSplitMeshBoneRLEValidate(rtSkinSplitGeomStat* geomStat,
                                                              rtSkinSplitMatTriList* matTriList,
                                                              RpMesh* mesh, unsigned int meshId) {
    unsigned int i, j, idx;
    unsigned char* rle;
    unsigned char* skinMeshRLECount;
    unsigned char* skinMeshRLE;
    unsigned int skinBoneIdx;
    RwMatrixWeights* skinBoneWeight;
    unsigned int numRLE, numMtx;
    rtSkinSplitBoneFlag boneFlag;

    skinMeshRLECount = geomStat->skinMeshBoneRLECount;
    skinMeshRLE = geomStat->skinMeshBoneRLE;

    boneFlag.flag[0] = 0; boneFlag.flag[1] = 0; boneFlag.flag[2] = 0; boneFlag.flag[3] = 0;
    boneFlag.flag[4] = 0; boneFlag.flag[5] = 0; boneFlag.flag[6] = 0; boneFlag.flag[7] = 0;

    numRLE = skinMeshRLECount[meshId * 2 + 1];
    rle = skinMeshRLE + skinMeshRLECount[meshId * 2] * 2;
    for (i = 0; i < numRLE; i++) {
        idx = rle[i * 2];
        numMtx = rle[i * 2 + 1];
        for (j = 0; j < numMtx; j++) {
            boneFlag.flag[idx >> 5] |= 1 << (idx & 31);
            idx++;
        }
    }

    for (i = 0; i < mesh->numIndices; i++) {
        skinBoneIdx = geomStat->skinBoneIndex[mesh->indices[i]];
        skinBoneWeight = &geomStat->skinBoneWeight[mesh->indices[i]];
        if (skinBoneWeight->w0 > 0.0f) {
            idx = skinBoneIdx & 0xFF;
            if (!(matTriList->boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
            if (!(boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
        }
        if (skinBoneWeight->w1 > 0.0f) {
            skinBoneIdx >>= 8;
            idx = skinBoneIdx & 0xFF;
            if (!(matTriList->boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
            if (!(boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
        }
        if (skinBoneWeight->w2 > 0.0f) {
            skinBoneIdx >>= 8;
            idx = skinBoneIdx & 0xFF;
            if (!(matTriList->boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
            if (!(boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
        }
        if (skinBoneWeight->w3 > 0.0f) {
            skinBoneIdx >>= 8;
            idx = skinBoneIdx & 0xFF;
            if (!(matTriList->boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
            if (!(boneFlag.flag[idx >> 5] &
                  (1 << (idx & 31)))) {
                RWASSERT(FALSE);
            }
        }
    }

    return matTriList;
} // 0x802F0160

/*
 * ==================== _rtSkinSplitMeshSetBoneRLE ====================
 * RpGeometryForAllMeshes callback: finds the list whose material the mesh has and writes its runs
 * into the skin's split data. Each mesh has two bytes of counts: where its runs start, in pairs, and
 * how many it has. The mesh gets the geometry's original material back.
 */
static RpMesh* _rtSkinSplitMeshSetBoneRLE(RpMesh* mesh, RpMeshHeader* meshHeader, void* data) {
    unsigned int i;
    unsigned int meshId;
    RpMesh* meshHead;
    unsigned char* rle;
    unsigned char* skinMeshRLECount;
    unsigned char* skinMeshRLE;
    rtSkinSplitGeomStat* geomStat;
    rtSkinSplitMatTriList* matTriList;

    geomStat = (rtSkinSplitGeomStat*)data;

    meshHead = (RpMesh*)(meshHeader + 1) + meshHeader->firstMeshOffset;
    meshId = mesh - meshHead;

    matTriList = geomStat->matTriListDone;
    skinMeshRLECount = geomStat->skinMeshBoneRLECount;
    skinMeshRLE = geomStat->skinMeshBoneRLE;

    while (matTriList->mat != mesh->material) {
        matTriList = matTriList->next;
    }

    skinMeshRLECount[meshId * 2 + 1] = matTriList->numRLE;
    if (meshId == 0) {
        skinMeshRLECount[0] = 0;
    } else {
        skinMeshRLECount[meshId * 2] = skinMeshRLECount[meshId * 2 - 1] + skinMeshRLECount[meshId * 2 - 2];
    }

    rle = skinMeshRLE + skinMeshRLECount[meshId * 2] * 2;
    for (i = 0; i < matTriList->numRLE * 2; i++) {
        rle[i] = matTriList->rleMap[i];
    }

    _rtSkinSplitMeshBoneRLEValidate(geomStat, matTriList, mesh, meshId);

    mesh->material = _rpMaterialListGetMaterial(&geomStat->geom->matList, matTriList->origMatIdx);

    return mesh;
} // 0x802F0168

/*
 * ==================== _rtSkinSplitGeomResetTriMat ====================
 * Sets each triangle of the lists back to the material index it had before the split.
 */
static RpGeometry* _rtSkinSplitGeomResetTriMat(rtSkinSplitGeomStat* geomStat, RpGeometry* geom) {
    unsigned int i;
    unsigned int j;
    RpTriangle* tri;
    rtSkinSplitMatTriList* matTriList;

    tri = geom->triangles;
    matTriList = geomStat->matTriListDone;
    while (matTriList) {
        for (j = 0; j < matTriList->numTris; j++) {
            i = matTriList->triList[j];
            tri[i].matIndex = matTriList->origMatIdx;
        }
        matTriList = matTriList->next;
    }

    return geom;
} // no copy in retail (inlined)

/*
 * ==================== _rtSkinSplitCreateGeometryMesh ====================
 * Gives the geometry's triangles the materials of the lists they ended in, makes the skin's split
 * data for the lists' runs, and rebuilds the meshes from it (which gives each mesh the material of
 * its list for the RLE callback), then sets the triangles' material indices back to the geometry's
 * originals.
 */
static RpAtomic* _rtSkinSplitCreateGeometryMesh(rtSkinSplitGeomStat* geomStat, RpAtomic* atomic) {
    unsigned int i;
    unsigned int j;
    RpAtomic* result;
    RpGeometry* geom;
    RpTriangle* tri;
    rtSkinSplitMatTriList* matTriList;

    result = atomic;

    geom = geomStat->geom;

    RpGeometryLock(geom, rpGEOMETRYLOCKPOLYGONS);
    tri = geom->triangles;

    matTriList = geomStat->matTriListDone;
    while (matTriList) {
        for (j = 0; j < matTriList->numTris; j++) {
            RpGeometryTriangleSetMaterial(geom, &tri[matTriList->triList[j]], matTriList->mat);
        }
        matTriList = matTriList->next;
    }

    RpGeometryUnlock(geom);

    matTriList = geomStat->matTriListDone;
    i = 0;
    while (matTriList) {
        i += matTriList->numRLE;
        matTriList = matTriList->next;
    }

    _rpSkinSplitDataDestroy(geomStat->skin);
    _rpSkinSplitDataCreate(geomStat->skin, geomStat->boneLimit, geomStat->numBones, geomStat->numMatTriLists, i);

    geomStat->skinBoneRemapIndices = _rpSkinGetMeshBoneRemapIndices(geomStat->skin);
    geomStat->skinMeshBoneRLECount = _rpSkinGetMeshBoneRLECount(geomStat->skin);
    geomStat->skinMeshBoneRLE = _rpSkinGetMeshBoneRLE(geomStat->skin);

    for (i = 0; i < geomStat->numBones; i++) {
        geomStat->skinBoneRemapIndices[i] = geomStat->boneRemapIndex[i];
    }

    RpGeometryForAllMeshes(geom, _rtSkinSplitMeshSetBoneRLE, geomStat);

    _rpSkinDeinitialize(geom);
    _rpSkinInitialize(geom);

    _rtSkinSplitGeomResetTriMat(geomStat, geom);

    return result;
} // 0x802F0290

/*
 * ==================== RtSkinSplitAtomicSplitGeometry ====================
 * Splits the atomic's skinned geometry so that no mesh uses more than boneLimit bones. It returns
 * the atomic when the split was made, or when the skin already fits, and NULL when the atomic has
 * no skin or hierarchy, its geometry is native, or the limit is too low.
 */
RpAtomic* RtSkinSplitAtomicSplitGeometry(RpAtomic* atomic, unsigned int boneLimit) {
    rtSkinSplitGeomStat geomStat;
    rtSkinSplitMatTriList* matTriList;
    rtSkinSplitMatTriList* matTriListNew;
    rtSkinSplitMatTriList* matTriListNext;
    RpAtomic* result;
    RpMaterial* mat;
    unsigned int i;
    unsigned int size;

    result = atomic;

    memset(&geomStat, 0, sizeof(rtSkinSplitGeomStat));

    geomStat.atomic = atomic;
    geomStat.boneLimit = boneLimit;

    geomStat.hierarchy = RpSkinAtomicGetHAnimHierarchy(atomic);
    geomStat.geom = RpAtomicGetGeometry(geomStat.atomic);
    geomStat.skin = RpSkinGeometryGetSkin(geomStat.geom);
    geomStat.numBones = RpSkinGetNumBones(geomStat.skin);

    if (geomStat.skin == NULL) {
        return NULL;
    }
    if (geomStat.hierarchy == NULL) {
        return NULL;
    }
    if (geomStat.geom->flags & rpGEOMETRYNATIVE) {
        return NULL;
    }
    if (geomStat.numBones <= boneLimit) {
        return atomic;
    }

    geomStat.skinBoneIndex = RpSkinGetVertexBoneIndices(geomStat.skin);
    geomStat.skinBoneWeight = RpSkinGetVertexBoneWeights(geomStat.skin);

    geomStat.numTris = geomStat.geom->numTriangles;
    geomStat.numMats = geomStat.geom->matList.numMaterials;
    geomStat.numVerts = geomStat.geom->numVertices;
    geomStat.matList = &geomStat.geom->matList;

    size = geomStat.numBones * sizeof(rtSkinSplitBoneFlag) +
           ((geomStat.numMats + geomStat.numBones) * 2 + geomStat.numMats * geomStat.numBones) *
               sizeof(unsigned int);

    geomStat.meshBoneCount = (unsigned int*)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_SKINSPLITTOOKIT);
    if (geomStat.meshBoneCount == NULL) {
        return NULL;
    }

    memset(geomStat.meshBoneCount, 0, size);

    geomStat.boneMeshCount = geomStat.meshBoneCount + geomStat.numMats;
    geomStat.meshTriCount = geomStat.boneMeshCount + geomStat.numBones;
    geomStat.boneVertCount = geomStat.meshTriCount + geomStat.numMats;
    geomStat.boneBoneCount = geomStat.boneVertCount + geomStat.numMats * geomStat.numBones;
    geomStat.boneBoneFlag = (rtSkinSplitBoneFlag*)(geomStat.boneBoneCount + geomStat.numBones);

    _rtSkinSplitGeomGetStat(&geomStat);

    if (boneLimit < geomStat.numWeights) {
        RwFree(geomStat.meshBoneCount);
        return NULL;
    }

    if (boneLimit < geomStat.maxTriBones) {
        RwFree(geomStat.meshBoneCount);
        return NULL;
    }

    for (i = 0; i < geomStat.numMats; i++) {
        matTriList = _rtSkinSplitMatTriListCreate(geomStat.meshTriCount[i]);
        geomStat.numMatTriLists++;
        if (matTriList) {
            mat = _rpMaterialListGetMaterial(geomStat.matList, i);
            _rtSkinSplitMatTriListAddTriByMat(&geomStat, matTriList, mat, i);
            _rtSkinSplitMatTriListGetBoneFlag(&geomStat, matTriList);

            if (matTriList->numBones > boneLimit) {
                geomStat.matTriListStack = _rtSkinSplitMatTriListPush(geomStat.matTriListStack, matTriList);
            } else {
                geomStat.matTriListDone = _rtSkinSplitMatTriListPush(geomStat.matTriListDone, matTriList);
            }
        } else {
            result = NULL;
        }
    }

    if (result) {
        geomStat.matTriListStack = _rtSkinSplitMatTriListPop(geomStat.matTriListStack, &matTriList);
        while (matTriList) {
            if (matTriList->numBones > boneLimit) {
                matTriListNew = _rtSkinSplitMatTriListSplit(&geomStat, matTriList, boneLimit);
                if (matTriListNew) {
                    geomStat.matTriListStack = _rtSkinSplitMatTriListPush(geomStat.matTriListStack, matTriList);
                    geomStat.matTriListStack = _rtSkinSplitMatTriListPush(geomStat.matTriListStack, matTriListNew);
                } else {
                    result = NULL;
                }
            } else {
                geomStat.matTriListDone = _rtSkinSplitMatTriListPush(geomStat.matTriListDone, matTriList);
            }
            geomStat.matTriListStack = _rtSkinSplitMatTriListPop(geomStat.matTriListStack, &matTriList);
        }
    }

    if (result) {
        if (_rtSkinSplitMatTriListGetBoneLockFlag(&geomStat, geomStat.matTriListDone) != &geomStat) {
            result = NULL;
        }
    }

    if (result) {
        if (_rtSkinSplitMatTriListRemapBoneIndex(&geomStat, geomStat.matTriListDone) != &geomStat) {
            result = NULL;
        }
    }

    if (result) {
        if (_rtSkinSplitCreateGeometryMesh(&geomStat, atomic) != atomic) {
            result = NULL;
        }
    }

    RwFree(geomStat.meshBoneCount);
    RwFree(geomStat.boneRemapIndex);

    matTriList = geomStat.matTriListDone;
    while (matTriList) {
        matTriListNext = matTriList->next;
        _rtSkinSplitMatTriListDestroy(matTriList);
        matTriList = matTriListNext;
    }

    return result;
} // 0x802F0434
