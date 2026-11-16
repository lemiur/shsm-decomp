// Reference: tgsm/pbg include/rwsdk/baraster.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BARASTER_H
#define RWSDK_BARASTER_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwRasterLockMode: how RwRasterLock and RwRasterLockPalette lock a raster. */
typedef enum RwRasterLockMode {
    rwRASTERLOCKWRITE = 0x01,
    rwRASTERLOCKREAD = 0x02,
    rwRASTERLOCKNOFETCH = 0x04,
    rwRASTERLOCKRAW = 0x08,
    rwRASTERLOCKMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwRasterLockMode;

#define rwRASTERLOCKREADWRITE (rwRASTERLOCKREAD | rwRASTERLOCKWRITE)

/* RwRasterType: the kind of raster (cType), with the creation flags above the type mask. */
typedef enum RwRasterType {
    rwRASTERTYPENORMAL = 0x00,
    rwRASTERTYPEZBUFFER = 0x01,
    rwRASTERTYPECAMERA = 0x02,
    rwRASTERTYPETEXTURE = 0x04,
    rwRASTERTYPECAMERATEXTURE = 0x05,
    rwRASTERTYPEMASK = 0x07,
    rwRASTERPALETTEVOLATILE = 0x40,
    rwRASTERDONTALLOCATE = 0x80,
    rwRASTERTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwRasterType;

/* RwRasterFormat: the pixel format; a raster keeps its upper byte in cFormat. */
typedef enum RwRasterFormat {
    rwRASTERFORMATDEFAULT = 0x0000,
    rwRASTERFORMAT1555 = 0x0100,
    rwRASTERFORMAT565 = 0x0200,
    rwRASTERFORMAT4444 = 0x0300,
    rwRASTERFORMATLUM8 = 0x0400,
    rwRASTERFORMAT8888 = 0x0500,
    rwRASTERFORMAT888 = 0x0600,
    rwRASTERFORMAT16 = 0x0700,
    rwRASTERFORMAT24 = 0x0800,
    rwRASTERFORMAT32 = 0x0900,
    rwRASTERFORMAT555 = 0x0A00,
    rwRASTERFORMATAUTOMIPMAP = 0x1000,
    rwRASTERFORMATPAL8 = 0x2000,
    rwRASTERFORMATPAL4 = 0x4000,
    rwRASTERFORMATMIPMAP = 0x8000,
    rwRASTERFORMATPIXELFORMATMASK = 0x0F00,
    rwRASTERFORMATMASK = 0xFF00,
    rwRASTERFORMATFORCEENUMSIZEINT = 0x7FFFFFFF
} RwRasterFormat;

/* RwRasterPrivateFlag: the raster's state (privateFlags). */
typedef enum RwRasterPrivateFlag {
    rwRASTERGAMMACORRECTED = 0x01,
    rwRASTERPIXELLOCKEDREAD = 0x02,
    rwRASTERPIXELLOCKEDWRITE = 0x04,
    rwRASTERPALETTELOCKEDREAD = 0x08,
    rwRASTERPALETTELOCKEDWRITE = 0x10,
    rwRASTERPIXELLOCKEDRAW = 0x20,
    rwRASTERPRIVATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwRasterPrivateFlag;

#define rwRASTERPIXELLOCKED (rwRASTERPIXELLOCKEDREAD | rwRASTERPIXELLOCKEDWRITE)
#define rwRASTERPALETTELOCKED (rwRASTERPALETTELOCKEDREAD | rwRASTERPALETTELOCKEDWRITE)
#define rwRASTERLOCKED (rwRASTERPIXELLOCKED | rwRASTERPALETTELOCKED)

/*
 * RwRaster: a block of pixels (a camera's frame or Z buffer, a texture's image) in the device's
 * own format, which the device driver creates, locks and draws. A sub raster shares its parent's
 * pixels from the offset (nOffsetX, nOffsetY); a raster that is not a sub raster is its own
 * parent. The driver's data follows (the plugins registered with rasterTKList).
 */
typedef struct RwRaster {
    struct RwRaster* parent;    /* 0x0 */
    RwUInt8* cpPixels;          /* 0x4: the locked pixels */
    RwUInt8* palette;           /* 0x8: the locked palette */
    RwInt32 width;              /* 0xC */
    RwInt32 height;             /* 0x10 */
    RwInt32 depth;              /* 0x14: bits per pixel */
    RwInt32 stride;             /* 0x18: bytes per line of the locked pixels */
    RwInt16 nOffsetX;           /* 0x1C */
    RwInt16 nOffsetY;           /* 0x1E */
    RwUInt8 cType;              /* 0x20: RwRasterType */
    RwUInt8 cFlags;             /* 0x21 */
    RwUInt8 privateFlags;       /* 0x22: RwRasterPrivateFlag */
    RwUInt8 cFormat;            /* 0x23: RwRasterFormat >> 8 */
    RwUInt8* originalPixels;    /* 0x24 */
    RwInt32 originalWidth;      /* 0x28 */
    RwInt32 originalHeight;     /* 0x2C */
    RwInt32 originalStride;     /* 0x30 */
} RwRaster;

#define RwRasterGetWidth(_raster) ((_raster)->width)
#define RwRasterGetHeight(_raster) ((_raster)->height)
#define RwRasterGetStride(_raster) ((_raster)->stride)
#define RwRasterGetDepth(_raster) ((_raster)->depth)
#define RwRasterGetFormat(_raster) ((((_raster)->cFormat) & (rwRASTERFORMATMASK >> 8)) << 8)
#define RwRasterGetType(_raster) (((_raster)->cType) & rwRASTERTYPEMASK)
#define RwRasterGetParent(_raster) ((_raster)->parent)

/* The plugins registered with rasters (the device driver's raster data among them). */
extern RwPluginRegistry rasterTKList;

void* _rwRasterOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwRasterClose(void* instance, RwInt32 offset, RwInt32 size);
RwRaster* RwRasterUnlock(RwRaster* raster);
RwRaster* RwRasterUnlockPalette(RwRaster* raster);
RwBool RwRasterDestroy(RwRaster* raster);
RwInt32 RwRasterRegisterPlugin(RwInt32 size, RwUInt32 pluginID,
                               RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwUInt8* RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode);
RwInt32 RwRasterGetNumLevels(RwRaster* raster);
RwRaster* RwRasterShowRaster(RwRaster* raster, void* dev, RwUInt32 flags);
RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags);
RwUInt8* RwRasterLock(RwRaster* raster, RwUInt8 level, RwInt32 lockMode);

#ifdef __cplusplus
}
#endif

#endif
