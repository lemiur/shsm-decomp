// Layout and names: Gormiti DWARF (RpSkin2.a bsplit.obj) and link map
// Header name invented: Gormiti's skin2 headers are not recorded in its DWARF.
#ifndef RWSDK_PLUGIN_SKIN2_RPSKIN_H
#define RWSDK_PLUGIN_SKIN2_RPSKIN_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/badevice.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/vtxfmt.h>
#include <rwsdk/plugin/hanim/rphanim.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpSkin2: skinning. A skin ties a geometry's vertices to a skeleton: each vertex has up to four
 * bone weights and indices, and the bones' inverse matrices take a vertex from the bone's space
 * to the skin's.
 */

/* The plugin's ID, in the memory hints of its allocations and in its stream chunks. */
#define rwID_SKINPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x16)

/* The weights of one vertex's four bones. */
typedef struct RwMatrixWeights {
    float w0; /* 0x00 */
    float w1; /* 0x04 */
    float w2; /* 0x08 */
    float w3; /* 0x0C */
} RwMatrixWeights;

/* The skeleton: its bones, the bones a skin uses, and their inverse matrices. */
typedef struct SkinBoneData {
    unsigned int numBones;           /* 0x00 */
    unsigned int numUsedBones;       /* 0x04 */
    unsigned char* usedBoneList;     /* 0x08 */
    RwMatrix* invBoneToSkinMat;      /* 0x0C */
} SkinBoneData;

/* The weights and bone indices of the geometry's vertices. */
typedef struct SkinVertexMaps {
    unsigned int maxWeights;         /* 0x00 */
    unsigned int* matrixIndices;     /* 0x04 */
    RwMatrixWeights* matrixWeights;  /* 0x08 */
} SkinVertexMaps;

/* The platform's copy of the skin (GameCube: the vertex data in its display-list format). */
typedef struct SkinPlatformData {
    void* vertices;                  /* 0x00 */
    void* normals;                   /* 0x04 */
    unsigned char* weights;          /* 0x08 */
    unsigned char* indices;          /* 0x0C */
    unsigned int skinType;           /* 0x10 */
} SkinPlatformData;

/*
 * How a skin's meshes are split so that none uses more bones than the platform can hold: one
 * block holds the remap from each mesh's bones to the skin's, then each mesh's run-length counts
 * and then the runs.
 */
typedef struct SkinSplitData {
    unsigned int boneLimit;              /* 0x00 */
    unsigned int numMeshes;              /* 0x04 */
    unsigned int numRLE;                 /* 0x08 */
    unsigned char* matrixRemapIndices;   /* 0x0C: the block's start, one byte per bone */
    unsigned char* meshRLECount;         /* 0x10 */
    unsigned char* meshRLE;              /* 0x14 */
} SkinSplitData;

/* The plugin's extension of a geometry. */
typedef struct RpSkin {
    SkinBoneData boneData;               /* 0x00 */
    SkinVertexMaps vertexMaps;           /* 0x10 */
    SkinPlatformData platformData;       /* 0x1C */
    SkinSplitData skinSplitData;         /* 0x30 */
    void** unaligned;                    /* 0x48 */
} RpSkin;

/* The skin matrices of the frame being skinned (Gormiti DWARF: rescache.c). */
typedef struct SkinMatrixCache {
    RwMatrix* aligned;                   /* 0x00 */
    void* unaligned;                     /* 0x04 */
} SkinMatrixCache;

/* The pipelines of the platform's skinning (Gormiti DWARF: rescache.c). */
typedef struct SkinGlobalPlatform {
    RxPipeline* pipelines[5];            /* 0x00 */
} SkinGlobalPlatform;

/* The plugin's globals (Gormiti DWARF: skingcn.c): where its extensions lie and what it allocates from. */
typedef struct SkinGlobals {
    int engineOffset;                    /* 0x00 */
    int atomicOffset;                    /* 0x04 */
    int geometryOffset;                  /* 0x08: the geometry's RpSkin* */
    SkinMatrixCache matrixCache;         /* 0x0C */
    unsigned int numMatrices;            /* 0x14 */
    RwFreeList* freeList;                /* 0x18: of RpSkin */
    RwModuleInfo module;                 /* 0x1C */
    SkinGlobalPlatform platform;         /* 0x24 */
    SkinSplitData* skinSplitData;        /* 0x38 */
} SkinGlobals;

extern SkinGlobals _rpSkinGlobals;

