// Unit name and boundary: Gormiti link map (RPworld.a instanceworld.obj)
// Names and layout: Gormiti link map and DWARF (RPworld.a instanceworld.obj)
/*
 * instanceworld.c: instances an RpWorldSector into a GameCube vertex buffer and display lists.
 * The optimized path condenses duplicate vertex attributes per attribute stream and strips
 * degenerate triangles; the fast path copies the sector's vertex arrays directly.
 */
#include <string.h>

#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamesh.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/geomcond.h>
#include <rwsdk/world/ibuffer.h>
#include <rwsdk/world/instanceworld.h>
#include <rwsdk/world/itools.h>
#include <rwsdk/world/vbuffer.h>
#include <rwsdk/world/vtools.h>
#include <rwsdk/world/vtxdesc.h>
#include <rwsdk/world/vtxfmt.h>

/* Vertex descriptor populated by VtxDescInitOptimized and VtxDescInitFast. */
static rwVertexDescriptor lbl_80519530;

/*
 * Condenses a world sector's vertex attribute streams and builds per-mesh remapped vertex and index
 * tables for the optimized instancer.
 */
static int ReconditionVertexIndexData(RpWorld* world, RpWorldSector* sector,
                                      rwGCNVtxDataRemapped** vertexDataRemapped,
                                      rwGCNIndexDataRemapped*** indexDataRemapped) {
    rwGCNVtxData vtxData[16];
    rwGCNVtxDataMap* vertexDataMaps;
    rwGCNIndexData* indexData;
    int numEntries;
    int numMeshes;
    int i;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    numEntries = 0;
    vtxData[numEntries].data = sector->vertices;
    vtxData[numEntries].type = rwDATATYPE_V3D;
    vtxData[numEntries].dep[0] = -1;
    numEntries++;

    if (world->flags & rpWORLDNORMALS) {
        int i;

        for (i = 0; i < sector->numVertices; i++) {
            sector->normals[i].pad = 0;
        }
        vtxData[numEntries].data = sector->normals;
        vtxData[numEntries].type = rwDATATYPE_INT32;
        vtxData[numEntries].dep[0] = -1;
        numEntries++;
    }

    if (world->flags & rpWORLDPRELIT) {
        vtxData[numEntries].data = sector->preLitLum;
        vtxData[numEntries].type = rwDATATYPE_INT32;
        vtxData[numEntries].dep[0] = -1;
        numEntries++;
    }

    if (world->flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        for (i = 0; i < world->numTexCoordSets; i++) {
            vtxData[numEntries].data = sector->texCoords[i];
            vtxData[numEntries].type = rwDATATYPE_V2D;
            vtxData[numEntries].dep[0] = -1;
            numEntries++;
        }
    }

    vertexDataMaps = VertexDataCreateMaps(vtxData, numEntries, sector->numVertices);
    if (vertexDataMaps == NULL) {
        return 0;
    }

    *vertexDataRemapped = VertexDataCreateRemapped(vertexDataMaps, vtxData, numEntries, sector->numVertices);
    if (*vertexDataRemapped == NULL) {
        RwFree(vertexDataMaps);
        return 0;
    }

    numMeshes = sector->mesh->numMeshes;
    indexData = (rwGCNIndexData*)RwMalloc(numEntries * (numMeshes * sizeof(rwGCNIndexData)),
                                          rwMEMHINTDUR_FUNCTION | rwID_WORLDPIPEMODULE);
    if (indexData == NULL) {
        RwFree(*vertexDataRemapped);
        RwFree(vertexDataMaps);
        return 0;
    }

    memset(indexData, 0, numEntries * (numMeshes * sizeof(rwGCNIndexData)));
    for (i = 0; i < numMeshes; i++) {
        RpMesh* mesh = &((RpMesh*)(sector->mesh + 1))[i];
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
        RpMesh* mesh = &((RpMesh*)(sector->mesh + 1))[i];

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
} // 0x802E5F60

/* Initializes the shared vertex descriptor for an optimized (condensed) world sector. */
static rwVertexDescriptor* VtxDescInitOptimized(RpWorld* world, rwGCNVtxDataRemapped* vertexDataRemapped) {
    unsigned int numAttr;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    _rwVertexDescriptorInit(&lbl_80519530);
    _rwGCNVertexDescSetVAT(&lbl_80519530, 0);
    _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_POS, rwGCNCC_POS_XYZ, (rwGCNCompType)vtxFmt->pos,
                                   vtxFmt->posFrac);
    _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_POS,
                                   (vertexDataRemapped[0].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
    numAttr = 1;

    if (world->flags & rpWORLDNORMALS) {
        if (vtxFmt->nbt != 0) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_NBT, rwGCNCC_NRM_NBT, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_NBT,
                                           (vertexDataRemapped[1].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        } else {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_NRM, rwGCNCC_NRM_XYZ, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_NRM,
                                           (vertexDataRemapped[1].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        }
        numAttr = 2;
    }

    if (world->flags & rpWORLDPRELIT) {
        _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_CLR0,
                                       (vtxFmt->preLight > 2) ? rwGCNCC_CLR_RGBA : rwGCNCC_CLR_RGB,
                                       (rwGCNCompType)vtxFmt->preLight, 0);
        _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_CLR0,
                                       (vertexDataRemapped[numAttr].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        numAttr++;
    }

    if (world->flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        int j;

        for (j = 0; j < world->numTexCoordSets; j++) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j), rwGCNCC_TEX_ST,
                                           (rwGCNCompType)vtxFmt->texCoord[j], vtxFmt->texCoordFrac[j]);
            _rwGCNVertexDescSetElementDesc(
                &lbl_80519530, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j),
                (vertexDataRemapped[numAttr].num >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
            numAttr++;
        }
    }

    _rwGCNVertexDescSetNumIndexedAttr(&lbl_80519530, numAttr);
    return &lbl_80519530;
} // 0x802E63C0

