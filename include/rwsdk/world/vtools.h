// Name: Gormiti link map (RPworld.a vtools)
#ifndef RWSDK_WORLD_VTOOLS_H
#define RWSDK_WORLD_VTOOLS_H

#include <rwsdk/plcore/batypes.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/vbuffer.h>
#include <rwsdk/world/vtxdesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fills an instanced vertex buffer's attribute arrays from a geometry's vertex data (_rwGCNVertexBufferFill in Gormiti). */
extern void fn_802E8B10(rwVertexDescriptor* vtxDesc, RxGameCubeVertexBuffer* vtxBufHeader,
                        rwGCNVertexBufferData* vtxBufData, int cmpNrm, RwV3d* posOrigin);

#ifdef __cplusplus
}
#endif

#endif
