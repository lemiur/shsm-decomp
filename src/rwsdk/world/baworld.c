// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (names of the functions and statics)
// Reference: electronicarts/RenderWare3Docs whitepapers/worlds.pdf (the BSP tree walk)
/*
 * baworld.c: RenderWare worlds. A world holds the static scene in a BSP tree of plane sectors and
 * world sectors; the world module keeps a list of the worlds that exist (with the size of each
 * single allocation) so that a world sector can be traced back to its world. Unlocking a world
 * builds its sectors' meshes; locking it destroys them.
 */

#include <rwsdk/world/rpplugin.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/badevice.h>
#include <rwsdk/batextur.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/babinwor.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/world/bameshop.h>
#include <rwsdk/world/baworobj.h>
#include <string.h>

/* The most texture coordinate sets a vertex has (bfbbdecomp/bfbb rwplcore.h). */
#ifndef rwMAXTEXTURECOORDS
#define rwMAXTEXTURECOORDS 8
#endif

/* The type of a sector that only exists while a world is built. Name invented. */
#define rwSECTORBUILD (-2)

/*
 * A world in the world module's list: the world, the size of its single allocation (the world's
 * own size when it was created empty), and its link. Type and member names invented.
 */
typedef struct rpWorldListEntry {
    RpWorld* world;         /* 0x0 */
    RwUInt32 memorySize;    /* 0x4 */
    RwLLLink link;          /* 0x8 */
} rpWorldListEntry;

/* The world module's globals: the free list of list entries and the list of worlds. Names invented. */
typedef struct rpWorldGlobals {
    RwFreeList* worldListFreeList;  /* 0x0 */
    RwLinkList worldList;           /* 0x4 */
} rpWorldGlobals;

#define RWWORLDGLOBAL(var) \
    (RWPLUGINOFFSET(rpWorldGlobals, RwEngineInstance, worldModule.globalsOffset)->var)

/* The plugins registered with worlds: no plugin data yet, so a world is just an RpWorld. */
RwPluginRegistry worldTKList = {
    sizeof(RpWorld),
    sizeof(RpWorld),
    0,
    0,
    NULL,
    NULL,
};

/* The free list of world list entries; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rpWorldListFreeList;

/* The world module's registration (offset of its globals in the engine instance). */
static RwModuleInfo worldModule;

/*
 * What WorldFindSector looks for and whether it found it. Type and member names invented.
 */
typedef struct rpWorldFindSectorData {
    const RpWorldSector* sector;    /* 0x0 */
    RwBool found;                   /* 0x4 */
} rpWorldFindSectorData;

/* Stops a world sector walk at the sector looked for. No copy in retail (inlined). */
static RpWorldSector* WorldFindSector(RpWorldSector* sector, void* data) {
    rpWorldFindSectorData* findData = (rpWorldFindSectorData*)data;

    if (findData->sector == sector) {
        findData->found = TRUE;
        return NULL;
    }

    return sector;
}

/*
 * Builds a world sector's meshes from its triangles, giving each triangle the index of its
 * material's texture, raster and pipeline among those of the sector so far, then optimises them
 * (into strips when the world says so). Returns NULL if the meshes cannot be built.
 */
