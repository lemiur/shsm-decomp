// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function and data names, unit name)
// Unit name and boundary: Gormiti link map (RPworld.a baworobj)
// Locals and names of WorldSyncCamera and SectorsInFrustumAddSpace: Gormiti DWARF
/*
 * baworobj.c: the world's objects. The world extends cameras, atomics, clumps and lights with the
 * world they are in, and hooks their frame syncs: a moved atomic or light is re-tied to the world
 * sectors its bounding box touches, and a moved camera re-collects the world sectors in its view
 * frustum. The ties come from two free lists the world object module owns. Also the mesh and
 * native data stream callbacks of geometries and world sectors, and RpWorldAdd and Remove of each
 * kind of object.
 */
#include <rwsdk/world/rpplugin.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/babinwor.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/world/baworobj.h>

/* Entries a camera's frustum sector list grows by when it is full. Name invented. */
#define rpWORLDOBJFRUSTUMSECTORSGROW 50

/* The free lists of atomic ties and light ties; RwFreeListCreateAndPreallocateSpace builds them in place here. */
static RwFreeList _rpTieFreeList;
static RwFreeList _rpLightTieFreeList;

/* Ties per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rpTieFreeListBlockSize = 256;
static RwInt32 _rpTieFreeListPreallocBlocks = 1;
static RwInt32 _rpLightTieFreeListBlockSize = 32;
static RwInt32 _rpLightTieFreeListPreallocBlocks = 1;

/* Offsets of the world's extensions in cameras, atomics, clumps and lights. */
static RwInt32 cameraExtOffset = 0;
static RwInt32 atomicExtOffset = 0;
static RwInt32 clumpExtOffset = 0;
static RwInt32 lightExtOffset = 0;

/* The world object module's registration (offset of its globals in the engine instance). */
static RwModuleInfo worldObjModule;

/* The world object module's globals. Type, macro and field names invented. */
typedef struct rpWorldObjGlobals {
    RwFreeList* ties;       /* 0x0: RpTie */
    RwFreeList* lightTies;  /* 0x4: RpLightTie */
    RwInt32 field_0x8;      /* 0x8: zeroed on open, copied into every clump's extension */
} rpWorldObjGlobals;

#define RWWORLDOBJGLOBAL(var) \
    (RWPLUGINOFFSET(rpWorldObjGlobals, RwEngineInstance, worldObjModule.globalsOffset)->var)

/* The world's extension of a camera, an atomic, a clump, a light. Names invented. */
#define WORLDCAMERAEXT(camera) RWPLUGINOFFSET(rpWorldObjCameraExt, camera, cameraExtOffset)
#define WORLDATOMICEXT(atomic) RWPLUGINOFFSET(rpWorldObjAtomicExt, atomic, atomicExtOffset)
#define WORLDCLUMPEXT(clump) RWPLUGINOFFSET(rpWorldObjClumpExt, clump, clumpExtOffset)
#define WORLDLIGHTEXT(light) RWPLUGINOFFSET(rpWorldObjLightExt, light, lightExtOffset)

/*
 * The coordinate of a vector on the axis a plane sector cuts: the sector's type is that axis's
 * byte offset in an RwV3d (the BSP trees white paper). Name invented.
 */
#define RwV3dGetSectorCoord(_v, _type) (*(const RwReal*)((const RwUInt8*)&(_v) + (_type)))

/* Sets the light tie free list's block size and the number of blocks allocated when the module opens. */
void RpLightTieSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    _rpLightTieFreeListBlockSize = blockSize;
    _rpLightTieFreeListPreallocBlocks = numBlocksToPrealloc;
} // 0x802D9578

/* Opens the world object module: creates the tie and light tie free lists. */
static void* WorldObjectOpen(void* instance, RwInt32 offset, RwInt32 size) {
    worldObjModule.globalsOffset = offset;

    RWWORLDOBJGLOBAL(ties) = RwFreeListCreateAndPreallocateSpace(
        sizeof(RpTie), _rpTieFreeListBlockSize, sizeof(RwUInt32), _rpTieFreeListPreallocBlocks,
        &_rpTieFreeList, rwMEMHINTDUR_GLOBAL | rwID_WORLDMODULE);
    if (RWWORLDOBJGLOBAL(ties) == NULL) {
        return NULL;
    }

    RWWORLDOBJGLOBAL(lightTies) = RwFreeListCreateAndPreallocateSpace(
        sizeof(RpLightTie), _rpLightTieFreeListBlockSize, sizeof(RwUInt32), _rpLightTieFreeListPreallocBlocks,
        &_rpLightTieFreeList, rwMEMHINTDUR_GLOBAL | rwID_WORLDMODULE);
    if (RWWORLDOBJGLOBAL(lightTies) == NULL) {
        RwFreeListDestroy(RWWORLDOBJGLOBAL(ties));
        RWWORLDOBJGLOBAL(ties) = NULL;
        return NULL;
    }

    RWSRCGLOBAL(renderFrame) = 1;
    RWWORLDOBJGLOBAL(field_0x8) = 0;

    worldObjModule.numInstances++;

    return instance;
} // 0x802D9584

