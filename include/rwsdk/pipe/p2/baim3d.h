// Reference: tgsm/pbg src/rwsdk/src/badevice.c (declarations)
// Layout: Gormiti DWARF (RxObjSpace3DVertex, rwImmediGlobals and its members)
#ifndef RWSDK_PIPE_P2_BAIM3D_H
#define RWSDK_PIPE_P2_BAIM3D_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/pipe/p2/bapipe.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RxCamSpace3DVertex;
struct rwGameCube2DVertex;
struct RxMeshStateVector;
struct RxRenderStateVector;

/* An index into an immediate-mode vertex array. */
// Reference: the public RW 3.7 SDK: RwImVertexIndex, rwIM3D_VERTEX*
typedef RwUInt16 RwImVertexIndex;

/* What RwIm3DTransform's flags say about the vertices (names as the SDK's rwcore.h). */
#define rwIM3D_VERTEXUV     0x01
#define rwIM3D_VERTEXXYZ    0x08
#define rwIM3D_VERTEXRGBA   0x10

/* An immediate-mode vertex in object space: position, normal, colour and texture coordinates. Size 0x24. */
typedef struct RxObjSpace3DVertex {
    RwReal x;           /* 0x00 */
    RwReal y;           /* 0x04 */
    RwReal z;           /* 0x08 */
    RwReal nx;          /* 0x0C */
    RwReal ny;          /* 0x10 */
    RwReal nz;          /* 0x14 */
    RwUInt8 r;          /* 0x18 */
    RwUInt8 g;          /* 0x19 */
    RwUInt8 b;          /* 0x1A */
    RwUInt8 a;          /* 0x1B */
    RwReal u;           /* 0x1C */
    RwReal v;           /* 0x20 */
} RxObjSpace3DVertex;

typedef RxObjSpace3DVertex RwIm3DVertex;

/*
 * The state of the vertices and indices being drawn: what RwIm3DTransform and the render calls
 * hand the pipeline. Size 0x30.
 */
typedef struct _rwIm3DPoolStash {
    RwUInt32 flags;                             /* 0x00 */
    RwMatrix* ltm;                              /* 0x04 */
    RwUInt32 numVerts;                          /* 0x08 */
    RxObjSpace3DVertex* objVerts;               /* 0x0C */
    struct RxCamSpace3DVertex* camVerts;        /* 0x10 */
    struct rwGameCube2DVertex* devVerts;        /* 0x14 */
    struct RxMeshStateVector* meshState;        /* 0x18 */
    struct RxRenderStateVector* renderState;    /* 0x1C */
    RxPipeline* pipeline;                       /* 0x20 */
    RwPrimitiveType primType;                   /* 0x24 */
    RwUInt16* indices;                          /* 0x28 */
    RwUInt32 numIndices;                        /* 0x2C */
} _rwIm3DPoolStash;

/* The vertices handed to RwIm3DTransform and what is being drawn from them. Size 0x3C. */
typedef struct rwIm3DPool {
    RwUInt16 numElements;       /* 0x00 */
    RwUInt16 pad;               /* 0x02 */ // pad: Gormiti DWARF rwIm3DPool member
    void* elements;             /* 0x04 */
    RwInt32 stride;             /* 0x08 */
    _rwIm3DPoolStash stash;     /* 0x0C */
} rwIm3DPool;

/* One render pipeline for each primitive type. Size 0x1C. */
typedef struct rwIm3DRenderPipelines {
    RxPipeline* triList;        /* 0x00 */
    RxPipeline* triFan;         /* 0x04 */
    RxPipeline* triStrip;       /* 0x08 */
    RxPipeline* lineList;       /* 0x0C */
    RxPipeline* polyLine;       /* 0x10 */
    RxPipeline* pointList;      /* 0x14 */
    RxPipeline* spriteList;     /* 0x18 */
} rwIm3DRenderPipelines;

/*
 * The immediate-mode module's globals: the pipelines in use (set by the application, or the
 * platform's own) and the pool being drawn. Size 0x7C.
 */
typedef struct rwImmediGlobals {
    RxPipeline* im3DTransformPipeline;                      /* 0x00 */
    rwIm3DRenderPipelines im3DRenderPipelines;              /* 0x04 */
    RxPipeline* platformIm3DTransformPipeline;              /* 0x20 */
    rwIm3DRenderPipelines platformIm3DRenderPipelines;      /* 0x24 */
    rwIm3DPool curPool;                                     /* 0x40 */
} rwImmediGlobals;

/* The immediate-mode module's registration, and the globals it keeps in the engine instance. */
extern rwImmediGlobals* _rwIm3DGlobals;
extern RwModuleInfo _rwIm3DModule;

/* A member of the module's globals. Macro name as the SDK's. */
#define RWIMMEDIGLOBAL(var) \
    (RWPLUGINOFFSET(rwImmediGlobals, RwEngineInstance, _rwIm3DModule.globalsOffset)->var)

void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags);
RwBool RwIm3DEnd(void);
RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices,
                                    RwInt32 numIndices);
RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType);
RxPipeline* RwIm3DSetTransformPipeline(RxPipeline* pipeline);
RxPipeline* RwIm3DSetRenderPipeline(RxPipeline* pipeline, RwPrimitiveType primType);

/* The 3D immediate mode module (rwID_IMMEDIATEMODULE). */
void* _rwIm3DOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwIm3DClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
