// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (names of the functions and statics)
// Reference: bfbbdecomp/bfbb include/rwcore.h (RxPipeline)
/*
 * babinwor.c: worlds in streams: a world is read as its structure chunk, its material list, its
 * BSP tree (plane sectors and world sectors, each with its own structure chunk) and its plugin
 * data, into one allocation that holds the world, its sectors and their geometry. The rights
 * callbacks save and restore the plugin that owns a world's or a sector's pipeline.
 */

#include <rwsdk/world/rpplugin.h>
#include <rwsdk/world/babinwor.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/bamateri.h>
#include <string.h>

/* The number of texture coordinate sets, when a world's flags give it. Name invented. */
#define rpWORLDTEXCOORDSETSMASK 0x00FF0000

/* The first version whose polygons are stored with their material index last. Name invented. */
#define rpWORLDPOLYGONMATINDEXLASTVERSION 0x36002

/*
 * Takes size bytes from the front of a world's single allocation (*memory, advanced past them).
 * Name invented.
 */
static void* WorldMemoryTake(RwUInt8** memory, RwInt32 size) {
    void* block = *memory;

    *memory += size;
    return block;
}

/* The binary world module's registration. */
static RwModuleInfo binWorldModule;

/* The rights plugin and its data last read with a world or a world sector (bfbb names). */
static RwInt32 lastSeenWorldRightsPluginId;
static RwInt32 lastSeenWorldExtraData;
static RwInt32 lastSeenSectRightsPluginId;
static RwInt32 lastSeenSectExtraData;

/* Reads the plugin owning a world's pipeline, and its data when the chunk has it. */
RwStream* _rpReadWorldRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                             RwInt32 sizeInObject) {
    if (!RwStreamReadInt32(stream, &lastSeenWorldRightsPluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (binaryLength == 2 * (RwInt32)sizeof(RwInt32)) {
        if (!RwStreamReadInt32(stream, &lastSeenWorldExtraData, sizeof(RwInt32))) {
            return NULL;
        }
    }

    return stream;
} // 0x802CDCA8

/* Writes the plugin owning a world's pipeline and its data. */
RwStream* _rpWriteWorldRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                              RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpWorld* world = (const RpWorld*)object;

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&world->pipeline->pluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (RwStreamWriteInt32(stream, (const RwInt32*)&world->pipeline->pluginData, sizeof(RwInt32))) {
        return stream;
    }

    return NULL;
} // 0x802CDD24

/* The size of a world's rights chunk: written only when a plugin owns its pipeline. */
RwInt32 _rpSizeWorldRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpWorld* world = (const RpWorld*)object;

    if (world->pipeline != NULL && world->pipeline->pluginId != 0) {
        return 2 * sizeof(RwInt32);
    }

    return 0;
} // 0x802CDDA0

/* Reads the plugin owning a world sector's pipeline, and its data when the chunk has it. */
RwStream* _rpReadSectRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                            RwInt32 sizeInObject) {
    if (!RwStreamReadInt32(stream, &lastSeenSectRightsPluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (binaryLength == 2 * (RwInt32)sizeof(RwInt32)) {
        if (!RwStreamReadInt32(stream, &lastSeenSectExtraData, sizeof(RwInt32))) {
            return NULL;
        }
    }

    return stream;
} // 0x802CDDC8

/* Writes the plugin owning a world sector's pipeline and its data. */
RwStream* _rpWriteSectRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                             RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpWorldSector* sector = (const RpWorldSector*)object;

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&sector->pipeline->pluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (RwStreamWriteInt32(stream, (const RwInt32*)&sector->pipeline->pluginData, sizeof(RwInt32))) {
        return stream;
    }

    return NULL;
} // 0x802CDE44

/* The size of a world sector's rights chunk: written only when a plugin owns its pipeline. */
RwInt32 _rpSizeSectRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpWorldSector* sector = (const RpWorldSector*)object;

    if (sector->pipeline != NULL && sector->pipeline->pluginId != 0) {
        return 2 * sizeof(RwInt32);
    }

    return 0;
} // 0x802CDEC0