static RpWorld* WorldBuildMeshAtomicSector(RpWorld* world, RpBuildMesh* buildMesh, RpWorldSector* sector,
                                           RpMaterial** materials) {
    RwInt32 i;
    RwTexture** textures;
    RwRaster** rasters;
    RxPipeline** pipelines;
    RwUInt16 numTextures = 0;
    RwUInt16 numRasters = 0;
    RwUInt16 numPipelines = 0;
    RpMeshHeader* mesh;

    if (RpWorldGetNumMaterials(world) != 0) {
        RwInt32 size = RpWorldGetNumMaterials(world) * sizeof(void*);

        textures = (RwTexture**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_TEXTURE);
        rasters = (RwRaster**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_WORLDMODULE);
        pipelines = (RxPipeline**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_WORLDMODULE);

        for (i = 0; i < sector->numTriangles; i++) {
            RpTriangle* triangle = &sector->triangles[i];
            RpMaterial* material = materials[triangle->matIndex];
            RwTexture* texture = material->texture;
            RwRaster* raster = NULL;
            RwUInt16 textureIndex;
            RwUInt16 rasterIndex;
            RwUInt16 pipelineIndex;
            RxPipeline* pipeline;

            for (textureIndex = 0; textureIndex < numTextures; textureIndex++) {
                if (textures[textureIndex] == texture) {
                    break;
                }
            }
            if (textureIndex == numTextures) {
                textures[textureIndex] = texture;
                numTextures++;
            }

            if (texture != NULL) {
                raster = texture->raster;
            }
            for (rasterIndex = 0; rasterIndex < numRasters; rasterIndex++) {
                if (rasters[rasterIndex] == raster) {
                    break;
                }
            }
            if (rasterIndex == numRasters) {
                rasters[rasterIndex] = raster;
                numRasters++;
            }

            pipeline = material->pipeline;
            for (pipelineIndex = 0; pipelineIndex < numPipelines; pipelineIndex++) {
                if (pipelines[pipelineIndex] == pipeline) {
                    break;
                }
            }
            if (pipelineIndex == numPipelines) {
                pipelines[pipelineIndex] = pipeline;
                numPipelines++;
            }

            _rpBuildMeshAddTriangle(buildMesh, material, triangle->vertIndex[0], triangle->vertIndex[1],
                                    triangle->vertIndex[2], triangle->matIndex, textureIndex, rasterIndex,
                                    pipelineIndex);
        }

        RwFree(textures);
        RwFree(rasters);
        RwFree(pipelines);
    }

    if (RpWorldGetFlags(world) & rpWORLDTRISTRIP) {
        mesh = _rpMeshOptimise(buildMesh, rpMESHHEADERTRISTRIP);
    } else {
        mesh = _rpMeshOptimise(buildMesh, 0);
    }

    if (mesh != NULL) {
        sector->mesh = mesh;
    } else {
        _rpBuildMeshDestroy(buildMesh);
        return NULL;
    }

    return world;
} // 0x802D8154

/*
 * Releases what a BSP subtree of a single-allocation world holds outside that allocation: each
 * world sector's instanced data, its atomic and light ties, its plugin data and its meshes.
 */
void WorldSectorDeinstanceAll(RpSector* sector) {
    switch (sector->type) {
        case rwSECTORATOMIC: {
            RpWorldSector* worldSector = (RpWorldSector*)sector;
            RwLLLink* cur;
            RwLLLink* end;

            if (worldSector->repEntry != NULL) {
                RwResourcesFreeResEntry(worldSector->repEntry);
            }

            cur = rwLinkListGetFirstLLLink(&worldSector->collAtomicsInWorldSector);
            end = rwLinkListGetTerminator(&worldSector->collAtomicsInWorldSector);
            while (cur != end) {
                RpTie* tie = rwLLLinkGetData(cur, RpTie, lAtomicInWorldSector);

                cur = rwLLLinkGetNext(cur);
                _rpTieDestroy(tie);
            }

            cur = rwLinkListGetFirstLLLink(&worldSector->lightsInWorldSector);
            end = rwLinkListGetTerminator(&worldSector->lightsInWorldSector);
            while (cur != end) {
                RpLightTie* lightTie = rwLLLinkGetData(cur, RpLightTie, lightInWorldSector);

                cur = rwLLLinkGetNext(cur);
                _rpLightTieDestroy(lightTie);
            }

            _rwPluginRegistryDeInitObject(&sectorTKList, worldSector);

            if (worldSector->mesh != NULL) {
                _rpMeshDestroy(worldSector->mesh);
                worldSector->mesh = NULL;
            }
            break;
        }
        case rwSECTORBUILD:
            break;
        default: {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            if (plane->leftSubTree != NULL) {
                WorldSectorDeinstanceAll(plane->leftSubTree);
            }
            if (plane->rightSubTree != NULL) {
                WorldSectorDeinstanceAll(plane->rightSubTree);
            }
            break;
        }
    }
} // 0x802D83D0

