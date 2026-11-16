// Unit name and boundary: Gormiti link map (RPworld.a instancegeom.obj)
// Names and layout: Gormiti link map and DWARF (RPworld.a instancegeom.obj)
/*
 * instancegeom.c: instances an RpGeometry into a GameCube vertex buffer and display lists.
 * The optimized path condenses duplicate vertex attributes per attribute stream and strips
 * degenerate triangles; the fast path copies the geometry's vertex arrays directly.
 */
#include <string.h>

#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/geomcond.h>
#include <rwsdk/world/ibuffer.h>
#include <rwsdk/world/instancegeom.h>
#include <rwsdk/world/itools.h>
#include <rwsdk/world/vbuffer.h>
#include <rwsdk/world/vtools.h>
#include <rwsdk/world/vtxdesc.h>
#include <rwsdk/world/vtxfmt.h>

/* Vertex descriptor populated by VtxDescInitOptimized and VtxDescInitFast. */
static rwVertexDescriptor lbl_80519510;

/*
 * Condenses a geometry's vertex attribute streams and builds per-mesh remapped vertex and index
 * tables for the optimized instancer.
 */
static int ReconditionVertexIndexData(RpGeometry* geometry, rwGCNVtxDataRemapped** vertexDataRemapped,
                                      rwGCNIndexDataRemapped*** indexDataRemapped) {
    rwGCNVtxData vtxData[16];
    rwGCNVtxDataMap* vertexDataMaps;
    rwGCNIndexData* indexData;
    RpMorphTarget* morphTarget;
    int numEntries;
    int numMeshes;
    int i;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    morphTarget = geometry->morphTarget;
    numEntries = 0;
    vtxData[numEntries].data = morphTarget->verts;
    vtxData[numEntries].type = rwDATATYPE_V3D;
    vtxData[numEntries].dep[0] = -1;
    numEntries++;

    if (geometry->flags & rpGEOMETRYNORMALS) {
        vtxData[numEntries].data = morphTarget->normals;
        vtxData[numEntries].type = rwDATATYPE_V3D;
        vtxData[numEntries].dep[0] = -1;
        numEntries++;
    }

    if (geometry->flags & rpGEOMETRYPRELIT) {
        vtxData[numEntries].data = geometry->preLitLum;
        vtxData[numEntries].type = rwDATATYPE_INT32;
        vtxData[numEntries].dep[0] = -1;
        numEntries++;
    }

    if (geometry->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        for (i = 0; i < geometry->numTexCoordSets; i++) {
            vtxData[numEntries].data = RpGeometryGetVertexTexCoords(geometry, rwTEXTURECOORDINATEINDEX0 + i);
            vtxData[numEntries].type = rwDATATYPE_V2D;
            vtxData[numEntries].dep[0] = -1;
            numEntries++;
        }
    }

    vertexDataMaps = VertexDataCreateMaps(vtxData, numEntries, geometry->numVertices);
    if (vertexDataMaps == NULL) {
        return 0;
    }

    *vertexDataRemapped = VertexDataCreateRemapped(vertexDataMaps, vtxData, numEntries, geometry->numVertices);
    if (*vertexDataRemapped == NULL) {
        RwFree(vertexDataMaps);
        return 0;
    }

    numMeshes = geometry->mesh->numMeshes;
    indexData = (rwGCNIndexData*)RwMalloc(numEntries * (numMeshes * sizeof(rwGCNIndexData)),
                                          rwMEMHINTDUR_FUNCTION | rwID_WORLDPIPEMODULE);
    if (indexData == NULL) {
        RwFree(*vertexDataRemapped);
        RwFree(vertexDataMaps);
        return 0;
    }

    memset(indexData, 0, numEntries * (numMeshes * sizeof(rwGCNIndexData)));
    for (i = 0; i < numMeshes; i++) {
        RpMesh* mesh = &((RpMesh*)(geometry->mesh + 1))[i];
        int j;

        for (j = 0; j < numEntries; j++) {
            indexData[i * numEntries + j].indices = mesh->indices;
        }
    }

    *indexDataRemapped = (rwGCNIndexDataRemapped**)RwMalloc(numMeshes * sizeof(rwGCNIndexDataRemapped*),
                                                            rwMEMHINTDUR_FUNCTION | rwID_WORLDPIPEMODULE);
    if (*indexDataRemapped == NULL) {
        RwFree(indexData);
        RwFree(*vertexDataRemapped);
        RwFree(vertexDataMaps);
        return 0;
    }

    for (i = 0; i < numMeshes; i++) {
        RpMesh* mesh = &((RpMesh*)(geometry->mesh + 1))[i];

        (*indexDataRemapped)[i] =
            IndexDataCreateRemapped(vertexDataMaps, &indexData[i * numEntries], numEntries, mesh->numIndices);
        if ((*indexDataRemapped)[i] == NULL) {
            while (--i > 0) {
                RwFree((*indexDataRemapped)[i]);
            }
            return 0;
        }
    }

    RwFree(indexData);
    RwFree(vertexDataMaps);
    return 1;
} // 0x802E4B90

