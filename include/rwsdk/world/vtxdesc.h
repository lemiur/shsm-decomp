// Name: Gormiti link map (RPworld.a vtxdesc)
// Layout and enums: Gormiti DWARF (rwVertexDescriptor, rwGCNVertexAttribute, rwGCNCompCnt, rwGCNCompType, rwGCNAttrType)
#ifndef RWSDK_WORLD_VTXDESC_H
#define RWSDK_WORLD_VTXDESC_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * rwVertexDescriptor: the GX vertex layout of a geometry, as the register images the hardware is
 * programmed with: the three vertex attribute table registers, the vertex descriptor registers
 * (low: matrix indices, position, normal and colours; high: texture coordinates) and the XF
 * vertex specification.
 */
typedef struct rwVertexDescriptor {
    RwUInt32 VATID;             /* 0x00 */
    RwUInt32 VATRegA;           /* 0x04 */
    RwUInt32 VATRegB;           /* 0x08 */
    RwUInt32 VATRegC;           /* 0x0C */
    RwUInt32 VCDRegLO;          /* 0x10 */
    RwUInt32 VCDRegHI;          /* 0x14 */
    RwUInt32 XFRegINVTXSPEC;    /* 0x18 */
    RwUInt8 numAttrArrays;      /* 0x1C */
    RwUInt8 field_0x1D[3];      /* 0x1D */
} rwVertexDescriptor;

/* The vertex attributes, numbered as GX numbers them (GXAttr). */
typedef enum rwGCNVertexAttribute {
    rwGCNVA_PNMTXIDX = 0,
    rwGCNVA_TEX0MTXIDX = 1,
    rwGCNVA_TEX1MTXIDX = 2,
    rwGCNVA_TEX2MTXIDX = 3,
    rwGCNVA_TEX3MTXIDX = 4,
    rwGCNVA_TEX4MTXIDX = 5,
    rwGCNVA_TEX5MTXIDX = 6,
    rwGCNVA_TEX6MTXIDX = 7,
    rwGCNVA_TEX7MTXIDX = 8,
    rwGCNVA_POS = 9,
    rwGCNVA_NRM = 10,
    rwGCNVA_CLR0 = 11,
    rwGCNVA_CLR1 = 12,
    rwGCNVA_TEX0 = 13,
    rwGCNVA_TEX1 = 14,
    rwGCNVA_TEX2 = 15,
    rwGCNVA_TEX3 = 16,
    rwGCNVA_TEX4 = 17,
    rwGCNVA_TEX5 = 18,
    rwGCNVA_TEX6 = 19,
    rwGCNVA_TEX7 = 20,
    rwGCNVA_NBT = 25,
    rwGCNVA_MAX_ATTR = 26,
    rwGCNVA_NULL = 255,
    rwGCNVA_MAX_ENUM = 2147483647
} rwGCNVertexAttribute;

/* The component count of an attribute: its meaning depends on the attribute. */
typedef enum rwGCNCompCnt {
    rwGCNCC_POS_XY = 0,
    rwGCNCC_POS_XYZ = 1,
    rwGCNCC_NRM_XYZ = 0,
    rwGCNCC_NRM_NBT = 1,
    rwGCNCC_NRM_NBT3 = 2,
    rwGCNCC_CLR_RGB = 0,
    rwGCNCC_CLR_RGBA = 1,
    rwGCNCC_TEX_S = 0,
    rwGCNCC_TEX_ST = 1,
    rwGCNCC_MAX_ENUM = 2147483647
} rwGCNCompCnt;

/* The component type of an attribute: its meaning depends on the attribute. */
typedef enum rwGCNCompType {
    rwGCNCT_U8 = 0,
    rwGCNCT_S8 = 1,
    rwGCNCT_U16 = 2,
    rwGCNCT_S16 = 3,
    rwGCNCT_F32 = 4,
    rwGCNCT_RGB565 = 0,
    rwGCNCT_RGB8 = 1,
    rwGCNCT_RGBX8 = 2,
    rwGCNCT_RGBA4 = 3,
    rwGCNCT_RGBA6 = 4,
    rwGCNCT_RGBA8 = 5,
    rwGCNCT_MAX_ENUM = 2147483647
} rwGCNCompType;

/* How an attribute reaches the vertex stream: not at all, in the stream, or through an 8 or 16 bit index. */
typedef enum rwGCNAttrType {
    rwGCNAT_NONE = 0,
    rwGCNAT_DIRECT = 1,
    rwGCNAT_INDEX8 = 2,
    rwGCNAT_INDEX16 = 3,
    rwGCNAT_MAX_ENUM = 2147483647
} rwGCNAttrType;

/* Clears a descriptor to the default layout and returns it. */
extern rwVertexDescriptor* _rwVertexDescriptorInit(rwVertexDescriptor* vtxDesc);

/* Selects the vertex attribute table the descriptor's attributes are formatted for. */
extern void _rwGCNVertexDescSetVAT(rwVertexDescriptor* vtxDesc, RwUInt32 vat);

/* Sets the component count, component type and fraction bits of an attribute in the vertex attribute table registers. */
extern void _rwGCNVertexDescSetElementAttr(rwVertexDescriptor* vtxDesc, rwGCNVertexAttribute attr, rwGCNCompCnt cnt,
                                           rwGCNCompType fmt, unsigned char frac);

/* Sets how an attribute reaches the vertex stream, in the vertex descriptor registers. */
extern void _rwGCNVertexDescSetElementDesc(rwVertexDescriptor* vtxDesc, rwGCNVertexAttribute attr,
                                           rwGCNAttrType type);

/* Sets the number of attribute arrays the vertex indexes into. */
extern void _rwGCNVertexDescSetNumIndexedAttr(rwVertexDescriptor* vtxDesc, unsigned char numIndxAttr);

#ifdef __cplusplus
}
#endif

#endif