/*
 * Reads a world sector into the world's allocation (*memory, advanced past what it takes): its
 * structure chunk, its vertices, normals, pre-lit colours and texture coordinates (unless the
 * world is native), its triangles, then its plugin data. On a failure the sector is torn down.
 */
static RpWorldSector* WorldSectorStreamRead(RwStream* stream, RwUInt8** memory, RpWorld* world,
                                            RwUInt32 flags) {
    RpWorldSector* sector;
    RwBool status = TRUE;
    RwUInt32 version;
    RpWorldSectorChunkInfo sectorData;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, NULL, &version)) {
        return NULL;
    }

    if (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    if (RwStreamRead(stream, &sectorData, sizeof(sectorData)) != sizeof(sectorData)) {
        return NULL;
    }
    RwMemNative32(&sectorData, sizeof(sectorData));

    sector = (RpWorldSector*)*memory;
    *memory += sectorTKList.sizeOfStruct;
    if (sector == NULL) {
        RWERROR((E_RW_NOMEM, sectorTKList.sizeOfStruct));
        return NULL;
    }

    sector->type = rwSECTORATOMIC;
    sector->tightBoundingBox.inf = sectorData.inf;
    sector->tightBoundingBox.sup = sectorData.sup;
    sector->matListWindowBase = (RwUInt16)sectorData.matListWindowBase;
    sector->numTriangles = (RwUInt16)sectorData.numTriangles;
    sector->numVertices = (RwUInt16)sectorData.numVertices;
    sector->triangles = NULL;
    sector->vertices = NULL;
    sector->preLitLum = NULL;
    sector->normals = NULL;
    memset(sector->texCoords, 0, sizeof(sector->texCoords));
    sector->pipeline = NULL;
    sector->repEntry = NULL;
    sector->mesh = NULL;
    rwLinkListInitialize(&sector->collAtomicsInWorldSector);
    rwLinkListInitialize(&sector->lightsInWorldSector);

    if (!(RpWorldGetFlags(world) & rpWORLDNATIVE)) {
        if (sectorData.numVertices != 0) {
            RwInt32 size = sectorData.numVertices * sizeof(RwV3d);

            sector->vertices = (RwV3d*)WorldMemoryTake(memory, size);
            if (sector->vertices == NULL) {
                RWERROR((E_RW_NOMEM, size));
                status = FALSE;
            }
            if (status && !RwStreamReadReal(stream, (RwReal*)sector->vertices, size)) {
                status = FALSE;
            }

            if (status && (flags & rpWORLDNORMALS)) {
                size = sectorData.numVertices * sizeof(RpVertexNormal);

                sector->normals = (RpVertexNormal*)WorldMemoryTake(memory, size);
                if (sector->normals == NULL) {
                    RWERROR((E_RW_NOMEM, size));
                    status = FALSE;
                }
                if (status && RwStreamRead(stream, sector->normals, size) != size) {
                    status = FALSE;
                }
            }

            if (status && (flags & rpWORLDPRELIT)) {
                size = sectorData.numVertices * sizeof(RwRGBA);

                sector->preLitLum = (RwRGBA*)WorldMemoryTake(memory, size);
                if (sector->preLitLum == NULL) {
                    RWERROR((E_RW_NOMEM, size));
                    status = FALSE;
                }
                if (status && !RwStreamRead(stream, sector->preLitLum, size)) {
                    status = FALSE;
                }
            }

            if (status && world->numTexCoordSets > 0) {
                RwInt32 i;
                RwInt32 texCoordsSize = sector->numVertices * sizeof(RwTexCoords);

                for (i = 0; i < world->numTexCoordSets; i++) {
                    sector->texCoords[i] = (RwTexCoords*)WorldMemoryTake(memory, texCoordsSize);
                    if (status && sector->texCoords[i] == NULL) {
                        RWERROR((E_RW_NOMEM, texCoordsSize));
                        status = FALSE;
                        break;
                    }
                    if (status && !RwStreamReadReal(stream, (RwReal*)sector->texCoords[i], texCoordsSize)) {
                        status = FALSE;
                        break;
                    }
                }
            }
        }

        if (status && sectorData.numTriangles != 0) {
            RwInt32 size = sectorData.numTriangles * sizeof(RpTriangle);

            sector->triangles = (RpTriangle*)WorldMemoryTake(memory, size);
            if (sector->triangles == NULL) {
                RWERROR((E_RW_NOMEM, size));
                status = FALSE;
            }
            if (status && !RwStreamReadInt16(stream, (RwInt16*)sector->triangles, size)) {
                status = FALSE;
            }

            /*
             * Older files store the material index first: as read, vertIndex[0] holds it and the
             * vertices follow. Move it to the end.
             */
            if (status && version < rpWORLDPOLYGONMATINDEXLASTVERSION) {
                RwInt32 i;

                for (i = 0; i < sectorData.numTriangles; i++) {
                    RpTriangle oldTriangle = sector->triangles[i];

                    sector->triangles[i].vertIndex[0] = oldTriangle.vertIndex[1];
                    sector->triangles[i].vertIndex[1] = oldTriangle.vertIndex[2];
                    sector->triangles[i].vertIndex[2] = oldTriangle.matIndex;
                    sector->triangles[i].matIndex = oldTriangle.vertIndex[0];
                }
            }
        }
    }

    if (status) {
        _rwPluginRegistryInitObject(&sectorTKList, sector);

        lastSeenSectRightsPluginId = 0;
        lastSeenSectExtraData = 0;
        if (status && !_rwPluginRegistryReadDataChunks(&sectorTKList, stream, sector)) {
            status = FALSE;
        }
        if (status && lastSeenSectRightsPluginId != 0) {
            _rwPluginRegistryInvokeRights(&sectorTKList, lastSeenSectRightsPluginId, sector, lastSeenSectExtraData);
        }
    }

    if (!status) {
        if (rwObjectTestPrivateFlags(world, rpWORLDSINGLEMALLOC)) {
            WorldSectorDeinstanceAll((RpSector*)sector);
        } else {
            WorldSectorDestroyRecurse((RpSector*)sector);
        }
        return NULL;
    }

    return sector;
} // 0x802CDEE8