/* Initializes the shared vertex descriptor for an optimized (condensed) geometry. */
static rwVertexDescriptor* VtxDescInitOptimized(RpGeometry* geometry, rwGCNVtxDataRemapped* vertexDataRemapped) {
    unsigned int numAttr;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    _rwVertexDescriptorInit(&lbl_80519510);
    _rwGCNVertexDescSetVAT(&lbl_80519510, 0);
    _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_POS, rwGCNCC_POS_XYZ, (rwGCNCompType)vtxFmt->pos,
                                   vtxFmt->posFrac);
    _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_POS,
                                   (vertexDataRemapped[0].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
    numAttr = 1;

    if (geometry->flags & rpGEOMETRYNORMALS) {
        if (vtxFmt->nbt != 0) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_NBT, rwGCNCC_NRM_NBT, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_NBT,
                                           (vertexDataRemapped[1].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        } else {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_NRM, rwGCNCC_NRM_XYZ, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_NRM,
                                           (vertexDataRemapped[1].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        }
        numAttr = 2;
    }

    if (geometry->flags & rpGEOMETRYPRELIT) {
        _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_CLR0,
                                       (vtxFmt->preLight > 2) ? rwGCNCC_CLR_RGBA : rwGCNCC_CLR_RGB,
                                       (rwGCNCompType)vtxFmt->preLight, 0);
        _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_CLR0,
                                       (vertexDataRemapped[numAttr].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        numAttr++;
    }

    if (geometry->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        int j;

        for (j = 0; j < geometry->numTexCoordSets; j++) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j), rwGCNCC_TEX_ST,
                                           (rwGCNCompType)vtxFmt->texCoord[j], vtxFmt->texCoordFrac[j]);
            _rwGCNVertexDescSetElementDesc(
                &lbl_80519510, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j),
                (vertexDataRemapped[numAttr].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
            numAttr++;
        }
    }

    _rwGCNVertexDescSetNumIndexedAttr(&lbl_80519510, numAttr);
    return &lbl_80519510;
} // 0x802E4FBC

/* Populates vertex buffer attribute counts and pointers from condensed vertex data (VertexDataSetupOptimized in Gormiti). */
static void fn_802E51CC(rwGCNVertexBufferData* vtxBufferData, rwGCNVtxDataRemapped* vtxDataRemapped,
                        RpGeometry* geometry) {
    unsigned int numElements;
    unsigned int flags = geometry->flags;
    unsigned char numTexCoords = (unsigned char)geometry->numTexCoordSets;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    vtxBufferData->num[GX_VA_POS] = vtxDataRemapped[0].num;
    vtxBufferData->data[GX_VA_POS] = vtxDataRemapped[0].data;
    numElements = 1;

    if (flags & rpGEOMETRYNORMALS) {
        vtxBufferData->num[GX_VA_NRM] = vtxDataRemapped[1].num;
        vtxBufferData->data[GX_VA_NRM] = vtxDataRemapped[1].data;
        numElements = 2;
    }

    if (flags & rpGEOMETRYPRELIT) {
        vtxBufferData->num[GX_VA_CLR0] = vtxDataRemapped[numElements].num;
        vtxBufferData->data[GX_VA_CLR0] = vtxDataRemapped[numElements].data;
        numElements++;
    }

    if (flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        unsigned int i;

        for (i = 0; i < numTexCoords; i++) {
            vtxBufferData->num[GX_VA_TEX0 + i] = vtxDataRemapped[numElements].num;
            vtxBufferData->data[GX_VA_TEX0 + i] = vtxDataRemapped[numElements].data;
            numElements++;
        }
    }
} // 0x802E51CC

