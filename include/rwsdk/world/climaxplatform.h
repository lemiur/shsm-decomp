// Name: Gormiti link map (RPworld.a climaxplatform)
// Signatures: Gormiti DWARF; callback type names invented
#ifndef RWSDK_WORLD_CLIMAXPLATFORM_H
#define RWSDK_WORLD_CLIMAXPLATFORM_H

#include <revolution/gx/GXEnum.h>
#include <revolution/mtx.h>
#include <rwsdk/batextur.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/rwtypes.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/gcpipe.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RpGameCubeVtxFmt;
/* What a TEV callback is told about the TEV setup of the effect being sent, and may change. Layout and names: Gormiti DWARF. */
typedef struct StTevSetupData {
    unsigned int Source;               /* 0x00 */
    unsigned int SourceParm;           /* 0x04 */
    GXTevColorArg LightingSrc;         /* 0x08 */
    unsigned int Count_TexGens;        /* 0x0C */
    unsigned int Count_IndTevs;        /* 0x10 */
    unsigned int Count_Tevs;           /* 0x14 */
    RxGameCubePipeData* pipeData;      /* 0x18 */
} StTevSetupData;

/* Called to set up an object's transform for the GX pipe: the local to world matrix, whether it has normals, the model view. */
typedef unsigned int (*RwClimaxRVLTransformCallBack)(RwMatrix* ltm, int normals, float (*ModelView)[4]);

/* Called before and after the TEV stages are set up. */
typedef unsigned int (*RwClimaxRVLTevCallBack)(StTevSetupData* TevSetupData);

/* Called to process the vertices of an instanced vertex buffer. */
typedef unsigned int (*RwClimaxRVLVertexProcCallBack)(RwMatrix* Ltm, struct RpGameCubeVtxFmt* vtxFmt,
                                                      RxGameCubeVertexBuffer* vtxBuf, unsigned int flags,
                                                      int PNMatrixInd);

/* The render callbacks the game installed; the world plugin's pipelines call them when they are not NULL. */
extern RwClimaxRVLVertexProcCallBack _Current_RwClimaxVertexProcCB;
extern RwClimaxRVLTevCallBack _Current_RwClimaxRvlPostTevCB;
extern RwClimaxRVLTevCallBack _Current_RwClimaxRvlPreTevCB;
extern RwClimaxRVLTransformCallBack _Current_RwClimaxRvlTransformCB;

/* Renders the world: its sectors in the camera's frustum, with the render flags raised for the pass. */
extern RpWorld* RwClimaxWorldRender(RpWorld* world, RwUInt32 flags);

/* Sets a texture into a texture map of the GX. A NULL texture is ignored. */
extern void RwClimaxSetTexture(RwTexture* texture, RwInt32 index);

extern void RwClimaxRVLSetRenderCallbacks(RwClimaxRVLTransformCallBack transformCB, RwClimaxRVLTevCallBack preTevCB,
                                          RwClimaxRVLTevCallBack postTevCB);
extern void RwClimaxRVLSetVertexProcCallback(RwClimaxRVLVertexProcCallBack vertexProcCB);

/* Converts a RenderWare matrix (axes in rows) to a GX matrix (axes in columns, three rows). */
extern void RwClimaxRVLConvertMatrix(const RwMatrix* rwMatrix, Mtx mtx);

/* Halves, and doubles, the colours of an instanced attribute array for the pseudo HDR lighting. */
extern void RwClimaxRVLHDRPrelightApply(RwUInt8 vtxFmtByte, RxGameCubeVertexAttr* Attr);
extern void RwClimaxRVLHDRPrelightRemove(RwUInt8 vtxFmtByte, RxGameCubeVertexAttr* Attr);

#ifdef __cplusplus
}
#endif

#endif