/* The platform's part of an atomic's skin data (Gormiti DWARF: rpskin.c). */
typedef struct SkinAtomicPlatformData {
    void* vertices;                      /* 0x00 */
    void* normals;                       /* 0x04 */
    unsigned int lastUpdatedFrameID;     /* 0x08 */
    RwResEntry* skinResEntry;            /* 0x0C: the skinned vertex data, cached (rescache.c) */
} SkinAtomicPlatformData;

/* The plugin's extension of an atomic (Gormiti DWARF: rpskin.c). */
typedef struct SkinAtomicData {
    void* hierarchy;                     /* 0x00: the atomic's RpHAnimHierarchy */
    SkinAtomicPlatformData platformData; /* 0x04 */
} SkinAtomicData;

/* One skinned atomic's resource entry in the skin cache (Gormiti DWARF: rescache.c). */
typedef struct SkinResCacheEntry {
    struct SkinResCacheEntry* Next;      /* 0x00 */
    RpAtomic* Atomic;                    /* 0x04 */
    RpGeometry* Geometry;                /* 0x08 */
    RwResEntry* skinResEntry;            /* 0x0C */
    RxGameCubeVertexBuffer* VBOwner;     /* 0x10 */
    unsigned int lastFrameID;            /* 0x14: the frame the entry was last used in */
} SkinResCacheEntry;

/* rescache.c: the cache of the skinned vertex data, one resource entry per skinned atomic. */
extern void _clSkinCache_UnRegisterSkinRes(RwResEntry* SkinRes);
extern void _clSkinCache_RepEntryDestroyNotify(RwResEntry* RepEntry);
extern void _clSkinCache_OrphanSkinRes(RwResEntry* SkinRes);
extern RwResEntry* _clSkinCache_GetUsableResource(RpAtomic* Atomic, RwResEntry** ownerRef, int size,
                                                  RwResEntryDestroyNotify destroyNotify,
                                                  RxGameCubeVertexBuffer* vbHeader);
extern int _clSkinCache_CheckResource(RpAtomic* Atomic, RwResEntry* SkinRes, RxGameCubeVertexBuffer* vbHeader);
extern void _clSkinCache_TokenStampResource(RxGameCubeVertexBuffer* vbHeader);
extern RwResEntry* _clSkinCache_ReAquireResource(RpAtomic* Atomic, RwResEntry* SkinRes,
                                                 RxGameCubeVertexBuffer* vbHeader);
extern void _clSkinCache_AtomicDestruct(RpAtomic* Atomic);

/* The pipeline a skin is drawn through (Gormiti DWARF: rpskin.c). */
typedef enum RpSkinType {
    rpNASKINTYPE = 0,
    rpSKINTYPEGENERIC = 1, /* the generic pipeline */
    rpSKINTYPEMATFX = 2,   /* with material effects */
    rpSKINTYPETOON = 3,    /* with toon shading */
    rpSKINTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RpSkinType;

/* rpskin.c: the plugin, and the skin of a geometry. */
extern RwBool RpSkinPluginAttach(void);
extern RpGeometry* RpSkinGeometrySetSkin(RpGeometry* geometry, RpSkin* skin);

/* skinstream.c: the native stream of a skin. */
extern int _rpSkinGeometryNativeSize(RpGeometry* geometry);
extern RwStream* _rpSkinGeometryNativeWrite(RwStream* stream, RpGeometry* geometry);
extern RwStream* _rpSkinGeometryNativeRead(RwStream* stream, RpGeometry* geometry);

/* rpskin.c: the skin of an atomic and of a geometry, and what a skin holds. */
extern RpAtomic* RpSkinAtomicSetHAnimHierarchy(RpAtomic* atomic, RpHAnimHierarchy* hierarchy);
extern RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic);
extern RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry);
extern unsigned int RpSkinGetNumBones(RpSkin* skin);
extern RwMatrixWeights* RpSkinGetVertexBoneWeights(RpSkin* skin);
extern unsigned int* RpSkinGetVertexBoneIndices(RpSkin* skin);
extern RwMatrix* RpSkinGetSkinToBoneMatrices(RpSkin* skin);

/* skingcng.c: the platform's pipelines (names: Gormiti; this build's skingcng.c is another revision than Gormiti's). */
extern void _rpSkinPipelinesCreate(unsigned int pipes);
extern void _rpSkinPipelinesDestroy(void);
extern RpAtomic* _rpSkinPipelinesAttach(RpAtomic* atomic, RpSkinType type);

