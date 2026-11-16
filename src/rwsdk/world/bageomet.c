// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h (function names and signatures)
// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (geometryTKList, geometryModule)
// Unit name and boundary: Gormiti link map (RPworld.a bageomet)
// Locals of RpGeometryStreamRead: Gormiti DWARF
/*
 * bageomet.c: RenderWare geometries, the shapes atomics share: vertices in one or more morph
 * targets, triangles, texture coordinates and pre-lit colours, the materials the triangles use
 * and the meshes built from them. A geometry is reference counted; locking it lets the caller
 * change its arrays, and unlocking rebuilds what the change invalidated.
 */
#include <rwsdk/world/rpplugin.h>

#include <string.h>

#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bameshop.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/batkreg.h>

/* The plugins registered with geometries: no plugin data yet, so a geometry is just an RpGeometry. */
RwPluginRegistry geometryTKList = {
    sizeof(RpGeometry),
    sizeof(RpGeometry),
    0,
    0,
    NULL,
    NULL,
};

/* The geometry module's registration (offset of its globals in the engine instance). */
static RwModuleInfo geometryModule;

/* A triangle in a geometry's structure chunk, as two words. Type and member names invented. */
typedef struct rpBinaryTriangle {
    RwUInt32 vertex01;      /* 0x0: the first vertex index in the high half, the second in the low */
    RwUInt32 vertex2Mat;    /* 0x4: the third vertex index in the high half, the material index in the low */
} rpBinaryTriangle;

/*
 * A morph target's header in a geometry's structure chunk: its bounds and whether its positions
 * and normals follow. Type and member names invented.
 */
typedef struct rpBinaryMorphTarget {
    RwSphere boundingSphere;    /* 0x0 */
    RwBool pointsPresent;       /* 0x10 */
    RwBool normalsPresent;      /* 0x14 */
} rpBinaryMorphTarget;

/* Opens the geometry module. */
void* _rpGeometryOpen(void* instance, RwInt32 offset, RwInt32 size) {
    geometryModule.globalsOffset = offset;
    geometryModule.numInstances++;

    return instance;
} // 0x802D1B5C

/* Closes the geometry module. */
void* _rpGeometryClose(void* instance, RwInt32 offset, RwInt32 size) {
    geometryModule.numInstances--;

    return instance;
} // 0x802D1B74

/*
 * Adds mtcount morph targets to the geometry, each with room for its vertex positions (and
 * normals, if the geometry has them) unless the geometry is native. The morph targets and their
 * vertex arrays share one block: growing it moves the old arrays up past the new structures.
 * Returns the index of the first new morph target, or -1 when the block cannot grow.
 */
RwInt32 RpGeometryAddMorphTargets(RpGeometry* geometry, RwInt32 mtcount) {
    RwUInt32 morphTargetSize;
    RwUInt32 size;
    RpMorphTarget* morphTarget;
    RwUInt8* vertexData;
    RwInt32 i;

    if (geometry->flags & rpGEOMETRYNATIVE) {
        morphTargetSize = sizeof(RpMorphTarget);
    } else {
        morphTargetSize = sizeof(RpMorphTarget) + geometry->numVertices * sizeof(RwV3d);
        if (geometry->flags & rpGEOMETRYNORMALS) {
            morphTargetSize += geometry->numVertices * sizeof(RwV3d);
        }
    }
    size = morphTargetSize * (geometry->numMorphTargets + mtcount);

    if (geometry->morphTarget != NULL) {
        RwUInt8* src;
        RwUInt8* dst;
        RwUInt32 count;

        morphTarget = (RpMorphTarget*)RwRealloc(geometry->morphTarget, size, rwMEMHINTDUR_EVENT | rwID_GEOMETRY);
        if (morphTarget == NULL) {
            RWERROR((E_RW_NOMEM, size));
            return -1;
        }

        /* Move the old vertex arrays up, last byte first, past the new morph targets. */
        src = (RwUInt8*)morphTarget + morphTargetSize * geometry->numMorphTargets - 1;
        dst = src + mtcount * sizeof(RpMorphTarget);
        count = morphTargetSize * geometry->numMorphTargets - geometry->numMorphTargets * sizeof(RpMorphTarget);
        while (count--) {
            *dst-- = *src--;
        }
    } else {
        morphTarget = (RpMorphTarget*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_GEOMETRY);
        if (morphTarget == NULL) {
            RWERROR((E_RW_NOMEM, size));
            return -1;
        }
    }

    geometry->morphTarget = morphTarget;
    geometry->numMorphTargets += mtcount;

    /* Point every morph target at its arrays, which follow the morph target structures. */
    vertexData = (RwUInt8*)(morphTarget + geometry->numMorphTargets);
    for (i = 0; i < geometry->numMorphTargets; i++) {
        RpMorphTarget* target = &geometry->morphTarget[i];

        target->verts = NULL;
        target->normals = NULL;
        if (!(geometry->flags & rpGEOMETRYNATIVE) && geometry->numVertices != 0) {
            target->verts = (RwV3d*)vertexData;
            vertexData += geometry->numVertices * sizeof(RwV3d);
            if (geometry->flags & rpGEOMETRYNORMALS) {
                target->normals = (RwV3d*)vertexData;
                vertexData += geometry->numVertices * sizeof(RwV3d);
            }
        }
    }

    for (i = geometry->numMorphTargets - mtcount; i < geometry->numMorphTargets; i++) {
        RpMorphTarget* target = &geometry->morphTarget[i];

        target->boundingSphere.center.x = 0.0f;
        target->boundingSphere.center.y = 0.0f;
        target->boundingSphere.center.z = 0.0f;
        target->boundingSphere.radius = 0.0f;
        target->parentGeom = geometry;
    }

    return geometry->numMorphTargets - mtcount;
} // 0x802D1B88