/*
 * Destroys a BSP subtree allocated sector by sector: each world sector's instanced data, ties,
 * plugin data, geometry and meshes, then the sectors themselves.
 */
void WorldSectorDestroyRecurse(RpSector* sector) {
    switch (sector->type) {
        case rwSECTORATOMIC: {
            RpWorldSector* worldSector = (RpWorldSector*)sector;
            RwLLLink* cur;
            RwLLLink* end;
            RwInt32 i;

            if (worldSector->repEntry != NULL) {
                RwResourcesFreeResEntry(worldSector->repEntry);
            }

            cur = rwLinkListGetFirstLLLink(&worldSector->collAtomicsInWorldSector);
            end = rwLinkListGetTerminator(&worldSector->collAtomicsInWorldSector);
            while (cur != end) {
                RpTie* tie = rwLLLinkGetData(cur, RpTie, lAtomicInWorldSector);

                cur = rwLLLinkGetNext(cur);
                _rpTieDestroy(tie);
            }

            cur = rwLinkListGetFirstLLLink(&worldSector->lightsInWorldSector);
            end = rwLinkListGetTerminator(&worldSector->lightsInWorldSector);
            while (cur != end) {
                RpLightTie* lightTie = rwLLLinkGetData(cur, RpLightTie, lightInWorldSector);

                cur = rwLLLinkGetNext(cur);
                _rpLightTieDestroy(lightTie);
            }

            _rwPluginRegistryDeInitObject(&sectorTKList, worldSector);

            if (worldSector->vertices != NULL) {
                RwFree(worldSector->vertices);
                worldSector->vertices = NULL;
            }
            if (worldSector->normals != NULL) {
                RwFree(worldSector->normals);
                worldSector->normals = NULL;
            }
            if (worldSector->preLitLum != NULL) {
                RwFree(worldSector->preLitLum);
                worldSector->preLitLum = NULL;
            }
            if (worldSector->triangles != NULL) {
                RwFree(worldSector->triangles);
                worldSector->triangles = NULL;
            }
            for (i = 0; i < rwMAXTEXTURECOORDS; i++) {
                if (worldSector->texCoords[i] != NULL) {
                    RwFree(worldSector->texCoords[i]);
                    worldSector->texCoords[i] = NULL;
                }
            }
            if (worldSector->mesh != NULL) {
                _rpMeshDestroy(worldSector->mesh);
                worldSector->mesh = NULL;
            }

            RwFree(worldSector);
            break;
        }
        case rwSECTORBUILD:
            RwFree(sector);
            break;
        default: {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            if (plane->leftSubTree != NULL) {
                WorldSectorDestroyRecurse(plane->leftSubTree);
            }
            plane->leftSubTree = NULL;
            if (plane->rightSubTree != NULL) {
                WorldSectorDestroyRecurse(plane->rightSubTree);
            }
            plane->rightSubTree = NULL;

            RwFree(plane);
            break;
        }
    }
} // 0x802D84BC

/* Closes the world module: destroys the free list of world list entries and the world pipelines. */
static void* WorldClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWWORLDGLOBAL(worldListFreeList) != NULL) {
        RwFreeListDestroy(RWWORLDGLOBAL(worldListFreeList));
        RWWORLDGLOBAL(worldListFreeList) = NULL;
    }

    _rpWorldPipelineClose();

    worldModule.numInstances--;
    return instance;
} // 0x802D86C8