/* Populates display list index pointers from remapped mesh indices. */
static void IndexDataSetupOptimized(rwGCNDisplayListData* indexBufferData, rwGCNIndexDataRemapped* indexDataRemapped,
                                    RpGeometry* geometry) {
    unsigned int numElements;
    unsigned int flags = geometry->flags;
    unsigned char numTexCoords = (unsigned char)geometry->numTexCoordSets;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    indexBufferData->data[GX_VA_POS] = indexDataRemapped[0].indices;
    numElements = 1;

    if (flags & rpGEOMETRYNORMALS) {
        indexBufferData->data[GX_VA_NRM] = indexDataRemapped[1].indices;
        numElements = 2;
    }

    if (flags & rpGEOMETRYPRELIT) {
        indexBufferData->data[GX_VA_CLR0] = indexDataRemapped[numElements].indices;
        numElements++;
    }

    if (flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        unsigned int i;

        for (i = 0; i < numTexCoords; i++) {
            indexBufferData->data[GX_VA_TEX0 + i] = indexDataRemapped[numElements].indices;
            numElements++;
        }
    }
} // 0x802E53D0

/* Instances a geometry using condensed vertex streams and stripped display lists. */
RwResEntry* _rwDlGeometryInstanceOptimized(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner) {
    /* RULE-EXCEPTION: R30, declaration order: Gormiti DWARF lists i first and displayLists after vbHeader;
       this declaration order gives retail's register numbering */
    RxGameCubeDisplayList* displayLists;
    unsigned int size;
    unsigned int vBufferHeaderSize;
    unsigned int displayListSize;
    unsigned int i;
    unsigned int vBufferSize;
    RwResEntry* resEntry;
    void* memory;
    RxGameCubeVertexBuffer* vbHeader;
    rwGCNVtxDataRemapped* vertexDataRemapped;
    rwGCNIndexDataRemapped** indexDataRemapped;
    rwVertexDescriptor* vtxDesc;
    rwGCNVertexBufferData vtxBufData;
    unsigned char primTypeVAT;
    unsigned int numStrips;
    unsigned int numIndices;

    ReconditionVertexIndexData(geometry, &vertexDataRemapped, &indexDataRemapped);
    vtxDesc = VtxDescInitOptimized(geometry, vertexDataRemapped);
    fn_802E51CC(&vtxBufData, vertexDataRemapped, geometry);

    vBufferHeaderSize = _rwGCNVertexBufferHeaderGetSize(vtxDesc);
    displayListSize = geometry->mesh->numMeshes * sizeof(RxGameCubeDisplayList);
    size = vBufferHeaderSize + displayListSize + 31;

    if (geometry->flags & rpGEOMETRYTRISTRIP) {
        primTypeVAT = GX_TRIANGLESTRIP;
    } else {
        primTypeVAT = GX_TRIANGLES;
    }

    for (i = 0; i < geometry->mesh->numMeshes; i++) {
        if (geometry->flags & rpGEOMETRYTRISTRIP) {
            _rwGCNTriStripGetStats(indexDataRemapped[i][0].indices,
                                   ((RpMesh*)(geometry->mesh + 1))[i].numIndices, &numStrips, &numIndices, 1);
            size += _rwGCNDisplayListGetSize(vtxDesc, numStrips, numIndices);
        } else {
            numIndices = ((RpMesh*)(geometry->mesh + 1))[i].numIndices;
            size += _rwGCNDisplayListGetSize(vtxDesc, 1, numIndices);
        }
    }

    vBufferSize = _rwGCNVertexBufferGetSize(vtxDesc, &vtxBufData);
    size += vBufferSize;

    resEntry = (RwResEntry*)RwMalloc(size + sizeof(RwResEntry), rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    resEntry->link.next = NULL;
    resEntry->link.prev = NULL;
    resEntry->owner = owner;
    resEntry->size = size;
    resEntry->ownerRef = resEntryOwner;
    resEntry->destroyNotify = _rxGCResEntryWaitDone;
    *resEntryOwner = resEntry;

    memory = (void*)(resEntry + 1);
    vbHeader = (RxGameCubeVertexBuffer*)memory;
    memset(vbHeader, 0, size);
    vbHeader->token = _RwDlTokenLastSeen;
    vbHeader->serialNumber = geometry->mesh->serialNum;
    vbHeader->flags = 0;

    if (geometry->flags & rpGEOMETRYPRELIT) {
        RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);

        if (vtxFmt == NULL) {
            int i;

            vbHeader->flags &= ~1;
            for (i = 0; i < geometry->numVertices; i++) {
                if (geometry->preLitLum[i].alpha < 255) {
                    vbHeader->flags |= 1;
                    break;
                }
            }
        } else if (vtxFmt->preLight > 2) {
            vbHeader->flags |= 1;
        } else {
            vbHeader->flags &= ~1;
        }
    }

    displayLists = (RxGameCubeDisplayList*)((unsigned char*)vbHeader + vBufferHeaderSize);
    memory = (void*)(((unsigned int)displayLists + displayListSize + 31) & ~31);

    for (i = 0; i < geometry->mesh->numMeshes; i++) {
        rwGCNDisplayListData displayListData;
        unsigned int displayListSize;
        unsigned int displayListstride;

        if (geometry->flags & rpGEOMETRYTRISTRIP) {
            _rwGCNTriStripGetStats(indexDataRemapped[i][0].indices,
                                   ((RpMesh*)(geometry->mesh + 1))[i].numIndices, &numStrips, &numIndices, 1);
        } else {
            numIndices = ((RpMesh*)(geometry->mesh + 1))[i].numIndices;
            numStrips = 1;
        }

        displayListSize = _rwGCNDisplayListGetSize(vtxDesc, numStrips, numIndices);
        _rwGCNDisplayListInitialize(&displayLists[i], i, displayListSize, memory);
        IndexDataSetupOptimized(&displayListData, indexDataRemapped[i], geometry);
        displayListstride = _rwGCNDisplayListGetStride(vtxDesc);
        _rwGCNDisplayListFill(vtxDesc, &displayLists[i], &displayListData,
                              ((RpMesh*)(geometry->mesh + 1))[i].numIndices, geometry->flags & rpGEOMETRYTRISTRIP,
                              displayListstride, 1, primTypeVAT, NULL);
        memory = (unsigned char*)memory + displayListSize;
    }

    _rwGCNVertexBufferInitialize(vtxDesc, vbHeader, &vtxBufData, (unsigned char*)memory);
    fn_802E8B10(vtxDesc, vbHeader, &vtxBufData, 0, NULL);
    DCFlushRange(vbHeader, size);
    GXInvalidateVtxCache();

    for (i = 0; i < geometry->mesh->numMeshes; i++) {
        RwFree(indexDataRemapped[i]);
    }
    RwFree(indexDataRemapped);
    RwFree(vertexDataRemapped);

    return resEntry;
} // 0x802E5568

