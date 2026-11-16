// Reference: zeldaret/tp libs/revolution/src/gx/GXTexture.c
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/gx/GXTexture.c and SMGCommunity/Petari src/RVL_SDK/gx/GXTexture.c
// (const object parameters of GXLoadTexObjPreLoaded and GXLoadTlut)
#include <revolution/gx.h>
#include <revolution/os.h>
#include <string.h>

#include <revolution/gx/__gx.h>

// GXTexObj internal data
typedef struct __GXTexObjInt_struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    void* userData;
    GXTexFmt fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} __GXTexObjInt;

// GXTexRegion internal data
typedef struct __GXTexRegionInt_struct {
    u32 image1;
    u32 image2;
    u16 sizeEven;
    u16 sizeOdd;
    u8 is32bMipmap;
    u8 isCached;
} __GXTexRegionInt;

// GXTlutObj internal data
typedef struct __GXTlutObjInt_struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
} __GXTlutObjInt;

// GXTlutRegion internal data
typedef struct __GXTlutRegionInt_struct {
    u32 loadTlut1;
    __GXTlutObjInt tlutObj;
} __GXTlutRegionInt;

u8 GXTexMode0Ids[8] = { 0x80, 0x81, 0x82, 0x83, 0xA0, 0xA1, 0xA2, 0xA3 };
u8 GXTexMode1Ids[8] = { 0x84, 0x85, 0x86, 0x87, 0xA4, 0xA5, 0xA6, 0xA7 };
u8 GXTexImage0Ids[8] = { 0x88, 0x89, 0x8A, 0x8B, 0xA8, 0xA9, 0xAA, 0xAB };
u8 GXTexImage1Ids[8] = { 0x8C, 0x8D, 0x8E, 0x8F, 0xAC, 0xAD, 0xAE, 0xAF };
u8 GXTexImage2Ids[8] = { 0x90, 0x91, 0x92, 0x93, 0xB0, 0xB1, 0xB2, 0xB3 };
u8 GXTexImage3Ids[8] = { 0x94, 0x95, 0x96, 0x97, 0xB4, 0xB5, 0xB6, 0xB7 };
u8 GXTexTlutIds[8] = { 0x98, 0x99, 0x9A, 0x9B, 0xB8, 0xB9, 0xBA, 0xBB };
static u8 GX2HWFiltConv[6] = { 0x00, 0x04, 0x01, 0x05, 0x02, 0x06 };
static u8 HW2GXFiltConv[8] = { 0x00, 0x02, 0x04, 0x00, 0x01, 0x03, 0x05, 0x00 };

// __GXGetTexTileShift: tile width/height shifts (in texels) for a texture format.
// Not in the retail binary (dead-stripped).
static void __GXGetTexTileShift(GXTexFmt fmt, u32* rowTileS, u32* colTileS) {
    switch (fmt) {
    case GX_TF_I4:
    case 0x8:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        *rowTileS = 3;
        *colTileS = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case 0x9:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_TF_A8:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        *rowTileS = 3;
        *colTileS = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
    case 0xA:
    case GX_TF_Z16:
    case GX_TF_Z24X8:
    case GX_CTF_RA8:
    case GX_CTF_RG8:
    case GX_CTF_GB8:
    case GX_CTF_Z16L:
        *rowTileS = 2;
        *colTileS = 2;
        break;
    default:
        *rowTileS = *colTileS = 0;
        ASSERTMSG1(0, "%s: invalid texture format", "GX");
        break;
    }
}

// GXGetTexBufferSize: size in bytes of a texture with the given format, optionally including mip levels.
// Not in the retail binary (dead-stripped).
u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, GXBool mipmap, u8 max_lod) {
    u32 tileShiftX;
    u32 tileShiftY;
    u32 tileBytes;
    u32 bufferSize;
    u32 nx;
    u32 ny;
    u32 level;

    ASSERTMSG1(width <= 1024, "%s: width too large", "GXGetTexBufferSize");
    ASSERTMSG1(height <= 1024, "%s: height too large", "GXGetTexBufferSize");

    __GXGetTexTileShift(format, &tileShiftX, &tileShiftY);
    if (format == GX_TF_RGBA8 || format == GX_TF_Z24X8) {
        tileBytes = 64;
    } else {
        tileBytes = 32;
    }

    if (mipmap == GX_TRUE) {
        nx = 1 << (31 - __cntlzw(width));
        ASSERTMSG1(width == nx, "%s: width must be a power of 2", "GXGetTexBufferSize");
        ny = 1 << (31 - __cntlzw(height));
        ASSERTMSG1(height == ny, "%s: height must be a power of 2", "GXGetTexBufferSize");

        bufferSize = 0;
        for (level = 0; level < max_lod; level++) {
            nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
            ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
            bufferSize += tileBytes * (nx * ny);
            if (width == 1 && height == 1) {
                break;
            }
            width = (width > 1) ? width >> 1 : 1;
            height = (height > 1) ? height >> 1 : 1;
        }
    } else {
        nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
        ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
        bufferSize = nx * ny * tileBytes;
    }

    return bufferSize;
}

// __GetImageTileCount: number of tile rows, columns and compression units of an image.
// 0x8023BF40
void __GetImageTileCount(GXTexFmt fmt, u16 wd, u16 ht, u32* rowTiles, u32* colTiles, u32* cmpTiles) {
    u32 texRowShift;
    u32 texColShift;

    __GXGetTexTileShift(fmt, &texRowShift, &texColShift);
    if (wd == 0) {
        wd = 1;
    }
    if (ht == 0) {
        ht = 1;
    }
    *rowTiles = (wd + (1 << texRowShift) - 1) >> texRowShift;
    *colTiles = (ht + (1 << texColShift) - 1) >> texColShift;
    *cmpTiles = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 2 : 1;
}

#define SOME_SET_REG_MACRO(reg, size, shift, val)                                                   \
	do {                                                                                            \
		(reg) = (u32)__rlwimi((u32)(reg), (val), (shift), (32 - (shift) - (size)), (31 - (shift))); \
	} while (0);

