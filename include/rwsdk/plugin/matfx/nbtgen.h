// Names: Gormiti DWARF and link map (RpMatFx.a nbtGen.obj; RpGameCubeVtxFmt is seen from effectPipesGcn.obj)
// Header name invented: no evidence names Gormiti's header for the GameCube normal-binormal-tangent generator.
#ifndef RWSDK_PLUGIN_MATFX_NBTGEN_H
#define RWSDK_PLUGIN_MATFX_NBTGEN_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/vtxfmt.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The normal-binormal-tangent generator of the multitexture pipeline (nbtGen.c): whether an
 * instanced object has a material whose effect needs bump vectors, and the pass that computes them
 * into the instanced vertex buffer.
 */
extern int _rpGameCubeMTPipeDataQueryNBTs(RxGameCubePipeData* pipeData);
extern void _rpGameCubeMTPipeDataCalcNBTs(RxGameCubePipeData* pipeData, RpGameCubeVtxFmt* vtxFmt, int numVerts);

#ifdef __cplusplus
}
#endif

#endif
