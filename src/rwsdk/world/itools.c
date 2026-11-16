// Unit name and boundary: Gormiti link map (RPworld.a itools)
// Names, parameters and locals: Gormiti DWARF
/*
 * itools.c: instancing tools of the GameCube pipeline: they turn a geometry's triangle indices into
 * triangle strips and write the strips and the vertex data of each strip into a GX display list.
 */
#include <revolution/gx/GXEnum.h>
#include <rwsdk/world/geominst.h>
#include <rwsdk/world/itools.h>

/* The strip joiner treats a triangle as degenerate when two of its indices are equal; a degenerate triangle ends the strip. */
void _rwGCNTriStripGetStats(unsigned short* indices, unsigned int numIndices, unsigned int* numStripsOut,
                            unsigned int* numIndicesOut, int preserveWindingOrder) {
    unsigned int length = 0;
    unsigned int i;

    *numStripsOut = 0;
    *numIndicesOut = 0;
    for (i = 0; i < numIndices - 2; i++) {
        if (indices[i] != indices[i + 1] && indices[i] != indices[i + 2] && indices[i + 1] != indices[i + 2]) {
            if (length == 0 && (i & 1) && preserveWindingOrder) {
                length = 2;
            } else {
                length++;
            }
        } else if (length != 0) {
            if (preserveWindingOrder && i < numIndices - 3) {
                if (indices[i + 1] != indices[i + 2] && indices[i + 1] != indices[i + 3] &&
                    indices[i + 2] != indices[i + 3]) {
                    if (((i + 1) & 1) == 0) {
                        *numIndicesOut += length + 2;
                        (*numStripsOut)++;
                        length = 0;
                    } else {
                        length++;
                    }
                } else {
                    *numIndicesOut += length + 2;
                    (*numStripsOut)++;
                    length = 0;
                }
            } else {
                *numIndicesOut += length + 2;
                (*numStripsOut)++;
                length = 0;
            }
        }
    }
    if (length != 0) {
        *numIndicesOut += length + 2;
        (*numStripsOut)++;
    }
} // 0x802E7370

/* Copies the indices into memory one every stride bytes, as 8 or 16 bit values to match the descriptor's index type. */
void _rwGCNInstanceIndicesCopy(unsigned short* indices, unsigned int numIndices, unsigned int stride,
                               unsigned int indexType, void* memory) {
    if (indexType == GX_INDEX8) {
        unsigned int i;

        for (i = 0; i < numIndices; i++) {
            *(unsigned char*)memory = (unsigned char)*indices;
            indices++;
            memory = (unsigned char*)memory + stride;
        }
    } else if (indexType == GX_INDEX16) {
        unsigned int i;

        for (i = 0; i < numIndices; i++) {
            *(unsigned short*)memory = *indices;
            indices++;
            memory = (unsigned char*)memory + stride;
        }
    }
} // 0x802E74C8

/*
 * Writes the vertex indices of a display list's triangle strips into memory, each strip's indices
 * after the 3 byte header WriteHeaders fills in. With removeDegen the triangles are cut into strips
 * by the same rule as _rwGCNTriStripGetStats, tested on the position indices; otherwise the whole
 * list is one run of indices.
 */
void _rwGCNInstanceIndices(unsigned short* posIndices, unsigned short* indices, unsigned int numIndices, int removeDegen,
                           unsigned int stride, unsigned int indexType, int preserveWindingOrder, void* memory) {
    unsigned int numStripsWritten = 0;
    unsigned int numIndicesWritten = 0;
    unsigned int length = 0;
    unsigned int i;

    if (removeDegen) {
        for (i = 0; i < numIndices - 2; i++) {
            if (posIndices[i] != posIndices[i + 1] && posIndices[i] != posIndices[i + 2] &&
                posIndices[i + 1] != posIndices[i + 2]) {
                if (length == 0 && (i & 1) && preserveWindingOrder) {
                    length = 2;
                } else {
                    length++;
                }
            } else if (length != 0) {
                if (preserveWindingOrder && i < numIndices - 3) {
                    if (posIndices[i + 1] != posIndices[i + 2] && posIndices[i + 1] != posIndices[i + 3] &&
                        posIndices[i + 2] != posIndices[i + 3]) {
                        if (((i + 1) & 1) == 0) {
                            _rwGCNInstanceIndicesCopy(&indices[i - length], length + 2, stride, indexType,
                                                      (unsigned char*)memory + numStripsWritten * 3 +
                                                          numIndicesWritten * stride);
                            numIndicesWritten += length + 2;
                            numStripsWritten++;
                            length = 0;
                        } else {
                            length++;
                        }
                    } else {
                        _rwGCNInstanceIndicesCopy(&indices[i - length], length + 2, stride, indexType,
                                                  (unsigned char*)memory + numStripsWritten * 3 +
                                                      numIndicesWritten * stride);
                        numIndicesWritten += length + 2;
                        numStripsWritten++;
                        length = 0;
                    }
                } else {
                    _rwGCNInstanceIndicesCopy(&indices[i - length], length + 2, stride, indexType,
                                              (unsigned char*)memory + numStripsWritten * 3 +
                                                  numIndicesWritten * stride);
                    numIndicesWritten += length + 2;
                    numStripsWritten++;
                    length = 0;
                }
            }
        }
        if (length != 0) {
            _rwGCNInstanceIndicesCopy(&indices[i - length], length + 2, stride, indexType,
                                      (unsigned char*)memory + numStripsWritten * 3 + numIndicesWritten * stride);
        }
    } else {
        _rwGCNInstanceIndicesCopy(indices, numIndices, stride, indexType, memory);
    }
} // 0x802E7658