/*
 * Reads a plane sector into the world's allocation (*memory, advanced past what it takes): its
 * structure chunk, then its left and right subtrees. On a failure the subtree read is torn down.
 */
static RpPlaneSector* PlaneSectorStreamRead(RwStream* stream, RwUInt8** memory, RpWorld* world,
                                            RwUInt32 flags) {
    RpPlaneSector* plane;
    RwBool status = TRUE;
    RwUInt32 length;
    RwUInt32 version;
    RpPlaneSectorChunkInfo planeData;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    memset(&planeData, 0, sizeof(planeData));
    if (RwStreamRead(stream, &planeData, length) != length) {
        return NULL;
    }
    RwMemNative32(&planeData, sizeof(planeData));

    plane = (RpPlaneSector*)*memory;
    *memory += sizeof(RpPlaneSector);
    if (plane == NULL) {
        RWERROR((E_RW_NOMEM, sizeof(RpPlaneSector)));
        return NULL;
    }

    plane->type = planeData.type;
    plane->value = planeData.value;
    if (flags & rpWORLDSECTORSOVERLAP) {
        plane->leftValue = planeData.leftValue;
        plane->rightValue = planeData.rightValue;
    } else {
        plane->leftValue = planeData.value;
        plane->rightValue = planeData.value;
    }

    if (planeData.leftIsWorldSector) {
        if (!RwStreamFindChunk(stream, rwID_ATOMICSECT, NULL, &version)) {
            status = FALSE;
        }
        if (status && (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION)) {
            RWERROR((E_RW_BADVERSION));
            status = FALSE;
        }
        if (status) {
            plane->leftSubTree = (RpSector*)WorldSectorStreamRead(stream, memory, world, flags);
            if (plane->leftSubTree == NULL) {
                status = FALSE;
            }
        }
    } else {
        if (!RwStreamFindChunk(stream, rwID_PLANESECT, NULL, &version)) {
            status = FALSE;
        }
        if (status && (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION)) {
            RWERROR((E_RW_BADVERSION));
            status = FALSE;
        }
        if (status) {
            plane->leftSubTree = (RpSector*)PlaneSectorStreamRead(stream, memory, world, flags);
            if (plane->leftSubTree == NULL) {
                status = FALSE;
            }
        }
    }

    if (status) {
        if (planeData.rightIsWorldSector) {
            if (!RwStreamFindChunk(stream, rwID_ATOMICSECT, NULL, &version)) {
                status = FALSE;
            }
            if (status && (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION)) {
                RWERROR((E_RW_BADVERSION));
                status = FALSE;
            }
            if (status) {
                plane->rightSubTree = (RpSector*)WorldSectorStreamRead(stream, memory, world, flags);
                if (plane->rightSubTree == NULL) {
                    status = FALSE;
                }
            }
        } else {
            if (!RwStreamFindChunk(stream, rwID_PLANESECT, NULL, &version)) {
                status = FALSE;
            }
            if (status && (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION)) {
                RWERROR((E_RW_BADVERSION));
                status = FALSE;
            }
            if (status) {
                plane->rightSubTree = (RpSector*)PlaneSectorStreamRead(stream, memory, world, flags);
                if (plane->rightSubTree == NULL) {
                    status = FALSE;
                }
            }
        }
    }

    if (!status) {
        if (plane->leftSubTree != NULL) {
            if (rwObjectTestPrivateFlags(world, rpWORLDSINGLEMALLOC)) {
                WorldSectorDeinstanceAll(plane->leftSubTree);
            } else {
                WorldSectorDestroyRecurse(plane->leftSubTree);
            }
            plane->leftSubTree = NULL;
        }
        return NULL;
    }

    return plane;
} // 0x802CE47C

