// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
#ifndef RWSDK_WORLD_BAMESHOP_H
#define RWSDK_WORLD_BAMESHOP_H

#include <rwsdk/world/bamesh.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A tristrip method: builds a mesh header from a build mesh (RW 3.7 API reference). */
typedef RpMeshHeader* (*RpTriStripMeshCallBack)(RpBuildMesh* buildMesh, void* data);

RpMeshHeader* RpBuildMeshGenerateDefaultTriStrip(RpBuildMesh* buildmesh, void* data);
RpMeshHeader* _rpTriListMeshGenerate(RpBuildMesh* buildMesh, void* data);
RpMeshHeader* _rpMeshOptimise(RpBuildMesh* buildmesh, RwUInt32 flags);

#ifdef __cplusplus
}
#endif

#endif
