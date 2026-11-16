// Unit name and boundary: Gormiti link map (RPworld.a vtxdesc)
// Locals, parameters and enums: Gormiti DWARF
/*
 * vtxdesc.c: the vertex descriptor of the GameCube pipeline. It keeps the register images that
 * tell the GX hardware which attributes a vertex carries (the vertex descriptor, VCD) and how
 * each attribute is formatted (the vertex attribute table, VAT).
 */
#include <rwsdk/world/vtxdesc.h>

/* Bits of VATRegA, VATRegB and VATRegC holding the component count, type and fraction of an attribute. */
#define rwVAT_A_POS 0x000001FF
#define rwVAT_A_NRM 0x00001E00
#define rwVAT_A_NRM_INDEX3 0x80000000
#define rwVAT_A_CLR0 0x0001E000
#define rwVAT_A_CLR1 0x001E0000
#define rwVAT_A_TEX0 0x3FE00000
#define rwVAT_B_TEX1 0x000001FF
#define rwVAT_B_TEX2 0x0003FE00
#define rwVAT_B_TEX3 0x0FFC0000
#define rwVAT_B_TEX4 0xF0000000
#define rwVAT_C_TEX4 0x0000001F
#define rwVAT_C_TEX5 0x00003FE0
#define rwVAT_C_TEX6 0x007FC000
#define rwVAT_C_TEX7 0xFF800000

/* Bits of VCDRegLO: which attributes the vertex carries, and how. */
#define rwVCD_LO_PNMTXIDX 0x00000001
#define rwVCD_LO_POS 0x00000600
#define rwVCD_LO_NRM 0x00001800
#define rwVCD_LO_CLR0 0x00006000
#define rwVCD_LO_CLR1 0x00018000

/* Bits of XFRegINVTXSPEC: the number of colours, normals and texture coordinates in the vertex. */
#define rwXF_NUMCOLS 0x00000003
#define rwXF_NUMNRM 0x0000000C
#define rwXF_NUMTEX 0x000000F0

/* The number of texture coordinate sets a vertex can carry. */
#define rwMAX_TEXCOORDS 8

/* Replaces the bits of reg under mask with the bits of val under mask. */
#define rwSetBits(reg, mask, val) \
    do { \
        (reg) &= ~(mask); \
        (reg) |= (val) & (mask); \
    } while (0)

/* Sets how texture coordinate set texCoord reaches the vertex stream and recounts the sets in use. */
#define rwSetTexCoordDesc(vtxDesc, texCoord, type) \
    do { \
        rwSetBits((vtxDesc)->VCDRegHI, 3 << ((texCoord) * 2), (type) << ((texCoord) * 2)); \
        i = rwMAX_TEXCOORDS; \
        numTex = 0; \
        while (i--) { \
            if ((vtxDesc)->VCDRegHI & (3 << (i * 2))) { \
                numTex++; \
            } \
        } \
        rwSetBits((vtxDesc)->XFRegINVTXSPEC, rwXF_NUMTEX, numTex << 4); \
    } while (0)

/* Clears a descriptor to the default layout and returns it. */
rwVertexDescriptor* _rwVertexDescriptorInit(rwVertexDescriptor* vtxDesc) {
    vtxDesc->VATID = 0;
    vtxDesc->VATRegA = 0x40000000;
    vtxDesc->VATRegB = 0x80000000;
    vtxDesc->VATRegC = 0;
    vtxDesc->VCDRegLO = 0;
    vtxDesc->VCDRegHI = 0;
    vtxDesc->XFRegINVTXSPEC = 0;

    return vtxDesc;
} // 0x802E8FC4

/* Selects the vertex attribute table the descriptor's attributes are formatted for. */
void _rwGCNVertexDescSetVAT(rwVertexDescriptor* vtxDesc, unsigned int vat) {
    vtxDesc->VATID = vat;
} // 0x802E8FF0

/*
 * Sets the component count, component type and fraction bits of an attribute in the vertex attribute table.
 * Each field is written count, type, fraction (Gormiti's Debug build evaluates the POS value as
 * (cnt | fmt << 1) | frac << 4); the unsigned char fraction is widened to RwUInt32 before it is
 * shifted, which is what places its shift ahead of the register load in the POS and TEX1 cases.
 */
