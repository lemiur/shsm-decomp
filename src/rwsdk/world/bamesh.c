// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h (function names and signatures)
// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (meshModule)
/*
 * bamesh.c: mesh headers and build meshes. A mesh header holds a geometry's or world sector's
 * triangles grouped by material, one RpMesh per material followed by the indices; a build mesh
 * collects triangles before bameshop sorts them into meshes. The module also streams mesh
 * headers and owns the tables that map mesh header flags to primitive types.
 */
#include <rwsdk/world/rpplugin.h>

#include <rwsdk/world/bamesh.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/baworld.h>

/* Build meshes per free-list block. */
#define rpBUILDMESHFREELISTBLOCKSIZE 50

/* Indices streamed per write or read, through a buffer on the stack. */
#define rpMESHSTREAMINDICES 256

/* Uninitialised .sbss statics are emitted in reverse declaration order. */
RwModuleInfo meshModule;

/* The free list of build meshes. Name invented. */
static RwFreeList* buildMeshFreeList;

/*
 * Whether the object's meshes carry their indices: not when the geometry or world is in its
 * platform's native form (its indices live in the instanced data). Name invented.
 */
static RwBool MeshObjectHasIndices(const void* object) {
    if (RwObjectGetType(object) == rpGEOMETRY && (((const RpGeometry*)object)->flags & rpGEOMETRYNATIVE)) {
        return FALSE;
    }
    if (RwObjectGetType(object) == rpWORLD && (((const RpWorld*)object)->flags & rpWORLDNATIVE)) {
        return FALSE;
    }
    return TRUE;
}

/* Creates the build mesh free list; whether it could. Name invented. */
static RwBool BuildMeshFreeListCreate(void) {
    buildMeshFreeList = RwFreeListCreate(sizeof(RpBuildMesh), rpBUILDMESHFREELISTBLOCKSIZE, sizeof(RwUInt32),
                                         rwMEMHINTDUR_GLOBAL | rwID_MESHMODULE);
    return buildMeshFreeList != NULL;
}

/* Destroys the build mesh free list, if there is one. Name invented. */
static void BuildMeshFreeListDestroy(void) {
    if (buildMeshFreeList != NULL) {
        RwFreeListDestroy(buildMeshFreeList);
        buildMeshFreeList = NULL;
    }
}

/* Allocates a mesh header with room for size bytes in all (meshes and indices included). */
RpMeshHeader* _rpMeshHeaderCreate(RwUInt32 size) {
    return (RpMeshHeader*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
} // 0x802D4E0C

/* Closes the mesh module: the last instance destroys the build mesh free list. */
void* _rpMeshClose(void* instance, RwInt32 offset, RwInt32 size) {
    meshModule.numInstances--;
    if (meshModule.numInstances == 0) {
        BuildMeshFreeListDestroy();
    }

    return instance;
} // 0x802D4E24

/*
 * Opens the mesh module: the first instance creates the build mesh free list; every instance
 * restarts the serial numbers and fills the tables between mesh header flags and primitive types.
 */
void* _rpMeshOpen(void* instance, RwInt32 offset, RwInt32 size) {
    meshModule.globalsOffset = offset;

    if (meshModule.numInstances == 0) {
        if (BuildMeshFreeListCreate() == FALSE) {
            BuildMeshFreeListDestroy();
            return NULL;
        }
    }

    RPMESHGLOBAL(nextSerialNum) = 1;

    meshModule.numInstances++;

    RPMESHGLOBAL(meshFlagsToPrimType)[0] = rwPRIMTYPETRILIST;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERTRISTRIP] = rwPRIMTYPETRISTRIP;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERTRIFAN] = rwPRIMTYPETRIFAN;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERLINELIST] = rwPRIMTYPELINELIST;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERPOLYLINE] = rwPRIMTYPEPOLYLINE;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERPOINTLIST] = rwPRIMTYPEPOINTLIST;
    RPMESHGLOBAL(meshFlagsToPrimType)[rpMESHHEADERPRIMTYPE7] = rwPRIMTYPE7;

    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPELINELIST] = rpMESHHEADERLINELIST;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPEPOLYLINE] = rpMESHHEADERPOLYLINE;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPETRILIST] = 0;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPETRISTRIP] = rpMESHHEADERTRISTRIP;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPETRIFAN] = rpMESHHEADERTRIFAN;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPEPOINTLIST] = rpMESHHEADERPOINTLIST;
    RPMESHGLOBAL(primTypeToMeshFlags)[rwPRIMTYPE7] = rpMESHHEADERPRIMTYPE7;

    return instance;
} // 0x802D4E7C