// GXInitTexObj: fills a texture object from an image pointer, size, format and wrap modes.
// 0x8023C000
void GXInitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap) {
    u32 imageBase;
    u32 maxLOD;
    u16 rowT;
    u16 colT;
    u32 rowC;
    u32 colC;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObj");
    ASSERTMSG1(width <= 1024, "%s: width too large", "GXInitTexObj");
    ASSERTMSG1(height <= 1024, "%s: height too large", "GXInitTexObj");
    ASSERTMSG1(!(format & _GX_TF_CTF), "%s: invalid texture format", "GXInitTexObj");

#if DEBUG
    if (wrap_s != GX_CLAMP || mipmap) {
        u32 mask = 1 << (31 - __cntlzw(width));
        ASSERTMSG1(width == mask, "%s: width must be a power of 2", "GXInitTexObj");
    }
    if (wrap_t != GX_CLAMP || mipmap) {
        u32 mask = 1 << (31 - __cntlzw(height));
        ASSERTMSG1(height == mask, "%s: height must be a power of 2", "GXInitTexObj");
    }
#endif

    memset(t, 0, 0x20);
    SET_REG_FIELD(__LINE__, t->mode0, 2, 0, wrap_s);
    SET_REG_FIELD(__LINE__, t->mode0, 2, 2, wrap_t);
    SET_REG_FIELD(__LINE__, t->mode0, 1, 4, 1);

    if (mipmap) {
        u8 lmax;
        t->flags |= 1;

        if (format == 8 || format == 9 || format == 10) {
            SOME_SET_REG_MACRO(t->mode0, 3, 5, 5);
        } else {
            SOME_SET_REG_MACRO(t->mode0, 3, 5, 6);
        }


        if (width > height) {
            maxLOD = 31 - __cntlzw(width);
        } else {
            maxLOD = 31 - __cntlzw(height);
        }
    
        lmax = 16.0f * maxLOD;
        SET_REG_FIELD(__LINE__, t->mode1, 8, 8, lmax);
    } else {
        SOME_SET_REG_MACRO(t->mode0, 3, 5, 4);
    }

    t->fmt = format;
    SET_REG_FIELD(__LINE__, t->image0, 10, 0, width - 1);
    SET_REG_FIELD(__LINE__, t->image0, 10, 10, height - 1);
    SET_REG_FIELD(__LINE__, t->image0, 4, 20, format & 0xF);
    ASSERTMSG2(((u32)image_ptr & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexObj", "image");
    imageBase = GX_PHY_ADDR(image_ptr) >> 5;
    SET_REG_FIELD(__LINE__, t->image3, 24, 0, imageBase);

    switch (format & 0xF) {
    case GX_TF_I4:
    case 8:
        t->loadFmt = 1;
        rowT = 3;
        colT = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case 9:
        t->loadFmt = 2;
        rowT = 3;
        colT = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case 10:
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    case GX_TF_RGBA8:
        t->loadFmt = 3;
        rowT = 2;
        colT = 2;
        break;
    case GX_TF_CMPR:
        t->loadFmt = 0;
        rowT = 3;
        colT = 3;
        break;
    default:
        ASSERTMSG1(0, "%s: invalid texture format", "GXPreLoadEntireTexture");
        t->loadFmt = 2;
        rowT = 2;
        colT = 2;
        break;
    }

    rowC = (width + (1 << rowT) - 1) >> rowT;
    colC = (height + (1 << colT) - 1) >> colT;
    t->loadCnt = (rowC * colC) & 0x7FFF;
    t->flags |= 2;
}

// GXInitTexObjCI: GXInitTexObj for a color-index (palette) texture, bound to a TLUT name.
// 0x8023C210
void GXInitTexObjCI(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXCITexFmt format, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap, u32 tlut_name) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjCI");
    GXInitTexObj(obj, image_ptr, width, height, format, wrap_s, wrap_t, mipmap);
    t->flags &= 0xFFFFFFFD;
    t->tlutName = tlut_name;
}

// GXInitTexObjLOD: sets filters, LOD range and bias, edge LOD and anisotropy of a texture object.
// 0x8023C260
void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt, f32 min_lod, f32 max_lod, f32 lod_bias, u8 bias_clamp, u8 do_edge_lod, GXAnisotropy max_aniso) {
    u8 lbias;
    u8 lmin;
    u8 lmax;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjLOD");

    if (lod_bias < -4.0f) {
        lod_bias = -4.0f;
    } else if (lod_bias >= 4.0f) {
        lod_bias = 3.99f;
    }

    lbias = (u8)(32.0f * lod_bias) & 0xFF;
    SET_REG_FIELD(__LINE__, t->mode0, 8, 9, lbias);
    ASSERTMSG1((u32)mag_filt <= 1, "%s: invalid mag_filt value", "GXInitTexObjLOD");
    SET_REG_FIELD(__LINE__, t->mode0, 1, 4, (mag_filt == GX_LINEAR) ? 1 : 0);
    ASSERTMSG1((u32)min_filt <= 5, "%s: invalid min_filt value", "GXInitTexObjLOD");
    SET_REG_FIELD(__LINE__, t->mode0, 3, 5, GX2HWFiltConv[min_filt]);
    SET_REG_FIELD(__LINE__, t->mode0, 1, 8, do_edge_lod ? 0 : 1);
    SET_REG_FIELD(__LINE__, t->mode0, 1, 17, 0);
    SET_REG_FIELD(__LINE__, t->mode0, 1, 18, 0);
    SET_REG_FIELD(__LINE__, t->mode0, 2, 19, max_aniso);
    SET_REG_FIELD(__LINE__, t->mode0, 1, 21, bias_clamp);

    if (min_lod < 0.0f) {
        min_lod = 0.0f;
    } else if (min_lod > 10.0f) {
        min_lod = 10.0f;
    }
    lmin = 16.0f * min_lod;
    if (max_lod < 0.0f) {
        max_lod = 0.0f;
    } else if (max_lod > 10.0f) {
        max_lod = 10.0f;
    }
    lmax = 16.0f * max_lod;
    SET_REG_FIELD(__LINE__, t->mode1, 8, 0, lmin);
    SET_REG_FIELD(__LINE__, t->mode1, 8, 8, lmax);
}

// GXInitTexObjData: replaces the image pointer of a texture object.
// Not in the retail binary (dead-stripped).
void GXInitTexObjData(GXTexObj* obj, void* image_ptr) {
    u32 imageBase;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjData");
    ASSERTMSG2(((u32)image_ptr & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexObjData", "image");
    imageBase = ((u32)image_ptr >> 5) & 0x01FFFFFF;
    SET_REG_FIELD(__LINE__, t->image3, 21, 0, imageBase);
}

// GXInitTexObjWrapMode: sets the S and T wrap modes of a texture object.
// 0x8023C370
void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode sm, GXTexWrapMode tm) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjWrapMode");
    SET_REG_FIELD(__LINE__, t->mode0, 2, 0, sm);
    SET_REG_FIELD(__LINE__, t->mode0, 2, 2, tm);
}

// GXInitTexObjTlut: binds a texture object to a TLUT name.
// 0x8023C390
void GXInitTexObjTlut(GXTexObj* obj, u32 tlut_name) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjTlut");
    t->tlutName = tlut_name;
}

// GXInitTexObjFilter: sets the minification and magnification filters.
// Not in the retail binary (dead-stripped).
void GXInitTexObjFilter(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjFilter");

    ASSERTMSG1((u32)mag_filt <= 1, "%s: invalid mag_filt value", "GXInitTexObjFilter");
    SET_REG_FIELD(__LINE__, t->mode0, 1, 4, mag_filt == 1 ? 1 : 0);

    ASSERTMSG1((u32)min_filt <= 5, "%s: invalid min_filt value", "GXInitTexObjFilter");
    SET_REG_FIELD(__LINE__, t->mode0, 3, 5, GX2HWFiltConv[min_filt]);
}

// GXInitTexObjMaxLOD: sets the maximum LOD.
// Not in the retail binary (dead-stripped).
void GXInitTexObjMaxLOD(GXTexObj* obj, f32 max_lod) {
    u8 lmax;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjMaxLOD");

    if (max_lod < 0.0f) {
        max_lod = 0.0f;
    } else if (max_lod > 10.0f) {
        max_lod = 10.0f;
    }
    lmax = 16.0f * max_lod;
    SET_REG_FIELD(__LINE__, t->mode1, 8, 8, lmax);
}