/* Closes the world object module: destroys the light tie and tie free lists. */
static void* WorldObjectClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWWORLDOBJGLOBAL(lightTies) != NULL) {
        RwFreeListDestroy(RWWORLDOBJGLOBAL(lightTies));
        RWWORLDOBJGLOBAL(lightTies) = NULL;
    }

    if (RWWORLDOBJGLOBAL(ties) != NULL) {
        RwFreeListDestroy(RWWORLDOBJGLOBAL(ties));
        RWWORLDOBJGLOBAL(ties) = NULL;
    }

    worldObjModule.numInstances--;

    return instance;
} // 0x802D969C

/* A camera's begin update: makes the camera's world current and starts a new render frame. */
static RwCamera* WorldCameraBeginUpdate(RwCamera* camera) {
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    RWSRCGLOBAL(curWorld) = cameraExt->world;
    RWSRCGLOBAL(renderFrame)++;

    return cameraExt->oldBeginUpdate(camera);
} // 0x802D9730

/* A camera's end update: no world is current any more. */
static RwCamera* WorldCameraEndUpdate(RwCamera* camera) {
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    RWSRCGLOBAL(curWorld) = NULL;

    return cameraExt->oldEndUpdate(camera);
} // 0x802D9760

static RpWorld* WorldSyncCamera(RpWorld* world, RwCamera* camera);

/*
 * A camera's frame sync: after the camera's own sync, re-collects the world sectors in its view
 * frustum and tells the device whether the world renders front to back.
 */
static RwObjectHasFrame* WorldCameraSync(RwObjectHasFrame* object) {
    RwCamera* camera = (RwCamera*)object;
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    if (cameraExt->oldSync(object)) {
        RpWorld* world = cameraExt->world;

        if (world) {
            RwStandardFunc hintRenderF2B = RWSRCGLOBAL(stdFunc)[rwSTANDARDHINTRENDERF2B];

            WorldSyncCamera(world, camera);
            hintRenderF2B(NULL, NULL, world->renderOrder == rpWORLDRENDERFRONT2BACK);
        }

        return object;
    }

    return NULL;
} // 0x802D9780

/* Initialises a camera's world extension: in no world, and hooks the camera's update and sync functions. */
static void* WorldInitCameraExt(void* object, RwInt32 offset, RwInt32 size) {
    RwCamera* camera = (RwCamera*)object;
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    cameraExt->frustumSectors = NULL;
    cameraExt->space = 0;
    cameraExt->position = 0;

    cameraExt->oldBeginUpdate = camera->beginUpdate;
    cameraExt->oldEndUpdate = camera->endUpdate;
    cameraExt->oldSync = camera->object.sync;

    camera->object.sync = WorldCameraSync;
    camera->beginUpdate = WorldCameraBeginUpdate;
    camera->endUpdate = WorldCameraEndUpdate;

    cameraExt->world = NULL;

    return object;
} // 0x802D9824

/* Copies a camera's world extension: the copy starts with no frustum sectors, in the same world. */
static void* WorldCopyCameraExt(void* dstObject, const void* srcObject, RwInt32 offset, RwInt32 size) {
    rpWorldObjCameraExt* dstCameraExt = WORLDCAMERAEXT(dstObject);
    const rpWorldObjCameraExt* srcCameraExt = WORLDCAMERAEXT(srcObject);
    RpWorld* world;

    dstCameraExt->frustumSectors = NULL;
    dstCameraExt->space = 0;
    dstCameraExt->position = 0;

    world = srcCameraExt->world;
    if (world) {
        RpWorldAddCamera(world, (RwCamera*)dstObject);
    }

    return dstObject;
} // 0x802D9880

/* De-initialises a camera's world extension: frees its frustum sector list and unhooks the camera. */
static void* WorldDeInitCameraExt(void* object, RwInt32 offset, RwInt32 size) {
    RwCamera* camera = (RwCamera*)object;
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    if (cameraExt->frustumSectors) {
        RwFree(cameraExt->frustumSectors);
    }
    cameraExt->frustumSectors = NULL;
    cameraExt->space = 0;
    cameraExt->position = 0;

    camera->beginUpdate = cameraExt->oldBeginUpdate;
    camera->endUpdate = cameraExt->oldEndUpdate;
    camera->object.sync = cameraExt->oldSync;

    return object;
} // 0x802D9904

/* Unties a light from a world sector and frees the tie. */
RwBool _rpLightTieDestroy(RpLightTie* tie) {
    rwLinkListRemoveLLLink(&tie->WorldSectorInLight);
    rwLinkListRemoveLLLink(&tie->lightInWorldSector);

    RwFreeListFree(RWWORLDOBJGLOBAL(lightTies), tie);

    return TRUE;
} // 0x802D9984

/* Unties an atomic from a world sector and frees the tie; a tie missing either end is left alone. */
RwBool _rpTieDestroy(RpTie* tie) {
    if (tie->apAtom && tie->worldSector) {
        rwLinkListRemoveLLLink(&tie->lWorldSectorInAtomic);
        rwLinkListRemoveLLLink(&tie->lAtomicInWorldSector);

        RwFreeListFree(RWWORLDOBJGLOBAL(ties), tie);
    }

    return TRUE;
} // 0x802D99F4

