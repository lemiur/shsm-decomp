// Name: Gormiti link map (RPworld.a geomcond)
// Layout and enums: Gormiti DWARF (rwDataType, rwGCNVtxData, rwGCNVtxDataMap, rwGCNVtxDataRemapped,
// rwGCNIndexData, rwGCNIndexDataRemapped)
#ifndef RWSDK_WORLD_GEOMCOND_H
#define RWSDK_WORLD_GEOMCOND_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The element type of one vertex attribute array; it fixes the bytes an element takes. */
typedef enum rwDataType {
    rwNADATATYPE = 0,
    rwDATATYPE_INT8 = 1,
    rwDATATYPE_INT16 = 2,
    rwDATATYPE_INT24 = 3,
    rwDATATYPE_INT32 = 4,
    rwDATATYPE_INT64 = 5,
    rwDATATYPE_REAL = 6,
    rwDATATYPE_V2D = 7,
    rwDATATYPE_V3D = 8,
    rwDATATYPE_V4D = 9,
    rwDATATYPE_RGBA = 10,
    rwDATATYPE_FORCEENUMSIZEINT = 2147483647
} rwDataType;

/*
 * rwGCNVtxData: one attribute array of a geometry's vertices, with the attributes whose values it
 * depends on (dep holds attribute indices, a negative entry ends the list).
 */
typedef struct rwGCNVtxData {
    void* data;         /* 0x00 */
    rwDataType type;    /* 0x04 */
    RwInt8 dep[16];     /* 0x08 */
} rwGCNVtxData;

/* rwGCNVtxDataMap: per vertex, the index it moves to in the condensed array, -1 when it is dropped. */
typedef struct rwGCNVtxDataMap {
    RwInt32* map;       /* 0x00 */
    RwUInt32 num;       /* 0x04 */
} rwGCNVtxDataMap;

/* rwGCNVtxDataRemapped: a condensed attribute array and the number of elements it holds. */
typedef struct rwGCNVtxDataRemapped {
    void* data;         /* 0x00 */
    RwUInt32 num;       /* 0x04 */
} rwGCNVtxDataRemapped;

/* rwGCNIndexData: the index array of one set of triangle indices. */
typedef struct rwGCNIndexData {
    RwUInt16* indices;  /* 0x0 */
} rwGCNIndexData;

/* rwGCNIndexDataRemapped: the index array after the vertices it refers to were condensed. */
typedef struct rwGCNIndexDataRemapped {
    RwUInt16* indices;  /* 0x0 */
} rwGCNIndexDataRemapped;

extern rwGCNIndexDataRemapped* IndexDataCreateRemapped(rwGCNVtxDataMap* vtxDataMap, rwGCNIndexData* indexData,
                                                       unsigned int numEntries, unsigned int numIndices);
extern rwGCNVtxDataRemapped* VertexDataCreateRemapped(rwGCNVtxDataMap* vtxDataMap, rwGCNVtxData* vtxData,
                                                      unsigned int numEntries, unsigned int numVerts);
extern rwGCNVtxDataMap* VertexDataCreateMaps(rwGCNVtxData* vtxData, unsigned int numEntries, unsigned int numVerts);

#ifdef __cplusplus
}
#endif

#endif