// GXInitTexObjMinLOD: sets the minimum LOD.
// Not in the retail binary (dead-stripped).
void GXInitTexObjMinLOD(GXTexObj* obj, f32 min_lod) {
    u8 lmin;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjMinLOD");

    if (min_lod < 0.0f) {
        min_lod = 0.0f;
    } else if (min_lod > 10.0f) {
        min_lod = 10.0f;
    }
    lmin = 16.0f * min_lod;
    SET_REG_FIELD(__LINE__, t->mode1, 8, 0, lmin);
}

// GXInitTexObjLODBias: sets the LOD bias.
// Not in the retail binary (dead-stripped).
void GXInitTexObjLODBias(GXTexObj* obj, f32 lod_bias) {
    u8 lbias;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjLODBias");

    if (lod_bias < -4.0f) {
        lod_bias = -4.0f;
    } else if (lod_bias >= 4.0f) {
        lod_bias = 3.99f;
    }
    lbias = 32.0f * lod_bias;
    SET_REG_FIELD(__LINE__, t->mode0, 8, 9, lbias);
}

// GXInitTexObjBiasClamp: enables or disables LOD bias clamping.
// Not in the retail binary (dead-stripped).
void GXInitTexObjBiasClamp(GXTexObj* obj, u8 bias_clamp) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjBiasClamp");

    SET_REG_FIELD(__LINE__, t->mode0, 1, 21, bias_clamp);
}

// GXInitTexObjEdgeLOD: enables or disables edge LOD.
// Not in the retail binary (dead-stripped).
void GXInitTexObjEdgeLOD(GXTexObj* obj, u8 do_edge_lod) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjEdgeLOD");

    SET_REG_FIELD(__LINE__, t->mode0, 1, 8, do_edge_lod ? 0 : 1);
}

// GXInitTexObjMaxAniso: sets the maximum anisotropy.
// Not in the retail binary (dead-stripped).
void GXInitTexObjMaxAniso(GXTexObj* obj, GXAnisotropy max_aniso) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjMaxAniso");

    SET_REG_FIELD(__LINE__, t->mode0, 2, 19, max_aniso);
}

// GXInitTexObjUserData: stores a user pointer in a texture object.
// 0x8023C3A0
void GXInitTexObjUserData(GXTexObj* obj, void* user_data) {
    __GXTexObjInt* t = (__GXTexObjInt*)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexObjUserData");
    t->userData = user_data;
}

// GXGetTexObjUserData: returns the user pointer of a texture object.
// 0x8023C3B0
void* GXGetTexObjUserData(const GXTexObj* obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    return t->userData;
}

// GXGetTexObjAll: returns image pointer, size, format, wrap modes and mipmap flag.
// Not in the retail binary (dead-stripped).
void GXGetTexObjAll(const GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format, GXTexWrapMode* wrap_s, GXTexWrapMode* wrap_t, u8* mipmap) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)obj;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    *image_ptr = (void *)(GET_REG_FIELD(t->image3, 21, 0) << 5);
    *width = (u32)GET_REG_FIELD(t->image0, 10, 0) + 1;
    *height = (u32)GET_REG_FIELD(t->image0, 10, 10) + 1;
    *format = t->fmt;
    *wrap_s = GET_REG_FIELD(t->mode0, 2, 0);
    *wrap_t = GET_REG_FIELD(t->mode0, 2, 2);
    *mipmap = (t->flags & 1) == 1;
}

// GXGetTexObjData: returns the image pointer.
// Not in the retail binary (dead-stripped).
void* GXGetTexObjData(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return (void*)(GET_REG_FIELD(t->image3, 21, 0) << 5);
}

// GXGetTexObjWidth: returns the image width.
// 0x8023C3C0
u16 GXGetTexObjWidth(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return (u32)GET_REG_FIELD(t->image0, 10, 0) + 1;
}

// GXGetTexObjHeight: returns the image height.
// 0x8023C3E0
u16 GXGetTexObjHeight(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return (u32)GET_REG_FIELD(t->image0, 10, 10) + 1;
}

// GXGetTexObjFmt: returns the texture format.
// 0x8023C400
GXTexFmt GXGetTexObjFmt(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return t->fmt;
}

// GXGetTexObjWrapS: returns the S wrap mode.
// 0x8023C410
GXTexWrapMode GXGetTexObjWrapS(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return GET_REG_FIELD(t->mode0, 2, 0);
}

// GXGetTexObjWrapT: returns the T wrap mode.
// 0x8023C420
GXTexWrapMode GXGetTexObjWrapT(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return GET_REG_FIELD(t->mode0, 2, 2);
}

// GXGetTexObjMipMap: returns whether the texture has mip levels.
// 0x8023C430
GXBool GXGetTexObjMipMap(const GXTexObj* to) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)to;

    ASSERTMSG(to, "Texture Object Pointer is null");
    return (t->flags & 1) == 1;
}

// GXGetTexObjLODAll: returns filters, LOD range and bias, edge LOD and anisotropy.
// Not in the retail binary (dead-stripped).
void GXGetTexObjLODAll(const GXTexObj* tex_obj, GXTexFilter* min_filt, GXTexFilter* mag_filt, f32* min_lod, f32* max_lod, f32* lod_bias, u8* bias_clamp, u8* do_edge_lod, GXAnisotropy* max_aniso) {
    s16 tmp;
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    *min_filt = HW2GXFiltConv[GET_REG_FIELD(t->mode0, 3, 5)];
    *mag_filt = GET_REG_FIELD(t->mode0, 1, 4);
    *min_lod = (u8)t->mode1 / 16.0f;
    *max_lod = (u32)GET_REG_FIELD(t->mode1, 8, 8) / 16.0f;
    tmp = (s32)GET_REG_FIELD(t->mode0, 8, 9);
    *lod_bias = (s8)tmp / 32.0f;
    *bias_clamp = (u32)GET_REG_FIELD(t->mode0, 1, 21);
    *do_edge_lod = !GET_REG_FIELD(t->mode0, 1, 8);
    *max_aniso = GET_REG_FIELD(t->mode0, 2, 19);
}

// GXGetTexObjMinFilt: returns the minification filter.
// Not in the retail binary (dead-stripped).
GXTexFilter GXGetTexObjMinFilt(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return HW2GXFiltConv[GET_REG_FIELD(t->mode0, 3, 5)];
}

// GXGetTexObjMagFilt: returns the magnification filter.
// Not in the retail binary (dead-stripped).
GXTexFilter GXGetTexObjMagFilt(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return GET_REG_FIELD(t->mode0, 1, 4);
}

// GXGetTexObjMinLOD: returns the minimum LOD.
// Not in the retail binary (dead-stripped).
f32 GXGetTexObjMinLOD(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return (u32)GET_REG_FIELD(t->mode1, 8, 0) / 16.0f;
}

// GXGetTexObjMaxLOD: returns the maximum LOD.
// Not in the retail binary (dead-stripped).
f32 GXGetTexObjMaxLOD(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return (u32)GET_REG_FIELD(t->mode1, 8, 8) / 16.0f;
}