/* Unties an atomic from every world sector it lies in. Name invented. */
static void WorldAtomicDestroyTies(RpAtomic* atomic) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&atomic->llWorldSectorsInAtomic);
    RwLLLink* end = rwLinkListGetTerminator(&atomic->llWorldSectorsInAtomic);

    while (cur != end) {
        RpTie* tie = rwLLLinkGetData(cur, RpTie, lWorldSectorInAtomic);

        cur = rwLLLinkGetNext(cur);
        _rpTieDestroy(tie);
    }
}

/* Unties a light from every world sector it lights. Name invented. */
static void WorldLightDestroyTies(RpLight* light) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&light->WorldSectorsInLight);
    RwLLLink* end = rwLinkListGetTerminator(&light->WorldSectorsInLight);

    while (cur != end) {
        RpLightTie* tie = rwLLLinkGetData(cur, RpLightTie, WorldSectorInLight);

        cur = rwLLLinkGetNext(cur);
        _rpLightTieDestroy(tie);
    }
}

/* Ties an atomic to a world sector. Name invented. */
static void WorldAtomicTieSector(RpAtomic* atomic, RpWorldSector* worldSector) {
    RpTie* tie = (RpTie*)RwFreeListAlloc(RWWORLDOBJGLOBAL(ties), rwMEMHINTDUR_EVENT | rwID_WORLDMODULE);

    tie->worldSector = worldSector;
    tie->apAtom = atomic;
    rwLinkListAddLLLink(&worldSector->collAtomicsInWorldSector, &tie->lAtomicInWorldSector);
    rwLinkListAddLLLink(&atomic->llWorldSectorsInAtomic, &tie->lWorldSectorInAtomic);
}

/*
 * Ties an atomic to every world sector its world bounding sphere's box touches, walking the
 * world's BSP tree with a stack of the subtrees still to visit.
 */
static void WorldAttachAtomicSphere(RpWorld* world, RpAtomic* atomic) {
    RpSector* sector;
    RpSector* sectorStack[rpWORLDMAXBSPDEPTH];
    RwInt32 stack = 0;
    const RwSphere* sphere = RpAtomicGetWorldBoundingSphere(atomic);
    RwBBox bbox;

    bbox.inf = sphere->center;
    bbox.sup = sphere->center;
    bbox.inf.x -= sphere->radius;
    bbox.inf.y -= sphere->radius;
    bbox.inf.z -= sphere->radius;
    bbox.sup.x += sphere->radius;
    bbox.sup.y += sphere->radius;
    bbox.sup.z += sphere->radius;

    sector = world->rootSector;
    do {
        if (sector->type < 0) {
            WorldAtomicTieSector(atomic, (RpWorldSector*)sector);

            sector = sectorStack[stack--];
        } else {
            RpPlaneSector* planeSector = (RpPlaneSector*)sector;

            if (RwV3dGetSectorCoord(bbox.inf, sector->type) < planeSector->leftValue) {
                sector = planeSector->leftSubTree;
                if (planeSector->rightValue < RwV3dGetSectorCoord(bbox.sup, planeSector->type)) {
                    stack++;
                    sectorStack[stack] = planeSector->rightSubTree;
                }
            } else if (planeSector->rightValue < RwV3dGetSectorCoord(bbox.sup, sector->type)) {
                sector = planeSector->rightSubTree;
            } else {
                sector = sectorStack[stack--];
            }
        }
    } while (stack >= 0);
} // 0x802D9A78

/* An atomic's frame sync: after the atomic's own sync, re-ties it to the world sectors it now lies in. */
static RwObjectHasFrame* WorldAtomicSync(RwObjectHasFrame* object) {
    RpAtomic* atomic = (RpAtomic*)object;
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    if (atomicExt->oldSync(object)) {
        RpWorld* world = atomicExt->world;

        if (world) {
            WorldAtomicDestroyTies(atomic);
            WorldAttachAtomicSphere(world, atomic);
        }

        return object;
    }

    return NULL;
} // 0x802D9C20

/* Initialises an atomic's world extension: in no world, and hooks the atomic's sync. */
static void* WorldInitAtomicExt(void* object, RwInt32 offset, RwInt32 size) {
    RpAtomic* atomic = (RpAtomic*)object;
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    atomicExt->world = NULL;
    atomic->renderFrame = RWSRCGLOBAL(renderFrame) - 1;
    atomicExt->oldSync = atomic->object.sync;
    atomic->object.sync = WorldAtomicSync;

    return object;
} // 0x802D9D1C

/* Copies an atomic's world extension: the copy is in no world. */
static void* WorldCopyAtomicExt(void* dstObject, const void* srcObject, RwInt32 offset, RwInt32 size) {
    return dstObject;
} // 0x802D9D54

/* De-initialises an atomic's world extension: unties it from its world sectors and unhooks its sync. */
static void* WorldDeInitAtomicExt(void* object, RwInt32 offset, RwInt32 size) {
    RpAtomic* atomic = (RpAtomic*)object;
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    WorldAtomicDestroyTies(atomic);
    atomic->object.sync = atomicExt->oldSync;

    return object;
} // 0x802D9D58

/* Initialises a clump's world extension: in no world. */
static void* WorldInitClumpExt(void* object, RwInt32 offset, RwInt32 size) {
    rpWorldObjClumpExt* clumpExt = WORLDCLUMPEXT(object);

    clumpExt->world = NULL;
    clumpExt->field_0x4 = RWWORLDOBJGLOBAL(field_0x8);

    return object;
} // 0x802D9E28