/*
 * Writes the header of each triangle strip that _rwGCNInstanceIndices lays out: the primitive
 * command and the strip's vertex count as a big endian 16 bit value, 3 bytes. The strips are cut
 * by the same rule as _rwGCNTriStripGetStats.
 */
static void WriteHeaders(unsigned short* posIndices, unsigned int numIndices, int removeDegen, unsigned int stride,
                         unsigned char primTypeVAT, int preserveWindingOrder, void* memory) {
    unsigned int i;
    unsigned int numStripsWritten = 0;
    unsigned int numIndicesWritten = 0;
    unsigned int length = 0;

    if (removeDegen) {
        for (i = 0; i < numIndices - 2; i++) {
            if (posIndices[i] != posIndices[i + 1] && posIndices[i] != posIndices[i + 2] &&
                posIndices[i + 1] != posIndices[i + 2]) {
                if (length == 0 && (i & 1) && preserveWindingOrder) {
                    length = 2;
                } else {
                    length++;
                }
            } else if (length != 0) {
                if (preserveWindingOrder && i < numIndices - 3) {
                    if (posIndices[i + 1] != posIndices[i + 2] && posIndices[i + 1] != posIndices[i + 3] &&
                        posIndices[i + 2] != posIndices[i + 3]) {
                        if (((i + 1) & 1) == 0) {
                            unsigned char* header =
                                (unsigned char*)memory + numStripsWritten * 3 + numIndicesWritten * stride;

                            header[0] = primTypeVAT;
                            *(unsigned short*)(header + 1) = length + 2;
                            numIndicesWritten += length + 2;
                            numStripsWritten++;
                            length = 0;
                        } else {
                            length++;
                        }
                    } else {
                        unsigned char* header =
                            (unsigned char*)memory + numStripsWritten * 3 + numIndicesWritten * stride;

                        header[0] = primTypeVAT;
                        *(unsigned short*)(header + 1) = length + 2;
                        numIndicesWritten += length + 2;
                        numStripsWritten++;
                        length = 0;
                    }
                } else {
                    unsigned char* header = (unsigned char*)memory + numStripsWritten * 3 + numIndicesWritten * stride;

                    header[0] = primTypeVAT;
                    *(unsigned short*)(header + 1) = length + 2;
                    numIndicesWritten += length + 2;
                    numStripsWritten++;
                    length = 0;
                }
            }
        }
        if (length != 0) {
            unsigned char* header = (unsigned char*)memory + numStripsWritten * 3 + numIndicesWritten * stride;

            header[0] = primTypeVAT;
            *(unsigned short*)(header + 1) = length + 2;
        }
    } else {
        unsigned char* header = (unsigned char*)memory;

        header[0] = primTypeVAT;
        *(unsigned short*)(header + 1) = numIndices;
    }
} // 0x802E7884

/*
 * Fills a display list's vertex data, strip by strip: first the headers, then for each vertex
 * attribute in GX order the attribute's values (as direct data) or its indices (as index data).
 * The matrix index attributes are written as 8 bit indices of the position indices. The vertex
 * descriptor registers hold a 2 bit type (none, direct, index8, index16) for each attribute:
 * VCDRegLO from bit 9 (position, normal, colour 0 and 1), VCDRegHI from bit 0 (texture coordinates 0 to 7).
 */