/* Initializes the shared vertex descriptor for a fast-instanced geometry. */
static rwVertexDescriptor* VtxDescInitFast(RpGeometry* geometry) {
    unsigned int numAttr;
    unsigned int numVerts;
    RpGameCubeVtxFmt* vtxFmt;

    numVerts = geometry->numVertices;
    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    _rwVertexDescriptorInit(&lbl_80519510);
    _rwGCNVertexDescSetVAT(&lbl_80519510, 0);
    _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_POS, rwGCNCC_POS_XYZ, (rwGCNCompType)vtxFmt->pos,
                                   vtxFmt->posFrac);
    _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_POS,
                                   (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
    numAttr = 1;

    if (geometry->flags & rpGEOMETRYNORMALS) {
        if (vtxFmt->nbt != 0) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_NBT, rwGCNCC_NRM_NBT, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_NBT,
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        } else {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_NRM, rwGCNCC_NRM_XYZ, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_NRM,
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        }
        numAttr = 2;
    }

    if (geometry->flags & rpGEOMETRYPRELIT) {
        _rwGCNVertexDescSetElementAttr(&lbl_80519510, rwGCNVA_CLR0,
                                       (vtxFmt->preLight > 2) ? rwGCNCC_CLR_RGBA : rwGCNCC_CLR_RGB,
                                       (rwGCNCompType)vtxFmt->preLight, 0);
        _rwGCNVertexDescSetElementDesc(&lbl_80519510, rwGCNVA_CLR0,
                                       (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        numAttr++;
    }

    if (geometry->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        int j;

        for (j = 0; j < geometry->numTexCoordSets; j++) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519510, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j), rwGCNCC_TEX_ST,
                                           (rwGCNCompType)vtxFmt->texCoord[j], vtxFmt->texCoordFrac[j]);
            _rwGCNVertexDescSetElementDesc(&lbl_80519510, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j),
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
            numAttr++;
        }
    }

    _rwGCNVertexDescSetNumIndexedAttr(&lbl_80519510, numAttr);
    return &lbl_80519510;
} // 0x802E594C

