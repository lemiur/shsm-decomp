// Unit name and boundary: Gormiti link map (RPworld.a vbuffer)
// Locals and names: Gormiti DWARF
/*
 * vbuffer.c: the vertex buffer the GameCube pipeline instances a geometry's vertex arrays into.
 * It is a header, one record for each attribute array the geometry draws through an index, and
 * the arrays themselves, each starting on a 32 byte boundary for the GX array fetch.
 */
#include <revolution/gx/GXEnum.h>
#include <rwsdk/world/geominst.h>
#include <rwsdk/world/vbuffer.h>

/* The size of the header: the buffer record and an attribute record for each attribute array after the first. */
RwUInt32 _rwGCNVertexBufferHeaderGetSize(rwVertexDescriptor* vtxDesc) {
    unsigned int size = sizeof(RxGameCubeVertexBuffer) + (vtxDesc->numAttrArrays - 1) * sizeof(RxGameCubeVertexAttr);

    return size;
} // 0x802E82D8

/* The bytes the indexed attribute arrays take, each one padded to 32 bytes. */
RwUInt32 _rwGCNVertexBufferGetSize(rwVertexDescriptor* vtxDesc, rwGCNVertexBufferData* vtxBufData) {
    unsigned int i;
    unsigned int type;
    unsigned int size = 0;

    for (i = GX_VA_POS; i < 21; i++) {
        switch (i) {
        case GX_VA_POS:
            type = (vtxDesc->VCDRegLO >> 9) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNPosGetSize(vtxDesc) + 31) & ~31;
            }
            break;

        case GX_VA_NRM:
            type = (vtxDesc->VCDRegLO >> 11) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                if (((vtxDesc->VATRegA >> 9) & 1) == 1) {
                    size += (vtxBufData->num[GX_VA_NRM] * rwGCNNrmGetSize(vtxDesc) * 3 + 31) & ~31;
                } else {
                    size += (vtxBufData->num[GX_VA_NRM] * rwGCNNrmGetSize(vtxDesc) + 31) & ~31;
                }
            }
            break;

        case GX_VA_CLR0:
        case GX_VA_CLR1:
            type = (vtxDesc->VCDRegLO >> ((i - GX_VA_CLR0) * 2 + 13)) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNClrGetSize(vtxDesc, i - GX_VA_CLR0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX0:
            type = vtxDesc->VCDRegHI & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX1:
            type = (vtxDesc->VCDRegHI >> 2) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX2:
            type = (vtxDesc->VCDRegHI >> 4) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX3:
            type = (vtxDesc->VCDRegHI >> 6) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX4:
            type = (vtxDesc->VCDRegHI >> 8) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX5:
            type = (vtxDesc->VCDRegHI >> 10) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX6:
            type = (vtxDesc->VCDRegHI >> 12) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;

        case GX_VA_TEX7:
            type = (vtxDesc->VCDRegHI >> 14) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size += (vtxBufData->num[i] * rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0) + 31) & ~31;
            }
            break;
        }
    }

    return size;
} // 0x802E82EC

/*
 * Describes each attribute array that is fetched through an index: where it lies in the buffer's memory, its
 * element size, index type and element count. The arrays follow one another, each padded to 32 bytes.
 */
void _rwGCNVertexBufferInitialize(rwVertexDescriptor* vtxDesc, RxGameCubeVertexBuffer* vtxBufHeader,
                                  rwGCNVertexBufferData* vtxBufData, unsigned char* memory) {
    unsigned int count = 0;
    unsigned int offset = 0;
    unsigned int i;
    unsigned int type;
    unsigned int size;
    unsigned char* mem;

    for (i = GX_VA_POS; i < 21; i++) {
        mem = memory + offset;

        switch (i) {
        case GX_VA_POS:
            type = (vtxDesc->VCDRegLO >> 9) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNPosGetSize(vtxDesc);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_NRM:
            type = (vtxDesc->VCDRegLO >> 11) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                if (((vtxDesc->VATRegA >> 9) & 1) == 1) {
                    size = rwGCNNrmGetSize(vtxDesc) * 3;
                    vtxBufHeader->attr[count].array = mem;
                    vtxBufHeader->attr[count].attr = i;
                    vtxBufHeader->attr[count].stride = size;
                    vtxBufHeader->attr[count].indexType = type;
                    vtxBufHeader->attr[count].Count = vtxBufData->num[GX_VA_NRM];
                    offset += (size * vtxBufData->num[GX_VA_NRM] + 31) & ~31;
                } else {
                    size = rwGCNNrmGetSize(vtxDesc);
                    vtxBufHeader->attr[count].array = mem;
                    vtxBufHeader->attr[count].attr = i;
                    vtxBufHeader->attr[count].stride = size;
                    vtxBufHeader->attr[count].indexType = type;
                    vtxBufHeader->attr[count].Count = vtxBufData->num[GX_VA_NRM];
                    offset += (size * vtxBufData->num[GX_VA_NRM] + 31) & ~31;
                }
                count++;
            }
            break;

        case GX_VA_CLR0:
        case GX_VA_CLR1:
            type = (vtxDesc->VCDRegLO >> ((i - GX_VA_CLR0) * 2 + 13)) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNClrGetSize(vtxDesc, i - GX_VA_CLR0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX0:
            type = vtxDesc->VCDRegHI & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX1:
            type = (vtxDesc->VCDRegHI >> 2) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX2:
            type = (vtxDesc->VCDRegHI >> 4) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX3:
            type = (vtxDesc->VCDRegHI >> 6) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX4:
            type = (vtxDesc->VCDRegHI >> 8) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX5:
            type = (vtxDesc->VCDRegHI >> 10) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX6:
            type = (vtxDesc->VCDRegHI >> 12) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;

        case GX_VA_TEX7:
            type = (vtxDesc->VCDRegHI >> 14) & 3;
            if (type == GX_INDEX8 || type == GX_INDEX16) {
                size = rwGCNTexGetSize(vtxDesc, i - GX_VA_TEX0);
                vtxBufHeader->attr[count].array = mem;
                vtxBufHeader->attr[count].attr = i;
                vtxBufHeader->attr[count].stride = size;
                vtxBufHeader->attr[count].indexType = type;
                vtxBufHeader->attr[count].Count = vtxBufData->num[i];
                offset += (size * vtxBufData->num[i] + 31) & ~31;
                count++;
            }
            break;
        }
    }

    vtxBufHeader->numAttrArrays = count;
} // 0x802E8620
