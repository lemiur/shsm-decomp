// Name: Gormiti link map (RPworld.a ibuffer)
// Layout: Gormiti DWARF (RxGameCubeDisplayList)
#ifndef RWSDK_WORLD_IBUFFER_H
#define RWSDK_WORLD_IBUFFER_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/vtxdesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RxGameCubeDisplayList: a GX display list and its size in bytes. */
typedef struct RxGameCubeDisplayList {
    void* displayList;  /* 0x0 */
    RwUInt32 size;      /* 0x4 */
} RxGameCubeDisplayList;

/* The size in bytes of one vertex of a display list: what the descriptor's attributes add up to. */
extern RwUInt32 _rwGCNDisplayListGetStride(rwVertexDescriptor* vtxDesc);

/* The size in bytes of a display list of numStrips triangle strips and numIndices vertices, rounded up to 32 bytes. */
extern RwUInt32 _rwGCNDisplayListGetSize(rwVertexDescriptor* vtxDesc, RwUInt32 numStrips, RwUInt32 numIndices);

/* Points a display list at its memory and clears it. */
extern void _rwGCNDisplayListInitialize(RxGameCubeDisplayList* displayList, RwUInt32 reserved, RwUInt32 displayListSize,
                                        void* memory);

#ifdef __cplusplus
}
#endif

#endif
