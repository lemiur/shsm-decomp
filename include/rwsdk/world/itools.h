// Name: Gormiti link map (RPworld.a itools)
// Parameters: Gormiti DWARF (Debug build)
#ifndef RWSDK_WORLD_ITOOLS_H
#define RWSDK_WORLD_ITOOLS_H

#include <rwsdk/plcore/batypes.h>
#include <rwsdk/rwtypes.h>
#include <rwsdk/world/ibuffer.h>
#include <rwsdk/world/vtxdesc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* rwGCNDisplayListData: for each vertex attribute, where the geometry's data of that attribute is. */
typedef struct rwGCNDisplayListData {
    void* data[26];     /* 0x00 */
} rwGCNDisplayListData;

/* Counts the triangle strips and the indices a list of triangle indices becomes when it is joined into strips. */
extern void _rwGCNTriStripGetStats(unsigned short* indices, unsigned int numIndices, unsigned int* numStripsOut,
                                   unsigned int* numIndicesOut, int preserveWindingOrder);

/* Copies numIndices indices into memory, one every stride bytes, as 8 or 16 bit values (indexType). */
extern void _rwGCNInstanceIndicesCopy(unsigned short* indices, unsigned int numIndices, unsigned int stride,
                                      unsigned int indexType, void* memory);

/* Writes the indices of a display list's triangle strips into memory (the strips' vertex data, after their headers). */
extern void _rwGCNInstanceIndices(unsigned short* posIndices, unsigned short* indices, unsigned int numIndices,
                                  int removeDegen, unsigned int stride, unsigned int indexType, int preserveWindingOrder,
                                  void* memory);

/* Fills a display list with the vertex data of a geometry's triangle strips. */
extern void _rwGCNDisplayListFill(rwVertexDescriptor* vtxDesc, RxGameCubeDisplayList* displayList,
                                  rwGCNDisplayListData* displayListData, unsigned int numIndices, int removeDegen,
                                  unsigned int stride, int preserveWindingOrder, unsigned char primTypeVAT,
                                  RwV3d* posOrigin);

#ifdef __cplusplus
}
#endif

#endif