/* Populates vertex buffer attribute counts and pointers directly from the geometry's vertex arrays. */
static void VertexDataFastSetup(rwGCNVertexBufferData* vtxBufferData, RpGeometry* geometry) {
    RpMorphTarget* morphTarget;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    morphTarget = geometry->morphTarget;
    vtxBufferData->num[GX_VA_POS] = geometry->numVertices;
    vtxBufferData->data[GX_VA_POS] = morphTarget->verts;

    if (geometry->flags & rpGEOMETRYNORMALS) {
        vtxBufferData->num[GX_VA_NRM] = geometry->numVertices;
        vtxBufferData->data[GX_VA_NRM] = morphTarget->normals;
    }

    if (geometry->flags & rpGEOMETRYPRELIT) {
        vtxBufferData->num[GX_VA_CLR0] = geometry->numVertices;
        vtxBufferData->data[GX_VA_CLR0] = geometry->preLitLum;
    }

    if (geometry->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        int i;

        for (i = 0; i < geometry->numTexCoordSets; i++) {
            vtxBufferData->num[GX_VA_TEX0 + i] = geometry->numVertices;
            vtxBufferData->data[GX_VA_TEX0 + i] =
                RpGeometryGetVertexTexCoords(geometry, rwTEXTURECOORDINATEINDEX0 + i);
        }
    }
} // 0x802E5B34

/* Populates display list index pointers directly from a mesh's index array (helper inlined in the fast instancer; name invented). */
static inline void IndexDataFastSetup(rwGCNDisplayListData* indexBufferData, unsigned short* indices,
                                      RpGeometry* geometry) {
    RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);

    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    indexBufferData->data[GX_VA_POS] = indices;

    if (geometry->flags & rpGEOMETRYNORMALS) {
        indexBufferData->data[GX_VA_NRM] = indices;
    }

    if (geometry->flags & rpGEOMETRYPRELIT) {
        indexBufferData->data[GX_VA_CLR0] = indices;
    }

    if (geometry->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        int i;

        for (i = 0; i < geometry->numTexCoordSets; i++) {
            indexBufferData->data[GX_VA_TEX0 + i] = indices;
        }
    }
}