/* Populates vertex buffer attribute counts and pointers from condensed vertex data (VertexDataSetupOptimized in Gormiti). */
static void fn_802E65D0(rwGCNVertexBufferData* vtxBufferData, rwGCNVtxDataRemapped* vtxDataRemapped, RpWorld* world) {
    unsigned int numElements;
    unsigned int flags = world->flags;
    unsigned char numTexCoords = (unsigned char)world->numTexCoordSets;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    vtxBufferData->num[GX_VA_POS] = vtxDataRemapped[0].num;
    vtxBufferData->data[GX_VA_POS] = vtxDataRemapped[0].data;
    numElements = 1;

    if (flags & rpWORLDNORMALS) {
        vtxBufferData->num[GX_VA_NRM] = vtxDataRemapped[1].num;
        vtxBufferData->data[GX_VA_NRM] = vtxDataRemapped[1].data;
        numElements = 2;
    }

    if (flags & rpWORLDPRELIT) {
        vtxBufferData->num[GX_VA_CLR0] = vtxDataRemapped[numElements].num;
        vtxBufferData->data[GX_VA_CLR0] = vtxDataRemapped[numElements].data;
        numElements++;
    }

    if (flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        unsigned int i;

        for (i = 0; i < numTexCoords; i++) {
            vtxBufferData->num[GX_VA_TEX0 + i] = vtxDataRemapped[numElements].num;
            vtxBufferData->data[GX_VA_TEX0 + i] = vtxDataRemapped[numElements].data;
            numElements++;
        }
    }
} // 0x802E65D0

/* Populates display list index pointers from remapped mesh indices. */
static void IndexDataSetupOptimized(rwGCNDisplayListData* indexBufferData, rwGCNIndexDataRemapped* indexDataRemapped,
                                    RpWorld* world) {
    unsigned int numElements;
    unsigned int flags = world->flags;
    unsigned char numTexCoords = (unsigned char)world->numTexCoordSets;
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    indexBufferData->data[GX_VA_POS] = indexDataRemapped[0].indices;
    numElements = 1;

    if (flags & rpWORLDNORMALS) {
        indexBufferData->data[GX_VA_NRM] = indexDataRemapped[1].indices;
        numElements = 2;
    }

    if (flags & rpWORLDPRELIT) {
        indexBufferData->data[GX_VA_CLR0] = indexDataRemapped[numElements].indices;
        numElements++;
    }

    if (flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        unsigned int i;

        for (i = 0; i < numTexCoords; i++) {
            indexBufferData->data[GX_VA_TEX0 + i] = indexDataRemapped[numElements].indices;
            numElements++;
        }
    }
} // 0x802E67D4