/* Opens the world module: the world pipelines, the free list of world list entries and the list of worlds. */
static void* WorldOpen(void* instance, RwInt32 offset, RwInt32 size) {
    worldModule.globalsOffset = offset;

    if (!_rpWorldPipelineOpen()) {
        return NULL;
    }

    RWWORLDGLOBAL(worldListFreeList) = RwFreeListCreateAndPreallocateSpace(
        sizeof(rpWorldListEntry), 8, 4, 1, &_rpWorldListFreeList, rwMEMHINTDUR_GLOBAL | rwID_WORLD);
    if (RWWORLDGLOBAL(worldListFreeList) == NULL) {
        return NULL;
    }

    rwLinkListInitialize(&RWWORLDGLOBAL(worldList));

    worldModule.numInstances++;
    return instance;
} // 0x802D8730

/*
 * Renders a world sector through its own pipeline, else its world's, else the default world
 * sector pipeline. A sector without triangles renders nothing.
 */
RpWorldSector* _rpSectorDefaultRenderCallBack(RpWorldSector* sector) {
    RxPipeline* pipeline;

    if (sector->numTriangles == 0) {
        return sector;
    }

    pipeline = (sector->pipeline != NULL) ? sector->pipeline
             : (((RpWorld*)RWSRCGLOBAL(curWorld))->pipeline != NULL) ? ((RpWorld*)RWSRCGLOBAL(curWorld))->pipeline
             : RXPIPELINEGLOBAL(currentWorldSectorPipeline);

    if (RxPipelineExecute(pipeline, sector, TRUE)) {
        return sector;
    }

    return NULL;
} // 0x802D87EC

/*
 * Gives each world sector the box its plane sectors cut from the world's bounding box: walking
 * the BSP tree, a plane sector's left side keeps the box up to leftValue on its axis, its right
 * side from rightValue.
 */
RpWorld* _rpWorldSetupSectorBoundingBoxes(RpWorld* world) {
    RwInt32 nStack = 0;
    RpSector* sector = world->rootSector;
    RpSector* sectorStack[rpWORLDMAXBSPDEPTH];
    RwBBox bbox = world->boundingBox;
    RwBBox bboxStack[rpWORLDMAXBSPDEPTH];

    while (nStack >= 0) {
        if (sector->type < 0) {
            ((RpWorldSector*)sector)->boundingBox = bbox;

            sector = sectorStack[nStack];
            bbox = bboxStack[nStack];
            nStack--;
        } else {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            nStack++;
            sectorStack[nStack] = plane->rightSubTree;
            bboxStack[nStack] = bbox;
            GETCOORD(bboxStack[nStack].inf, plane->type) = plane->rightValue;

            GETCOORD(bbox.sup, plane->type) = plane->leftValue;
            sector = plane->leftSubTree;
        }
    }

    return world;
} // 0x802D8878

/* Adds a world to the world module's list, with the size of its allocation. */
void _rpWorldRegisterWorld(RpWorld* world, RwUInt32 memorySize) {
    rpWorldListEntry* entry =
        (rpWorldListEntry*)RwFreeListAlloc(RWWORLDGLOBAL(worldListFreeList), rwMEMHINTDUR_GLOBAL | rwID_WORLDMODULE);

    if (entry != NULL) {
        entry->world = world;
        entry->memorySize = memorySize;
        rwLinkListAddLLLink(&RWWORLDGLOBAL(worldList), &entry->link);
    }
} // 0x802D89D8

/* Removes a world from the world module's list. Global in bfbbdecomp/bfbb; no copy in retail (inlined). */
static void _rpWorldUnregisterWorld(RpWorld* world) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&RWWORLDGLOBAL(worldList));
    RwLLLink* end = rwLinkListGetTerminator(&RWWORLDGLOBAL(worldList));

    while (cur != end) {
        rpWorldListEntry* entry = rwLLLinkGetData(cur, rpWorldListEntry, link);

        if (entry->world == world) {
            rwLinkListRemoveLLLink(&entry->link);
            RwFreeListFree(RWWORLDGLOBAL(worldListFreeList), entry);
            return;
        }
        cur = rwLLLinkGetNext(cur);
    }
}

/*
 * Locks a world: destroys its world sectors' meshes (RpWorldUnlock builds them again). The name
 * is the pair of RpWorldUnlock's.
 */