void _rwGCNDisplayListFill(rwVertexDescriptor* vtxDesc, RxGameCubeDisplayList* displayList,
                           rwGCNDisplayListData* displayListData, unsigned int numIndices, int removeDegen,
                           unsigned int stride, int preserveWindingOrder, unsigned char primTypeVAT, RwV3d* posOrigin) {
    unsigned int offset = 0;
    unsigned int type;
    unsigned int fmt;
    float scale;
    unsigned int i;

    WriteHeaders(displayListData->data[rwGCNVA_POS], numIndices, removeDegen, stride, primTypeVAT, preserveWindingOrder,
                 displayList->displayList);
    offset += 3;
    for (i = 0; i < 21; i++) {
        void* memory = (unsigned char*)displayList->displayList + offset;

        switch (i) {
        case rwGCNVA_PNMTXIDX:
        case rwGCNVA_TEX0MTXIDX:
        case rwGCNVA_TEX1MTXIDX:
        case rwGCNVA_TEX2MTXIDX:
        case rwGCNVA_TEX3MTXIDX:
        case rwGCNVA_TEX4MTXIDX:
        case rwGCNVA_TEX5MTXIDX:
        case rwGCNVA_TEX6MTXIDX:
        case rwGCNVA_TEX7MTXIDX:
            if ((1 << i) & vtxDesc->VCDRegLO) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, GX_INDEX8, preserveWindingOrder, memory);
                offset += 1;
            }
            break;

        case rwGCNVA_POS:
            type = (vtxDesc->VCDRegLO & 0x600) >> 9;
            if (type == GX_DIRECT) {
                unsigned int cnt = vtxDesc->VATRegA & 1;

                if (cnt == rwGCNCC_POS_XYZ) {
                    unsigned int posFmt = (vtxDesc->VATRegA >> 1) & 7;
                    float posScale = 1 << ((vtxDesc->VATRegA >> 4) & 0x1f);

                    _rwGCNVtxFmtInstPos3D(memory, displayListData->data[i], posFmt, posScale, numIndices, stride,
                                          posOrigin);
                    offset += rwGCNPosGetSize(vtxDesc);
                }
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_NRM: {
            unsigned int cnt = (vtxDesc->VATRegA >> 9) & 1;

            type = (vtxDesc->VCDRegLO & 0x1800) >> 11;
            if (cnt == rwGCNCC_NRM_NBT) {
                if (type == GX_DIRECT) {
                    offset += rwGCNNrmGetSize(vtxDesc) * 3;
                } else if (type != GX_NONE) {
                    unsigned int indexCnt = vtxDesc->VATRegA >> 31;

                    if (indexCnt == 0) {
                        indexCnt = 1;
                        _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i],
                                              numIndices, removeDegen, stride, type, preserveWindingOrder, memory);
                    } else {
                        indexCnt = 3;
                    }
                    if (type == GX_INDEX8) {
                        offset += indexCnt;
                    } else if (type == GX_INDEX16) {
                        offset += indexCnt * 2;
                    }
                }
            } else if (type == GX_DIRECT) {
                unsigned int nrmFmt = (vtxDesc->VATRegA >> 10) & 7;

                _rwGCNVtxFmtInstNrm(memory, displayListData->data[i], nrmFmt, numIndices, stride);
                offset += rwGCNNrmGetSize(vtxDesc);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;
        }

        case rwGCNVA_CLR0:
        case rwGCNVA_CLR1:
            type = (vtxDesc->VCDRegLO >> ((i - rwGCNVA_CLR0) * 2 + 13)) & 3;
            if (type == GX_DIRECT) {
                unsigned int clrFmt = (vtxDesc->VATRegA >> ((i - rwGCNVA_CLR0) * 4 + 14)) & 7;

                _rwGCNVtxFmtInstClr(memory, displayListData->data[i], clrFmt, numIndices, stride);
                offset += rwGCNClrGetSize(vtxDesc, i - rwGCNVA_CLR0);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX0:
            type = vtxDesc->VCDRegHI & 3;
            if (type == GX_DIRECT) {
                scale = 1 << ((vtxDesc->VATRegA >> 25) & 0x1f);
                fmt = (vtxDesc->VATRegA >> 22) & 7;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 0);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX1:
            type = (vtxDesc->VCDRegHI & 0xC) >> 2;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegB >> 1) & 7;
                scale = (1 << (vtxDesc->VATRegB >> 4)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 1);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX2:
            type = (vtxDesc->VCDRegHI & 0x30) >> 4;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegB >> 10) & 7;
                scale = (1 << (vtxDesc->VATRegB >> 13)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 2);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX3:
            type = (vtxDesc->VCDRegHI & 0xC0) >> 6;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegB >> 19) & 7;
                scale = (1 << (vtxDesc->VATRegB >> 22)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 3);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX4:
            type = (vtxDesc->VCDRegHI & 0x300) >> 8;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegB >> 28) & 7;
                scale = (1 << vtxDesc->VATRegC) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 4);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX5:
            type = (vtxDesc->VCDRegHI & 0xC00) >> 10;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegC >> 6) & 7;
                scale = (1 << (vtxDesc->VATRegC >> 9)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 5);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX6:
            type = (vtxDesc->VCDRegHI & 0x3000) >> 12;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegC >> 15) & 7;
                scale = (1 << (vtxDesc->VATRegC >> 18)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 6);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;

        case rwGCNVA_TEX7:
            type = (vtxDesc->VCDRegHI & 0xC000) >> 14;
            if (type == GX_DIRECT) {
                fmt = (vtxDesc->VATRegC >> 24) & 7;
                scale = (1 << (vtxDesc->VATRegC >> 27)) & 0x1f;
                _rwGCNVtxFmtInstTex(memory, displayListData->data[i], fmt, scale, numIndices, stride);
                offset += rwGCNTexGetSize(vtxDesc, 7);
            } else if (type != GX_NONE) {
                _rwGCNInstanceIndices(displayListData->data[rwGCNVA_POS], displayListData->data[i], numIndices,
                                      removeDegen, stride, type, preserveWindingOrder, memory);
                if (type == GX_INDEX8) {
                    offset += 1;
                } else if (type == GX_INDEX16) {
                    offset += 2;
                }
            }
            break;
        }
    }
} // 0x802E7A4C