/*
 * Reads a world: its structure chunk, then one allocation for the world, its sectors and their
 * geometry, its material list, its BSP tree, its bounding boxes and its plugin data. The world
 * comes back unlocked.
 */
RpWorld* RpWorldStreamRead(RwStream* stream) {
    RwUInt8* memory;
    RwUInt32 length;
    RwUInt32 version;       /* the structure chunk's */
    RwUInt32 chunkVersion;  /* the material list's and the root sector's */
    RpWorldChunkInfo worldData;
    RwInt32 flags;
    RpWorld* world;
    RwInt32 numTexCoordSets;
    RwUInt32 size;
    RwInt32 i;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    memset(&worldData, 0, sizeof(worldData));
    if (RwStreamRead(stream, &worldData, length) != length) {
        return NULL;
    }

    /* Collision data of old files is not read. */
    if (worldData.colSectorSize > 0) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
    RwMemNative32(&worldData, sizeof(worldData));
    flags = worldData.format;

    size = worldTKList.sizeOfStruct;
    size += worldData.numPlaneSectors * sizeof(RpPlaneSector);
    size += worldData.numWorldSectors * sectorTKList.sizeOfStruct;

    if (flags & rpWORLDTEXCOORDSETSMASK) {
        numTexCoordSets = (flags & rpWORLDTEXCOORDSETSMASK) >> 16;
    } else if (flags & rpWORLDTEXTURED2) {
        numTexCoordSets = 2;
    } else {
        numTexCoordSets = (flags & rpWORLDTEXTURED) ? 1 : 0;
    }

    if (!(flags & rpWORLDNATIVE)) {
        size += worldData.numVertices * sizeof(RwV3d);
        if (flags & rpWORLDNORMALS) {
            size += worldData.numVertices * sizeof(RpVertexNormal);
        }
        if (flags & rpWORLDPRELIT) {
            size += worldData.numVertices * sizeof(RwRGBA);
        }
        if (numTexCoordSets != 0) {
            size += numTexCoordSets * worldData.numVertices * sizeof(RwTexCoords);
        }
        size += worldData.numTriangles * sizeof(RpTriangle);
    }

    world = (RpWorld*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_WORLD);
    if (world == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }
    memset(world, 0, size);
    memory = (RwUInt8*)world + worldTKList.sizeOfStruct;

    rwObjectInitialize(world, rpWORLD, 0);
    rwObjectSetPrivateFlags(world, rpWORLDSINGLEMALLOC);
    _rpWorldRegisterWorld(world, size);

    world->flags = flags;
    world->renderOrder = rpWORLDRENDERBACK2FRONT;
    RwV3dScaleMacro(&world->worldOrigin, &worldData.invWorldOrigin, -1.0f);
    world->numTexCoordSets = numTexCoordSets;
    world->rootSector = NULL;
    world->pipeline = NULL;
    rwLinkListInitialize(&world->clumpList);
    rwLinkListInitialize(&world->lightList);
    rwLinkListInitialize(&world->directionalLightList);

    if (!RwStreamFindChunk(stream, rwID_MATLIST, NULL, &chunkVersion)) {
        RpWorldDestroy(world);
        return NULL;
    }
    if (chunkVersion < rwLIBRARYBASEVERSION || chunkVersion > rwLIBRARYCURRENTVERSION) {
        RWERROR((E_RW_BADVERSION));
        RpWorldDestroy(world);
        return NULL;
    }
    if (!_rpMaterialListStreamRead(stream, &world->matList)) {
        RpWorldDestroy(world);
        return NULL;
    }

    if (worldData.rootIsWorldSector) {
        if (!RwStreamFindChunk(stream, rwID_ATOMICSECT, NULL, &chunkVersion)) {
            RpWorldDestroy(world);
            return NULL;
        }
        if (chunkVersion < rwLIBRARYBASEVERSION || chunkVersion > rwLIBRARYCURRENTVERSION) {
            RWERROR((E_RW_BADVERSION));
            RpWorldDestroy(world);
            return NULL;
        }
        world->rootSector = (RpSector*)WorldSectorStreamRead(stream, &memory, world, flags);
        if (world->rootSector == NULL) {
            RpWorldDestroy(world);
            return NULL;
        }
    } else {
        if (!RwStreamFindChunk(stream, rwID_PLANESECT, NULL, &chunkVersion)) {
            RpWorldDestroy(world);
            return NULL;
        }
        if (chunkVersion < rwLIBRARYBASEVERSION || chunkVersion > rwLIBRARYCURRENTVERSION) {
            RWERROR((E_RW_BADVERSION));
            RpWorldDestroy(world);
            return NULL;
        }
        world->rootSector = (RpSector*)PlaneSectorStreamRead(stream, &memory, world, flags);
        if (world->rootSector == NULL) {
            RpWorldDestroy(world);
            return NULL;
        }
    }

    world->numClumpsInWorld = 0;
    world->currentClumpLink = rwLinkListGetTerminator(&world->clumpList);
    world->boundingBox = worldData.boundingBox;

    _rpWorldSetupSectorBoundingBoxes(world);
    RpWorldSetSectorRenderCallBack(world, NULL);

    /* Set each texture again so that the material holds its reference. */
    for (i = 0; i < RpWorldGetNumMaterials(world); i++) {
        RpMaterial* material = RpWorldGetMaterial(world, i);

        if (material->texture != NULL) {
            RpMaterialSetTexture(material, material->texture);
        }
    }

    _rwPluginRegistryInitObject(&worldTKList, world);

    lastSeenWorldRightsPluginId = 0;
    lastSeenWorldExtraData = 0;
    if (!_rwPluginRegistryReadDataChunks(&worldTKList, stream, world)) {
        RpWorldDestroy(world);
        return NULL;
    }
    if (lastSeenWorldRightsPluginId != 0) {
        _rwPluginRegistryInvokeRights(&worldTKList, lastSeenWorldRightsPluginId, world, lastSeenWorldExtraData);
    }

    if (!RpWorldUnlock(world)) {
        RpWorldDestroy(world);
        return NULL;
    }

    return world;
} // 0x802CE884

/* Closes the binary world module. */
void* _rpBinaryWorldClose(void* instance, RwInt32 offset, RwInt32 size) {
    binWorldModule.numInstances--;

    return instance;
} // 0x802CEE24

/* Opens the binary world module. */
void* _rpBinaryWorldOpen(void* instance, RwInt32 offset, RwInt32 size) {
    binWorldModule.numInstances++;

    return instance;
} // 0x802CEE38
