// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: electronicarts/RenderWare3Docs whitepapers/worlds.pdf (RpSector, RpPlaneSector, rwSECTORATOMIC)
#ifndef RWSDK_WORLD_BASECTOR_H
#define RWSDK_WORLD_BASECTOR_H

#include <rwsdk/babbox.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamesh.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The type of a world sector (a leaf of the world's BSP tree); a plane sector's type is
 * non-negative, the byte offset of its plane's axis in an RwV3d (the BSP trees white paper).
 */
#define rwSECTORATOMIC (-1)

/* The deepest a world's BSP tree goes (the BSP trees white paper). */
#define rpWORLDMAXBSPDEPTH 64

/* RpVertexNormal: a vertex normal packed into signed bytes. */
typedef struct RpVertexNormal {
    RwInt8 x;       /* 0x0 */
    RwInt8 y;       /* 0x1 */
    RwInt8 z;       /* 0x2 */
    RwUInt8 pad;    /* 0x3 */ // pad: bfbbdecomp/bfbb rpworld.h RpVertexNormal member name
} RpVertexNormal;

/*
 * RpWorldSector: a leaf of the world's BSP tree: its static geometry (triangles, vertices,
 * normals, texture coordinates, pre-lit colours), the instanced copy, the atomics and lights in
 * it, its bounding boxes, its meshes and the pipeline that renders it.
 * Size 0x88, from retail's sector plugin registry; bfbbdecomp/bfbb's GameCube RpWorldSector is 0x90
 * with a second atomic list (noCollAtomicsInWorldSector, 0x40) that this build does not have:
 * WorldSectorDeinstanceAll walks the atomic ties from 0x38 and the light ties from 0x40, and frees
 * the meshes at 0x78.
 */
typedef struct RpWorldSector {
    RwInt32 type;                           /* 0x0 */
    RpTriangle* triangles;                  /* 0x4: the material index last (RpTriangle, bageomet.h) */
    RwV3d* vertices;                        /* 0x8 */
    RpVertexNormal* normals;                /* 0xC */
    RwTexCoords* texCoords[8];              /* 0x10 */
    RwRGBA* preLitLum;                      /* 0x30 */
    RwResEntry* repEntry;                   /* 0x34 */
    RwLinkList collAtomicsInWorldSector;    /* 0x38 */
    RwLinkList lightsInWorldSector;         /* 0x40 */
    RwBBox boundingBox;                     /* 0x48 */
    RwBBox tightBoundingBox;                /* 0x60 */
    RpMeshHeader* mesh;                     /* 0x78 */
    RxPipeline* pipeline;                   /* 0x7C */
    RwUInt16 matListWindowBase;             /* 0x80 */
    RwUInt16 numVertices;                   /* 0x82 */
    RwUInt16 numTriangles;                  /* 0x84 */
    RwUInt16 pad;                           /* 0x86 */ // pad: bfbbdecomp/bfbb rpworld.h RpWorldSector member name
} RpWorldSector;

/* Called for each world sector of an iteration; NULL stops it. */
typedef RpWorldSector* (*RpWorldSectorCallBack)(RpWorldSector* worldSector, void* data);

/* RpSector: a node of the world's BSP tree, a world sector or a plane sector by its type. */
typedef struct RpSector {
    RwInt32 type;   /* 0x0 */
} RpSector;

/*
 * RpPlaneSector: an inner node of the world's BSP tree: the plane cutting it (its axis in type,
 * its offset in value), the two subtrees, and the plane values at the extents of the two sides
 * (which differ where sectors overlap). Field names from the BSP trees white paper.
 */
typedef struct RpPlaneSector {
    RwInt32 type;               /* 0x0 */
    RwReal value;               /* 0x4 */
    RpSector* leftSubTree;      /* 0x8 */
    RpSector* rightSubTree;     /* 0xC */
    RwReal leftValue;           /* 0x10 */
    RwReal rightValue;          /* 0x14 */
} RpPlaneSector;

/* The world sector plugin registry (bfbbdecomp/bfbb symbols). */
extern RwPluginRegistry sectorTKList;

void* _rpSectorOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rpSectorClose(void* instance, RwInt32 offset, RwInt32 size);
RpWorldSector* RpWorldSectorRender(RpWorldSector* worldSector);
RwInt32 RpWorldSectorRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                    RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpWorldSectorRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                          RwPluginDataChunkWriteCallBack writeCB,
                                          RwPluginDataChunkGetSizeCallBack getSizeCB);

#ifdef __cplusplus
}
#endif

#endif