/*
 * Sets the material of one of the geometry's triangles, adding the material to the geometry's
 * list if it is not there yet; a NULL material clears it. NULL when the list cannot grow.
 */
RpGeometry* RpGeometryTriangleSetMaterial(RpGeometry* geometry, RpTriangle* triangle, RpMaterial* material) {
    if (material != NULL) {
        RwInt32 matIndex = _rpMaterialListFindMaterialIndex(&geometry->matList, material);

        if (matIndex < 0) {
            matIndex = _rpMaterialListAppendMaterial(&geometry->matList, material);
            if (matIndex < 0) {
                return NULL;
            }
        }
        triangle->matIndex = (RwUInt16)matIndex;
    } else {
        triangle->matIndex = 0xFFFF;
    }

    return geometry;
} // 0x802D1E2C

/* Calls fpCallBack for each material of the geometry, until one returns NULL. */
RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* pData) {
    RwInt32 numMaterials = geometry->matList.numMaterials;
    RwInt32 i;

    for (i = 0; i < numMaterials; i++) {
        if (fpCallBack(geometry->matList.materials[i], pData) == NULL) {
            return geometry;
        }
    }

    return geometry;
} // 0x802D1EBC

/* Locks the arrays lockMode names for changing; locking the triangles drops the meshes. */
RpGeometry* RpGeometryLock(RpGeometry* geometry, RwInt32 lockMode) {
    geometry->lockedSinceLastInst |= lockMode;

    if (lockMode & rpGEOMETRYLOCKPOLYGONS) {
        if (geometry->mesh != NULL) {
            _rpMeshDestroy(geometry->mesh);
            geometry->mesh = NULL;
        }
    }

    return geometry;
} // 0x802D1F38

/*
 * Unlocks the geometry: when its triangles were locked (so it has no meshes), builds the meshes
 * again from the triangles, sorted by material, texture, raster and pipeline. NULL when they
 * cannot be built.
 */
