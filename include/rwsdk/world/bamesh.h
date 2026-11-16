// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: electronicarts/RenderWare3Docs (RpMeshHeaderFlags)
#ifndef RWSDK_WORLD_BAMESH_H
#define RWSDK_WORLD_BAMESH_H

#include <rwsdk/badevice.h>
#include <rwsdk/world/bamatlst.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpBuildMeshTriangle: a triangle collected while meshes are built, with its material and the
 * indices the mesh sort orders by. Stride 0x14, from _rpBuildMeshAddTriangle.
 */
typedef struct RpBuildMeshTriangle {
    RwUInt16 vertIndex[3];      /* 0x0 */
    RpMaterial* material;       /* 0x8 */
    RwUInt16 matIndex;          /* 0xC */
    RwUInt16 textureIndex;      /* 0xE */
    RwUInt16 rasterIndex;       /* 0x10 */
    RwUInt16 pipelineIndex;     /* 0x12 */
} RpBuildMeshTriangle;

/* RpBuildMesh: the triangles collected for a mesh build, and the room allocated for them. */
typedef struct RpBuildMesh {
    RwUInt32 triangleBufferSize;            /* 0x0 */
    RwUInt32 numTriangles;                  /* 0x4 */
    RpBuildMeshTriangle* meshTriangles;     /* 0x8 */
} RpBuildMesh;

/* RpMesh: the indices of the triangles (or strips) of one material. */
typedef struct RpMesh {
    RxVertexIndex* indices;     /* 0x0 */
    RwUInt32 numIndices;        /* 0x4 */
    RpMaterial* material;       /* 0x8 */
} RpMesh;

/* RpMeshHeader: a geometry's or world sector's meshes, which follow it in memory. */
typedef struct RpMeshHeader {
    RwUInt32 flags;                 /* 0x0 */
    RwUInt16 numMeshes;             /* 0x4 */
    RwUInt16 serialNum;             /* 0x6 */
    RwUInt32 totalIndicesInMesh;    /* 0x8 */
    RwUInt32 firstMeshOffset;       /* 0xC */
} RpMeshHeader;

/*
 * RpMeshHeaderFlags: the primitive a mesh header's indices form (none set: a triangle list), and
 * whether the meshes carry indices at all. rpMESHHEADERPRIMTYPE7 is the flag of the seventh
 * primitive type this build's lookup tables carry beyond the six of the RW 3.7 API (name invented).
 */
typedef enum RpMeshHeaderFlags {
    rpMESHHEADERTRISTRIP = 0x0001,
    rpMESHHEADERTRIFAN = 0x0002,
    rpMESHHEADERLINELIST = 0x0004,
    rpMESHHEADERPOLYLINE = 0x0008,
    rpMESHHEADERPOINTLIST = 0x0010,
    rpMESHHEADERPRIMTYPE7 = 0x0020,
    rpMESHHEADERPRIMMASK = 0x00FF,
    rpMESHHEADERUNINDEXED = 0x0100,
    rpMESHHEADERFLAGSFORCEENUMSIZEINT = 0x7FFFFFFF
} RpMeshHeaderFlags;

/* The seventh primitive type, which rpMESHHEADERPRIMTYPE7 selects (name invented). */
#define rwPRIMTYPE7 7

/*
 * rpMeshGlobals: the mesh module's globals: the serial number the next mesh header gets, the free
 * list of bameshop's tri-strip entries, and the tables that turn a mesh header's primitive flags
 * into a primitive type and back (each indexed by the other's values). Size 0x50, the size retail
 * registers the module with. The SDK sizes the tables by rpMESHHEADERPRIMTYPEOR and rwPRIMTYPEOR,
 * the ORs of the flags and types; with this build's seventh type the first OR is 0x3F, and the
 * second table has room for type 7 (eight entries, as retail's offsets show).
 */
typedef struct rpMeshGlobals {
    RwInt16 nextSerialNum;                  /* 0x0 */
    RwFreeList* triStripListEntryFreeList;  /* 0x4 */
    RwUInt8 meshFlagsToPrimType[0x3F];      /* 0x8 */
    RwUInt8 primTypeToMeshFlags[8];         /* 0x47 */
} rpMeshGlobals;

/* The mesh module's registration (offset of its globals in the engine instance). */
extern RwModuleInfo meshModule;

#define RPMESHGLOBAL(var)     (RWPLUGINOFFSET(rpMeshGlobals, RwEngineInstance, meshModule.globalsOffset)->var)

/* Called for each mesh of an iteration; NULL stops it. */
typedef RpMesh* (*RpMeshCallBack)(RpMesh* mesh, RpMeshHeader* meshHeader, void* pData);

RpMeshHeader* _rpMeshHeaderCreate(RwUInt32 size);
void* _rpMeshClose(void* instance, RwInt32 offset, RwInt32 size);
void* _rpMeshOpen(void* instance, RwInt32 offset, RwInt32 size);
RpBuildMesh* _rpBuildMeshCreate(RwUInt32 bufferSize);
RwBool _rpBuildMeshDestroy(RpBuildMesh* mesh);
RwBool _rpMeshDestroy(RpMeshHeader* mesh);
RpBuildMesh* _rpBuildMeshAddTriangle(RpBuildMesh* mesh, RpMaterial* material, RwInt32 vert1, RwInt32 vert2,
                                     RwInt32 vert3, RwUInt16 matIndex, RwUInt16 textureIndex,
                                     RwUInt16 rasterIndex, RwUInt16 pipelineIndex);
RpMeshHeader* _rpMeshHeaderForAllMeshes(RpMeshHeader* meshHeader, RpMeshCallBack fpCallBack, void* pData);
RwStream* _rpMeshWrite(const RpMeshHeader* meshHeader, const void* object, RwStream* stream,
                       const RpMaterialList* matList);
RpMeshHeader* _rpMeshRead(RwStream* stream, const void* object, const RpMaterialList* matList);
RwInt32 _rpMeshSize(const RpMeshHeader* meshHeader, const void* object);

#ifdef __cplusplus
}
#endif

#endif