/* Copies a clump's world extension: the copy is added to the same world. */
static void* WorldCopyClumpExt(void* dstObject, const void* srcObject, RwInt32 offset, RwInt32 size) {
    RpWorld* world = WORLDCLUMPEXT(srcObject)->world;

    if (world) {
        RpWorldAddClump(world, (RpClump*)dstObject);
    }

    return dstObject;
} // 0x802D9E50

/* De-initialises a clump's world extension: nothing to do. */
static void* WorldDeInitClumpExt(void* object, RwInt32 offset, RwInt32 size) {
    return object;
} // 0x802D9F44

/* Ties a light to a world sector. Name invented. */
static void WorldLightTieSector(RpLight* light, RpWorldSector* worldSector) {
    RpLightTie* tie = (RpLightTie*)RwFreeListAlloc(RWWORLDOBJGLOBAL(lightTies), rwMEMHINTDUR_EVENT | rwID_WORLDMODULE);

    tie->sect = worldSector;
    tie->light = light;
    rwLinkListAddLLLink(&worldSector->lightsInWorldSector, &tie->lightInWorldSector);
    rwLinkListAddLLLink(&light->WorldSectorsInLight, &tie->WorldSectorInLight);
}

/*
 * A light's frame sync: after the light's own sync, re-ties a positioned light to the world
 * sectors its radius reaches.
 */
static RwObjectHasFrame* WorldLightSync(RwObjectHasFrame* object) {
    RpLight* light = (RpLight*)object;
    rpWorldObjLightExt* lightExt = WORLDLIGHTEXT(light);

    if (lightExt->oldSync(object)) {
        RpWorld* world;
        RwFrame* frame;

        if (RpLightGetType(light) < rpLIGHTPOSITIONINGSTART) {
            return object;
        }

        world = lightExt->world;
        frame = RpLightGetFrame(light);
        if (world && frame) {
            RpSector* sectorStack[rpWORLDMAXBSPDEPTH];
            RwInt32 stack = 0;
            RwReal radius = light->radius;
            const RwV3d* pos;
            RwBBox bbox;
            RpSector* sector;

            WorldLightDestroyTies(light);

            pos = RwMatrixGetPos(RwFrameGetLTM(frame));
            bbox.inf = *pos;
            bbox.sup = *pos;
            bbox.sup.x += radius;
            bbox.sup.y += radius;
            bbox.sup.z += radius;
            bbox.inf.x -= radius;
            bbox.inf.y -= radius;
            bbox.inf.z -= radius;

            sector = world->rootSector;
            do {
                if (sector->type < 0) {
                    WorldLightTieSector(light, (RpWorldSector*)sector);

                    sector = sectorStack[stack--];
                } else {
                    RpPlaneSector* planeSector = (RpPlaneSector*)sector;

                    if (RwV3dGetSectorCoord(bbox.inf, sector->type) < planeSector->leftValue) {
                        sector = planeSector->leftSubTree;
                        if (planeSector->rightValue < RwV3dGetSectorCoord(bbox.sup, planeSector->type)) {
                            stack++;
                            sectorStack[stack] = planeSector->rightSubTree;
                        }
                    } else if (planeSector->rightValue < RwV3dGetSectorCoord(bbox.sup, sector->type)) {
                        sector = planeSector->rightSubTree;
                    } else {
                        sector = sectorStack[stack--];
                    }
                }
            } while (stack >= 0);
        }
    } else {
        RWERROR((E_RW_NULLP));
    }

    return object;
} // 0x802D9F48

/* Initialises a light's world extension: in no world, and hooks the light's sync. */
static void* WorldInitLightExt(void* object, RwInt32 offset, RwInt32 size) {
    RpLight* light = (RpLight*)object;
    rpWorldObjLightExt* lightExt = WORLDLIGHTEXT(light);

    lightExt->world = NULL;
    lightExt->oldSync = light->object.sync;
    light->object.sync = WorldLightSync;

    return object;
} // 0x802DA1DC

/* Copies a light's world extension: the copy is added to the same world. */
static void* WorldCopyLightExt(void* dstObject, const void* srcObject, RwInt32 offset, RwInt32 size) {
    RpWorld* world = WORLDLIGHTEXT(srcObject)->world;

    if (world) {
        RpWorldAddLight(world, (RpLight*)dstObject);
    }

    return dstObject;
} // 0x802DA200

/* De-initialises a light's world extension: unties it from the world sectors it lights. */
static void* WorldDeInitLightExt(void* object, RwInt32 offset, RwInt32 size) {
    RpLight* light = (RpLight*)object;

    WorldLightDestroyTies(light);

    return object;
} // 0x802DA2A8

/* Grows a camera's frustum sector list by nNum entries; FALSE when memory runs out. Name: Gormiti DWARF. */
static RwBool SectorsInFrustumAddSpace(rpWorldObjCameraExt* cameraExt, RwInt32 nNum) {
    RwInt32 memSize = (cameraExt->space + nNum) * sizeof(RpWorldSector*);
    RpWorldSector** newFrustumSectors;

    if (cameraExt->frustumSectors) {
        newFrustumSectors = (RpWorldSector**)RwRealloc(cameraExt->frustumSectors, memSize,
                                                     rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_WORLDMODULE);
    } else {
        newFrustumSectors = (RpWorldSector**)RwMalloc(memSize,
                                                    rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT | rwID_WORLDMODULE);
    }

    if (newFrustumSectors != NULL) {
        cameraExt->frustumSectors = newFrustumSectors;
        cameraExt->space += nNum;
        return TRUE;
    }

    RWERROR((E_RW_NOMEM, memSize));
    return FALSE;
}