RpWorld* RpWorldLock(RpWorld* world) {
    RwInt32 nStack = 0;
    RpSector* sector = world->rootSector;
    RpSector* sectorStack[rpWORLDMAXBSPDEPTH];

    if (sector == NULL) {
        return NULL;
    }

    do {
        if (sector->type < 0) {
            RpWorldSector* worldSector = (RpWorldSector*)sector;

            if (worldSector->mesh != NULL) {
                _rpMeshDestroy(worldSector->mesh);
                worldSector->mesh = NULL;
            }

            sector = sectorStack[nStack--];
        } else {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            sectorStack[++nStack] = plane->rightSubTree;
            sector = plane->leftSubTree;
        }
    } while (nStack >= 0);

    return world;
} // 0x802D8A8C

/* Unlocks a world: builds the meshes of its world sectors that have none. */
RpWorld* RpWorldUnlock(RpWorld* world) {
    RwInt32 nStack = 0;
    RpMaterial** materials;
    RpSector* sector = world->rootSector;
    RpSector* sectorStack[rpWORLDMAXBSPDEPTH];

    if (sector == NULL) {
        return NULL;
    }

    do {
        if (sector->type < 0) {
            RpWorldSector* worldSector = (RpWorldSector*)sector;

            materials = &RpWorldGetMaterial(world, worldSector->matListWindowBase);

            if (worldSector->mesh == NULL) {
                RpBuildMesh* buildMesh = _rpBuildMeshCreate(worldSector->numTriangles);

                if (buildMesh != NULL) {
                    world = WorldBuildMeshAtomicSector(world, buildMesh, worldSector, materials);
                    if (world == NULL) {
                        return NULL;
                    }
                } else {
                    return NULL;
                }
            }

            sector = sectorStack[nStack--];
        } else {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            sectorStack[++nStack] = plane->rightSubTree;
            sector = plane->leftSubTree;
        }
    } while (nStack >= 0);

    return world;
} // 0x802D8B24

/*
 * Finds the world a world sector belongs to: inside the single allocation of a world read from a
 * stream, else among the sectors of a world built sector by sector. The entries are read through
 * rwLLLinkGetConstData (the function changes none): with const reads the flag and range tests
 * share one load of entry->world and the inlined walk loads it again, as in retail.
 */
RpWorld* RpWorldSectorGetWorld(const RpWorldSector* sector) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&RWWORLDGLOBAL(worldList));
    RwLLLink* end = rwLinkListGetTerminator(&RWWORLDGLOBAL(worldList));

    while (cur != end) {
        const rpWorldListEntry* entry = rwLLLinkGetConstData(cur, rpWorldListEntry, link);

        if (rwObjectTestPrivateFlags(entry->world, rpWORLDSINGLEMALLOC)) {
            if ((const RwUInt8*)sector >= (const RwUInt8*)entry->world &&
                (const RwUInt8*)sector < (const RwUInt8*)entry->world + entry->memorySize) {
                return entry->world;
            }
        } else {
            rpWorldFindSectorData findData;

            findData.sector = sector;
            findData.found = FALSE;
            RpWorldForAllWorldSectors(entry->world, WorldFindSector, &findData);
            if (findData.found) {
                return entry->world;
            }
        }

        cur = rwLLLinkGetNext(cur);
    }

    return NULL;
} // 0x802D8C00

/* Destroys a world: its meshes, its material list, its sectors and its plugin data. */
RwBool RpWorldDestroy(RpWorld* world) {
    _rpWorldUnregisterWorld(world);

    RpWorldLock(world);
    _rpMaterialListDeinitialize(&world->matList);

    if (rwObjectTestPrivateFlags(world, rpWORLDSINGLEMALLOC)) {
        if (world->rootSector != NULL) {
            WorldSectorDeinstanceAll(world->rootSector);
        }
        _rwPluginRegistryDeInitObject(&worldTKList, world);
        RwFree(world);
    } else {
        if (world->rootSector != NULL) {
            WorldSectorDestroyRecurse(world->rootSector);
        }
        _rwPluginRegistryDeInitObject(&worldTKList, world);
        RwFree(world);
    }

    return TRUE;
} // 0x802D8CE8

