// Name: Gormiti link map (RPworld.a geominst)
#ifndef RWSDK_WORLD_GEOMINST_H
#define RWSDK_WORLD_GEOMINST_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/world/vtxdesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The size in bytes of one vertex's position, normal, colour or texture coordinate set in an instanced vertex buffer. */
extern RwUInt32 rwGCNPosGetSize(rwVertexDescriptor* vtxDesc);
extern RwUInt32 rwGCNNrmGetSize(rwVertexDescriptor* vtxDesc);
extern RwUInt32 rwGCNClrGetSize(rwVertexDescriptor* vtxDesc, RwUInt8 index);
extern RwUInt32 rwGCNTexGetSize(rwVertexDescriptor* vtxDesc, RwUInt8 index);

/* Instances the position, normal, colour or texture coordinate arrays of a geometry into display list vertex data. */
extern unsigned int _rwGCNVtxFmtInstPos3D(void* mem, RwV3d* srcPosition, unsigned int fmt, float scale, int numVerts,
                                  unsigned int stride, RwV3d* offset);
extern unsigned int _rwGCNVtxFmtInstNrm(void* mem, RwV3d* srcNormal, unsigned int fmt, int numVerts, unsigned int stride);
extern unsigned int _rwGCNVtxFmtInstClr(void* mem, RwRGBA* srcColor, unsigned int fmt, int numVerts, unsigned int stride);
extern unsigned int _rwGCNVtxFmtInstTex(void* mem, RwTexCoords* srcTexCoord, unsigned int fmt, float scale, int numVerts,
                                unsigned int stride);

#ifdef __cplusplus
}
#endif

#endif