/*
 * Collects the world sectors in the camera's view frustum into the camera's frustum sector list,
 * walking the world's BSP tree in the world's render order: where the frustum's box straddles a
 * plane, the side the camera is on is visited first (last when rendering back to front).
 * Locals and names: Gormiti DWARF (the frustum test is written out in the loop there; spSect is
 * declared last here, which is what gives retail's register numbering).
 */
static RpWorld* WorldSyncCamera(RpWorld* world, RwCamera* camera) {
    RpSector* spaStack[rpWORLDMAXBSPDEPTH];
    RwV3d vViewPoint;
    RwV3d inf, sup;
    const RwFrustumPlane* plpPlanes;
    rpWorldObjCameraExt* cameraExt;
    RwBool goBackToFront;
    RwInt32 nStack;
    RwInt32 position;
    /* RULE-EXCEPTION: R30, declaration order: the DWARF lists spSect earlier; declared last it gets retail's
       register numbering */
    RpSector* spSect;

    cameraExt = WORLDCAMERAEXT(camera);
    nStack = 0;
    position = 0;
    vViewPoint = *RwMatrixGetPos(RwFrameGetLTM(RwCameraGetFrame(camera)));
    goBackToFront = (world->renderOrder == rpWORLDRENDERBACK2FRONT);
    inf = camera->frustumBoundBox.inf;
    sup = camera->frustumBoundBox.sup;
    plpPlanes = camera->frustumPlanes;
    spSect = cameraExt->world->rootSector;

    do {
        RwBool isAtomic = (spSect->type < 0);

        if (isAtomic) {
            RwBool outside;
            RwInt32 i;
            RpWorldSector* worldSector = (RpWorldSector*)spSect;
            RwV3d* base = &worldSector->boundingBox.sup;

            for (i = 0; i < 6; i++) {
                RwV3d vCorner;
                RwSplitBits sbSide;

                vCorner.x = base[plpPlanes[i].closestX].x;
                vCorner.y = base[plpPlanes[i].closestY].y;
                vCorner.z = base[plpPlanes[i].closestZ].z;
                sbSide.nReal = RwV3dDotProductMacro(&vCorner, &plpPlanes[i].plane.normal) - plpPlanes[i].plane.distance;

                outside = (sbSide.nInt > 0);
                if (outside) {
                    break;
                }
            }

            if (!outside) {
                if (position >= cameraExt->space && !SectorsInFrustumAddSpace(cameraExt, rpWORLDOBJFRUSTUMSECTORSGROW)) {
                    cameraExt->position = position;
                    return world;
                }

                cameraExt->frustumSectors[position++] = worldSector;
            }

            spSect = spaStack[nStack--];
        } else {
            RpPlaneSector* pspPlane = (RpPlaneSector*)spSect;
            RwSplitBits sbLeft;
            RwSplitBits sbRight;

            sbLeft.nReal = RwV3dGetSectorCoord(inf, spSect->type) - pspPlane->leftValue;
            sbRight.nReal = pspPlane->rightValue - RwV3dGetSectorCoord(sup, spSect->type);

            if (sbLeft.nInt < 0 && sbRight.nInt < 0) {
                RwBool viewPointIsHigher = RwV3dGetSectorCoord(vViewPoint, spSect->type) > pspPlane->value;

                if ((goBackToFront && viewPointIsHigher) || (!goBackToFront && !viewPointIsHigher)) {
                    nStack++;
                    spaStack[nStack] = pspPlane->rightSubTree;
                    spSect = pspPlane->leftSubTree;
                } else {
                    nStack++;
                    spaStack[nStack] = pspPlane->leftSubTree;
                    spSect = pspPlane->rightSubTree;
                }
            } else if (sbLeft.nInt < 0) {
                spSect = pspPlane->leftSubTree;
            } else {
                spSect = pspPlane->rightSubTree;
            }
        }
    } while (nStack >= 0);

    cameraExt->position = position;

    return world;
} // 0x802DA34C

/* Adds a clump's atomic to the clump's world (RpClumpForAllAtomics callback). */
static RpAtomic* WorldAddClumpAtomic(RpAtomic* atomic, void* data) {
    RpWorldAddAtomic((RpWorld*)data, atomic);

    return atomic;
} // 0x802DA6D8

/* Removes a clump's atomic from the world it was added to (RpClumpForAllAtomics callback). */
static RpAtomic* WorldRemoveClumpAtomic(RpAtomic* atomic, void* data) {
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    if (atomicExt && (RpWorld*)data == atomicExt->world) {
        RpWorldRemoveAtomic((RpWorld*)data, atomic);
    }

    return atomic;
} // 0x802DA734

/* Adds a clump's light to the clump's world (RpClumpForAllLights callback). */
static RpLight* WorldAddClumpLight(RpLight* light, void* data) {
    RpWorldAddLight((RpWorld*)data, light);

    return light;
} // 0x802DA824