/* Creates a build mesh with room for bufferSize triangles. */
RpBuildMesh* _rpBuildMeshCreate(RwUInt32 bufferSize) {
    RpBuildMesh* mesh;

    mesh = (RpBuildMesh*)RwFreeListAlloc(buildMeshFreeList, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
    if (mesh != NULL) {
        mesh->numTriangles = 0;
        if (bufferSize != 0) {
            RwUInt32 bytes = bufferSize * sizeof(RpBuildMeshTriangle);

            mesh->meshTriangles = (RpBuildMeshTriangle*)RwMalloc(
                bytes, rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_MESHMODULE);
            if (mesh->meshTriangles == NULL) {
                RwFreeListFree(buildMeshFreeList, mesh);
                RWERROR((E_RW_NOMEM, bytes));
                return NULL;
            }
            mesh->triangleBufferSize = bufferSize;
        } else {
            mesh->meshTriangles = NULL;
            mesh->triangleBufferSize = 0;
        }

        return mesh;
    }

    RWERROR((E_RW_NOMEM, sizeof(RpBuildMesh)));
    return NULL;
} // 0x802D503C

/* Destroys a build mesh and its triangles. */
RwBool _rpBuildMeshDestroy(RpBuildMesh* mesh) {
    if (mesh->meshTriangles != NULL) {
        RwFree(mesh->meshTriangles);
        mesh->meshTriangles = NULL;
    }

    RwFreeListFree(buildMeshFreeList, mesh);

    return TRUE;
} // 0x802D5164

/* Destroys a mesh header, unless every field of it is clear. */
RwBool _rpMeshDestroy(RpMeshHeader* mesh) {
    if (mesh->flags != 0 || mesh->numMeshes != 0 || mesh->serialNum != 0 || mesh->totalIndicesInMesh != 0 ||
        mesh->firstMeshOffset != 0) {
        RwFree(mesh);
    }

    return TRUE;
} // 0x802D51D0

/* Adds a triangle to a build mesh, growing its triangle buffer by one when it is full. */
RpBuildMesh* _rpBuildMeshAddTriangle(RpBuildMesh* mesh, RpMaterial* material, RwInt32 vert1, RwInt32 vert2,
                                     RwInt32 vert3, RwUInt16 matIndex, RwUInt16 textureIndex,
                                     RwUInt16 rasterIndex, RwUInt16 pipelineIndex) {
    if (mesh->numTriangles >= mesh->triangleBufferSize) {
        RpBuildMeshTriangle* newTriangles;
        RwUInt32 bytes = (mesh->numTriangles + 1) * sizeof(RpBuildMeshTriangle);

        if (mesh->numTriangles != 0) {
            newTriangles = (RpBuildMeshTriangle*)RwRealloc(
                mesh->meshTriangles, bytes, rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_MESHMODULE);
        } else {
            newTriangles = (RpBuildMeshTriangle*)RwMalloc(
                bytes, rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_MESHMODULE);
        }
        if (newTriangles == NULL) {
            RWERROR((E_RW_NOMEM, bytes));
            return NULL;
        }

        mesh->meshTriangles = newTriangles;
        mesh->triangleBufferSize = mesh->numTriangles + 1;
    }

    mesh->meshTriangles[mesh->numTriangles].material = material;
    mesh->meshTriangles[mesh->numTriangles].vertIndex[0] = (RwUInt16)vert1;
    mesh->meshTriangles[mesh->numTriangles].vertIndex[1] = (RwUInt16)vert2;
    mesh->meshTriangles[mesh->numTriangles].vertIndex[2] = (RwUInt16)vert3;
    mesh->meshTriangles[mesh->numTriangles].matIndex = matIndex;
    mesh->meshTriangles[mesh->numTriangles].textureIndex = textureIndex;
    mesh->meshTriangles[mesh->numTriangles].rasterIndex = rasterIndex;
    mesh->meshTriangles[mesh->numTriangles].pipelineIndex = pipelineIndex;
    mesh->numTriangles++;

    return mesh;
} // 0x802D523C

/* Calls fpCallBack for each mesh of the header, until one returns NULL. */
RpMeshHeader* _rpMeshHeaderForAllMeshes(RpMeshHeader* meshHeader, RpMeshCallBack fpCallBack, void* pData) {
    RwInt32 numMeshes = meshHeader->numMeshes;
    RpMesh* mesh = (RpMesh*)((RwUInt8*)(meshHeader + 1) + meshHeader->firstMeshOffset);

    while (numMeshes--) {
        if (fpCallBack(mesh, meshHeader, pData) == NULL) {
            return meshHeader;
        }
        mesh++;
    }

    return meshHeader;
} // 0x802D53D8

/*
 * Writes a mesh header: its flags and counts, then per mesh its index count, its material's index
 * in matList and (unless the object is native) its indices, widened to 32 bits.
 */
RwStream* _rpMeshWrite(const RpMeshHeader* meshHeader, const void* object, RwStream* stream,
                       const RpMaterialList* matList) {
    RwInt32 meshData[2];
    RwInt32 meshHeaderData[3];
    RwInt32 indices[rpMESHSTREAMINDICES];
    RwInt32 numMeshes;
    const RpMesh* mesh;

    meshHeaderData[0] = meshHeader->flags;
    meshHeaderData[1] = meshHeader->numMeshes;
    meshHeaderData[2] = meshHeader->totalIndicesInMesh;
    if (!RwStreamWriteInt32(stream, meshHeaderData, sizeof(meshHeaderData))) {
        return NULL;
    }

    mesh = (const RpMesh*)(meshHeader + 1);
    numMeshes = meshHeader->numMeshes;
    while (numMeshes--) {
        meshData[0] = mesh->numIndices;
        meshData[1] = _rpMaterialListFindMaterialIndex(matList, mesh->material);
        if (meshData[1] < 0) {
            meshData[1] = 0;
        }
        if (!RwStreamWriteInt32(stream, meshData, sizeof(meshData))) {
            return NULL;
        }

        if (MeshObjectHasIndices(object)) {
            RwUInt32 remaining = mesh->numIndices;
            const RxVertexIndex* src = mesh->indices;

            while (remaining != 0) {
                RwUInt32 count = (remaining < rpMESHSTREAMINDICES) ? remaining : rpMESHSTREAMINDICES;
                RwUInt32 i;

                for (i = 0; i < count; i++) {
                    indices[i] = *src++;
                }
                if (!RwStreamWriteInt32(stream, indices, count * sizeof(RwInt32))) {
                    return NULL;
                }
                remaining -= count;
            }
        }

        mesh++;
    }

    return stream;
} // 0x802D5458

/*
 * Reads a mesh header that _rpMeshWrite wrote, giving it the next serial number and taking each
 * mesh's material from matList. Room is allocated for each mesh as 16 bytes (4 more than an
 * RpMesh, which nothing uses: the original's sizing).
 */
RpMeshHeader* _rpMeshRead(RwStream* stream, const void* object, const RpMaterialList* matList) {
    RwInt32 meshData[2];
    RwInt32 meshHeaderData[3];
    RwInt32 indices[rpMESHSTREAMINDICES];
    RwUInt32 size;
    RpMeshHeader* meshHeader;

    if (!RwStreamReadInt32(stream, meshHeaderData, sizeof(meshHeaderData))) {
        return NULL;
    }

    size = sizeof(RpMeshHeader) + meshHeaderData[1] * (sizeof(RpMesh) + sizeof(RwUInt32));
    if (MeshObjectHasIndices(object)) {
        size += meshHeaderData[2] * sizeof(RxVertexIndex);
    }

    meshHeader = _rpMeshHeaderCreate(size);
    if (meshHeader != NULL) {
        RpMesh* mesh = (RpMesh*)(meshHeader + 1);
        RxVertexIndex* meshIndices = (RxVertexIndex*)(mesh + meshHeaderData[1]);
        RwInt32 numMeshes;

        meshHeader->flags = meshHeaderData[0];
        meshHeader->numMeshes = (RwUInt16)meshHeaderData[1];
        meshHeader->serialNum = RPMESHGLOBAL(nextSerialNum);
        meshHeader->totalIndicesInMesh = meshHeaderData[2];
        meshHeader->firstMeshOffset = 0;
        RPMESHGLOBAL(nextSerialNum)++;

        numMeshes = meshHeader->numMeshes;
        while (numMeshes--) {
            if (!RwStreamReadInt32(stream, meshData, sizeof(meshData))) {
                return NULL;
            }

            mesh->numIndices = meshData[0];
            mesh->material = _rpMaterialListGetMaterial(matList, meshData[1]);
            mesh->indices = meshIndices;

            if (MeshObjectHasIndices(object)) {
                RwUInt32 remaining = mesh->numIndices;

                while (remaining != 0) {
                    RwInt32* src = indices;
                    RwUInt32 count = (remaining < rpMESHSTREAMINDICES) ? remaining : rpMESHSTREAMINDICES;

                    if (!RwStreamReadInt32(stream, src, count * sizeof(RwInt32))) {
                        return NULL;
                    }
                    remaining -= count;

                    while (count--) {
                        *meshIndices++ = (RxVertexIndex)*src++;
                    }
                }
            }

            mesh++;
        }
    }

    return meshHeader;
} // 0x802D5670

/* The size _rpMeshWrite writes for a mesh header. */
RwInt32 _rpMeshSize(const RpMeshHeader* meshHeader, const void* object) {
    if (MeshObjectHasIndices(object)) {
        return sizeof(RwInt32) * 3 + sizeof(RwInt32) * 2 * meshHeader->numMeshes +
               sizeof(RwInt32) * meshHeader->totalIndicesInMesh;
    }

    return sizeof(RwInt32) * 3 + sizeof(RwInt32) * 2 * meshHeader->numMeshes;
} // 0x802D58FC
