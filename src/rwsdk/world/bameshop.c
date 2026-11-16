// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (function names)
// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Unit name and boundary: Gormiti link map (RPworld.a bameshop)
// Locals of SortPolygons: Gormiti DWARF
/*
 * bameshop.c: mesh building. A build mesh's triangles are sorted by material into a triangle-list
 * mesh header, or joined into triangle strips by the default tristripper.
 */
#include <rwsdk/world/rpplugin.h>

#include <stdlib.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batextur.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bameshop.h>

/*
 * The tristripper's triangles and edges. Type and member names invented.
 *
 * A triangle sits in the bin of its count of unused neighbours (0 to 3); the strips start from
 * the triangles with the fewest.
 */
typedef struct TriStripTri TriStripTri;
typedef struct TriStripEdge TriStripEdge;

struct TriStripTri {
    RwUInt32 index;             /* 0x0: index of the triangle in the build mesh */
    TriStripEdge* edges[3];     /* 0x4 */
    TriStripTri* next;          /* 0x10: next triangle of its bin */
    TriStripTri* prev;          /* 0x14: previous triangle of its bin */
    RwBool used;                /* 0x18: in a strip */
    RwBool testUsed;            /* 0x1C: in the strip being tried */
    RwUInt8 numNeighbours;      /* 0x20: unused neighbours; the triangle's bin */
};

/* An edge, shared by up to two triangles. */
struct TriStripEdge {
    RxVertexIndex vertIndex[2]; /* 0x0 */
    TriStripTri* tris[2];       /* 0x4 */
    TriStripEdge* next;         /* 0xC: next edge of the edge list */
};

/* The free lists the triangles and edges come from. */
typedef struct TriStripFreeLists {
    RwFreeList* triFreeList;    /* 0x0 */
    RwFreeList* edgeFreeList;   /* 0x4 */
} TriStripFreeLists;

/* A strip of the strip list. Type and member names invented. */
typedef struct TriStripListEntry TriStripListEntry;

struct TriStripListEntry {
    RxVertexIndex* indices;     /* 0x0 */
    RwUInt32 numIndices;        /* 0x4 */
    RwUInt32 maxIndices;        /* 0x8: room the strips being joined need */
    TriStripListEntry* next;    /* 0xC */
};

/* The final pass of the stripper; the passes before it only try strips. Name invented. */
#define TRISTRIPFINALPASS 4

/* The count of triangles on an edge that are not yet in a strip (flag testUsed or used). */
#define TriStripEdgeCountFreeTris(_edge, _flag)                                                \
    (((_edge)->tris[0] != NULL && !(_edge)->tris[0]->_flag) +                                  \
     ((_edge)->tris[1] != NULL && !(_edge)->tris[1]->_flag))

/* An edge that leads to no triangle not yet in a strip (flag testUsed or used). */
#define TriStripEdgeIsDeadEnd(_edge, _flag)                                                    \
    ((_edge) == NULL || TriStripEdgeCountFreeTris(_edge, _flag) == 0)

/* An edge that leads to a triangle not yet in a strip (flag testUsed or used). */
#define TriStripEdgeHasFreeTris(_edge, _flag)                                                  \
    ((_edge) != NULL && TriStripEdgeCountFreeTris(_edge, _flag) != 0)

/* The first triangle on an edge that is not yet in a strip (flag testUsed or used), or NULL. */
#define TriStripEdgeGetFreeTri(_edge, _tri, _flag)                                             \
    do {                                                                                       \
        if ((_edge)->tris[0] != NULL && !(_edge)->tris[0]->_flag) {                            \
            (_tri) = (_edge)->tris[0];                                                         \
        } else if ((_edge)->tris[1] != NULL && !(_edge)->tris[1]->_flag) {                     \
            (_tri) = (_edge)->tris[1];                                                         \
        }                                                                                      \
    } while (0)

/*
 * The empty mesh header returned for a build mesh without triangles, and the tristrip method
 * (RpMeshSetTriStripMethod's callback and data). Type and member names invented.
 */
typedef struct rpMeshopStatic {
    RpMeshHeader emptyMeshHeader;           /* 0x0 */
    RpTriStripMeshCallBack meshGenerator;   /* 0x10 */
    void* meshGeneratorData;                /* 0x14 */
} rpMeshopStatic;

static rpMeshopStatic MeshopStatic = {
    {0, 0, 0, 0},
    RpBuildMeshGenerateDefaultTriStrip,
    NULL,
};

/* A material is transparent when its colour or its texture's raster has alpha. */
#define RasterHasAlpha(_raster)                                                                \
    ((RwRasterGetFormat(_raster) & rwRASTERFORMATPIXELFORMATMASK) == rwRASTERFORMAT1555 ||     \
     (RwRasterGetFormat(_raster) & rwRASTERFORMATPIXELFORMATMASK) == rwRASTERFORMAT4444 ||     \
     (RwRasterGetFormat(_raster) & rwRASTERFORMATPIXELFORMATMASK) == rwRASTERFORMAT8888)

/* The two triangles a qsort callback compares, reached through its element pointers. */
#define SORT_TRIANGLE_A (*(RpBuildMeshTriangle* const*)pA)
#define SORT_TRIANGLE_B (*(RpBuildMeshTriangle* const*)pB)

/*
 * qsort comparison of two build-mesh triangles: opaque before transparent, then by raster,
 * pipeline, texture and material index.
 * Locals: Gormiti DWARF (materialA, materialB, orderA, orderB; no triangle locals).
 */
