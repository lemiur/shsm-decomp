// Name: Gormiti link map (RPworld.a gcpipe; RxGameCubeVertexBuffer, RxGameCubeVertexAttr, _RwDlTokenCurrent: Gormiti DWARF)
#ifndef RWSDK_WORLD_GCPIPE_H
#define RWSDK_WORLD_GCPIPE_H

#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/rwtypes.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamesh.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The value the display list draw sync token wraps at. Name invented. */
#define rwDLTOKENMODULUS 0xE000

/* RxGameCubeVertexAttr: one vertex attribute array of an instanced vertex buffer. */
typedef struct RxGameCubeVertexAttr {
    void* array;            /* 0x0 */
    RwUInt8 attr;           /* 0x4 */
    RwUInt8 stride;         /* 0x5 */
    RwUInt8 indexType;      /* 0x6 */
    RwUInt8 field_0x07;      /* 0x7 */
    RwUInt16 Count;         /* 0x8 */
    RwUInt8 field_0x0A[2];   /* 0xA */
} RxGameCubeVertexAttr;

/*
 * Flags of RxGameCubeVertexBuffer.flags for the pseudo HDR lighting (names invented): the buffer's
 * colours are to be kept at half range, and they are at half range now.
 */
#define rxGCVBHDRPRELIGHT 0x200
#define rxGCVBHDRPRELIGHTAPPLIED 0x100

/* RxGameCubeVertexBuffer: the header of an instanced vertex buffer, stored right after its resource entry. */
typedef struct RxGameCubeVertexBuffer {
    RwUInt16 token;                 /* 0x0 draw sync token of the last draw that read the buffer */
    RwUInt16 serialNumber;          /* 0x2 */
    RwUInt32 flags;                 /* 0x4 */
    RwUInt32 numAttrArrays;         /* 0x8 */
    RxGameCubeVertexAttr attr[1];   /* 0xC */
} RxGameCubeVertexBuffer;

/* The display list module's current draw sync token (data of the display list module). */
extern volatile RwUInt16 _RwDlTokenCurrent; // volatile: retail reloads it for every read (the GX draw sync callback advances it)

/* Whether the draw sync token has been reached (display list module). */
extern RwInt32 _rwDlTokenQueryDone(RwUInt16 token);

/* Waits until the GX pipe has drawn from a resource entry's vertex buffer. */
extern void _rxGCResEntryWaitDone(RwResEntry* resEntry);

/*
 * Brings a morphing geometry's vertex buffer up to date with its interpolator (the morph targets
 * it is between and how far). Retail's copy of Gormiti's _rxGCInstanceMorphUpdate, which has one
 * parameter less; its unit is not split yet.
 */
extern void fn_802DC4B8(RpGeometry* geometry, RxGameCubeVertexBuffer* vbHeader, RpInterpolator* interpolator);

/*
 * RxGameCubePipeData: what an all-in-one node hands its four stages: the object's instanced resource
 * entry and meshes, the flags of its world or geometry, the ambient light and the lights found for
 * it, and the node's data (Gormiti DWARF). Size 0x30.
 */
typedef struct RxGameCubePipeData {
    RwResEntry* resEntry;           /* 0x00 */
    RpMeshHeader* meshHeader;       /* 0x04 */
    RwInt32 flags;                  /* 0x08 */
    RwRGBAReal ambientLightColor;   /* 0x0C */
    RwInt32 ambientLight;           /* 0x1C */
    RwUInt32 lightMask;             /* 0x20 */
    RwInt32 numActiveLights;        /* 0x24 */
    RwInt32 numConsideredLights;    /* 0x28 */
    void* nodeData;                 /* 0x2C */
} RxGameCubePipeData;

/* One stage of a GameCube all-in-one pipeline node: instance, reinstance, lighting or render (Gormiti DWARF). */
typedef void* (*RxGameCubePipeCallback)(void* object, struct RxGameCubePipeData* pipeData);

/* The private data of an all-in-one node: its four stages (Gormiti DWARF). Size 0x10. */
typedef struct _rxGameCubeAllInOneNodeData {
    RxGameCubePipeCallback instanceCallback;    /* 0x0 */
    RxGameCubePipeCallback reinstanceCallback;  /* 0x4 */
    RxGameCubePipeCallback lightingCallback;    /* 0x8 */
    RxGameCubePipeCallback renderCallback;      /* 0xC */
} _rxGameCubeAllInOneNodeData;

/* Sets one stage of an all-in-one node. */
extern void _rxGameCubeAllInOneSetInstanceCallBack(RxPipelineNode* node, RxGameCubePipeCallback callback);
extern void _rxGameCubeAllInOneSetReinstanceCallBack(RxPipelineNode* node, RxGameCubePipeCallback callback);
extern void _rxGameCubeAllInOneSetLightingCallBack(RxPipelineNode* node, RxGameCubePipeCallback callback);
extern void RxGameCubeAllInOneSetRenderCallBack(RxPipelineNode* node, RxGameCubePipeCallback callback);

/* Whether the world sector instancer takes the optimized pass (1) or the fast one. */
extern RwInt32 _RwDlPreInstanceOptimize;

/* The default stages of the atomic and world sector pipelines. */
extern void* _rxGCAtomicDefaultInstanceCallback(void* object, struct RxGameCubePipeData* pipeData);
extern void* _rxGCAtomicDefaultReinstanceCallback(void* object, struct RxGameCubePipeData* pipeData);
extern void* _rxGCSectorDefaultInstanceCallback(void* object, struct RxGameCubePipeData* pipeData);
extern void* _rxGCSectorDefaultLightingCallback(void* object, struct RxGameCubePipeData* pipeData);
extern void* _rxGCAtomicDefaultLightingCallback(void* object, struct RxGameCubePipeData* pipeData); /* the atomics' lighting stage */
extern void* _rxGCDefaultRenderCallback(void* object, struct RxGameCubePipeData* pipeData); /* the shared render stage */

/* Create the all-in-one pipelines of the GameCube world plugin from their four stages. */
extern RxPipeline* _rpDlAtomicPipelineCreate(unsigned int id, unsigned int type, RxGameCubePipeCallback instance,
                                             RxGameCubePipeCallback reinstance, RxGameCubePipeCallback lighting,
                                             RxGameCubePipeCallback render);
extern RxPipeline* _rpDlSectorPipelineCreate(unsigned int id, unsigned int type, RxGameCubePipeCallback instance,
                                             RxGameCubePipeCallback reinstance, RxGameCubePipeCallback lighting,
                                             RxGameCubePipeCallback render);

/* The vertex format and light plugin attach entries of the GameCube world plugin. */
extern RwInt32 _rpDlVtxFmtPluginAttach(void);
extern RwInt32 _rpDlLightPluginAttach(void);

/* Sets up lighting channels and TEV stages for an object and returns its material colour callback (_rwDlObjectRenderSetup in Gormiti). */
extern void (*fn_802DE568(unsigned int flags, unsigned int lightMask, int ambient, int prelightAlpha,
                          RxGameCubeVertexBuffer* vbHeader, RxGameCubePipeData* pipeData))(RwRGBAReal*, RwRGBA*, float);

#ifdef __cplusplus
}
#endif

#endif