/* Instances a geometry directly without condensing vertex streams. */
RwResEntry* _rwDlGeometryInstanceFast(RpGeometry* geometry, void* owner, RwResEntry** resEntryOwner) {
    unsigned int i;
    /* RULE-EXCEPTION: R30, declaration order: Gormiti DWARF lists displayLists after vbHeader;
       declared before size it gets retail's register numbering */
    RxGameCubeDisplayList* displayLists;
    unsigned int size;
    unsigned int vBufferHeaderSize;
    unsigned int vBufferSize;
    RwResEntry* resEntry;
    void* memory;
    RxGameCubeVertexBuffer* vbHeader;
    unsigned int displayListSize;
    rwVertexDescriptor* vtxDesc;
    rwGCNVertexBufferData vtxBufData;
    unsigned char primTypeVAT;

    vtxDesc = VtxDescInitFast(geometry);
    VertexDataFastSetup(&vtxBufData, geometry);

    vBufferHeaderSize = _rwGCNVertexBufferHeaderGetSize(vtxDesc);
    displayListSize = geometry->mesh->numMeshes * sizeof(RxGameCubeDisplayList);
    size = vBufferHeaderSize + displayListSize + 31;

    if (geometry->flags & rpGEOMETRYTRISTRIP) {
        primTypeVAT = GX_TRIANGLESTRIP;
    } else {
        primTypeVAT = GX_TRIANGLES;
    }

    for (i = 0; i < geometry->mesh->numMeshes; i++) {
        size += _rwGCNDisplayListGetSize(vtxDesc, 1, ((RpMesh*)(geometry->mesh + 1))[i].numIndices);
    }

    vBufferSize = _rwGCNVertexBufferGetSize(vtxDesc, &vtxBufData);
    size += vBufferSize;

    if (geometry->flags & rpGEOMETRYNATIVEINSTANCE) {
        resEntry = (RwResEntry*)RwMalloc(size + sizeof(RwResEntry), rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
        resEntry->link.next = NULL;
        resEntry->link.prev = NULL;
        resEntry->owner = owner;
        resEntry->size = size;
        resEntry->ownerRef = resEntryOwner;
        resEntry->destroyNotify = _rxGCResEntryWaitDone;
        *resEntryOwner = resEntry;
    } else {
        resEntry = RwResourcesAllocateResEntry(owner, resEntryOwner, size, _rxGCResEntryWaitDone);
    }

    memory = (void*)(resEntry + 1);
    vbHeader = (RxGameCubeVertexBuffer*)memory;
    memset(vbHeader, 0, size);
    vbHeader->token = _RwDlTokenLastSeen;
    vbHeader->serialNumber = geometry->mesh->serialNum;
    vbHeader->flags = 0;

    if (geometry->flags & rpGEOMETRYPRELIT) {
        RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);

        if (vtxFmt == NULL) {
            int i;

            vbHeader->flags &= ~1;
            for (i = 0; i < geometry->numVertices; i++) {
                if (geometry->preLitLum[i].alpha < 255) {
                    vbHeader->flags |= 1;
                    break;
                }
            }
        } else if (vtxFmt->preLight > 2) {
            vbHeader->flags |= 1;
        } else {
            vbHeader->flags &= ~1;
        }
    }

    displayLists = (RxGameCubeDisplayList*)((unsigned char*)vbHeader + vBufferHeaderSize);
    memory = (void*)(((unsigned int)displayLists + displayListSize + 31) & ~31);

    for (i = 0; i < geometry->mesh->numMeshes; i++) {
        rwGCNDisplayListData displayListData;
        unsigned int displayListSize;

        displayListSize = _rwGCNDisplayListGetSize(vtxDesc, 1, ((RpMesh*)(geometry->mesh + 1))[i].numIndices);
        _rwGCNDisplayListInitialize(&displayLists[i], i, displayListSize, memory);
        IndexDataFastSetup(&displayListData, ((RpMesh*)(geometry->mesh + 1))[i].indices, geometry);
        _rwGCNDisplayListFill(vtxDesc, &displayLists[i], &displayListData,
                              ((RpMesh*)(geometry->mesh + 1))[i].numIndices, 0, _rwGCNDisplayListGetStride(vtxDesc), 1,
                              primTypeVAT, NULL);
        memory = (unsigned char*)memory + displayListSize;
    }

    _rwGCNVertexBufferInitialize(vtxDesc, vbHeader, &vtxBufData, (unsigned char*)memory);
    fn_802E8B10(vtxDesc, vbHeader, &vtxBufData, 0, NULL);
    DCFlushRange(vbHeader, size);
    GXInvalidateVtxCache();

    return resEntry;
} // 0x802E5C0C