static int SortPolygons(const void* pA, const void* pB) {
    RwUInt32 orderA = 0;
    RwUInt32 orderB = 0;
    RpMaterial* materialA = SORT_TRIANGLE_A->material;
    RpMaterial* materialB = SORT_TRIANGLE_B->material;

    if (materialA == materialB) {
        return 0;
    }

    if (materialA != NULL) {
        if (materialA->texture != NULL && RasterHasAlpha(RwTextureGetRaster(materialA->texture))) {
            orderA |= 0x10;
        }
        if (materialA->color.alpha != 0xFF) {
            orderA |= 0x10;
        }
    }

    if (materialB != NULL) {
        if (materialB->texture != NULL && RasterHasAlpha(RwTextureGetRaster(materialB->texture))) {
            orderB |= 0x10;
        }
        if (materialB->color.alpha != 0xFF) {
            orderB |= 0x10;
        }
    }

    orderA |= (SORT_TRIANGLE_A->rasterIndex > SORT_TRIANGLE_B->rasterIndex) ? 8 : 0;
    orderB |= (SORT_TRIANGLE_A->rasterIndex < SORT_TRIANGLE_B->rasterIndex) ? 8 : 0;
    orderA |= (SORT_TRIANGLE_A->pipelineIndex > SORT_TRIANGLE_B->pipelineIndex) ? 4 : 0;
    orderB |= (SORT_TRIANGLE_A->pipelineIndex < SORT_TRIANGLE_B->pipelineIndex) ? 4 : 0;
    orderA |= (SORT_TRIANGLE_A->textureIndex > SORT_TRIANGLE_B->textureIndex) ? 2 : 0;
    orderB |= (SORT_TRIANGLE_A->textureIndex < SORT_TRIANGLE_B->textureIndex) ? 2 : 0;
    orderA |= (SORT_TRIANGLE_A->matIndex > SORT_TRIANGLE_B->matIndex) ? 1 : 0;
    orderB |= (SORT_TRIANGLE_A->matIndex < SORT_TRIANGLE_B->matIndex) ? 1 : 0;

    return orderA - orderB;
} // 0x802D5970

/*
 * _rpMeshHeaderForAllMeshes callback: reorders a triangle-list mesh's triangles so that
 * triangles sharing no vertex follow each other (each pass takes, in order, the triangles none of
 * whose vertices an earlier triangle of the pass used).
 */
