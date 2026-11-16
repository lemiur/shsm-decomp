// Unit name and boundary: Gormiti link map (RPworld.a geomcond)
// Names, parameters and locals: Gormiti DWARF; TypeGetSize and TypeCheckEqual: Gormiti Debug build
/*
 * geomcond.c: condenses the vertex arrays of a geometry before the GameCube pipeline instances
 * them. Vertices whose values match in every array they depend on collapse into one, so each
 * attribute array is mapped to a shorter one and the triangle indices are rewritten to follow.
 * The switch tables in .data belong to the inlined TypeGetSize and TypeCheckEqual.
 */
#include <string.h>

#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/geomcond.h>

/* The bytes one element of an attribute array of this type takes; 0 for a type that has no size here. */
// no copy in retail (inlined into its callers)
static unsigned int TypeGetSize(rwDataType type) {
    switch (type) {
    case rwDATATYPE_INT8:
        return 1;
    case rwDATATYPE_INT16:
        return 2;
    case rwDATATYPE_INT24:
        return 3;
    case rwDATATYPE_INT32:
        return 4;
    case rwDATATYPE_REAL:
        return 4;
    case rwDATATYPE_V2D:
        return 8;
    case rwDATATYPE_V3D:
        return 12;
    case rwDATATYPE_V4D:
        return 16;
    case rwDATATYPE_RGBA:
        return 4;
    default:
        return 0;
    }
}

/* Whether two elements of an attribute array of this type are bit for bit the same. */
// no copy in retail (inlined into its callers)
static inline int TypeCheckEqual(rwDataType type, void* a, void* b) {
    switch (type) {
    case rwDATATYPE_INT8:
        return *(unsigned char*)a == *(unsigned char*)b;
    case rwDATATYPE_INT16:
        return *(unsigned short*)a == *(unsigned short*)b;
    case rwDATATYPE_INT24: {
        unsigned char* pa = (unsigned char*)a;
        unsigned char* pb = (unsigned char*)b;

        return pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2];
    }
    case rwDATATYPE_INT32:
        return *(unsigned int*)a == *(unsigned int*)b;
    case rwDATATYPE_REAL:
        return *(unsigned int*)a == *(unsigned int*)b;
    case rwDATATYPE_V2D: {
        unsigned int* pa = (unsigned int*)a;
        unsigned int* pb = (unsigned int*)b;

        return pa[0] == pb[0] && pa[1] == pb[1];
    }
    case rwDATATYPE_V3D: {
        unsigned int* pa = (unsigned int*)a;
        unsigned int* pb = (unsigned int*)b;

        return pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2];
    }
    case rwDATATYPE_V4D: {
        unsigned int* pa = (unsigned int*)a;
        unsigned int* pb = (unsigned int*)b;

        return pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2] && pa[3] == pb[3];
    }
    case rwDATATYPE_RGBA:
        return *(unsigned int*)a == *(unsigned int*)b;
    default:
        return 0;
    }
}

/* Rewrites a list of indices for the condensed arrays: each index drops by the number of vertices before it that were kept. */
static void IndicesRemap(unsigned short* dstIndexList, unsigned short* srcIndexList, int* map, unsigned int numIndices) {
    unsigned int i;

    for (i = 0; i < numIndices; i++) {
        int numHoles = 0;
        unsigned short j;
        unsigned short index;

        if (map[srcIndexList[i]] != -1) {
            index = map[srcIndexList[i]];
        } else {
            index = srcIndexList[i];
        }

        for (j = 0; j < index; j++) {
            if (map[j] != -1) {
                numHoles++;
            }
        }

        dstIndexList[i] = index - numHoles;
    }
} // 0x802DF5B0

