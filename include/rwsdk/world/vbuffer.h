// Name: Gormiti link map (RPworld.a vbuffer)
// Layout: Gormiti DWARF (rwGCNVertexBufferData)
#ifndef RWSDK_WORLD_VBUFFER_H
#define RWSDK_WORLD_VBUFFER_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/vtxdesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* rwGCNVertexBufferData: for each vertex attribute, how many elements the geometry has and where they are. */
typedef struct rwGCNVertexBufferData {
    RwUInt32 num[26];   /* 0x00 */
    void* data[26];     /* 0x68 */
} rwGCNVertexBufferData;

/* The size in bytes of an instanced vertex buffer's header: one attribute record per attribute array of the descriptor. */
extern RwUInt32 _rwGCNVertexBufferHeaderGetSize(rwVertexDescriptor* vtxDesc);

/* The size in bytes of the attribute arrays an instanced vertex buffer holds, each rounded up to 32 bytes. */
extern RwUInt32 _rwGCNVertexBufferGetSize(rwVertexDescriptor* vtxDesc, rwGCNVertexBufferData* vtxBufData);

/* Fills in the header of an instanced vertex buffer, laying the indexed attribute arrays out in memory. */
extern void _rwGCNVertexBufferInitialize(rwVertexDescriptor* vtxDesc, RxGameCubeVertexBuffer* vtxBufHeader,
                                         rwGCNVertexBufferData* vtxBufData, unsigned char* memory);

#ifdef __cplusplus
}
#endif

#endif