/* Instances a world sector using condensed vertex streams and stripped display lists. */
RwResEntry* _rwDlWorldSectorInstanceOptimized(RpWorld* world, RpWorldSector* sector, void* owner,
                                               RwResEntry** resEntryOwner) {
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

    ReconditionVertexIndexData(world, sector, &vertexDataRemapped, &indexDataRemapped);
    vtxDesc = VtxDescInitOptimized(world, vertexDataRemapped);
    fn_802E65D0(&vtxBufData, vertexDataRemapped, world);

    vBufferHeaderSize = _rwGCNVertexBufferHeaderGetSize(vtxDesc);
    displayListSize = sector->mesh->numMeshes * sizeof(RxGameCubeDisplayList);
    size = vBufferHeaderSize + displayListSize + 31;

    if (world->flags & rpWORLDTRISTRIP) {
        primTypeVAT = GX_TRIANGLESTRIP;
    } else {
        primTypeVAT = GX_TRIANGLES;
    }

    for (i = 0; i < sector->mesh->numMeshes; i++) {
        if (world->flags & rpWORLDTRISTRIP) {
            unsigned int numStrips;
            unsigned int numIndices;

            _rwGCNTriStripGetStats(indexDataRemapped[i][0].indices,
                                   ((RpMesh*)(sector->mesh + 1))[i].numIndices, &numStrips, &numIndices, 1);
            size += _rwGCNDisplayListGetSize(vtxDesc, numStrips, numIndices);
        } else {
            unsigned int numIndices = ((RpMesh*)(sector->mesh + 1))[i].numIndices;
            size += _rwGCNDisplayListGetSize(vtxDesc, 1, numIndices);
        }
    }

    vBufferSize = _rwGCNVertexBufferGetSize(vtxDesc, &vtxBufData);
    size += vBufferSize;

    resEntry = (RwResEntry*)RwMalloc(size + sizeof(RwResEntry), rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    resEntry->link.next = NULL;
    resEntry->link.prev = NULL;
    resEntry->owner = sector;
    resEntry->size = size;
    resEntry->ownerRef = &sector->repEntry;
    resEntry->destroyNotify = _rxGCResEntryWaitDone;
    sector->repEntry = resEntry;

    memory = (void*)(resEntry + 1);
    vbHeader = (RxGameCubeVertexBuffer*)memory;
    memset(vbHeader, 0, size);
    vbHeader->token = _RwDlTokenLastSeen;
    vbHeader->serialNumber = sector->mesh->serialNum;
    vbHeader->flags = 0;

    if (world->flags & rpWORLDPRELIT) {
        RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);

        if (vtxFmt == NULL) {
            int i;

            vbHeader->flags &= ~1;
            for (i = 0; i < sector->numVertices; i++) {
                if (sector->preLitLum[i].alpha < 255) {
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

    for (i = 0; i < sector->mesh->numMeshes; i++) {
        unsigned int numStrips;
        unsigned int numIndices;
        unsigned int displayListstride;
        unsigned int displayListSize;
        rwGCNDisplayListData displayListData;

        if (world->flags & rpWORLDTRISTRIP) {
            _rwGCNTriStripGetStats(indexDataRemapped[i][0].indices,
                                   ((RpMesh*)(sector->mesh + 1))[i].numIndices, &numStrips, &numIndices, 1);
        } else {
            numIndices = ((RpMesh*)(sector->mesh + 1))[i].numIndices;
            numStrips = 1;
        }

        displayListSize = _rwGCNDisplayListGetSize(vtxDesc, numStrips, numIndices);
        _rwGCNDisplayListInitialize(&displayLists[i], i, displayListSize, memory);
        IndexDataSetupOptimized(&displayListData, indexDataRemapped[i], world);
        displayListstride = _rwGCNDisplayListGetStride(vtxDesc);
        _rwGCNDisplayListFill(vtxDesc, &displayLists[i], &displayListData,
                              ((RpMesh*)(sector->mesh + 1))[i].numIndices, world->flags & rpWORLDTRISTRIP,
                              displayListstride, 1, primTypeVAT, &sector->tightBoundingBox.inf);
        memory = (unsigned char*)memory + displayListSize;
    }

    _rwGCNVertexBufferInitialize(vtxDesc, vbHeader, &vtxBufData, (unsigned char*)memory);
    fn_802E8B10(vtxDesc, vbHeader, &vtxBufData, 1, &sector->tightBoundingBox.inf);
    DCFlushRange(vbHeader, size);
    GXInvalidateVtxCache();

    for (i = 0; i < sector->mesh->numMeshes; i++) {
        RwFree(indexDataRemapped[i]);
    }
    RwFree(indexDataRemapped);
    RwFree(vertexDataRemapped);

    return resEntry;
} // 0x802E696C

/* Initializes the shared vertex descriptor for a fast-instanced world sector. */
static rwVertexDescriptor* VtxDescInitFast(RpWorld* world, RpWorldSector* sector) {
    unsigned int numAttr;
    unsigned int numVerts;
    RpGameCubeVtxFmt* vtxFmt;

    numVerts = sector->numVertices;
    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    _rwVertexDescriptorInit(&lbl_80519530);
    _rwGCNVertexDescSetVAT(&lbl_80519530, 0);
    _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_POS, rwGCNCC_POS_XYZ, (rwGCNCompType)vtxFmt->pos,
                                   vtxFmt->posFrac);
    _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_POS,
                                   (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
    numAttr = 1;

    if (world->flags & rpWORLDNORMALS) {
        if (vtxFmt->nbt != 0) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_NBT, rwGCNCC_NRM_NBT, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_NBT,
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        } else {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_NRM, rwGCNCC_NRM_XYZ, (rwGCNCompType)vtxFmt->norm,
                                           0);
            _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_NRM,
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        }
        numAttr = 2;
    }

    if (world->flags & rpWORLDPRELIT) {
        _rwGCNVertexDescSetElementAttr(&lbl_80519530, rwGCNVA_CLR0,
                                       (vtxFmt->preLight > 2) ? rwGCNCC_CLR_RGBA : rwGCNCC_CLR_RGB,
                                       (rwGCNCompType)vtxFmt->preLight, 0);
        _rwGCNVertexDescSetElementDesc(&lbl_80519530, rwGCNVA_CLR0,
                                       (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
        numAttr++;
    }

    if (world->flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        int j;

        for (j = 0; j < world->numTexCoordSets; j++) {
            _rwGCNVertexDescSetElementAttr(&lbl_80519530, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j), rwGCNCC_TEX_ST,
                                           (rwGCNCompType)vtxFmt->texCoord[j], vtxFmt->texCoordFrac[j]);
            _rwGCNVertexDescSetElementDesc(&lbl_80519530, (rwGCNVertexAttribute)(rwGCNVA_TEX0 + j),
                                           (numVerts >= 255) ? rwGCNAT_INDEX16 : rwGCNAT_INDEX8);
            numAttr++;
        }
    }

    _rwGCNVertexDescSetNumIndexedAttr(&lbl_80519530, numAttr);
    return &lbl_80519530;
} // 0x802E6D4C

/* Populates vertex buffer attribute counts and pointers directly from the sector's vertex arrays. */
static void VertexDataFastSetup(rwGCNVertexBufferData* vtxBufferData, RpWorld* world, RpWorldSector* sector) {
    RpGameCubeVtxFmt* vtxFmt;

    vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);
    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    vtxBufferData->num[GX_VA_POS] = sector->numVertices;
    vtxBufferData->data[GX_VA_POS] = sector->vertices;

    if (world->flags & rpWORLDNORMALS) {
        vtxBufferData->num[GX_VA_NRM] = sector->numVertices;
        vtxBufferData->data[GX_VA_NRM] = sector->normals;
    }

    if (world->flags & rpWORLDPRELIT) {
        vtxBufferData->num[GX_VA_CLR0] = sector->numVertices;
        vtxBufferData->data[GX_VA_CLR0] = sector->preLitLum;
    }

    if (world->flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        int i;

        for (i = 0; i < world->numTexCoordSets; i++) {
            vtxBufferData->num[GX_VA_TEX0 + i] = sector->numVertices;
            vtxBufferData->data[GX_VA_TEX0 + i] = sector->texCoords[i];
        }
    }
} // 0x802E6F34

/* Populates display list index pointers directly from a mesh's index array (helper inlined in the fast instancer; name invented). */
static inline void IndexDataFastSetup(rwGCNDisplayListData* indexBufferData, unsigned short* indices,
                                      RpWorld* world) {
    RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);

    if (vtxFmt == NULL) {
        vtxFmt = fn_802DF0DC();
    }

    indexBufferData->data[GX_VA_POS] = indices;

    if (world->flags & rpWORLDNORMALS) {
        indexBufferData->data[GX_VA_NRM] = indices;
    }

    if (world->flags & rpWORLDPRELIT) {
        indexBufferData->data[GX_VA_CLR0] = indices;
    }

    if (world->flags & (rpWORLDTEXTURED | rpWORLDTEXTURED2)) {
        int i;

        for (i = 0; i < world->numTexCoordSets; i++) {
            indexBufferData->data[GX_VA_TEX0 + i] = indices;
        }
    }
}

/* Instances a world sector directly without condensing vertex streams. */
RwResEntry* _rwDlWorldSectorInstanceFast(RpWorld* world, RpWorldSector* sector, void* owner,
                                         RwResEntry** resEntryOwner) {
    unsigned int i;
    /* RULE-EXCEPTION: R30, declaration order: Gormiti DWARF lists displayLists after vbHeader;
       declared before size it gets retail's register numbering */
    RxGameCubeDisplayList* displayLists;
    unsigned int size;
    unsigned int vBufferHeaderSize;
    unsigned int displayListSize;
    unsigned int vBufferSize;
    RwResEntry* resEntry;
    void* memory;
    RxGameCubeVertexBuffer* vbHeader;
    rwVertexDescriptor* vtxDesc;
    rwGCNVertexBufferData vtxBufData;
    unsigned char primTypeVAT;

    vtxDesc = VtxDescInitFast(world, sector);
    VertexDataFastSetup(&vtxBufData, world, sector);

    vBufferHeaderSize = _rwGCNVertexBufferHeaderGetSize(vtxDesc);
    displayListSize = sector->mesh->numMeshes * sizeof(RxGameCubeDisplayList);
    size = vBufferHeaderSize + displayListSize + 31;

    if (world->flags & rpWORLDTRISTRIP) {
        primTypeVAT = GX_TRIANGLESTRIP;
    } else {
        primTypeVAT = GX_TRIANGLES;
    }

    for (i = 0; i < sector->mesh->numMeshes; i++) {
        unsigned int numIndices = ((RpMesh*)(sector->mesh + 1))[i].numIndices;
        size += _rwGCNDisplayListGetSize(vtxDesc, 1, numIndices);
    }

    vBufferSize = _rwGCNVertexBufferGetSize(vtxDesc, &vtxBufData);
    size += vBufferSize;

    if (world->flags & rpWORLDNATIVEINSTANCE) {
        resEntry = (RwResEntry*)RwMalloc(size + sizeof(RwResEntry), rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
        resEntry->link.next = NULL;
        resEntry->link.prev = NULL;
        resEntry->owner = sector;
        resEntry->size = size;
        resEntry->ownerRef = &sector->repEntry;
        resEntry->destroyNotify = _rxGCResEntryWaitDone;
        sector->repEntry = resEntry;
    } else {
        resEntry = RwResourcesAllocateResEntry(owner, resEntryOwner, size, _rxGCResEntryWaitDone);
    }

    memory = (void*)(resEntry + 1);
    vbHeader = (RxGameCubeVertexBuffer*)memory;
    memset(vbHeader, 0, size);
    vbHeader->token = _RwDlTokenLastSeen;
    vbHeader->serialNumber = sector->mesh->serialNum;
    vbHeader->flags = 0;

    if (world->flags & rpWORLDPRELIT) {
        RpGameCubeVtxFmt* vtxFmt = *RWPLUGINOFFSET(RpGameCubeVtxFmt*, world, _rpDlWorldVtxFmtOffset);

        if (vtxFmt == NULL) {
            int i;

            vbHeader->flags &= ~1;
            for (i = 0; i < sector->numVertices; i++) {
                if (sector->preLitLum[i].alpha < 255) {
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

    for (i = 0; i < sector->mesh->numMeshes; i++) {
        unsigned int displayListSize;
        rwGCNDisplayListData displayListData;

        displayListSize = _rwGCNDisplayListGetSize(vtxDesc, 1, ((RpMesh*)(sector->mesh + 1))[i].numIndices);
        _rwGCNDisplayListInitialize(&displayLists[i], i, displayListSize, memory);
        IndexDataFastSetup(&displayListData, ((RpMesh*)(sector->mesh + 1))[i].indices, world);
        _rwGCNDisplayListFill(vtxDesc, &displayLists[i], &displayListData,
                              ((RpMesh*)(sector->mesh + 1))[i].numIndices, 0, _rwGCNDisplayListGetStride(vtxDesc), 1,
                              primTypeVAT, &sector->tightBoundingBox.inf);
        memory = (unsigned char*)memory + displayListSize;
    }

    _rwGCNVertexBufferInitialize(vtxDesc, vbHeader, &vtxBufData, (unsigned char*)memory);
    fn_802E8B10(vtxDesc, vbHeader, &vtxBufData, 1, &sector->tightBoundingBox.inf);
    DCFlushRange(vbHeader, size);
    GXInvalidateVtxCache();

    return resEntry;
} // 0x802E7010
