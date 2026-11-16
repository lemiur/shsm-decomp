// Unit name and boundary: Gormiti link map (RPworld.a ibuffer)
// Locals and names: Gormiti DWARF
/*
 * ibuffer.c: the size of the display lists the GameCube pipeline instances triangle strips into.
 * A vertex in a display list holds, for each attribute the vertex descriptor enables, either
 * the attribute itself (GX_DIRECT) or an 8 or 16 bit index into its array.
 */
#include <revolution/gx/GXEnum.h>
#include <string.h>
#include <rwsdk/world/geominst.h>
#include <rwsdk/world/ibuffer.h>

/* Adds up the bytes one vertex takes in a display list, for every attribute the descriptor uses. */
RwUInt32 _rwGCNDisplayListGetStride(rwVertexDescriptor* vtxDesc) {
    int i;
    unsigned int type;
    unsigned int stride = 0;

    for (i = 0; i < 21; i++) {
        switch (i) {
        case GX_VA_PNMTXIDX:
        case GX_VA_TEX0MTXIDX:
        case GX_VA_TEX1MTXIDX:
        case GX_VA_TEX2MTXIDX:
        case GX_VA_TEX3MTXIDX:
        case GX_VA_TEX4MTXIDX:
        case GX_VA_TEX5MTXIDX:
        case GX_VA_TEX6MTXIDX:
        case GX_VA_TEX7MTXIDX:
            if ((1 << i) & vtxDesc->VCDRegLO) {
                stride++;
            }
            break;

        case GX_VA_POS:
            type = (vtxDesc->VCDRegLO >> 9) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNPosGetSize(vtxDesc);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_NRM: {
            unsigned int cnt = (vtxDesc->VATRegA >> 9) & 1;

            type = (vtxDesc->VCDRegLO >> 11) & 3;
            if (cnt == 1) {
                cnt = vtxDesc->VATRegA >> 31;
                if (cnt == 0) {
                    cnt = 1;
                } else {
                    cnt = 3;
                }

                if (type == GX_DIRECT) {
                    stride += cnt * rwGCNNrmGetSize(vtxDesc);
                } else if (type == GX_INDEX8) {
                    stride += cnt;
                } else if (type == GX_INDEX16) {
                    stride += cnt * 2;
                }
            } else {
                if (type == GX_DIRECT) {
                    stride += rwGCNNrmGetSize(vtxDesc);
                } else if (type == GX_INDEX8) {
                    stride += 1;
                } else if (type == GX_INDEX16) {
                    stride += 2;
                }
            }
            break;
        }

        case GX_VA_CLR0:
        case GX_VA_CLR1:
            type = (vtxDesc->VCDRegLO >> ((i - GX_VA_CLR0) * 2 + 13)) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNClrGetSize(vtxDesc, i - GX_VA_CLR0);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX0:
            type = vtxDesc->VCDRegHI & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 0);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX1:
            type = (vtxDesc->VCDRegHI >> 2) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 1);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX2:
            type = (vtxDesc->VCDRegHI >> 4) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 2);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX3:
            type = (vtxDesc->VCDRegHI >> 6) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 3);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX4:
            type = (vtxDesc->VCDRegHI >> 8) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 4);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX5:
            type = (vtxDesc->VCDRegHI >> 10) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 5);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX6:
            type = (vtxDesc->VCDRegHI >> 12) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 6);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;

        case GX_VA_TEX7:
            type = (vtxDesc->VCDRegHI >> 14) & 3;
            if (type == GX_DIRECT) {
                stride += rwGCNTexGetSize(vtxDesc, 7);
            } else if (type == GX_INDEX8) {
                stride += 1;
            } else if (type == GX_INDEX16) {
                stride += 2;
            }
            break;
        }
    }

    return stride;
} // 0x802E475C

/* The size of a display list that draws numIndices vertices in numStrips strips, rounded up to 32 bytes. */
RwUInt32 _rwGCNDisplayListGetSize(rwVertexDescriptor* vtxDesc, RwUInt32 numStrips, RwUInt32 numIndices) {
    unsigned int stride = _rwGCNDisplayListGetStride(vtxDesc);
    unsigned int size = numStrips * 3 + numIndices * stride;

    size = (size + 31) & ~31;

    return size;
} // 0x802E4B2C

/* Points a display list at its memory and clears the memory. */
void _rwGCNDisplayListInitialize(RxGameCubeDisplayList* displayList, RwUInt32 reserved, RwUInt32 displayListSize,
                                 void* memory) {
    displayList->displayList = memory;
    displayList->size = displayListSize;

    memset(memory, 0, displayListSize);
} // 0x802E4B7C