void _rwGCNVertexDescSetElementAttr(rwVertexDescriptor* vtxDesc, rwGCNVertexAttribute attr, rwGCNCompCnt cnt,
                                    rwGCNCompType fmt, unsigned char frac) {
    switch (attr) {
    case rwGCNVA_POS:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_POS, cnt | (fmt << 1) | ((RwUInt32)frac << 4));
        break;

    case rwGCNVA_NRM:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_NRM, (cnt << 9) | (fmt << 10));
        vtxDesc->VATRegA &= ~rwVAT_A_NRM_INDEX3;
        break;

    case rwGCNVA_NBT:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_NRM, (cnt << 9) | (fmt << 10));
        if (cnt == rwGCNCC_NRM_NBT3) {
            vtxDesc->VATRegA |= rwVAT_A_NRM_INDEX3;
        } else {
            vtxDesc->VATRegA &= ~rwVAT_A_NRM_INDEX3;
        }
        break;

    case rwGCNVA_CLR0:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_CLR0, (cnt << 13) | (fmt << 14));
        break;

    case rwGCNVA_CLR1:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_CLR1, (cnt << 17) | (fmt << 18));
        break;

    case rwGCNVA_TEX0:
        rwSetBits(vtxDesc->VATRegA, rwVAT_A_TEX0, (cnt << 21) | (fmt << 22) | ((RwUInt32)frac << 25));
        break;

    case rwGCNVA_TEX1:
        rwSetBits(vtxDesc->VATRegB, rwVAT_B_TEX1, cnt | (fmt << 1) | ((RwUInt32)frac << 4));
        break;

    case rwGCNVA_TEX2:
        rwSetBits(vtxDesc->VATRegB, rwVAT_B_TEX2, (cnt << 9) | (fmt << 10) | ((RwUInt32)frac << 13));
        break;

    case rwGCNVA_TEX3:
        rwSetBits(vtxDesc->VATRegB, rwVAT_B_TEX3, (cnt << 18) | (fmt << 19) | ((RwUInt32)frac << 22));
        break;

    case rwGCNVA_TEX4:
        // The mask keeps only the top four bits of VATRegB, so the count (bit 27) is dropped.
        rwSetBits(vtxDesc->VATRegB, rwVAT_B_TEX4, (cnt << 27) | (fmt << 28));
        rwSetBits(vtxDesc->VATRegC, rwVAT_C_TEX4, frac);
        break;

    case rwGCNVA_TEX5:
        rwSetBits(vtxDesc->VATRegC, rwVAT_C_TEX5, (cnt << 5) | (fmt << 6) | ((RwUInt32)frac << 9));
        break;

    case rwGCNVA_TEX6:
        rwSetBits(vtxDesc->VATRegC, rwVAT_C_TEX6, (cnt << 14) | (fmt << 15) | ((RwUInt32)frac << 18));
        break;

    case rwGCNVA_TEX7:
        rwSetBits(vtxDesc->VATRegC, rwVAT_C_TEX7, (cnt << 23) | (fmt << 24) | ((RwUInt32)frac << 27));
        break;
    }
} // 0x802E8FF8

/* Sets how an attribute reaches the vertex stream, and keeps the vertex specification's counts in step. */
void _rwGCNVertexDescSetElementDesc(rwVertexDescriptor* vtxDesc, rwGCNVertexAttribute attr, rwGCNAttrType type) {
    unsigned int numTex;
    unsigned int i;

    switch (attr) {
    case rwGCNVA_PNMTXIDX:
        rwSetBits(vtxDesc->VCDRegLO, rwVCD_LO_PNMTXIDX, type);
        break;

    case rwGCNVA_TEX0MTXIDX:
    case rwGCNVA_TEX1MTXIDX:
    case rwGCNVA_TEX2MTXIDX:
    case rwGCNVA_TEX3MTXIDX:
    case rwGCNVA_TEX4MTXIDX:
    case rwGCNVA_TEX5MTXIDX:
    case rwGCNVA_TEX6MTXIDX:
    case rwGCNVA_TEX7MTXIDX:
        rwSetBits(vtxDesc->VCDRegLO, 2 << (attr - 1), type << attr);
        break;

    case rwGCNVA_POS:
        rwSetBits(vtxDesc->VCDRegLO, rwVCD_LO_POS, type << 9);
        break;

    case rwGCNVA_NRM: {
        int numNrm = 0;

        rwSetBits(vtxDesc->VCDRegLO, rwVCD_LO_NRM, type << 11);
        if (type != rwGCNAT_NONE) {
            numNrm = 1;
        }
        rwSetBits(vtxDesc->XFRegINVTXSPEC, rwXF_NUMNRM, numNrm << 2);
        break;
    }

    case rwGCNVA_NBT: {
        int numNrm = 0;

        rwSetBits(vtxDesc->VCDRegLO, rwVCD_LO_NRM, type << 11);
        if (type != rwGCNAT_NONE) {
            numNrm = 2;
        }
        rwSetBits(vtxDesc->XFRegINVTXSPEC, rwXF_NUMNRM, numNrm << 2);
        break;
    }

    case rwGCNVA_CLR0:
    case rwGCNVA_CLR1: {
        unsigned int numcols = 0;

        rwSetBits(vtxDesc->VCDRegLO, rwVCD_LO_CLR0 << (attr - rwGCNVA_CLR0), type << (attr + 2));
        if (vtxDesc->VCDRegLO & rwVCD_LO_CLR0) {
            numcols++;
        }
        if (vtxDesc->VCDRegLO & rwVCD_LO_CLR1) {
            numcols++;
        }
        rwSetBits(vtxDesc->XFRegINVTXSPEC, rwXF_NUMCOLS, numcols);
        break;
    }

    case rwGCNVA_TEX0:
        rwSetTexCoordDesc(vtxDesc, 0, type);
        break;

    case rwGCNVA_TEX1:
        rwSetTexCoordDesc(vtxDesc, 1, type);
        break;

    case rwGCNVA_TEX2:
        rwSetTexCoordDesc(vtxDesc, 2, type);
        break;

    case rwGCNVA_TEX3:
        rwSetTexCoordDesc(vtxDesc, 3, type);
        break;

    case rwGCNVA_TEX4:
        rwSetTexCoordDesc(vtxDesc, 4, type);
        break;

    case rwGCNVA_TEX5:
        rwSetTexCoordDesc(vtxDesc, 5, type);
        break;

    case rwGCNVA_TEX6:
        rwSetTexCoordDesc(vtxDesc, 6, type);
        break;

    case rwGCNVA_TEX7:
        rwSetTexCoordDesc(vtxDesc, 7, type);
        break;
    }
} // 0x802E9220

/* Sets the number of attribute arrays the vertex indexes into. */
void _rwGCNVertexDescSetNumIndexedAttr(rwVertexDescriptor* vtxDesc, unsigned char numIndxAttr) {
    vtxDesc->numAttrArrays = numIndxAttr;
} // 0x802E9630
