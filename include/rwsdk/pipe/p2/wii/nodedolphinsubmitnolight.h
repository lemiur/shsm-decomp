// Unit name: Gormiti link map (RwCore.a nodeDolphinSubmitNoLight, pipe/p2/wii/nodeDolphinSubmitNoLight.c)
#ifndef RWSDK_PIPE_P2_WII_NODEDOLPHINSUBMITNOLIGHT_H
#define RWSDK_PIPE_P2_WII_NODEDOLPHINSUBMITNOLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

struct RxNodeDefinition;

/*
 * The nodes of the platform's immediate-mode pipelines: the one that instances the vertices
 * (transform pipeline) and the one that submits them unlit (render pipelines). Defined in
 * nodeDolphinSubmitNoLight.c, unit not yet reconstructed.
 */
struct RxNodeDefinition* RxNodeDefinitionGetGameCubeImmInstance(void);
struct RxNodeDefinition* RxNodeDefinitionGetGameCubeSubmitNoLight(void);

#ifdef __cplusplus
}
#endif

#endif