/* Removes a clump's light from the world it was added to (RpClumpForAllLights callback). */
static RpLight* WorldRemoveClumpLight(RpLight* light, void* data) {
    rpWorldObjLightExt* lightExt = WORLDLIGHTEXT(light);

    if (lightExt && (RpWorld*)data == lightExt->world) {
        RpWorldRemoveLight((RpWorld*)data, light);
    }

    return light;
} // 0x802DA8C4

/* Adds a clump's camera to the clump's world (RpClumpForAllCameras callback). */
static RwCamera* WorldAddClumpCamera(RwCamera* camera, void* data) {
    RpWorldAddCamera((RpWorld*)data, camera);

    return camera;
} // 0x802DA9A0

/* Removes a clump's camera from the world it was added to (RpClumpForAllCameras callback). */
static RwCamera* WorldRemoveClumpCamera(RwCamera* camera, void* data) {
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    if (cameraExt && (RpWorld*)data == cameraExt->world) {
        RpWorldRemoveCamera((RpWorld*)data, camera);
    }

    return camera;
} // 0x802DAA00

/* Writes a geometry's meshes (rwID_BINMESHPLUGIN stream callback). */
static RwStream* writeGeometryMesh(RwStream* stream, RwInt32 binaryLength, const void* object,
                                   RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpGeometry* geometry = (const RpGeometry*)object;

    return _rpMeshWrite(geometry->mesh, object, stream, &geometry->matList);
} // 0x802DAA30

/* Reads a geometry's meshes (rwID_BINMESHPLUGIN stream callback). */
static RwStream* readGeometryMesh(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                  RwInt32 sizeInObject) {
    RpGeometry* geometry = (RpGeometry*)object;

    geometry->mesh = _rpMeshRead(stream, object, &geometry->matList);
    if (geometry->mesh != NULL) {
        return stream;
    }

    return NULL;
} // 0x802DAA48

/* The size of a geometry's meshes in a stream (rwID_BINMESHPLUGIN stream callback). */
static RwInt32 sizeGeometryMesh(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpGeometry* geometry = (const RpGeometry*)object;

    return _rpMeshSize(geometry->mesh, object);
} // 0x802DAAA0

/* Writes a geometry's native data (rwID_NATIVEDATAPLUGIN stream callback). */
static RwStream* writeGeometryNative(RwStream* stream, RwInt32 binaryLength, const void* object,
                                     RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return _rpGeometryNativeWrite(stream, (const RpGeometry*)object);
} // 0x802DAAAC

/* Reads a geometry's native data (rwID_NATIVEDATAPLUGIN stream callback). */
static RwStream* readGeometryNative(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                    RwInt32 sizeInObject) {
    if (_rpGeometryNativeRead(stream, (RpGeometry*)object) != NULL) {
        return stream;
    }

    return NULL;
} // 0x802DAAB4

/* The size of a geometry's native data in a stream (rwID_NATIVEDATAPLUGIN stream callback). */
static RwInt32 sizeGeometryNative(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return _rpGeometryNativeSize((const RpGeometry*)object);
} // 0x802DAAF8

/* Writes a world sector's native data (rwID_NATIVEDATAPLUGIN stream callback). */
static RwStream* writeWorldSectorNative(RwStream* stream, RwInt32 binaryLength, const void* object,
                                        RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return _rpWorldSectorNativeWrite(stream, (const RpWorldSector*)object);
} // 0x802DAAFC

/* Reads a world sector's native data (rwID_NATIVEDATAPLUGIN stream callback). */
static RwStream* readWorldSectorNative(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                       RwInt32 sizeInObject) {
    if (_rpWorldSectorNativeRead(stream, (RpWorldSector*)object) != NULL) {
        return stream;
    }

    return NULL;
} // 0x802DAB04

/* The size of a world sector's native data in a stream (rwID_NATIVEDATAPLUGIN stream callback). */
static RwInt32 sizeWorldSectorNative(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return _rpWorldSectorNativeSize((const RpWorldSector*)object);
} // 0x802DAB48

/* Writes a world sector's meshes, their materials indexed in the world's list (rwID_BINMESHPLUGIN stream callback). */
static RwStream* writeSectorMesh(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject,
                                 RwInt32 sizeInObject) {
    const RpWorldSector* sector = (const RpWorldSector*)object;
    const RpWorld* world = RpWorldSectorGetWorld(sector);

    return _rpMeshWrite(sector->mesh, world, stream, &world->matList);
} // 0x802DAB4C

/* Reads a world sector's meshes (rwID_BINMESHPLUGIN stream callback). */
static RwStream* readSectorMesh(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                RwInt32 sizeInObject) {
    RpWorldSector* sector = (RpWorldSector*)object;
    const RpWorld* world = RpWorldSectorGetWorld(sector);

    sector->mesh = _rpMeshRead(stream, world, &world->matList);
    if (sector->mesh != NULL) {
        return stream;
    }

    return NULL;
} // 0x802DAB9C

/* The size of a world sector's meshes in a stream (rwID_BINMESHPLUGIN stream callback). */
static RwInt32 sizeSectorMesh(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpWorldSector* sector = (const RpWorldSector*)object;
    const RpWorld* world = RpWorldSectorGetWorld(sector);

    return _rpMeshSize(sector->mesh, world);
} // 0x802DAC00