/* Sets the callback that renders a world's sectors; NULL restores the default. */
RpWorld* RpWorldSetSectorRenderCallBack(RpWorld* world, RpWorldSectorCallBackRender fpCallBack) {
    if (fpCallBack == NULL) {
        fpCallBack = _rpSectorDefaultRenderCallBack;
    }
    world->renderCallBack = fpCallBack;

    return world;
} // 0x802D8E00

/* Returns the callback that renders a world's sectors. */
RpWorldSectorCallBackRender RpWorldGetSectorRenderCallBack(const RpWorld* world) {
    return world->renderCallBack;
} // 0x802D8E18

/* Creates an empty world: one world sector filling the bounding box, no materials. */
RpWorld* RpWorldCreate(RwBBox* boundingBox) {
    RpWorld* world;
    RpWorldSector* sector;

    world = (RpWorld*)RwMalloc(worldTKList.sizeOfStruct, rwMEMHINTDUR_EVENT | rwID_WORLD);
    if (world == NULL) {
        RWERROR((E_RW_NOMEM, worldTKList.sizeOfStruct));
        return NULL;
    }

    rwObjectInitialize(world, rpWORLD, 0);
    _rpMaterialListInitialize(&world->matList);
    world->renderOrder = rpWORLDRENDERBACK2FRONT;
    world->flags = 0;

    sector = (RpWorldSector*)RwMalloc(sectorTKList.sizeOfStruct, rwMEMHINTDUR_EVENT | rwID_WORLD);
    if (sector == NULL) {
        /* The size reported is a pointer's, not the sector's (a slip in the original). */
        RWERROR((E_RW_NOMEM, sizeof(RpWorldSector*)));
        RwFree(world);
        return NULL;
    }

    sector->type = rwSECTORATOMIC;
    sector->repEntry = NULL;
    sector->mesh = NULL;
    rwLinkListInitialize(&sector->collAtomicsInWorldSector);
    rwLinkListInitialize(&sector->lightsInWorldSector);
    sector->numVertices = 0;
    sector->numTriangles = 0;
    sector->vertices = NULL;
    sector->triangles = NULL;
    sector->normals = NULL;
    memset(sector->texCoords, 0, sizeof(sector->texCoords));
    sector->preLitLum = NULL;
    sector->boundingBox.inf = boundingBox->inf;
    sector->boundingBox.sup = boundingBox->sup;
    sector->tightBoundingBox.inf = boundingBox->inf;
    sector->tightBoundingBox.sup = boundingBox->sup;
    sector->pipeline = NULL;

    world->rootSector = (RpSector*)sector;
    world->numTexCoordSets = 0;
    world->worldOrigin.x = world->worldOrigin.y = world->worldOrigin.z = 0.0f;
    world->boundingBox.inf = boundingBox->inf;
    world->boundingBox.sup = boundingBox->sup;

    rwLinkListInitialize(&world->clumpList);
    world->numClumpsInWorld = 0;
    world->currentClumpLink = rwLinkListGetTerminator(&world->clumpList);
    rwLinkListInitialize(&world->lightList);
    rwLinkListInitialize(&world->directionalLightList);

    RpWorldSetSectorRenderCallBack(world, NULL);
    world->pipeline = NULL;

    _rpWorldRegisterWorld(world, worldTKList.sizeOfStruct);

    _rwPluginRegistryInitObject(&worldTKList, world);
    _rwPluginRegistryInitObject(&sectorTKList, sector);

    if (!RpWorldUnlock(world)) {
        RpWorldDestroy(world);
        return NULL;
    }

    return world;
} // 0x802D8E20