// GXGetTexObjLODBias: returns the LOD bias.
// 0x8023C440
f32 GXGetTexObjLODBias(const GXTexObj* tex_obj) {
    s16 tmp;
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    tmp = (s32)GET_REG_FIELD(t->mode0, 8, 9);
    return (s8)tmp / 32.0f;
}

// GXGetTexObjBiasClamp: returns whether LOD bias clamping is on.
// 0x8023C480
GXBool GXGetTexObjBiasClamp(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return (u32)GET_REG_FIELD(t->mode0, 1, 21);
}

// GXGetTexObjEdgeLOD: returns whether edge LOD is on.
// 0x8023C490
GXBool GXGetTexObjEdgeLOD(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return !GET_REG_FIELD(t->mode0, 1, 8);
}

// GXGetTexObjMaxAniso: returns the maximum anisotropy.
// 0x8023C4B0
GXAnisotropy GXGetTexObjMaxAniso(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return GET_REG_FIELD(t->mode0, 2, 19);
}

// GXGetTexObjTlut: returns the TLUT name.
// 0x8023C4C0
u32 GXGetTexObjTlut(const GXTexObj* tex_obj) {
    const __GXTexObjInt* t = (const __GXTexObjInt *)tex_obj;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    return t->tlutName;
}

// GXLoadTexObjPreLoaded: writes a texture object and its preloaded cache region to texture map id.
// The const parameters (2009 SDK, as in mscharged and Petari) let the compiler assume the objects do not alias the
// FIFO port, so the six updated words stay in registers across the FIFO writes. The function still writes the
// register ids into the objects through the cast-away pointers, as the SDK does.
// 0x8023C4D0
void GXLoadTexObjPreLoaded(const GXTexObj* obj, const GXTexRegion* region, GXTexMapID id) {
    __GXTlutRegionInt* tlr;
    __GXTexObjInt* t = (__GXTexObjInt*)obj;
    __GXTexRegionInt* r = (__GXTexRegionInt *)region;

    ASSERTMSG(obj, "Texture Object Pointer is null");
    ASSERTMSG(region, "TexRegion Object Pointer is null");
    CHECK_GXBEGIN("GXLoadTexObjPreLoaded");
    ASSERTMSG1(id < GX_MAX_TEXMAP, "%s: invalid texture map ID", "GXLoadTexObj");

    SET_REG_FIELD(__LINE__, t->mode0, 8, 24, GXTexMode0Ids[id]);
    SET_REG_FIELD(__LINE__, t->mode1, 8, 24, GXTexMode1Ids[id]);
    SET_REG_FIELD(__LINE__, t->image0, 8, 24, GXTexImage0Ids[id]);
    SET_REG_FIELD(__LINE__, r->image1, 8, 24, GXTexImage1Ids[id]);
    SET_REG_FIELD(__LINE__, r->image2, 8, 24, GXTexImage2Ids[id]);
    SET_REG_FIELD(__LINE__, t->image3, 8, 24, GXTexImage3Ids[id]);

    GX_WRITE_RAS_REG(t->mode0);
    GX_WRITE_RAS_REG(t->mode1);
    GX_WRITE_RAS_REG(t->image0);
    GX_WRITE_RAS_REG(r->image1);
    GX_WRITE_RAS_REG(r->image2);
    GX_WRITE_RAS_REG(t->image3);

    if (!(t->flags & 2)) {
        ASSERTMSG1(__GXData->tlutRegionCallback, "%s: Tex/Tlut Region Callback not set", "GXLoadTexObj/PreLoaded");
        tlr = (__GXTlutRegionInt*)__GXData->tlutRegionCallback(t->tlutName);
        ASSERTMSG1(tlr, "%s: Tex/Tlut Region Callback returns NULL", "GXLoadTexObj/PreLoaded");

        SET_REG_FIELD(__LINE__, tlr->tlutObj.tlut, 8, 24, GXTexTlutIds[id]);
        GX_WRITE_RAS_REG(tlr->tlutObj.tlut);
    }

    __GXData->tImage0[id] = t->image0;
    __GXData->tMode0[id] = t->mode0;
    __GXData->dirtyState |= 1;
    __GXData->bpSentNot = 0;
}

// GXLoadTexObj: picks a cache region through the region callback and loads the texture object into map id.
// 0x8023C640
void GXLoadTexObj(GXTexObj* obj, GXTexMapID id) {
    GXTexRegion* r;

    CHECK_GXBEGIN("GXLoadTexObj");
    ASSERTMSG1(id < 8, "%s: invalid texture map ID", "GXLoadTexObj");
    ASSERTMSG1(__GXData->texRegionCallback, "%s: Tex/Tlut Region Callback not set", "GXLoadTexObj");
    r = __GXData->texRegionCallback(obj, id);
    ASSERTMSG1(r, "%s: Tex/Tlut Region Callback returns NULL", "GXLoadTexObj");
    GXLoadTexObjPreLoaded(obj, r, id);
}