/*
 * Registers the world object module and the world's extensions of cameras, atomics, clumps and
 * lights, the mesh and native data streams of geometries and world sectors, and the rights to
 * render of atomics, worlds, world sectors and materials; then attaches the world's pipelines.
 */
RwBool _rpWorldObjRegisterExtensions(void) {
    RwInt32 result;

    result = RwEngineRegisterPlugin(sizeof(rpWorldObjGlobals), rwID_WORLDOBJMODULE, WorldObjectOpen,
                                    WorldObjectClose);

    cameraExtOffset = RwCameraRegisterPlugin(sizeof(rpWorldObjCameraExt), rwID_WORLDOBJMODULE, WorldInitCameraExt,
                                             WorldDeInitCameraExt, WorldCopyCameraExt);
    result |= cameraExtOffset;

    atomicExtOffset = RpAtomicRegisterPlugin(sizeof(rpWorldObjAtomicExt), rwID_WORLDOBJMODULE, WorldInitAtomicExt,
                                             WorldDeInitAtomicExt, WorldCopyAtomicExt);
    result |= atomicExtOffset;

    clumpExtOffset = RpClumpRegisterPlugin(sizeof(rpWorldObjClumpExt), rwID_WORLDOBJMODULE, WorldInitClumpExt,
                                           WorldDeInitClumpExt, WorldCopyClumpExt);
    result |= clumpExtOffset;

    lightExtOffset = RpLightRegisterPlugin(sizeof(rpWorldObjLightExt), rwID_WORLDOBJMODULE, WorldInitLightExt,
                                           WorldDeInitLightExt, WorldCopyLightExt);
    result |= lightExtOffset;

    result |= RpGeometryRegisterPlugin(0, rwID_BINMESHPLUGIN, NULL, NULL, NULL);
    result |= RpWorldSectorRegisterPlugin(0, rwID_BINMESHPLUGIN, NULL, NULL, NULL);
    result |= RpGeometryRegisterPluginStream(rwID_BINMESHPLUGIN, readGeometryMesh, writeGeometryMesh,
                                             sizeGeometryMesh);
    result |= RpWorldSectorRegisterPluginStream(rwID_BINMESHPLUGIN, readSectorMesh, writeSectorMesh, sizeSectorMesh);

    result |= RpGeometryRegisterPlugin(0, rwID_NATIVEDATAPLUGIN, NULL, NULL, NULL);
    result |= RpWorldSectorRegisterPlugin(0, rwID_NATIVEDATAPLUGIN, NULL, NULL, NULL);
    result |= RpGeometryRegisterPluginStream(rwID_NATIVEDATAPLUGIN, readGeometryNative, writeGeometryNative,
                                             sizeGeometryNative);
    result |= RpWorldSectorRegisterPluginStream(rwID_NATIVEDATAPLUGIN, readWorldSectorNative,
                                                writeWorldSectorNative, sizeWorldSectorNative);

    result |= RpAtomicRegisterPlugin(0, rwID_RIGHTTORENDER, NULL, NULL, NULL);
    result |= RpAtomicRegisterPluginStream(rwID_RIGHTTORENDER, _rpReadAtomicRights, _rpWriteAtomicRights,
                                           _rpSizeAtomicRights);
    result |= RpWorldRegisterPlugin(0, rwID_RIGHTTORENDER, NULL, NULL, NULL);
    result |= RpWorldRegisterPluginStream(rwID_RIGHTTORENDER, _rpReadWorldRights, _rpWriteWorldRights,
                                          _rpSizeWorldRights);
    result |= RpWorldSectorRegisterPlugin(0, rwID_RIGHTTORENDER, NULL, NULL, NULL);
    result |= RpWorldSectorRegisterPluginStream(rwID_RIGHTTORENDER, _rpReadSectRights, _rpWriteSectRights,
                                                _rpSizeSectRights);
    result |= RpMaterialRegisterPlugin(0, rwID_RIGHTTORENDER, NULL, NULL, NULL);
    result |= RpMaterialRegisterPluginStream(rwID_RIGHTTORENDER, _rpReadMaterialRights, _rpWriteMaterialRights,
                                             _rpSizeMaterialRights);

    if (result < 0) {
        return FALSE;
    }

    return _rpWorldPipeAttach() != FALSE;
} // 0x802DAC38

/* Adds a camera to a world: the camera's frame is synced at once, collecting its frustum sectors. */
RpWorld* RpWorldAddCamera(RpWorld* world, RwCamera* camera) {
    RwFrame* frame = RwCameraGetFrame(camera);
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    if (frame) {
        RwFrameUpdateObjects(frame);
    }

    cameraExt->world = world;

    return world;
} // 0x802DAF48

/* Removes a camera from its world; NULL when it is in none. */
RpWorld* RpWorldRemoveCamera(RpWorld* world, RwCamera* camera) {
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);

    if (cameraExt->world) {
        cameraExt->world = NULL;
        cameraExt->position = 0;

        return world;
    }

    return NULL;
} // 0x802DAF9C

/* The world a camera is in. */
RpWorld* RwCameraGetWorld(const RwCamera* camera) {
    return WORLDCAMERAEXT(camera)->world;
} // 0x802DAFC8

