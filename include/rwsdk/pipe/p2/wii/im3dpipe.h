// Unit name: Gormiti link map (RwCore.a im3dpipe, pipe/p2/wii/im3dpipe.c)
#ifndef RWSDK_PIPE_P2_WII_IM3DPIPE_H
#define RWSDK_PIPE_P2_WII_IM3DPIPE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/pipe/p2/baim3d.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The platform's immediate-mode pipelines, built when the immediate-mode module opens and
 * destroyed when it closes. Defined in im3dpipe.c.
 */
RwBool _rwIm3DCreatePlatformTransformPipeline(RxPipeline** pipe);
void _rwIm3DDestroyPlatformTransformPipeline(RxPipeline** pipe);
void _rwIm3DDestroyPlatformRenderPipelines(rwIm3DRenderPipelines* pipes);
RwBool _rwIm3DCreatePlatformRenderPipelines(rwIm3DRenderPipelines* pipes);

#ifdef __cplusplus
}
#endif

#endif