/* skingcn.c: the platform data of a skinned geometry, vertex/matrix blending, and pipeline callbacks. */
extern RpGeometry* _rpSkinInitialize(RpGeometry* geometry);
extern RpGeometry* _rpSkinDeinitialize(RpGeometry* geometry);
extern int _rpSkinVertexBuffersUpdate(RpSkin* skin, RpAtomic* atomic, RxGameCubeVertexBuffer* vbHeader,
                                      RxGameCubePipeData* pipeData);
extern void _rpSkinMatrixBlendUpdate(RwMatrix* matDst, RpSkin* skin, RwMatrix* ltm,
                                     RpHAnimHierarchy* hierarchy);
extern void _rpSkinBlendBodyP(RpSkin* skin, RwMatrix* matrixCache, void* srcVertices, void* dstVertices,
                              RpGameCubeVtxFmt* vtxFmt, int numVertices);
extern void _rpSkinBlendBodyPN(RpSkin* skin, RwMatrix* matrixCache, void* srcVertices, void* srcNormals,
                               void* dstVertices, void* dstNormals, RpGameCubeVtxFmt* vtxFmt,
                               int numVertices);
extern void* _rpSkinInstanceCallback(void* object, RxGameCubePipeData* pipeData);
extern void* _rpSkinAtomicReinstanceCallBack(void* object, RxGameCubePipeData* pipeData);
extern void _rpSkinLoadMatrix(RwMatrix* matrix, unsigned int index, int normals);
extern void _rpSkinLoadMatrixPalette(RpSkin* skin, unsigned int mesh, int normals);
extern void* _rpSkinRenderCallback(void* object, RxGameCubePipeData* pipeData);

/* instanceskin.c: instances a skinned geometry into a display list resource entry. */
extern RwResEntry* _rwDlGeometrySkinInstanceOptimized(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner);
extern RwResEntry* _rwDlGeometrySkinInstanceFast(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner);

/* bsplit.c: the split data of a skin. */
extern unsigned char* _rpSkinGetMeshBoneRemapIndices(const RpSkin* skin);
extern unsigned char* _rpSkinGetMeshBoneRLECount(const RpSkin* skin);
extern unsigned char* _rpSkinGetMeshBoneRLE(const RpSkin* skin);
extern RpSkin* _rpSkinSplitDataCreate(RpSkin* skin, unsigned int boneLimit,
                                      unsigned int numMatrices, unsigned int numMeshes,
                                      unsigned int numRLE);
extern int _rpSkinSplitDataDestroy(RpSkin* skin);
extern RwStream* _rpSkinSplitDataStreamWrite(RwStream* stream, const RpSkin* skin);
extern RwStream* _rpSkinSplitDataStreamRead(RwStream* stream, RpSkin* skin);
extern int _rpSkinSplitDataStreamGetSize(const RpSkin* skin);

/*
 * The job of one skinning loop (skingcnasm.c): the source and destination arrays and their strides.
 * posSize and nrmSize are the bytes of one component (x, y or z) of a position or normal.
 */
typedef struct SkinAsmData {
    void* dstVertices;                   /* 0x00 */
    void* dstNormals;                    /* 0x04 */
    void* srcVertices;                   /* 0x08 */
    void* srcNormals;                    /* 0x0C */
    int posSize;                         /* 0x10 */
    int nrmSize;                         /* 0x14 */
    int nrmSkip;                         /* 0x18: bytes after each normal, in both arrays */
    int numVerts;                        /* 0x1C */
} SkinAsmData;

/* skingcnasm.c: the skinning loops (assembly), by weights per vertex; P positions, PN positions and normals. */
extern void _rwDlSkinUpdate2WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);
extern void _rwDlSkinUpdate2WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);
extern void _rwDlSkinUpdate3WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);
extern void _rwDlSkinUpdate3WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);
extern void _rwDlSkinUpdate4WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);
extern void _rwDlSkinUpdate4WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data);

/* skinmatrixblend.c: the matrix palette blend (assembly). */
extern void _rpSkinMatrixBlendUpdateASM(RwMatrix* dstMat, const RwMatrix* matA, const RwMatrix* matB,
                                        const RwMatrix* matC, const unsigned char* usedBoneList,
                                        unsigned int numUsedBones);

#ifdef __cplusplus
}
#endif

#endif