// GXInitTlutObj: fills a TLUT object from a palette pointer, format and entry count.
// 0x8023C6A0
void GXInitTlutObj(GXTlutObj* tlut_obj, void* lut, GXTlutFmt fmt, u16 n_entries) {
    __GXTlutObjInt* t = (__GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    CHECK_GXBEGIN("GXInitTlutObj");
    ASSERTMSG1(n_entries <= 0x4000, "%s: number of entries exceeds maximum", "GXInitTlutObj");
    ASSERTMSG2(((u32)lut & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTlutObj", "Tlut");
    t->tlut = 0;
    SET_REG_FIELD(__LINE__, t->tlut, 2, 10, fmt);
    SET_REG_FIELD(__LINE__, t->loadTlut0, 24, 0, GX_PHY_ADDR(lut) >> 5);
    SET_REG_FIELD(__LINE__, t->loadTlut0, 8, 24, 0x64);
    t->numEntries = n_entries;
}

// GXGetTlutObjAll: returns palette pointer, format and entry count.
// Not in the retail binary (dead-stripped).
void GXGetTlutObjAll(const GXTlutObj* tlut_obj, void **data, GXTlutFmt* format, u16* numEntries) {
    const __GXTlutObjInt* t = (const __GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    *data = (void *)(GET_REG_FIELD(t->loadTlut0, 21, 0) << 5);
    *format = GET_REG_FIELD(t->tlut, 2, 10);
    *numEntries = t->numEntries;
}

// GXGetTlutObjData: returns the palette pointer.
// Not in the retail binary (dead-stripped).
void* GXGetTlutObjData(const GXTlutObj* tlut_obj) {
    const __GXTlutObjInt* t = (const __GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    return (void *)(GET_REG_FIELD(t->loadTlut0, 21, 0) << 5);
}

// GXGetTlutObjFmt: returns the palette format.
// Not in the retail binary (dead-stripped).
GXTlutFmt GXGetTlutObjFmt(const GXTlutObj* tlut_obj) {
    const __GXTlutObjInt* t = (const __GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    return GET_REG_FIELD(t->tlut, 2, 10);
}

// GXGetTlutObjNumEntries: returns the palette entry count.
// Not in the retail binary (dead-stripped).
u16 GXGetTlutObjNumEntries(const GXTlutObj* tlut_obj) {
    const __GXTlutObjInt* t = (const __GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    return t->numEntries;
}

// GXLoadTlut: loads a palette into the TMEM region the TLUT region callback chose for tlut_name.
// The const parameter (2009 SDK, mscharged and Petari) lets the store of the updated `t->tlut` sink below the copy
// into `r->tlutObj`.
// 0x8023C6D0
void GXLoadTlut(const GXTlutObj* tlut_obj, u32 tlut_name) {
    __GXTlutRegionInt* r;
    u32 tlut_offset;
    __GXTlutObjInt* t = (__GXTlutObjInt *)tlut_obj;

    ASSERTMSG(tlut_obj, "TLut Object Pointer is null");
    CHECK_GXBEGIN("GXLoadTlut");
    ASSERTMSG1(__GXData->tlutRegionCallback, "%s: Tex/Tlut Region Callback not set", "GXLoadTlut");
    r = (__GXTlutRegionInt *)__GXData->tlutRegionCallback(tlut_name);
    ASSERTMSG1(r, "%s: Tex/Tlut Region Callback returns NULL", "GXLoadTlut");

    __GXFlushTextureState();
    GX_WRITE_RAS_REG(t->loadTlut0);
    GX_WRITE_RAS_REG(r->loadTlut1);
    __GXFlushTextureState();
    tlut_offset = r->loadTlut1 & 0x3FF;
    SET_REG_FIELD(__LINE__, t->tlut, 10, 0, tlut_offset);
    r->tlutObj = *t;
}

// GXInitTexCacheRegion: sets up a texture cache region (TMEM addresses and sizes for even and odd lines).
// 0x8023C770
void GXInitTexCacheRegion(GXTexRegion* region, u8 is_32b_mipmap, u32 tmem_even, GXTexCacheSize size_even, u32 tmem_odd, GXTexCacheSize size_odd) {
    u32 WidthExp2;
    __GXTexRegionInt* t = (__GXTexRegionInt*)region;

    ASSERTMSG(region, "TexRegion Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexCacheRegion");
    ASSERTMSG2((tmem_even & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexCacheRegion", "tmem even");
    ASSERTMSG2((tmem_odd & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexCacheRegion", "tmem odd");

    switch (size_even) {
    case GX_TEXCACHE_32K:
        WidthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        WidthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        WidthExp2 = 5;
        break;
    default:
        ASSERTMSG2(0, "%s: Invalid %s size", "GXInitTexCacheRegion", "tmem even");
        break;
    }

    t->image1 = 0;
    SET_REG_FIELD(__LINE__, t->image1, 15, 0, tmem_even >> 5);
    SET_REG_FIELD(__LINE__, t->image1, 3, 15, WidthExp2);
    SET_REG_FIELD(__LINE__, t->image1, 3, 18, WidthExp2);
    SET_REG_FIELD(__LINE__, t->image1, 1, 21, 0);

    switch (size_odd) {
    case GX_TEXCACHE_32K:
        WidthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        WidthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        WidthExp2 = 5;
        break;
    case GX_TEXCACHE_NONE:
        WidthExp2 = 0;
        break;
    default:
        ASSERTMSG2(0, "%s: Invalid %s size", "GXInitTexCacheRegion", "tmem odd");
        break;
    }

    t->image2 = 0;
    SET_REG_FIELD(__LINE__, t->image2, 15, 0, tmem_odd >> 5);
    SET_REG_FIELD(__LINE__, t->image2, 3, 15, WidthExp2);
    SET_REG_FIELD(__LINE__, t->image2, 3, 18, WidthExp2);
    t->is32bMipmap = (u8)is_32b_mipmap;
    t->isCached = 1;
}

// GXInitTexPreLoadRegion: sets up a preload region of TMEM.
// Not in the retail binary (dead-stripped).
void GXInitTexPreLoadRegion(GXTexRegion* region, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd) {
    __GXTexRegionInt* t = (__GXTexRegionInt *)region;

    ASSERTMSG(region, "TexRegion Object Pointer is null");
    CHECK_GXBEGIN("GXInitTexPreLoadRegion");
    ASSERTMSG2((tmem_even & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexPreLoadRegion", "tmem even");
    ASSERTMSG2((tmem_odd & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexPreLoadRegion", "tmem odd");
    ASSERTMSG2((size_even & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexPreLoadRegion", "size even");
    ASSERTMSG2((size_odd & 0x1F) == 0, "%s: %s pointer not aligned to 32B", "GXInitTexPreLoadRegion", "size odd");

    t->image1 = 0;
    SET_REG_FIELD(__LINE__, t->image1, 15, 0, tmem_even >> 5);
    SET_REG_FIELD(__LINE__, t->image1, 3, 15, 0);
    SET_REG_FIELD(__LINE__, t->image1, 3, 18, 0);
    SET_REG_FIELD(__LINE__, t->image1, 1, 21, 1);

    t->image2 = 0;
    SET_REG_FIELD(__LINE__, t->image2, 15, 0, tmem_odd >> 5);
    SET_REG_FIELD(__LINE__, t->image2, 3, 15, 0);
    SET_REG_FIELD(__LINE__, t->image2, 3, 18, 0);
    t->is32bMipmap = 0;
    t->isCached = 0;
    t->sizeEven = (u16)(size_even >> 5);
    t->sizeOdd = (u16)(size_odd >> 5);
}

// GXGetTexRegionAll: returns the settings of a texture region.
// Not in the retail binary (dead-stripped).
void GXGetTexRegionAll(const GXTexRegion* region, u8* is_cached, u8* is_32b_mipmap, u32* tmem_even, u32* size_even, u32* tmem_odd, u32* size_odd) {
    const __GXTexRegionInt* t = (const __GXTexRegionInt *)region;

    ASSERTMSG(region, "TexRegion Object Pointer is null");
    *tmem_even = GET_REG_FIELD(t->image1, 15, 0) << 5;
    *tmem_odd = GET_REG_FIELD(t->image2, 15, 0) << 5;
    if (t->isCached) {
        switch (GET_REG_FIELD(t->image1, 3, 15)) {
        case 3:
            *size_even = 0x8000;
            break;
        case 4:
            *size_even = 0x20000;
            break;
        case 5:
            *size_even = 0x80000;
            break;
        default:
            *size_even = 0;
            break;
        }

        switch (GET_REG_FIELD(t->image2, 3, 15)) {
        case 3:
            *size_odd = 0x8000;
            break;
        case 4:
            *size_odd = 0x20000;
            break;
        case 5:
            *size_odd = 0x80000;
            break;
        default:
            *size_odd = 0;
            break;
        }
    } else {
        *size_even = t->sizeEven << 5;
        *size_odd = t->sizeOdd << 5;
    }

    *is_32b_mipmap = t->is32bMipmap;
    *is_cached = t->isCached;
}

// GXInitTlutRegion: sets up a TLUT region (TMEM address and size).
// 0x8023C820
void GXInitTlutRegion(GXTlutRegion* region, u32 tmem_addr, GXTlutSize tlut_size) {
    __GXTlutRegionInt* t = (__GXTlutRegionInt *)region;

    ASSERTMSG(region, "TLutRegion Object Pointer is null");
    CHECK_GXBEGIN("GXInitTlutRegion");
    ASSERTMSG1((tmem_addr & 0x1FF) == 0, "%s: tmem pointer is not aligned to 512B", "GXInitTlutRegion");
    ASSERTMSG1(tlut_size <= 0x400, "%s: tlut size exceeds 16K", "GXInitTlutRegion");
    t->loadTlut1 = 0;
    tmem_addr -= 0x80000;
    SET_REG_FIELD(__LINE__, t->loadTlut1, 10, 0, tmem_addr >> 9);
    SET_REG_FIELD(__LINE__, t->loadTlut1, 11, 10, tlut_size);
    SET_REG_FIELD(__LINE__, t->loadTlut1, 8, 24, 0x65);
}

// GXGetTlutRegionAll: returns the TMEM address and size of a TLUT region.
// Not in the retail binary (dead-stripped).
void GXGetTlutRegionAll(const GXTlutRegion* region, u32* tmem_addr, GXTlutSize* tlut_size) {
    const __GXTlutRegionInt* t = (const __GXTlutRegionInt *)region;

    ASSERTMSG(region, "TLutRegion Object Pointer is null");
    *tmem_addr = (GET_REG_FIELD(t->loadTlut1, 10, 0) << 9) + 0x80000;
    *tlut_size = GET_REG_FIELD(t->loadTlut1, 11, 10);
}

// GXInvalidateTexRegion: invalidates the TMEM cache lines of a texture region.
// The parameter is const like the other region and object parameters of the 2009 SDK (no reference has this
// function): only then are the loads from `*r` scheduled among the callee-saved register stores as in retail.
// 0x8023C840
void GXInvalidateTexRegion(const GXTexRegion* region) {
    s32 wle;
    s32 hle;
    s32 wlo;
    s32 hlo;
    s32 count;
    u32 reg0;
    u32 reg1;
    const __GXTexRegionInt* r = (const __GXTexRegionInt*)region;

    ASSERTMSG(region, "TexRegion Object Pointer is null");
    CHECK_GXBEGIN("GXInvalidateTexRegion");

    wle = GET_REG_FIELD(r->image1, 3, 15) - 1;
    hle = GET_REG_FIELD(r->image1, 3, 18) - 1;
    wlo = GET_REG_FIELD(r->image2, 3, 15) - 1;
    hlo = GET_REG_FIELD(r->image2, 3, 18) - 1;
    if (wle < 0) {
        wle = 0;
    }
    if (hle < 0) {
        hle = 0;
    }
    if (wlo < 0) {
        wlo = 0;
    }
    if (hlo < 0) {
        hlo = 0;
    }
    count = wle + hle;
    if (r->is32bMipmap) {
        count = wlo + hlo - 2 + count;
    }

    reg0 = 0;
    SET_REG_FIELD(__LINE__, reg0, 9, 0, GET_REG_FIELD(r->image1, 9, 6));
    SET_REG_FIELD(__LINE__, reg0, 4, 9, count);
    SET_REG_FIELD(__LINE__, reg0, 8, 24, 0x66);

    if (wlo != 0) {
        count = wlo + hlo;
        if (r->is32bMipmap) {
            count = wle + hle - 2 + count;
        }
        reg1 = 0;
        SET_REG_FIELD(__LINE__, reg1, 9, 0, GET_REG_FIELD(r->image2, 9, 6));
        SET_REG_FIELD(__LINE__, reg1, 4, 9, count);
        SET_REG_FIELD(__LINE__, reg1, 8, 24, 0x66);
    }

    __GXFlushTextureState();
    GX_WRITE_RAS_REG(reg0);
    if (wlo != 0) {
        GX_WRITE_RAS_REG(reg1);
    }
    __GXFlushTextureState();
}

// GXInvalidateTexAll: invalidates the whole texture cache.
// 0x8023C960
void GXInvalidateTexAll(void) {
    u32 reg0;
    u32 reg1;

    CHECK_GXBEGIN("GXInvalidateTexAll");
    reg0 = 0x66001000;
    reg1 = 0x66001100;
    __GXFlushTextureState();
    GX_WRITE_RAS_REG(reg0);
    GX_WRITE_RAS_REG(reg1);
    __GXFlushTextureState();
}

// GXSetTexRegionCallback: installs the texture region callback; returns the old one.
// 0x8023C9B0
GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback f) {
    GXTexRegionCallback oldcb = __GXData->texRegionCallback;

    __GXData->texRegionCallback = f;
    return oldcb;
}

// GXSetTlutRegionCallback: installs the TLUT region callback; returns the old one.
// 0x8023C9D0
GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback f) {
    GXTlutRegionCallback oldcb = __GXData->tlutRegionCallback;

    __GXData->tlutRegionCallback = f;
    return oldcb;
}

// GXPreLoadEntireTexture: loads all mip levels of a texture into a preload region.
// Not in the retail binary (dead-stripped).
void GXPreLoadEntireTexture(GXTexObj* tex_obj, GXTexRegion* region) {
    GXBool isMipMap;
    GXBool is32bit; 
    u32 wd;
    u32 ht;
    u32 maxLevelIndex;
    u32 loadImage0;
    u32 loadImage1;
    u32 loadImage2;
    u32 loadImage3;
    u32 base; 
    u32 tmem1; 
    u32 tmem2; 
    u32 tmemAR;
    u32 tmemGB;
    u32 nTiles;
#if DEBUG
    u32 totalOdd; 
    u32 totalEven;
    u32 count; 
#endif
    u32 rowTiles; 
    u32 colTiles; 
    u32 cmpTiles; 
    u32 i;
    __GXTexObjInt* t = (__GXTexObjInt*)tex_obj;
    __GXTexRegionInt* r = (__GXTexRegionInt*)region;

    ASSERTMSG(tex_obj, "Texture Object Pointer is null");
    ASSERTMSG(region, "TexRegion Object Pointer is null");
    CHECK_GXBEGIN("GXPreLoadEntireTexture");
    isMipMap = (t->flags & 1) == 1;
    is32bit = GET_REG_FIELD(t->image0, 4, 20) == 6;

    loadImage0 = 0;
    SET_REG_FIELD(__LINE__, loadImage0, 8, 24, 0x60);
    base = t->image3 & 0x1FFFFF;
    SET_REG_FIELD(__LINE__, loadImage0, 21, 0, base);

    loadImage1 = 0;
    SET_REG_FIELD(__LINE__, loadImage1, 8, 24, 0x61);
    tmem1 = r->image1 & 0x7FFF;
    SET_REG_FIELD(__LINE__, loadImage1, 15, 0, tmem1);

    loadImage2 = 0;
    SET_REG_FIELD(__LINE__, loadImage2, 8, 24, 0x62);
    tmem2 = r->image2 & 0x7FFF;
    SET_REG_FIELD(__LINE__, loadImage2, 15, 0, tmem2);

    loadImage3 = 0;
    SET_REG_FIELD(__LINE__, loadImage3, 8, 24, 0x63);
    SET_REG_FIELD(__LINE__, loadImage3, 15, 0, t->loadCnt);
    SET_REG_FIELD(__LINE__, loadImage3, 2, 15, t->loadFmt);
    maxLevelIndex = 0;
    nTiles = t->loadCnt;

    if (isMipMap) {
        wd = GET_REG_FIELD(t->image0, 10, 0) + 1;
        ht = GET_REG_FIELD(t->image0, 10, 10) + 1;
        if (wd > ht) {
            maxLevelIndex = (u16)(31 - __cntlzw(wd));
        } else {
            maxLevelIndex = (u16)(31 - __cntlzw(ht));
        }

#if DEBUG
        count = nTiles;
        totalOdd = totalEven = 0;

        for (i = 0; i < maxLevelIndex; i++) {
            if (i & 1) {
                if (count == 0) {
                    count = 1;
                }
                totalOdd += count;
            } else {
                if (count == 0) {
                    count = 1;
                }
                totalEven += count;
            }
            __GetImageTileCount(t->fmt, wd >> (i + 1), ht >> (i + 1), &rowTiles, &colTiles, &cmpTiles);
            count = rowTiles * colTiles;
        }
#endif
    } else {
#if DEBUG
        totalEven = (nTiles == 0) ? 1 : nTiles;
        totalOdd = totalEven;
#endif
    }

#if DEBUG
    if (is32bit) {
        totalOdd = isMipMap ? totalOdd : 0;
        totalEven = totalEven + totalOdd;
        ASSERTMSG(totalEven <= r->sizeEven, "GXPreLoadEntireTexture: Even tmem size does not match the texture size");
        ASSERTMSG(totalEven <= r->sizeOdd, "GXPreLoadEntireTexture: Odd tmem size does not match the texture size");
    } else if (isMipMap != 0) {
        if (r->sizeEven > r->sizeOdd) {
            ASSERTMSG(totalEven <= r->sizeEven, "GXPreLoadEntireTexture: Even tmem size does not match the texture size");
            ASSERTMSG(totalOdd <= r->sizeOdd, "GXPreLoadEntireTexture: Odd tmem size does not match the texture size");
        } else {
            ASSERTMSG(totalEven <= r->sizeOdd, "GXPreLoadEntireTexture: Odd tmem size does not match the texture size");
            ASSERTMSG(totalOdd <= r->sizeEven, "GXPreLoadEntireTexture: Even tmem size does not match the texture size");
        }
    } else if (r->sizeEven > r->sizeOdd) {
        ASSERTMSG(totalEven <= r->sizeEven, "GXPreLoadEntireTexture: Even tmem size does not match the texture size");
    } else {
        ASSERTMSG(totalOdd <= r->sizeOdd, "GXPreLoadEntireTexture: Odd tmem size does not match the texture size");
    }
#endif

    __GXFlushTextureState();
    GX_WRITE_RAS_REG(loadImage0);
    GX_WRITE_RAS_REG(loadImage1);
    GX_WRITE_RAS_REG(loadImage2);
    GX_WRITE_RAS_REG(loadImage3);

    if (maxLevelIndex != 0) {
        tmemAR = tmem1;
        tmemGB = tmem2;
        for (i = 0; i < maxLevelIndex; i++) {
            if (is32bit) {
                base += nTiles * 2;
                tmemAR += nTiles;
                tmemGB += nTiles;
            } else {
                base += nTiles;
                if (i & 1) {
                    tmemGB += nTiles;
                } else {
                    tmemAR += nTiles;
                }
            }
            tmem1 = (i & 1) ? tmemAR : tmemGB;
            tmem2 = (i & 1) ? tmemGB : tmemAR;
            __GetImageTileCount(t->fmt, (u16) (wd >> (i + 1)), (u16) (ht >> (i + 1)), &rowTiles, &colTiles, &cmpTiles);
            nTiles = rowTiles * colTiles;
            SET_REG_FIELD(__LINE__, loadImage0, 21, 0, base);
            SET_REG_FIELD(__LINE__, loadImage1, 15, 0, tmem1);
            SET_REG_FIELD(__LINE__, loadImage2, 15, 0, tmem2);
            SET_REG_FIELD(__LINE__, loadImage3, 15, 0, nTiles);
            GX_WRITE_RAS_REG(loadImage0);
            GX_WRITE_RAS_REG(loadImage1);
            GX_WRITE_RAS_REG(loadImage2);
            GX_WRITE_RAS_REG(loadImage3);
        }
    }
    __GXFlushTextureState();
}

// GXSetTexCoordScaleManually: enables manual S/T scale for a texcoord.
// 0x8023C9F0
void GXSetTexCoordScaleManually(GXTexCoordID coord, GXBool enable, u16 ss, u16 ts) {
    CHECK_GXBEGIN("GXSetTexCoordScaleManually");
    ASSERTMSG1(coord < GX_MAX_TEXCOORD, "%s: bad texcoord specified", "GXSetTexCoordScaleManually");
    __GXData->tcsManEnab = (__GXData->tcsManEnab & ~(1 << coord)) | (enable << coord);

    if (enable) {
        SET_REG_FIELD(__LINE__, __GXData->suTs0[coord], 16, 0, (u16)(ss - 1));
        SET_REG_FIELD(__LINE__, __GXData->suTs1[coord], 16, 0, (u16)(ts - 1));
        GX_WRITE_RAS_REG(__GXData->suTs0[coord]);
        GX_WRITE_RAS_REG(__GXData->suTs1[coord]);
        __GXData->bpSentNot = 0;
    }
}

// GXSetTexCoordCylWrap: enables cylindrical wrapping for a texcoord.
// Not in the retail binary (dead-stripped).
void GXSetTexCoordCylWrap(GXTexCoordID coord, u8 s_enable, u8 t_enable) {
    CHECK_GXBEGIN("GXSetTexCoordCylWrap");
    ASSERTMSG1(coord < GX_MAX_TEXCOORD, "%s: bad texcoord specified", "GXSetTexCoordCylWrap");
    SET_REG_FIELD(__LINE__, __GXData->suTs0[coord], 1, 17, s_enable);
    SET_REG_FIELD(__LINE__, __GXData->suTs1[coord], 1, 17, t_enable);

    if (__GXData->tcsManEnab & (1 << coord)) {
        GX_WRITE_RAS_REG(__GXData->suTs0[coord]);
        GX_WRITE_RAS_REG(__GXData->suTs1[coord]);
        __GXData->bpSentNot = 0;
    }
}

// GXSetTexCoordBias: enables the S/T bias of a texcoord.
// 0x8023CA70
void GXSetTexCoordBias(GXTexCoordID coord, u8 s_enable, u8 t_enable) {
    CHECK_GXBEGIN("GXSetTexCoordBias");
    ASSERTMSG1(coord < GX_MAX_TEXCOORD, "%s: bad texcoord specified", "GXSetTexCoordBias");
    SET_REG_FIELD(__LINE__, __GXData->suTs0[coord], 1, 16, s_enable);
    SET_REG_FIELD(__LINE__, __GXData->suTs1[coord], 1, 16, t_enable);

    if (__GXData->tcsManEnab & (1 << coord)) {
        GX_WRITE_RAS_REG(__GXData->suTs0[coord]);
        GX_WRITE_RAS_REG(__GXData->suTs1[coord]);
        __GXData->bpSentNot = 0;
    }
}

// __SetSURegs: writes the SU texture size and wrap registers of one texcoord from its texmap.
// 0x8023CAE0
static void __SetSURegs(u32 tmap, u32 tcoord) {
    u32 w;
    u32 h;
    u8 s_bias;
    u8 t_bias;

    w = GET_REG_FIELD(__GXData->tImage0[tmap], 10, 0);
    h = GET_REG_FIELD(__GXData->tImage0[tmap], 10, 10);
    SET_REG_FIELD(__LINE__, __GXData->suTs0[tcoord], 16, 0, w);
    SET_REG_FIELD(__LINE__, __GXData->suTs1[tcoord], 16, 0, h);
    s_bias = GET_REG_FIELD(__GXData->tMode0[tmap], 2, 0) == 1;
    t_bias = GET_REG_FIELD(__GXData->tMode0[tmap], 2, 2) == 1;
    SET_REG_FIELD(__LINE__, __GXData->suTs0[tcoord], 1, 16, s_bias);
    SET_REG_FIELD(__LINE__, __GXData->suTs1[tcoord], 1, 16, t_bias);
    GX_WRITE_RAS_REG(__GXData->suTs0[tcoord]);
    GX_WRITE_RAS_REG(__GXData->suTs1[tcoord]);
    __GXData->bpSentNot = 0;
}

// __GXSetSUTexRegs: recomputes the SU texture size registers of all texcoords from the TEV stage assignments.
// 0x8023CB70
void __GXSetSUTexRegs(void) {
    u32 nStages;
    u32 nIndStages;
    u32 i;
    u32 map;
    u32 tmap;
    u32 coord;
    u32* ptref;

    if (__GXData->tcsManEnab != 0xFF) {
        nStages = GET_REG_FIELD(__GXData->genMode, 4, 10) + 1;
        nIndStages = GET_REG_FIELD(__GXData->genMode, 3, 16);
        for (i = 0; i < nIndStages; i++) {
            switch (i) {
            case 0:
                tmap = GET_REG_FIELD(__GXData->iref, 3, 0);
                coord = GET_REG_FIELD(__GXData->iref, 3, 3);
                break;
            case 1:
                tmap = GET_REG_FIELD(__GXData->iref, 3, 6);
                coord = GET_REG_FIELD(__GXData->iref, 3, 9);
                break;
            case 2:
                tmap = GET_REG_FIELD(__GXData->iref, 3, 12);
                coord = GET_REG_FIELD(__GXData->iref, 3, 15);
                break;
            case 3:
                tmap = GET_REG_FIELD(__GXData->iref, 3, 18);
                coord = GET_REG_FIELD(__GXData->iref, 3, 21);
                break;
            }
            if (!(__GXData->tcsManEnab & (1 << coord))) {
                __SetSURegs(tmap, coord);
            }
        }

        for (i = 0; i < nStages; i++) {
            ptref = &__GXData->tref[i / 2];
            map = __GXData->texmapId[i];
            tmap = map & 0xFFFFFEFF;
            if (i & 1) {
                coord = GET_REG_FIELD(*ptref, 3, 15);
            } else {
                coord = GET_REG_FIELD(*ptref, 3, 3);
            }
            if ((tmap != 0xFF) && !(__GXData->tcsManEnab & (1 << coord)) && (__GXData->tevTcEnab & (1 << i))) {
                __SetSURegs(tmap, coord);
            }
        }
    }
}

// __GXGetSUTexSize: returns the size the texcoord's SU registers hold.
// Not in the retail binary (dead-stripped).
void __GXGetSUTexSize(GXTexCoordID coord, u16* width, u16* height) {
    *width = (u16)__GXData->suTs0[coord] + 1;
    *height = (u16)__GXData->suTs1[coord] + 1;
}

// __GXSetTmemConfig: programs the TMEM layout (config 0, 1 or 2).
// 0x8023CCE0
void __GXSetTmemConfig(u32 config) {
    switch (config) {
    case 2:
        GX_WRITE_RAS_REG(0x8c0d8000);
        GX_WRITE_RAS_REG(0x900dc000);

        GX_WRITE_RAS_REG(0x8d0d8800);
        GX_WRITE_RAS_REG(0x910dc800);

        GX_WRITE_RAS_REG(0x8e0d9000);
        GX_WRITE_RAS_REG(0x920dd000);

        GX_WRITE_RAS_REG(0x8f0d9800);
        GX_WRITE_RAS_REG(0x930dd800);

        GX_WRITE_RAS_REG(0xac0da000);
        GX_WRITE_RAS_REG(0xb00dc400);

        GX_WRITE_RAS_REG(0xad0da800);
        GX_WRITE_RAS_REG(0xb10dcc00);

        GX_WRITE_RAS_REG(0xae0db000);
        GX_WRITE_RAS_REG(0xb20dd400);

        GX_WRITE_RAS_REG(0xaf0db800);
        GX_WRITE_RAS_REG(0xb30ddc00);
        break;
    case 1:
        GX_WRITE_RAS_REG(0x8c0d8000);
        GX_WRITE_RAS_REG(0x900dc000);

        GX_WRITE_RAS_REG(0x8d0d8800);
        GX_WRITE_RAS_REG(0x910dc800);

        GX_WRITE_RAS_REG(0x8e0d9000);
        GX_WRITE_RAS_REG(0x920dd000);

        GX_WRITE_RAS_REG(0x8f0d9800);
        GX_WRITE_RAS_REG(0x930dd800);

        GX_WRITE_RAS_REG(0xac0da000);
        GX_WRITE_RAS_REG(0xb00de000);

        GX_WRITE_RAS_REG(0xad0da800);
        GX_WRITE_RAS_REG(0xb10de800);

        GX_WRITE_RAS_REG(0xae0db000);
        GX_WRITE_RAS_REG(0xb20df000);

        GX_WRITE_RAS_REG(0xaf0db800);
        GX_WRITE_RAS_REG(0xb30df800);

        break;
    case 0:
    default:
        GX_WRITE_RAS_REG(0x8c0d8000);
        GX_WRITE_RAS_REG(0x900dc000);

        GX_WRITE_RAS_REG(0x8d0d8400);
        GX_WRITE_RAS_REG(0x910dc400);

        GX_WRITE_RAS_REG(0x8e0d8800);
        GX_WRITE_RAS_REG(0x920dc800);

        GX_WRITE_RAS_REG(0x8f0d8c00);
        GX_WRITE_RAS_REG(0x930dcc00);

        GX_WRITE_RAS_REG(0xac0d9000);
        GX_WRITE_RAS_REG(0xb00dd000);

        GX_WRITE_RAS_REG(0xad0d9400);
        GX_WRITE_RAS_REG(0xb10dd400);

        GX_WRITE_RAS_REG(0xae0d9800);
        GX_WRITE_RAS_REG(0xb20dd800);

        GX_WRITE_RAS_REG(0xaf0d9c00);
        GX_WRITE_RAS_REG(0xb30ddc00);

        break;
    }
}