/* Calls fpCallBack for each clump in a world until it returns NULL. */
RpWorld* RpWorldForAllClumps(RpWorld* world, RpClumpCallBack fpCallBack, void* pData) {
    if (!rwLinkListEmpty(&world->clumpList)) {
        RwLLLink* cur = rwLinkListGetFirstLLLink(&world->clumpList);

        while (rwLinkListGetTerminator(&world->clumpList) != cur) {
            /* Step first: the callback may remove the clump. */
            RwLLLink* next = rwLLLinkGetNext(cur);

            if (!fpCallBack(rwLLLinkGetData(cur, RpClump, inWorldLink), pData)) {
                return world;
            }

            cur = next;
        }
    }

    return world;
} // 0x802D9228

/* Calls fpCallBack for each material in a world's list until it returns NULL. */
RpWorld* RpWorldForAllMaterials(RpWorld* world, RpMaterialCallBack fpCallBack, void* pData) {
    RwInt32 numMaterials = RpWorldGetNumMaterials(world);
    RwInt32 i;

    for (i = 0; i < numMaterials; i++) {
        if (!fpCallBack(RpWorldGetMaterial(world, i), pData)) {
            return world;
        }
    }

    return world;
} // 0x802D92A0

/* Calls fpCallBack for each world sector of a world, walking its BSP tree, until it returns NULL. */
RpWorld* RpWorldForAllWorldSectors(RpWorld* world, RpWorldSectorCallBack fpCallBack, void* pData) {
    RwInt32 nStack = 0;
    RpSector* sector = world->rootSector;
    RpSector* sectorStack[rpWORLDMAXBSPDEPTH];

    do {
        if (sector->type < 0) {
            if (!fpCallBack((RpWorldSector*)sector, pData)) {
                return world;
            }

            sector = sectorStack[nStack--];
        } else {
            RpPlaneSector* plane = (RpPlaneSector*)sector;

            sectorStack[++nStack] = plane->rightSubTree;
            sector = plane->leftSubTree;
        }
    } while (nStack >= 0);

    return world;
} // 0x802D931C

/* Registers a plugin with worlds. */
RwInt32 RpWorldRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&worldTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D93B8

/* Registers a plugin's stream callbacks with worlds. */
RwInt32 RpWorldRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&worldTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D93E8

/*
 * Attaches the world plugin: registers the world's modules with the engine (each with the size of
 * its globals), then the world object, clump and world pipeline extensions.
 */
RwBool RpWorldPluginAttach(void) {
    RwInt32 status;

    status = RwEngineRegisterPlugin(sizeof(rpMaterialGlobals), rwID_MATERIALMODULE, _rpMaterialOpen, _rpMaterialClose);
    status |= RwEngineRegisterPlugin(sizeof(rpMeshGlobals), rwID_MESHMODULE, _rpMeshOpen, _rpMeshClose);
    status |= RwEngineRegisterPlugin(sizeof(rpGeometryGlobals), rwID_GEOMETRYMODULE, _rpGeometryOpen, _rpGeometryClose);
    status |= RwEngineRegisterPlugin(sizeof(rpClumpGlobals), rwID_CLUMPMODULE, _rpClumpOpen, _rpClumpClose);
    status |= RwEngineRegisterPlugin(sizeof(rpLightGlobals), rwID_LIGHTMODULE, _rpLightOpen, _rpLightClose);
    status |= RwEngineRegisterPlugin(0, rwID_SECTORMODULE, _rpSectorOpen, _rpSectorClose);
    status |= RwEngineRegisterPlugin(sizeof(rpWorldGlobals), rwID_WORLDMODULE, WorldOpen, WorldClose);
    status |= RwEngineRegisterPlugin(0, rwID_BINWORLDMODULE, _rpBinaryWorldOpen, _rpBinaryWorldClose);
    if (status < 0) {
        return FALSE;
    }

    if (!_rpWorldObjRegisterExtensions()) {
        return FALSE;
    }
    if (!_rpClumpRegisterExtensions()) {
        return FALSE;
    }
    if (!_rxWorldDevicePluginAttach()) {
        return FALSE;
    }

    return TRUE;
} // 0x802D9410