/* Builds one remapped index list for each of numEntries sets of numIndices indices. */
rwGCNIndexDataRemapped* IndexDataCreateRemapped(rwGCNVtxDataMap* vtxDataMap, rwGCNIndexData* indexData,
                                                unsigned int numEntries, unsigned int numIndices) {
    rwGCNIndexDataRemapped* indexDataRemapped;
    unsigned int offset;
    unsigned int size;
    unsigned int i;

    size = numEntries * (numIndices * sizeof(unsigned short)) + numEntries * sizeof(rwGCNIndexDataRemapped);
    indexDataRemapped = (rwGCNIndexDataRemapped*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    if (indexDataRemapped == NULL) {
        return NULL;
    }

    memset(indexDataRemapped, 0, numEntries * sizeof(rwGCNIndexDataRemapped));
    offset = numEntries * sizeof(rwGCNIndexDataRemapped);

    for (i = 0; i < numEntries; i++) {
        indexDataRemapped[i].indices = (unsigned short*)((unsigned char*)indexDataRemapped + offset);
        offset += numIndices * sizeof(unsigned short);
    }

    for (i = 0; i < numEntries; i++) {
        IndicesRemap(indexDataRemapped[i].indices, indexData[i].indices, vtxDataMap[i].map, numIndices);
    }

    return indexDataRemapped;
} // 0x802DF624

/* Copies the vertices the maps keep into condensed arrays, one per attribute. */
rwGCNVtxDataRemapped* VertexDataCreateRemapped(rwGCNVtxDataMap* vtxDataMap, rwGCNVtxData* vtxData,
                                               unsigned int numEntries, unsigned int numVerts) {
    unsigned int i;
    unsigned int size;
    unsigned int offset;
    rwGCNVtxDataRemapped* vtxDataRemapped;

    size = numEntries * sizeof(rwGCNVtxDataRemapped);
    for (i = 0; i < numEntries; i++) {
        size += vtxDataMap[i].num * TypeGetSize(vtxData[i].type);
    }

    vtxDataRemapped = (rwGCNVtxDataRemapped*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    if (vtxDataRemapped == NULL) {
        return NULL;
    }

    memset(vtxDataRemapped, 0, numEntries * sizeof(rwGCNVtxDataRemapped));
    offset = numEntries * sizeof(rwGCNVtxDataRemapped);

    for (i = 0; i < numEntries; i++) {
        vtxDataRemapped[i].data = (unsigned char*)vtxDataRemapped + offset;
        offset += vtxDataMap[i].num * TypeGetSize(vtxData[i].type);
    }

    for (i = 0; i < numEntries; i++) {
        unsigned int j;
        unsigned char* srcData;
        unsigned char* dstData;

        size = TypeGetSize(vtxData[i].type);
        srcData = (unsigned char*)vtxData[i].data;
        dstData = (unsigned char*)vtxDataRemapped[i].data;
        for (j = 0; j < numVerts; j++) {
            if (vtxDataMap[i].map[j] == -1) {
                memcpy(dstData, srcData, size);
                dstData += size;
            }

            srcData += size;
        }

        vtxDataRemapped[i].num = vtxDataMap[i].num;
    }

    return vtxDataRemapped;
} // 0x802DF7B0

/*
 * Finds, for each attribute array, the vertices that repeat an earlier one: vertex k maps to vertex j when the
 * two hold the same value in this array and in every array this one depends on. A vertex that is kept maps to -1.
 */
rwGCNVtxDataMap* VertexDataCreateMaps(rwGCNVtxData* vtxData, unsigned int numEntries, unsigned int numVerts) {
    unsigned int offset;
    unsigned int count;
    unsigned int i;
    rwGCNVtxDataMap* vtxDataMap;

    count = numEntries * numVerts;
    vtxDataMap = (rwGCNVtxDataMap*)RwMalloc(numEntries * sizeof(rwGCNVtxDataMap) + count * sizeof(int),
                                            rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    if (vtxDataMap == NULL) {
        return NULL;
    }

    memset(vtxDataMap, 0, numEntries * sizeof(rwGCNVtxDataMap));
    offset = numEntries * sizeof(rwGCNVtxDataMap);

    for (i = 0; i < count; i++) {
        ((int*)(vtxDataMap + numEntries))[i] = -1;
    }

    for (i = 0; i < numEntries; i++) {
        vtxDataMap[i].map = (int*)((unsigned char*)vtxDataMap + offset);
        vtxDataMap[i].num = numVerts;
        offset += numVerts * sizeof(int);
    }

    for (i = 0; i < numEntries; i++) {
        unsigned int size = TypeGetSize(vtxData[i].type);
        unsigned int j;

        for (j = 0; j < numVerts; j++) {
            unsigned int k;

            if (vtxDataMap[i].map[j] != -1) {
                continue;
            }

            for (k = j; k < numVerts; k++) {
                unsigned int depSize;
                int equal;
                signed char dep;
                signed char depIndex;

                if (j == k) {
                    continue;
                }

                if (!TypeCheckEqual(vtxData[i].type, (unsigned char*)vtxData[i].data + size * j,
                                    (unsigned char*)vtxData[i].data + size * k)) {
                    continue;
                }

                equal = 1;
                dep = 0;
                depIndex = vtxData[i].dep[dep];
                depSize = TypeGetSize(vtxData[depIndex].type);
                while (depIndex >= 0) {
                    if (!TypeCheckEqual(vtxData[depIndex].type, (unsigned char*)vtxData[depIndex].data + depSize * j,
                                        (unsigned char*)vtxData[depIndex].data + depSize * k)) {
                        equal = 0;
                        break;
                    }

                    dep++;
                    depIndex = vtxData[i].dep[dep];
                    depSize = TypeGetSize(vtxData[depIndex].type);
                }

                if (equal) {
                    vtxDataMap[i].map[k] = j;
                    vtxDataMap[i].num--;
                }
            }
        }
    }

    return vtxDataMap;
} // 0x802DFA60