/* Adds an atomic to a world: the atomic's frame is synced at once, tying it to its world sectors. */
RpWorld* RpWorldAddAtomic(RpWorld* world, RpAtomic* atomic) {
    RwFrame* frame = RpAtomicGetFrame(atomic);
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    if (frame) {
        RwFrameUpdateObjects(frame);
    }

    atomicExt->world = world;

    return world;
} // 0x802DAFD8

/* Removes an atomic from its world: frees its instanced data and unties it from its world sectors. */
RpWorld* RpWorldRemoveAtomic(RpWorld* world, RpAtomic* atomic) {
    rpWorldObjAtomicExt* atomicExt = WORLDATOMICEXT(atomic);

    if (atomic->repEntry) {
        RwResourcesFreeResEntry(atomic->repEntry);
    }

    WorldAtomicDestroyTies(atomic);
    atomicExt->world = NULL;

    return world;
} // 0x802DB034

/* The world an atomic is in. */
RpWorld* RpAtomicGetWorld(const RpAtomic* atomic) {
    return WORLDATOMICEXT(atomic)->world;
} // 0x802DB100

/* Adds a clump to a world, with its atomics, lights and cameras, and syncs the clump's frame. */
RpWorld* RpWorldAddClump(RpWorld* world, RpClump* clump) {
    rpWorldObjClumpExt* clumpExt = WORLDCLUMPEXT(clump);
    RwFrame* frame = RpClumpGetFrame(clump);

    rwLinkListAddLLLink(&world->clumpList, &clump->inWorldLink);
    world->numClumpsInWorld++;
    clumpExt->world = world;

    RpClumpForAllAtomics(clump, WorldAddClumpAtomic, world);
    RpClumpForAllLights(clump, WorldAddClumpLight, world);
    RpClumpForAllCameras(clump, WorldAddClumpCamera, world);

    if (frame) {
        RwMatrixOptimize(RwFrameGetMatrix(frame), NULL);
        RwFrameUpdateObjects(frame);
    }

    clumpExt->field_0x4 = RWWORLDOBJGLOBAL(field_0x8);

    return world;
} // 0x802DB10C

/* Removes a clump from its world, with its atomics, lights and cameras; NULL when it is in none. */
RpWorld* RpWorldRemoveClump(RpWorld* world, RpClump* clump) {
    rpWorldObjClumpExt* clumpExt = WORLDCLUMPEXT(clump);

    if (clumpExt->world) {
        clumpExt->world->numClumpsInWorld--;

        if (&clump->inWorldLink == clumpExt->world->currentClumpLink) {
            clumpExt->world->currentClumpLink = rwLLLinkGetNext(&clump->inWorldLink);
        }
        rwLinkListRemoveLLLink(&clump->inWorldLink);

        RpClumpForAllAtomics(clump, WorldRemoveClumpAtomic, world);
        RpClumpForAllLights(clump, WorldRemoveClumpLight, world);
        RpClumpForAllCameras(clump, WorldRemoveClumpCamera, world);

        clumpExt->world = NULL;

        return world;
    }

    RWERROR((E_RP_WORLD_CLUMPNOTINWORLD));

    return NULL;
} // 0x802DB1FC

/* Calls callBack for each world sector in the camera's view frustum at its last sync, until it returns NULL. */
RwCamera* RwCameraForAllSectorsInFrustum(RwCamera* camera, RpWorldSectorCallBack callBack, void* pData) {
    rpWorldObjCameraExt* cameraExt = WORLDCAMERAEXT(camera);
    RpWorldSector** frustumSectors = cameraExt->frustumSectors;
    RwInt32 numSectors = cameraExt->position;

    while (numSectors) {
        if (!callBack(*frustumSectors, pData)) {
            return camera;
        }

        frustumSectors++;
        numSectors--;
    }

    return camera;
} // 0x802DB2F8

/* The world a clump is in. */
RpWorld* RpClumpGetWorld(const RpClump* clump) {
    return WORLDCLUMPEXT(clump)->world;
} // 0x802DB370

/*
 * Adds a light to a world: a directional or ambient light joins the world's directional lights,
 * a positioned one is synced at once (tying it to the world sectors it reaches).
 */
RpWorld* RpWorldAddLight(RpWorld* world, RpLight* light) {
    WORLDLIGHTEXT(light)->world = world;

    if (rwObjectGetSubType(light) < rpLIGHTPOSITIONINGSTART) {
        rwLinkListAddLLLink(&world->directionalLightList, &light->inWorld);
    } else {
        RwFrame* frame = RpLightGetFrame(light);

        if (frame) {
            RwFrameUpdateObjects(frame);
        }

        rwLinkListAddLLLink(&world->lightList, &light->inWorld);
    }

    return world;
} // 0x802DB37C

/* Removes a light from its world: unties it from the world sectors it lights. */
RpWorld* RpWorldRemoveLight(RpWorld* world, RpLight* light) {
    WORLDLIGHTEXT(light)->world = NULL;

    WorldLightDestroyTies(light);
    rwLinkListRemoveLLLink(&light->inWorld);

    return world;
} // 0x802DB41C

/* The world a light is in. */
RpWorld* RpLightGetWorld(const RpLight* light) {
    return WORLDLIGHTEXT(light)->world;
} // 0x802DB4F0
