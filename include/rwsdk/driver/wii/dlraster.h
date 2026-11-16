// Unit name: Gormiti link map (RwCore.a dlraster); only the declarations dltexdic.c needs
// Layout: Gormiti DWARF (RwGameCubeRasterExtension)
#ifndef RWSDK_DRIVER_WII_DLRASTER_H
#define RWSDK_DRIVER_WII_DLRASTER_H

#include <revolution/gx/GXStruct.h>
#include <rwsdk/baraster.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The fields of RwGameCubeRasterExtension.flags. */
#define rwGAMECUBERASTERALPHA 0x1 /* the pixels carry alpha */

/*
 * The driver's data in a raster: how GX sees its pixels. A sub raster shares its parent's
 * extension, which is why the stream writer reads the extension of the parent.
 */
typedef struct RwGameCubeRasterExtension {
    GXTlutObj tlutObj;            /* 0x00 */
    RwUInt32 format;              /* 0x0C: GXTexFmt */
    RwUInt32 tlutFmt;             /* 0x10: GXTlutFmt */
    RwUInt32 flags;               /* 0x14: rwGAMECUBERASTER* */
    RwUInt8* memory;              /* 0x18 */
    RwUInt8* pixels;              /* 0x1C */
    RwUInt8* palette;             /* 0x20 */
    RwUInt8* lockedPixels;        /* 0x24 */
    RwUInt8* lockedBuffer;        /* 0x28 */
    void* region;                 /* 0x2C */
    RwUInt16 token;               /* 0x30 */
    RwUInt8 maxLOD;               /* 0x32 */
    RwUInt8 lockedMipLevel;       /* 0x33 */
} RwGameCubeRasterExtension;

/* Where the extension sits in a raster (the offset the raster plugin was registered at). */
extern RwInt32 _RwGameCubeRasterExtOffset;

#define RASTEREXTFROMRASTER(_raster) \
    ((RwGameCubeRasterExtension*)(((RwUInt8*)(_raster)) + _RwGameCubeRasterExtOffset))

/* The size in bytes of the raster's pixels (every mip level). */
RwUInt32 _rwDlRasterGetSize(RwRaster* raster);

/* Allocates the pixels and palette of a raster created with rwRASTERDONTALLOCATE. */
RwBool _rwDlTextureRasterCreate(RwRaster* raster, RwUInt32 numMipLevels);

#ifdef __cplusplus
}
#endif

#endif