RpGeometry* RpGeometryUnlock(RpGeometry* geometry) {
    RwTexture** textures;
    RwRaster** rasters;
    RxPipeline** pipelines;
    RwUInt16 numTextures = 0;
    RwUInt16 numRasters = 0;
    RwUInt16 numPipelines = 0;

    if (geometry->mesh == NULL) {
        RpBuildMesh* buildMesh = _rpBuildMeshCreate(geometry->numTriangles);
        RpMeshHeader* mesh;

        if (buildMesh != NULL) {
            if (geometry->matList.numMaterials > 0) {
                RwUInt32 size = geometry->matList.numMaterials * sizeof(void*);
                RwInt32 i;

                textures = (RwTexture**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_TEXTURE);
                rasters = (RwRaster**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_GEOMETRYMODULE);
                pipelines = (RxPipeline**)RwMalloc(size, rwMEMHINTDUR_FUNCTION | rwID_GEOMETRYMODULE);

                for (i = 0; i < geometry->numTriangles; i++) {
                    RpTriangle* triangle = &geometry->triangles[i];
                    RwRaster* raster = NULL;
                    RpMaterial* material = _rpMaterialListGetMaterial(&geometry->matList, triangle->matIndex);
                    RwTexture* texture = material->texture;
                    RwUInt16 textureIndex;
                    RwUInt16 rasterIndex;
                    RwUInt16 pipelineIndex;
                    RxPipeline* pipeline;

                    /* Each texture, raster and pipeline gets the index of its first use. */
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

            if (geometry->flags & rpGEOMETRYTRISTRIP) {
                mesh = _rpMeshOptimise(buildMesh, rpMESHHEADERTRISTRIP);
            } else {
                mesh = _rpMeshOptimise(buildMesh, 0);
            }

            if (mesh != NULL) {
                geometry->mesh = mesh;
                return geometry;
            }

            _rpBuildMeshDestroy(buildMesh);
            return NULL;
        }

        return NULL;
    }

    return geometry;
} // 0x802D1F90

/*
 * Calls fpCallBack for each mesh of the geometry; NULL when it has no meshes or the iteration fails.
 * The geometry is const; the
 * qualifier decides when the compiler loads geometry->mesh.
 */
const RpGeometry* RpGeometryForAllMeshes(const RpGeometry* geometry, RpMeshCallBack fpCallBack, void* pData) {
    if (geometry->mesh != NULL && _rpMeshHeaderForAllMeshes(geometry->mesh, fpCallBack, pData) != NULL) {
        return geometry;
    }

    return NULL;
} // 0x802D2228

/*
 * Creates a geometry of numVert vertices and numTriangles triangles with one morph target. format
 * holds the rpGEOMETRY flags, the number of texture coordinate sets in its third byte (or the
 * TEXTURED/TEXTURED2 flags) and the native flags. Unless the geometry is native, its pre-lit
 * colours, texture coordinates and triangles share the geometry's block; the triangles start with
 * no material. NULL when there are more vertices than a triangle can index, or out of memory.
 */
RpGeometry* RpGeometryCreate(RwInt32 numVert, RwInt32 numTriangles, RwUInt32 format) {
    RwUInt32 numTexCoordSets;
    RwUInt32 native;
    RwUInt32 requested;
    RwUInt32 flags;
    RpGeometry* geometry;
    RwUInt32 size;

    if ((RwUInt32)numVert > 0xFFFF || numTriangles < 0) {
        if (numVert >= 0x10000) {
            RWERROR((E_RP_WORLD_TOOMANYVERTICES));
        }
        return NULL;
    }

    size = geometryTKList.sizeOfStruct;
    requested = format & rpGEOMETRYFLAGSMASK;

    if (format & rpGEOMETRYTEXCOORDSETSMASK) {
        numTexCoordSets = (format & rpGEOMETRYTEXCOORDSETSMASK) >> 16;
    } else if (format & rpGEOMETRYTEXTURED2) {
        numTexCoordSets = 2;
    } else {
        numTexCoordSets = (format & rpGEOMETRYTEXTURED) ? 1 : 0;
    }

    /* The texture flags follow the set count: TEXTURED for one set, TEXTURED2 for more. */
    flags = (requested & ~(rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) |
            (numTexCoordSets == 1 ? rpGEOMETRYTEXTURED : (numTexCoordSets > 1 ? rpGEOMETRYTEXTURED2 : 0));

    native = format & rpGEOMETRYNATIVE;
    if (!native) {
        if (flags & rpGEOMETRYPRELIT) {
            size += numVert * sizeof(RwRGBA);
        }
        if (numTexCoordSets != 0) {
            size += numVert * sizeof(RwTexCoords) * numTexCoordSets;
        }
        size += numTriangles * sizeof(RpTriangle);
    }

    geometry = (RpGeometry*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_GEOMETRY);
    if (geometry == NULL) {
        return NULL;
    }

    /* The original's quirk: the geometry's block leaks when its material list cannot start. */
    if (_rpMaterialListInitialize(&geometry->matList) == NULL) {
        return NULL;
    }

    geometry->morphTarget = NULL;
    geometry->numMorphTargets = 0;
    rwObjectInitialize(geometry, rpGEOMETRY, 0);
    geometry->repEntry = NULL;
    geometry->lockedSinceLastInst = 0;
    geometry->refCount = 1;
    geometry->mesh = NULL;
    geometry->numTexCoordSets = numTexCoordSets;
    memset(geometry->texCoords, 0, sizeof(geometry->texCoords));
    geometry->preLitLum = NULL;
    geometry->triangles = NULL;
    geometry->numTriangles = numTriangles;
    geometry->flags = flags | (format & rpGEOMETRYNATIVEFLAGSMASK);
    geometry->numVertices = numVert;

    if (!native) {
        RwUInt8* data = (RwUInt8*)geometry + geometryTKList.sizeOfStruct;

        if ((flags & rpGEOMETRYPRELIT) && numVert != 0) {
            geometry->preLitLum = (RwRGBA*)data;
            data += numVert * sizeof(RwRGBA);
        }

        if (numTexCoordSets != 0 && numVert != 0) {
            RwUInt32 i;

            for (i = 0; i < numTexCoordSets; i++) {
                geometry->texCoords[i] = (RwTexCoords*)data;
                data += numVert * sizeof(RwTexCoords);
            }
        }

        if (numTriangles != 0) {
            RwInt32 i;

            geometry->triangles = (RpTriangle*)data;
            for (i = 0; i < numTriangles; i++) {
                geometry->triangles[i].matIndex = 0xFFFF;
            }
        }
    }

    if (RpGeometryAddMorphTargets(geometry, 1) < 0) {
        _rpMaterialListDeinitialize(&geometry->matList);
        RwFree(geometry);
        return NULL;
    }

    _rwPluginRegistryInitObject(&geometryTKList, geometry);

    return geometry;
} // 0x802D2278

/* Adds a reference to the geometry. */
RpGeometry* _rpGeometryAddRef(RpGeometry* geometry) {
    geometry->refCount++;

    return geometry;
} // 0x802D2660

/*
 * Takes apart a geometry whose last reference has gone: its meshes, its plugin data, its morph
 * targets and its material list, then the geometry itself. It holds a reference of its own
 * meanwhile, so that nothing the plugins do destroys the geometry again. Name invented.
 */
static RwBool GeometryDestroy(RpGeometry* geometry) {
    _rpGeometryAddRef(geometry);

    RpGeometryLock(geometry, rpGEOMETRYLOCKALL);

    _rwPluginRegistryDeInitObject(&geometryTKList, geometry);

    if (geometry->morphTarget != NULL) {
        RwFree(geometry->morphTarget);
        geometry->morphTarget = NULL;
    }

    _rpMaterialListDeinitialize(&geometry->matList);

    geometry->refCount--;
    RwFree(geometry);

    return TRUE;
}

/*
 * Drops a reference to the geometry; the last one frees its instanced copy and destroys it.
 */
RwBool RpGeometryDestroy(RpGeometry* geometry) {
    RwBool result = TRUE;

    if (geometry->refCount - 1 <= 0) {
        if (geometry->repEntry != NULL) {
            RwResourcesFreeResEntry(geometry->repEntry);
        }

        geometry->refCount--;
        result = GeometryDestroy(geometry);
    } else {
        geometry->refCount--;
    }

    return result;
} // 0x802D2670

/* Registers a plugin with geometries: size bytes of data per geometry, and its callbacks. */
RwInt32 RpGeometryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                 RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&geometryTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D275C

/* Registers a plugin's stream callbacks for geometries. */
RwInt32 RpGeometryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                       RwPluginDataChunkWriteCallBack writeCB,
                                       RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&geometryTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D278C

/* The offset of a plugin's data in a geometry. */
RwInt32 RpGeometryGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&geometryTKList, pluginID);
} // 0x802D27B4

/*
 * Reads a geometry from the stream: its structure chunk (the header, then, unless the geometry is
 * native, its pre-lit colours, texture coordinates and triangles), its morph targets, its material
 * list chunk and its plugin data, then builds its meshes. NULL on a read error or a bad version.
 * A missing material list chunk returns NULL without destroying the geometry (the original leaks it).
 * Locals and their blocks: Gormiti DWARF (sizeLum, sizeTC, destTri/numTris/size and the morph target
 * index and pointer each live in their own block).
 */
RpGeometry* RpGeometryStreamRead(RwStream* stream) {
    RpGeometry* geometry;
    RpGeometryChunkInfo geom;
    RwUInt32 version;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, NULL, &version)) {
        return NULL;
    }

    if (!(version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION)) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    if (RwStreamRead(stream, &geom, sizeof(geom)) != sizeof(geom)) {
        return NULL;
    }
    RwMemNative32(&geom, sizeof(geom));

    geometry = RpGeometryCreate(geom.numVertices, geom.numTriangles, geom.format);
    if (geometry == NULL) {
        return NULL;
    }

    if (geom.numMorphTargets > 1) {
        if (RpGeometryAddMorphTargets(geometry, geom.numMorphTargets - 1) < 0) {
            RpGeometryDestroy(geometry);
            return NULL;
        }
    }

    if (!(geometry->flags & rpGEOMETRYNATIVE) && geometry->numVertices != 0) {
        if (geom.format & rpGEOMETRYPRELIT) {
            RwUInt32 sizeLum = geometry->numVertices * sizeof(RwRGBA);

            if (RwStreamRead(stream, geometry->preLitLum, sizeLum) != sizeLum) {
                RpGeometryDestroy(geometry);
                return NULL;
            }
        }

        if (geometry->numTexCoordSets > 0) {
            RwInt32 i;
            RwUInt32 sizeTC = geometry->numVertices * sizeof(RwTexCoords);

            for (i = 0; i < geometry->numTexCoordSets; i++) {
                if (!RwStreamReadReal(stream, (RwReal*)geometry->texCoords[i], sizeTC)) {
                    RpGeometryDestroy(geometry);
                    return NULL;
                }
            }
        }

        {
            RpTriangle* destTri;
            RwInt32 numTris;
            RwUInt32 size;

            numTris = geometry->numTriangles;

            if (numTris != 0) {
                destTri = geometry->triangles;
                size = numTris * sizeof(RpTriangle);

                if (RwStreamRead(stream, destTri, size) != size) {
                    RpGeometryDestroy(geometry);
                    return NULL;
                }
                RwMemNative32(destTri, size);

                /* Each triangle was read as two words: the first two vertices, then the third and the material. */
                for (; numTris != 0; numTris--) {
                    rpBinaryTriangle* srceTri = (rpBinaryTriangle*)destTri;
                    RwUInt32 vertex01 = srceTri->vertex01;
                    RwUInt32 vertex2Mat;

                    destTri->vertIndex[0] = (RwUInt16)(vertex01 >> 16);
                    destTri->vertIndex[1] = (RwUInt16)vertex01;
                    vertex2Mat = srceTri->vertex2Mat;
                    destTri->vertIndex[2] = (RwUInt16)(vertex2Mat >> 16);
                    destTri->matIndex = (RwUInt16)vertex2Mat;
                    destTri++;
                }
            }
        }
    }

    {
        RwInt32 i;

        for (i = 0; i < geometry->numMorphTargets; i++) {
            RpMorphTarget* morphTarget;
            rpBinaryMorphTarget kf;

            morphTarget = &geometry->morphTarget[i];

            if (RwStreamRead(stream, &kf, sizeof(kf)) != sizeof(kf)) {
                RpGeometryDestroy(geometry);
                return NULL;
            }
            RwMemNative32(&kf, sizeof(kf));

            morphTarget->boundingSphere = kf.boundingSphere;

            if (kf.pointsPresent && kf.normalsPresent) {
                /* The positions and the normals follow each other, in the stream as in memory. */
                if (!RwStreamReadReal(stream, (RwReal*)morphTarget->verts, geometry->numVertices * sizeof(RwV3d) * 2)) {
                    RpGeometryDestroy(geometry);
                    return NULL;
                }
            } else {
                if (kf.pointsPresent &&
                    !RwStreamReadReal(stream, (RwReal*)morphTarget->verts, geometry->numVertices * sizeof(RwV3d))) {
                    RpGeometryDestroy(geometry);
                    return NULL;
                }
                if (kf.normalsPresent &&
                    !RwStreamReadReal(stream, (RwReal*)morphTarget->normals, geometry->numVertices * sizeof(RwV3d))) {
                    RpGeometryDestroy(geometry);
                    return NULL;
                }
            }
        }
    }

    if (!RwStreamFindChunk(stream, rwID_MATLIST, NULL, &version)) {
        return NULL;
    }

    if (!(version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION)) {
        RpGeometryDestroy(geometry);
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    if (_rpMaterialListStreamRead(stream, &geometry->matList) == NULL) {
        RpGeometryDestroy(geometry);
        return NULL;
    }

    if (_rwPluginRegistryReadDataChunks(&geometryTKList, stream, geometry) == NULL) {
        RpGeometryDestroy(geometry);
        return NULL;
    }

    if (RpGeometryUnlock(geometry) == NULL) {
        RpGeometryDestroy(geometry);
        return NULL;
    }

    return geometry;
} // 0x802D27C4