static RpMesh* SortPolygonsInTriListMesh(RpMesh* mesh, RpMeshHeader* meshHeader, void* data) {
    RwUInt32 bitsSize;
    RwUInt32* vertexUsed;
    RwUInt32 numTris;
    RxVertexIndex* dest;
    RxVertexIndex* source;
    RwUInt32 numIndices;
    RwUInt32 maxIndex;
    RwUInt32 numSorted;
    RwUInt32 i;
    RwUInt32 sourceSize;

    maxIndex = 0;
    numIndices = mesh->numIndices;
    dest = mesh->indices;
    numTris = numIndices / 3;

    for (i = 0; i < numIndices; i++) {
        maxIndex |= dest[i];
    }

    bitsSize = (maxIndex + 31) >> 3;
    vertexUsed = (RwUInt32*)RwMalloc(bitsSize, rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    if (vertexUsed != NULL) {
        sourceSize = mesh->numIndices * sizeof(RxVertexIndex);
        source = (RxVertexIndex*)RwMalloc(sourceSize, rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
        if (source != NULL) {
            memcpy(source, dest, sourceSize);

            numSorted = 0;
            while (numSorted < numTris) {
                {
                    RwUInt8* bytes = (RwUInt8*)vertexUsed;
                    RwUInt32 count = bitsSize;

                    while (count--) {
                        *bytes++ = 0;
                    }
                }

                for (i = 0; i < numTris; i++) {
                    RxVertexIndex* tri = &source[i * 3];

                    if ((tri[0] & tri[1] & tri[2]) != 0xFFFF &&
                        !(vertexUsed[tri[0] >> 5] & (1 << (tri[0] & 31))) &&
                        !(vertexUsed[tri[1] >> 5] & (1 << (tri[1] & 31))) &&
                        !(vertexUsed[tri[2] >> 5] & (1 << (tri[2] & 31)))) {
                        vertexUsed[tri[0] >> 5] |= 1 << (tri[0] & 31);
                        numSorted++;
                        vertexUsed[tri[1] >> 5] |= 1 << (tri[1] & 31);
                        vertexUsed[tri[2] >> 5] |= 1 << (tri[2] & 31);

                        dest[0] = tri[0];
                        dest[1] = tri[1];
                        dest[2] = tri[2];
                        dest += 3;

                        tri[2] = 0xFFFF;
                        tri[1] = 0xFFFF;
                        tri[0] = 0xFFFF;
                    }
                }
            }

            RwFree(source);
        }
        RwFree(vertexUsed);
        return mesh;
    }

    return mesh;
} // 0x802D5AF0

/*
 * Adds the edge v0-v1 of triangle triIndex to the edge list. A triangle already there with the
 * edge the other way round becomes its neighbour.
 */
static TriStripEdge* TriStripAddEdge(RwFreeList* edgeFreeList, TriStripEdge** edgeList, RwUInt32 v0,
                                     RwUInt32 v1, RwUInt32 triIndex, TriStripTri** tris) {
    TriStripEdge* edge;

    for (edge = *edgeList; edge != NULL; edge = edge->next) {
        if (v0 == edge->vertIndex[1] && v1 == edge->vertIndex[0] && edge->tris[1] == NULL) {
            edge->tris[0]->numNeighbours++;
            tris[triIndex]->numNeighbours++;
            edge->tris[1] = tris[triIndex];
            return edge;
        }
    }

    edge = (TriStripEdge*)RwFreeListAlloc(edgeFreeList, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
    edge->vertIndex[0] = v0;
    edge->vertIndex[1] = v1;
    edge->tris[0] = tris[triIndex];
    edge->tris[1] = NULL;
    edge->next = *edgeList;
    *edgeList = edge;

    return edge;
} // 0x802D5DF0

/* Frees the edges and triangles and their free lists, and the triangle array. */
static TriStripTri** TriStripBinEntryArrayDestroy(RwUInt32 numTris, TriStripFreeLists* freeLists,
                                                  TriStripEdge* edgeList, TriStripTri** tris) {
    RwUInt32 i;

    while (edgeList != NULL) {
        TriStripEdge* next = edgeList->next;

        RwFreeListFree(freeLists->edgeFreeList, edgeList);
        edgeList = next;
    }
    RwFreeListDestroy(freeLists->edgeFreeList);
    freeLists->edgeFreeList = NULL;

    for (i = 0; i < numTris; i++) {
        RwFreeListFree(freeLists->triFreeList, tris[i]);
        tris[i] = NULL;
    }
    RwFreeListDestroy(freeLists->triFreeList);
    freeLists->triFreeList = NULL;

    RwFree(tris);

    return NULL;
} // 0x802D5ED4

/* Builds a triangle for each build-mesh triangle, with its three edges. */
static TriStripTri** TriStripBinEntryArrayCreate(RwUInt32 numTris, TriStripFreeLists* freeLists,
                                                 TriStripEdge** edgeList, RpBuildMeshTriangle* triangles) {
    TriStripTri** tris;
    RwUInt32 i;

    tris = (TriStripTri**)RwMalloc(numTris * sizeof(TriStripTri*), rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
    freeLists->triFreeList =
        RwFreeListCreate(sizeof(TriStripTri), numTris, 4, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
    freeLists->edgeFreeList =
        RwFreeListCreate(sizeof(TriStripEdge), numTris / 2 + 1, 4, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);

    for (i = 0; i < numTris; i++) {
        tris[i] = (TriStripTri*)RwFreeListAlloc(freeLists->triFreeList, rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
        tris[i]->numNeighbours = 0;
        tris[i]->index = i;
        tris[i]->prev = NULL;
        tris[i]->next = NULL;
        tris[i]->used = FALSE;
        tris[i]->edges[0] = TriStripAddEdge(freeLists->edgeFreeList, edgeList, triangles[i].vertIndex[0],
                                            triangles[i].vertIndex[1], i, tris);
        tris[i]->edges[1] = TriStripAddEdge(freeLists->edgeFreeList, edgeList, triangles[i].vertIndex[1],
                                            triangles[i].vertIndex[2], i, tris);
        tris[i]->edges[2] = TriStripAddEdge(freeLists->edgeFreeList, edgeList, triangles[i].vertIndex[2],
                                            triangles[i].vertIndex[0], i, tris);
    }

    return tris;
} // 0x802D5FA0

/* Whether an edge joins vertices v0 and v1 (either way round). */
#define TriStripEdgeJoins(_edge, _v0, _v1)                                                     \
    (((_edge)->vertIndex[0] == (_v0) && (_edge)->vertIndex[1] == (_v1)) ||                     \
     ((_edge)->vertIndex[0] == (_v1) && (_edge)->vertIndex[1] == (_v0)))

/* Returns the edge of a triangle between vertices v0 and v1 (either way round), or NULL. */
static TriStripEdge* TriStripGetTriEdge(TriStripTri* tri, RwUInt32 v0, RwUInt32 v1) {
    if (TriStripEdgeJoins(tri->edges[0], v0, v1)) {
        return tri->edges[0];
    }
    if (TriStripEdgeJoins(tri->edges[1], v0, v1)) {
        return tri->edges[1];
    }
    if (TriStripEdgeJoins(tri->edges[2], v0, v1)) {
        return tri->edges[2];
    }

    return NULL;
} // 0x802D60FC

/* Takes a triangle out of its bin. */
static void TriStripBinRemove(TriStripTri* tri, TriStripTri** bins) {
    if (bins[tri->numNeighbours] == tri) {
        bins[tri->numNeighbours] = bins[tri->numNeighbours]->next;
        if (bins[tri->numNeighbours] != NULL) {
            bins[tri->numNeighbours]->prev = NULL;
        }
    } else {
        if (tri->next != NULL) {
            tri->next->prev = tri->prev;
        }
        if (tri->prev != NULL) {
            tri->prev->next = tri->next;
        }
    }
}

/*
 * Marks a triangle used. A trial pass (below 4) only marks it for the strip being tried; the
 * final pass takes it out of its bin and moves each unused neighbour down one bin.
 */
static void TriStripMarkTriUsed(TriStripTri* tri, TriStripTri** bins, RwInt32 pass) {
    RwUInt8 i;

    if (pass < 4) {
        tri->testUsed = TRUE;
        return;
    }

    tri->used = TRUE;
    TriStripBinRemove(tri, bins);

    for (i = 0; i < 3; i++) {
        TriStripTri* neighbour = NULL;

        if (tri->edges[i]->tris[0] != NULL && tri->edges[i]->tris[0] != tri &&
            !tri->edges[i]->tris[0]->used) {
            neighbour = tri->edges[i]->tris[0];
        } else if (tri->edges[i]->tris[1] != NULL && !tri->edges[i]->tris[1]->used) {
            neighbour = tri->edges[i]->tris[1];
        }

        if (neighbour != NULL) {
            TriStripBinRemove(neighbour, bins);
            neighbour->numNeighbours--;
            neighbour->next = bins[neighbour->numNeighbours];
            if (neighbour->next != NULL) {
                neighbour->next->prev = neighbour;
            }
            bins[neighbour->numNeighbours] = neighbour;
            neighbour->prev = NULL;
        }
    }
} // 0x802D61A8

/*
 * Follows a strip across edge, adding the vertex of each triangle it reaches, until no unused
 * triangle is left. Where the strip would end, it swaps (repeats a vertex) to go on through the
 * triangle's third edge. Returns the number of triangles added.
 */
static RwInt32 TriStripFollow(TriStripListEntry* strip, TriStripEdge* edge, TriStripTri** bins,
                              RpBuildMeshTriangle* triangles, RwInt32 pass) {
    RwInt32 numTris = 0;
    TriStripEdge* lastEdge;
    TriStripEdge* swapEdge = NULL;
    TriStripTri* tri;
    RxVertexIndex vertex;
    RwInt32 edgeIndex;
    TriStripEdge* nextEdge;
    RwBool deadEnd;

    while (edge != NULL) {
        tri = NULL;
        edgeIndex = -1;

        if (pass < TRISTRIPFINALPASS) {
            TriStripEdgeGetFreeTri(edge, tri, testUsed);
        } else {
            TriStripEdgeGetFreeTri(edge, tri, used);
        }

        if (tri == NULL) {
            return numTris;
        }

        numTris++;
        TriStripMarkTriUsed(tri, bins, pass);

        if (edge == tri->edges[0]) {
            edgeIndex = 0;
        } else if (edge == tri->edges[1]) {
            edgeIndex = 1;
        } else if (edge == tri->edges[2]) {
            edgeIndex = 2;
        }

        /* The vertex opposite edge. */
        nextEdge = tri->edges[(edgeIndex + 1) % 3];
        if (nextEdge->tris[0] == tri) {
            vertex = nextEdge->vertIndex[1];
        } else {
            vertex = nextEdge->vertIndex[0];
        }

        lastEdge = edge;
        edge = TriStripGetTriEdge(tri, strip->indices[strip->numIndices - 1], vertex);

        if (pass < TRISTRIPFINALPASS) {
            deadEnd = TRUE;
            if (TriStripEdgeIsDeadEnd(edge, testUsed) == FALSE) {
                deadEnd = FALSE;
            }
        } else {
            deadEnd = TRUE;
            if (TriStripEdgeIsDeadEnd(edge, used) == FALSE) {
                deadEnd = FALSE;
            }
        }

        if (deadEnd) {
            RwBool canSwap;

            if (tri->edges[0] != lastEdge && tri->edges[0] != edge) {
                swapEdge = tri->edges[0];
            } else if (tri->edges[1] != lastEdge && tri->edges[1] != edge) {
                swapEdge = tri->edges[1];
            } else if (tri->edges[2] != lastEdge && tri->edges[2] != edge) {
                swapEdge = tri->edges[2];
            }

            if (pass < TRISTRIPFINALPASS) {
                canSwap = FALSE;
                if (TriStripEdgeHasFreeTris(swapEdge, testUsed) != FALSE) {
                    canSwap = TRUE;
                }
            } else {
                canSwap = FALSE;
                if (TriStripEdgeHasFreeTris(swapEdge, used) != FALSE) {
                    canSwap = TRUE;
                }
            }

            if (canSwap) {
                if (strip->numIndices & 1) {
                    edge = NULL;
                    strip->indices[strip->numIndices] = vertex;
                    strip->numIndices++;
                } else {
                    edge = swapEdge;
                    strip->indices[strip->numIndices] = strip->indices[strip->numIndices - 2];
                    strip->numIndices++;
                    strip->indices[strip->numIndices] = vertex;
                    strip->numIndices++;
                }
            } else {
                edge = NULL;
                strip->indices[strip->numIndices] = vertex;
                strip->numIndices++;
            }
        } else {
            strip->indices[strip->numIndices] = vertex;
            strip->numIndices++;
        }
    }

    return numTris;
} // 0x802D6338

/*
 * Strips the triangles: builds their edges, bins them by unused neighbours and grows strips from
 * the triangles with the fewest, adding each strip to the strip list. Returns TRUE.
 */
static RwBool TriStripStripTris(RpBuildMeshTriangle* triangles, RwUInt32 numTris,
                                TriStripListEntry** stripList, RwBool testStrips) {
    TriStripEdge* edgeList = NULL;
    TriStripTri* bins[4] = {NULL, NULL, NULL, NULL};
    TriStripFreeLists freeLists = {NULL, NULL};
    RwUInt32 edgeIndex;
    TriStripListEntry* strip;
    TriStripListEntry* backStrip;
    RwUInt32 numStripped = 0;
    RwUInt32 numStrippedBefore;
    TriStripTri** tris;
    TriStripEdge* edge = NULL;
    TriStripEdge* backEdge = NULL;
    RwUInt32 start;
    RwUInt32 bestEdge;
    RwInt32 pass;
    RwUInt32 best;
    TriStripListEntry* entry;
    RwUInt32 i;

    tris = TriStripBinEntryArrayCreate(numTris, &freeLists, &edgeList, triangles);

    for (i = 0; i < numTris; i++) {
        tris[i]->next = bins[tris[i]->numNeighbours];
        if (tris[i]->next != NULL) {
            tris[i]->next->prev = tris[i];
        }
        bins[tris[i]->numNeighbours] = tris[i];
        tris[i]->prev = NULL;
    }

    /* The strips being tried, forwards and backwards from the start triangle. */
    strip = (TriStripListEntry*)RwFreeListAlloc(RPMESHGLOBAL(triStripListEntryFreeList),
                                                rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    strip->maxIndices = numTris * 2 + 2;
    strip->numIndices = 0;
    strip->indices = (RxVertexIndex*)RwMalloc(strip->maxIndices * sizeof(RwUInt32),
                                              rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    backStrip = (TriStripListEntry*)RwFreeListAlloc(RPMESHGLOBAL(triStripListEntryFreeList),
                                                    rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    backStrip->maxIndices = numTris * 2 + 2;
    backStrip->numIndices = 0;
    backStrip->indices = (RxVertexIndex*)RwMalloc(backStrip->maxIndices * sizeof(RwUInt32),
                                                  rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);

    while (numStripped < numTris) {
        best = 0;
        pass = testStrips ? 0 : TRISTRIPFINALPASS - 1;

        if (bins[0] != NULL) {
            /* An isolated triangle is a strip of its own. */
            RwUInt32 index = bins[0]->index;

            entry = (TriStripListEntry*)RwFreeListAlloc(RPMESHGLOBAL(triStripListEntryFreeList),
                                                        rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
            entry->next = *stripList;
            *stripList = entry;
            entry->maxIndices = 3;
            entry->numIndices = 3;
            entry->indices = (RxVertexIndex*)RwMalloc(3 * sizeof(RwUInt32),
                                                      rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
            entry->indices[0] = triangles[index].vertIndex[0];
            entry->indices[1] = triangles[index].vertIndex[1];
            entry->indices[2] = triangles[index].vertIndex[2];

            bins[0]->used = TRUE;
            bins[0]->testUsed = TRUE;
            bins[0] = bins[0]->next;
            if (bins[0] != NULL) {
                bins[0]->prev = NULL;
            }
            numStripped++;
        } else {
            RwUInt32 bin = 1;
            RwBool backwards;
            RpBuildMeshTriangle* triangle;

            while (bins[bin] == NULL) {
                bin++;
            }

            /*
             * A strip from vertex edgeIndex of the triangle runs forwards across
             * edges[(edgeIndex + 1) % 3] and backwards across edges[edgeIndex]. Start where both
             * have an unused neighbour, else where the forward edge has the most.
             */
            if (TriStripEdgeCountFreeTris(bins[bin]->edges[2], used) >= 2 &&
                TriStripEdgeCountFreeTris(bins[bin]->edges[1], used) >= 2) {
                start = 1;
            } else if (TriStripEdgeCountFreeTris(bins[bin]->edges[0], used) >= 2 &&
                       TriStripEdgeCountFreeTris(bins[bin]->edges[2], used) >= 2) {
                start = 2;
            } else if (TriStripEdgeCountFreeTris(bins[bin]->edges[1], used) >= 2 &&
                       TriStripEdgeCountFreeTris(bins[bin]->edges[0], used) >= 2) {
                start = 0;
            } else if (TriStripEdgeCountFreeTris(bins[bin]->edges[0], used) >
                       TriStripEdgeCountFreeTris(bins[bin]->edges[1], used)) {
                if (TriStripEdgeCountFreeTris(bins[bin]->edges[0], used) >
                    TriStripEdgeCountFreeTris(bins[bin]->edges[2], used)) {
                    start = 2;
                } else {
                    start = 1;
                }
            } else {
                if (TriStripEdgeCountFreeTris(bins[bin]->edges[1], used) >
                    TriStripEdgeCountFreeTris(bins[bin]->edges[2], used)) {
                    start = 0;
                } else {
                    start = 1;
                }
            }

            bestEdge = start;
            numStrippedBefore = numStripped;

            /*
             * Trial passes try each start vertex and mark only testUsed; the final pass strips from
             * the one that took the most triangles and adds the strip to the list.
             */
            for (;;) {
                numStripped = numStrippedBefore;
                for (i = 0; i < numTris; i++) {
                    tris[i]->testUsed = tris[i]->used;
                }

                switch (pass++) {
                case 0:
                    edgeIndex = start % 3;
                    break;
                case 1:
                    edgeIndex = (start + 1) % 3;
                    break;
                case 2:
                    edgeIndex = (start + 2) % 3;
                    break;
                default:
                    edgeIndex = bestEdge;
                    break;
                }

                switch (edgeIndex) {
                case 0:
                    edge = bins[bin]->edges[1];
                    backEdge = bins[bin]->edges[0];
                    break;
                case 1:
                    edge = bins[bin]->edges[2];
                    backEdge = bins[bin]->edges[1];
                    break;
                case 2:
                    edge = bins[bin]->edges[0];
                    backEdge = bins[bin]->edges[2];
                    break;
                }

                triangle = &triangles[bins[bin]->index];
                strip->indices[0] = triangle->vertIndex[edgeIndex % 3];
                strip->indices[1] = triangle->vertIndex[(edgeIndex + 1) % 3];
                strip->indices[2] = triangle->vertIndex[(edgeIndex + 2) % 3];
                strip->numIndices = 3;
                TriStripMarkTriUsed(bins[bin], bins, pass);

                numStripped++; /* the start triangle */
                numStripped += TriStripFollow(strip, edge, bins, triangles, pass);

                if (pass < TRISTRIPFINALPASS) {
                    backwards = TriStripEdgeCountFreeTris(backEdge, testUsed) > 0;
                } else {
                    backwards = TriStripEdgeCountFreeTris(backEdge, used) > 0;
                }

                /* Each arm keeps the best start, and on the final pass adds the strip and stops. */
                if (backwards) {
                    backStrip->indices[0] = strip->indices[1];
                    backStrip->indices[1] = strip->indices[0];
                    backStrip->numIndices = 2;
                    numStripped += TriStripFollow(backStrip, backEdge, bins, triangles, pass);
                    if (backStrip->numIndices & 1) {
                        backStrip->indices[backStrip->numIndices] =
                            backStrip->indices[backStrip->numIndices - 2];
                        backStrip->numIndices++;
                    }
                    if (numStripped > best) {
                        best = numStripped;
                        bestEdge = edgeIndex;
                    }
                    if (pass >= TRISTRIPFINALPASS) {
                        /* The backward strip reversed, then the forward strip. */
                        entry = (TriStripListEntry*)RwFreeListAlloc(
                            RPMESHGLOBAL(triStripListEntryFreeList), rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
                        entry->next = *stripList;
                        *stripList = entry;
                        entry->maxIndices = strip->numIndices + backStrip->numIndices - 2;
                        entry->numIndices = 0;
                        entry->indices = (RxVertexIndex*)RwMalloc(
                            entry->maxIndices * sizeof(RwUInt32), rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
                        while (backStrip->numIndices > 2) {
                            entry->indices[entry->numIndices] =
                                backStrip->indices[backStrip->numIndices - 1];
                            entry->numIndices++;
                            backStrip->numIndices--;
                        }
                        memcpy(&entry->indices[entry->numIndices], strip->indices,
                               strip->numIndices * sizeof(RwUInt32));
                        entry->numIndices = entry->maxIndices;
                        break;
                    }
                } else {
                    if (numStripped > best) {
                        best = numStripped;
                        bestEdge = edgeIndex;
                    }
                    if (pass >= TRISTRIPFINALPASS) {
                        entry = (TriStripListEntry*)RwFreeListAlloc(
                            RPMESHGLOBAL(triStripListEntryFreeList), rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
                        entry->next = *stripList;
                        *stripList = entry;
                        entry->maxIndices = strip->numIndices;
                        entry->numIndices = strip->numIndices;
                        entry->indices = (RxVertexIndex*)RwMalloc(
                            strip->numIndices * sizeof(RwUInt32), rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
                        memcpy(entry->indices, strip->indices, strip->numIndices * sizeof(RwUInt32));
                        break;
                    }
                }
            }
        }
    }

    RwFree(backStrip->indices);
    backStrip->indices = NULL;
    RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), backStrip);
    RwFree(strip->indices);
    strip->indices = NULL;
    RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), strip);

    TriStripBinEntryArrayDestroy(numTris, &freeLists, edgeList, tris);

    return TRUE;
} // 0x802D679C

/*
 * Joins the strips of the list into one strip, through degenerate triangles; each next strip is
 * one that starts with the last vertex so far, else the last but one, else the first left.
 * keepWinding adds a vertex where a join would flip the winding of the next strip.
 */
static RwBool TriStripJoin(TriStripListEntry** stripList, RwBool keepWinding) {
    TriStripListEntry* joined;
    TriStripListEntry* strip;
    TriStripListEntry* remaining;
    RwUInt32 i;

    if (*stripList == NULL) {
        return FALSE;
    }

    joined = (TriStripListEntry*)RwFreeListAlloc(RPMESHGLOBAL(triStripListEntryFreeList),
                                                 rwMEMHINTDUR_EVENT | rwID_MESHMODULE);
    joined->numIndices = 0;
    joined->maxIndices = 0;
    for (remaining = *stripList; remaining != NULL; remaining = remaining->next) {
        joined->maxIndices += remaining->numIndices + 6;
    }
    joined->indices = (RxVertexIndex*)RwMalloc(joined->maxIndices * sizeof(RwUInt32),
                                               rwMEMHINTDUR_EVENT | rwID_MESHMODULE);

    /* The first strip starts the joined strip. */
    remaining = *stripList;
    for (i = 0; i < remaining->numIndices; i++) {
        joined->indices[joined->numIndices] = remaining->indices[i];
        joined->numIndices++;
    }
    RwFree(remaining->indices);
    remaining->indices = NULL;
    strip = remaining;
    remaining = remaining->next;
    RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), strip);

    while (remaining != NULL) {
        for (strip = remaining; strip != NULL; strip = strip->next) {
            if (strip->indices[0] == joined->indices[joined->numIndices - 1]) {
                if ((joined->numIndices & 1) && keepWinding) {
                    joined->indices[joined->numIndices] = strip->indices[0];
                    joined->numIndices++;
                }
                break;
            }
        }

        if (strip == NULL) {
            for (strip = remaining; strip != NULL; strip = strip->next) {
                if (strip->indices[0] == joined->indices[joined->numIndices - 2]) {
                    if (!(joined->numIndices & 1) && keepWinding) {
                        joined->indices[joined->numIndices] = strip->indices[0];
                        joined->indices[joined->numIndices + 1] = strip->indices[0];
                        joined->numIndices += 2;
                    } else {
                        joined->indices[joined->numIndices] = strip->indices[0];
                        joined->numIndices++;
                    }
                    break;
                }
            }
        }

        if (strip == NULL) {
            strip = remaining;
            if (!(joined->numIndices & 1) || !keepWinding) {
                joined->indices[joined->numIndices] = joined->indices[joined->numIndices - 1];
                joined->indices[joined->numIndices + 1] = strip->indices[0];
                joined->numIndices += 2;
            } else {
                joined->indices[joined->numIndices] = joined->indices[joined->numIndices - 1];
                joined->indices[joined->numIndices + 1] = strip->indices[0];
                joined->indices[joined->numIndices + 2] = strip->indices[0];
                joined->numIndices += 3;
            }
        }

        for (i = 0; i < strip->numIndices; i++) {
            joined->indices[joined->numIndices] = strip->indices[i];
            joined->numIndices++;
        }
        RwFree(strip->indices);
        strip->indices = NULL;

        if (remaining == strip) {
            remaining = remaining->next;
            RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), strip);
        } else {
            TriStripListEntry* prev = remaining;

            while (prev->next != strip) {
                prev = prev->next;
            }
            prev->next = strip->next;
            RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), strip);
        }
    }

    *stripList = joined;
    joined->next = NULL;

    return TRUE;
} // 0x802D7364

/*
 * The triangles of one material in the sorted triangle array: TriStripMeshGenerate strips each
 * run on its own. Type and member names invented; the layout is RpMesh's, and indices is cleared
 * and never read.
 */
typedef struct TriStripMaterialMesh {
    RxVertexIndex* indices;     /* 0x0 */
    RwUInt32 numTris;           /* 0x4 */
    RpMaterial* material;       /* 0x8 */
} TriStripMaterialMesh;

/*
 * The default tristripper: sorts the triangles by material as _rpTriListMeshGenerate does,
 * strips each material's triangles and joins its strips into one strip, one mesh per material.
 * testStrips goes to the stripper, keepWinding to the join. Only the triangle pointer array is
 * checked for a failed allocation.
 */
static RpMeshHeader* TriStripMeshGenerate(RpBuildMesh* buildMesh, RwBool testStrips, RwBool keepWinding) {
    RwUInt32 numMaterialMeshes;
    RwUInt32 j;
    TriStripMaterialMesh* materialMesh;
    RpMeshHeader* meshHeader;
    RpBuildMeshTriangle** triPointers;
    TriStripListEntry* stripList;
    RpBuildMeshTriangle** triPointer;
    RpBuildMeshTriangle* triangles;
    RpMesh* mesh;
    RwUInt32 numMeshes;
    TriStripListEntry* strip;
    RpMaterial* material;
    RpMesh** meshes;
    RwUInt16 numStripMeshes = 0;
    RwUInt32 totalIndices;
    RxVertexIndex* meshIndex;
    RwUInt32 size;
    RwUInt32 i;
    TriStripMaterialMesh* materialMeshes;

    triPointers = (RpBuildMeshTriangle**)RwMalloc(buildMesh->numTriangles * sizeof(RpBuildMeshTriangle*),
                                                  rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    if (triPointers == NULL) {
        return NULL;
    }

    for (i = 0; i < buildMesh->numTriangles; i++) {
        triPointers[i] = &buildMesh->meshTriangles[i];
    }

    qsort(triPointers, buildMesh->numTriangles, sizeof(RpBuildMeshTriangle*), SortPolygons);

    numMeshes = 1;
    if (buildMesh->numTriangles >= 2) {
        material = triPointers[0]->material;
        for (i = 1; i < buildMesh->numTriangles; i++) {
            if (triPointers[i]->material != material) {
                material = triPointers[i]->material;
                numMeshes++;
            }
        }
    }

    meshes = (RpMesh**)RwMalloc(numMeshes * sizeof(RpMesh*),
                                rwMEMHINTDUR_FUNCTION | rwMEMHINTFLAG_RESIZABLE | rwID_MESHMODULE);
    materialMeshes = (TriStripMaterialMesh*)RwMalloc(numMeshes * sizeof(TriStripMaterialMesh),
                                                     rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);

    /* numTris holds a run's first triangle until the next run starts, then its count. */
    materialMeshes[0].material = triPointers[0]->material;
    materialMeshes[0].numTris = 0;
    materialMeshes[0].indices = NULL;
    numMaterialMeshes = 1;
    if (buildMesh->numTriangles >= 2) {
        for (i = 0; i < buildMesh->numTriangles - 1; i++) {
            if (triPointers[i]->material != triPointers[i + 1]->material) {
                materialMeshes[numMaterialMeshes].material = triPointers[i + 1]->material;
                materialMeshes[numMaterialMeshes].numTris = i + 1;
                materialMeshes[numMaterialMeshes].indices = NULL;
                materialMeshes[numMaterialMeshes - 1].numTris =
                    i + 1 - materialMeshes[numMaterialMeshes - 1].numTris;
                numMaterialMeshes++;
            }
        }
    }
    materialMeshes[numMaterialMeshes - 1].numTris =
        buildMesh->numTriangles - materialMeshes[numMaterialMeshes - 1].numTris;

    RPMESHGLOBAL(triStripListEntryFreeList) =
        RwFreeListCreate(sizeof(TriStripListEntry), buildMesh->numTriangles / 10 + 5, 4,
                         rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);

    /* Each material's triangles are stripped from a contiguous copy of them. */
    stripList = NULL;
    triPointer = triPointers;
    materialMesh = materialMeshes;
    for (j = 0; j < numMaterialMeshes; j++) {
        triangles = (RpBuildMeshTriangle*)RwMalloc(materialMesh->numTris * sizeof(RpBuildMeshTriangle),
                                                   rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
        for (i = 0; i < materialMesh->numTris; i++) {
            triangles[i] = **triPointer;
            triPointer++;
        }

        TriStripStripTris(triangles, materialMesh->numTris, &stripList, testStrips);
        TriStripJoin(&stripList, keepWinding);

        for (strip = stripList; strip != NULL; strip = strip->next) {
            mesh = (RpMesh*)RwMalloc(sizeof(RpMesh) + strip->numIndices * sizeof(RxVertexIndex),
                                     rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
            mesh->material = materialMesh->material;
            mesh->numIndices = strip->numIndices;
            mesh->indices = (RxVertexIndex*)(mesh + 1);
            for (i = 0; i < mesh->numIndices; i++) {
                mesh->indices[i] = strip->indices[i];
            }
            meshes[numStripMeshes++] = mesh;
        }

        while (stripList != NULL) {
            strip = stripList;
            stripList = strip->next;
            RwFree(strip->indices);
            strip->indices = NULL;
            RwFreeListFree(RPMESHGLOBAL(triStripListEntryFreeList), strip);
        }

        RwFree(triangles);
        materialMesh++;
    }

    RwFreeListDestroy(RPMESHGLOBAL(triStripListEntryFreeList));
    RPMESHGLOBAL(triStripListEntryFreeList) = NULL;

    /* The header is followed by its meshes, then by their indices. */
    size = sizeof(RpMeshHeader);
    totalIndices = 0;
    for (i = 0; i < numStripMeshes; i++) {
        size += sizeof(RpMesh) + meshes[i]->numIndices * sizeof(RxVertexIndex);
        totalIndices += meshes[i]->numIndices;
    }

    meshHeader = _rpMeshHeaderCreate(size);
    meshHeader->flags = rpMESHHEADERTRISTRIP;
    meshHeader->numMeshes = numStripMeshes;
    meshHeader->serialNum = RPMESHGLOBAL(nextSerialNum);
    meshHeader->firstMeshOffset = 0;
    meshHeader->totalIndicesInMesh = totalIndices;
    RPMESHGLOBAL(nextSerialNum)++;

    mesh = (RpMesh*)(meshHeader + 1);
    meshIndex = (RxVertexIndex*)(mesh + numStripMeshes);
    for (i = 0; i < numStripMeshes; i++) {
        mesh->indices = meshIndex;
        mesh->numIndices = meshes[i]->numIndices;
        mesh->material = meshes[i]->material;
        memcpy(meshIndex, meshes[i]->indices, meshes[i]->numIndices * sizeof(RxVertexIndex));
        meshIndex += mesh->numIndices;
        mesh++;
        RwFree(meshes[i]);
        meshes[i] = NULL;
    }

    RwFree(triPointers);
    RwFree(meshes);
    RwFree(materialMeshes);

    return meshHeader;
} // 0x802D7740

/* The default tristrip method. */
RpMeshHeader* RpBuildMeshGenerateDefaultTriStrip(RpBuildMesh* buildmesh, void* data) {
    return TriStripMeshGenerate(buildmesh, 0, 1);
} // 0x802D7D74

/* Builds a triangle-list mesh header: one mesh per material, triangles sorted by SortPolygons. */
RpMeshHeader* _rpTriListMeshGenerate(RpBuildMesh* buildMesh, void* data) {
    RpBuildMeshTriangle** triPointers;
    RpMeshHeader* meshHeader;
    RpMesh* mesh;
    RxVertexIndex* meshIndex;
    RwUInt32 numMeshes;
    RpMaterial* material;
    RwUInt32 size;
    RwUInt32 i;

    triPointers = (RpBuildMeshTriangle**)RwMalloc(buildMesh->numTriangles * sizeof(RpBuildMeshTriangle*),
                                                  rwMEMHINTDUR_FUNCTION | rwID_MESHMODULE);
    if (triPointers != NULL) {
        for (i = 0; i < buildMesh->numTriangles; i++) {
            triPointers[i] = &buildMesh->meshTriangles[i];
        }

        qsort(triPointers, buildMesh->numTriangles, sizeof(RpBuildMeshTriangle*), SortPolygons);

        numMeshes = 1;
        if (buildMesh->numTriangles >= 2) {
            material = triPointers[0]->material;
            for (i = 1; i < buildMesh->numTriangles; i++) {
                if (triPointers[i]->material != material) {
                    material = triPointers[i]->material;
                    numMeshes++;
                }
            }
        }

        size = sizeof(RpMeshHeader) + numMeshes * sizeof(RpMesh) +
               3 * sizeof(RxVertexIndex) * buildMesh->numTriangles;
        meshHeader = _rpMeshHeaderCreate(size);
        if (meshHeader == NULL) {
            RwFree(triPointers);
            RWERROR((E_RW_NOMEM, size));
            return NULL;
        }

        meshHeader->flags = 0;
        meshHeader->numMeshes = 1;
        meshHeader->serialNum = RPMESHGLOBAL(nextSerialNum);
        meshHeader->firstMeshOffset = 0;
        meshHeader->totalIndicesInMesh = buildMesh->numTriangles * 3;
        RPMESHGLOBAL(nextSerialNum)++;

        mesh = (RpMesh*)(meshHeader + 1);
        meshIndex = (RxVertexIndex*)(mesh + numMeshes);
        mesh->numIndices = 0;
        mesh->material = triPointers[0]->material;
        mesh->indices = meshIndex;

        /* The build mesh has a triangle at least (_rpMeshOptimise). */
        i = 0;
        do {
            if (triPointers[i]->material != mesh->material) {
                mesh[1].numIndices = 0;
                mesh[1].material = triPointers[i]->material;
                mesh++;
                mesh->indices = meshIndex;
                meshHeader->numMeshes++;
            }

            meshIndex[0] = triPointers[i]->vertIndex[0];
            meshIndex[1] = triPointers[i]->vertIndex[1];
            meshIndex[2] = triPointers[i]->vertIndex[2];
            meshIndex += 3;
            mesh->numIndices += 3;
            i++;
        } while (i < buildMesh->numTriangles);

        _rpMeshHeaderForAllMeshes(meshHeader, SortPolygonsInTriListMesh, NULL);

        RwFree(triPointers);

        return meshHeader;
    }

    RWERROR((E_RW_NOMEM, buildMesh->numTriangles * sizeof(RpBuildMeshTriangle*)));
    return NULL;
} // 0x802D7D80

/*
 * Turns a build mesh into a mesh header (strips through the tristrip method when the flags ask
 * for them, else a triangle list) and destroys the build mesh. A build mesh without triangles
 * gives the static empty mesh header.
 */
RpMeshHeader* _rpMeshOptimise(RpBuildMesh* buildmesh, RwUInt32 flags) {
    if (buildmesh != NULL) {
        RpTriStripMeshCallBack meshGenerator;
        void* data;
        RpMeshHeader* meshHeader;

        if (buildmesh->numTriangles == 0) {
            _rpBuildMeshDestroy(buildmesh);
            return &MeshopStatic.emptyMeshHeader;
        }

        if (flags & rpGEOMETRYTRISTRIP) {
            meshGenerator = MeshopStatic.meshGenerator;
            data = MeshopStatic.meshGeneratorData;
        } else {
            meshGenerator = _rpTriListMeshGenerate;
            data = NULL;
        }

        meshHeader = meshGenerator(buildmesh, data);
        if (meshHeader != NULL) {
            _rpBuildMeshDestroy(buildmesh);
            return meshHeader;
        }
    }

    return NULL;
} // 0x802D8018
