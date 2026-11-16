// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: tgsm/pbg include/rwsdk/world/bageomet.h
#ifndef RWSDK_WORLD_BAGEOMET_H
#define RWSDK_WORLD_BAGEOMET_H

#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/bamatlst.h>
#include <rwsdk/world/bamesh.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a geometry. */
#define rpGEOMETRY 8

/* RpTriangle: a geometry triangle, its three vertices and its material's index in the geometry's list. */
typedef struct RpTriangle {
    RwUInt16 vertIndex[3];  /* 0x0 */
    RwUInt16 matIndex;      /* 0x6 */
} RpTriangle;

typedef struct RpGeometry RpGeometry;

/* RpMorphTarget: one key shape of a geometry: its vertex positions and normals, and their bounds. */
typedef struct RpMorphTarget {
    RpGeometry* parentGeom;     /* 0x0 */
    RwSphere boundingSphere;    /* 0x4 */
    RwV3d* verts;               /* 0x14 */
    RwV3d* normals;             /* 0x18 */
} RpMorphTarget;

/* RpGeometryFlag: what a geometry's vertices carry and how it is rendered. */
typedef enum RpGeometryFlag {
    rpGEOMETRYTRISTRIP = 0x0001,
    rpGEOMETRYPOSITIONS = 0x0002,
    rpGEOMETRYTEXTURED = 0x0004,
    rpGEOMETRYPRELIT = 0x0008,
    rpGEOMETRYNORMALS = 0x0010,
    rpGEOMETRYLIGHT = 0x0020,
    rpGEOMETRYMODULATEMATERIALCOLOR = 0x0040,
    rpGEOMETRYTEXTURED2 = 0x0080,
    rpGEOMETRYNATIVE = 0x01000000,
    rpGEOMETRYNATIVEINSTANCE = 0x02000000,
    rpGEOMETRYFLAGSMASK = 0x00FF,
    rpGEOMETRYNATIVEFLAGSMASK = 0x0F000000,
    rpGEOMETRYFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpGeometryFlag;

/* The number of texture coordinate sets, in the third byte of a geometry format (RW 3.7 API reference). */
#define rpGEOMETRYTEXCOORDSETS(_num) (((_num) & 0xFF) << 16)
/* Name invented: the format bits rpGEOMETRYTEXCOORDSETS fills. */
#define rpGEOMETRYTEXCOORDSETSMASK 0x00FF0000

/* RpGeometryLockMode: which of a geometry's arrays a lock lets the caller change (RW 3.7 API reference). */
typedef enum RpGeometryLockMode {
    rpGEOMETRYLOCKPOLYGONS = 0x0001,
    rpGEOMETRYLOCKVERTICES = 0x0002,
    rpGEOMETRYLOCKNORMALS = 0x0004,
    rpGEOMETRYLOCKPRELIGHT = 0x0008,
    rpGEOMETRYLOCKTEXCOORDS = 0x0010,
    rpGEOMETRYLOCKTEXCOORDS1 = 0x0010,
    rpGEOMETRYLOCKTEXCOORDS2 = 0x0020,
    rpGEOMETRYLOCKTEXCOORDS3 = 0x0040,
    rpGEOMETRYLOCKTEXCOORDS4 = 0x0080,
    rpGEOMETRYLOCKTEXCOORDS5 = 0x0100,
    rpGEOMETRYLOCKTEXCOORDS6 = 0x0200,
    rpGEOMETRYLOCKTEXCOORDS7 = 0x0400,
    rpGEOMETRYLOCKTEXCOORDS8 = 0x0800,
    rpGEOMETRYLOCKTEXCOORDSALL = 0x0FF0,
    rpGEOMETRYLOCKALL = 0x0FFF,
    rpGEOMETRYLOCKMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RpGeometryLockMode;

/*
 * RpGeometry: the shape atomics share: triangles, materials, texture coordinate sets, pre-lit
 * colours, the morph targets holding positions and normals, the meshes built from the triangles,
 * and the instanced copy the pipeline renders. Size 0x60, from retail's geometry plugin registry.
 */
struct RpGeometry {
    RwObject object;                /* 0x0 */
    RwUInt32 flags;                 /* 0x8 */
    RwUInt16 lockedSinceLastInst;   /* 0xC */
    RwInt16 refCount;               /* 0xE */
    RwInt32 numTriangles;           /* 0x10 */
    RwInt32 numVertices;            /* 0x14 */
    RwInt32 numMorphTargets;        /* 0x18 */
    RwInt32 numTexCoordSets;        /* 0x1C */
    RpMaterialList matList;         /* 0x20 */
    RpTriangle* triangles;          /* 0x2C */
    RwRGBA* preLitLum;              /* 0x30 */
    RwTexCoords* texCoords[8];      /* 0x34 */
    RpMeshHeader* mesh;             /* 0x54 */
    RwResEntry* repEntry;           /* 0x58 */
    RpMorphTarget* morphTarget;     /* 0x5C */
};

/* 1-based texture coordinate set index (RW 3.7 API reference). */
typedef enum RwTextureCoordinateIndex {
    rwNARWTEXTURECOORDINATEINDEX = 0,
    rwTEXTURECOORDINATEINDEX0 = 1,
    rwTEXTURECOORDINATEINDEX1,
    rwTEXTURECOORDINATEINDEX2,
    rwTEXTURECOORDINATEINDEX3,
    rwTEXTURECOORDINATEINDEX4,
    rwTEXTURECOORDINATEINDEX5,
    rwTEXTURECOORDINATEINDEX6,
    rwTEXTURECOORDINATEINDEX7,
    rwTEXTURECOORDINATEINDEXFORCEENUMSIZEINT = 0x7FFFFFFF
} RwTextureCoordinateIndex;

/* Returns the texture coordinate array for 1-based texture coordinate set uvIndex. */
#define RpGeometryGetVertexTexCoords(_geometry, _uvIndex) \
    ((_geometry)->texCoords[((RwUInt32)(_uvIndex)) - 1])

/*
 * RpGeometryChunkInfo: a geometry's structure chunk header: its flags (with the texture
 * coordinate set count and the native flags) and its triangle, vertex and morph target counts.
 */
typedef struct RpGeometryChunkInfo {
    RwInt32 format;             /* 0x0 */
    RwInt32 numTriangles;       /* 0x4 */
    RwInt32 numVertices;        /* 0x8 */
    RwInt32 numMorphTargets;    /* 0xC */
} RpGeometryChunkInfo;

/* rpGeometryGlobals: the geometry module's globals (the size the world attach registers). */
typedef struct rpGeometryGlobals {
    RwFreeList* geomFreeList; /* 0x0 */
} rpGeometryGlobals;

void* _rpGeometryOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rpGeometryClose(void* instance, RwInt32 offset, RwInt32 size);
RwInt32 RpGeometryAddMorphTargets(RpGeometry* geometry, RwInt32 mtcount);
RpGeometry* RpGeometryTriangleSetMaterial(RpGeometry* geometry, RpTriangle* triangle, RpMaterial* material);
RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* pData);
RpGeometry* RpGeometryLock(RpGeometry* geometry, RwInt32 lockMode);
RpGeometry* RpGeometryUnlock(RpGeometry* geometry);
const RpGeometry* RpGeometryForAllMeshes(const RpGeometry* geometry, RpMeshCallBack fpCallBack, void* pData);
RpGeometry* RpGeometryCreate(RwInt32 numVert, RwInt32 numTriangles, RwUInt32 format);
RpGeometry* _rpGeometryAddRef(RpGeometry* geometry);
RwBool RpGeometryDestroy(RpGeometry* geometry);
RwInt32 RpGeometryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                 RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpGeometryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                       RwPluginDataChunkWriteCallBack writeCB,
                                       RwPluginDataChunkGetSizeCallBack getSizeCB);
RwInt32 RpGeometryGetPluginOffset(RwUInt32 pluginID);
RpGeometry* RpGeometryStreamRead(RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
