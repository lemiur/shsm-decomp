// Reference: tgsm/pbg include/rwsdk/baimage.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BAIMAGE_H
#define RWSDK_BAIMAGE_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwImageFlag: an image's flags. */
typedef enum RwImageFlag {
    rwNAIMAGEFLAG = 0x00,
    rwIMAGEALLOCATED = 0x01,        /* the image owns its pixels */
    rwIMAGEGAMMACORRECTED = 0x02,
    rwIMAGEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwImageFlag;

/*
 * RwImage: a device-independent picture in memory, read from or written to a file and converted
 * to and from rasters: depth bits per pixel, palettised at 4 and 8 bits, else 32-bit RGBA.
 */
typedef struct RwImage {
    RwInt32 flags;              /* 0x0: RwImageFlag */
    RwInt32 width;              /* 0x4 */
    RwInt32 height;             /* 0x8 */
    RwInt32 depth;              /* 0xC: bits per pixel */
    RwInt32 stride;             /* 0x10: bytes per line */
    RwUInt8* cpPixels;          /* 0x14 */
    RwRGBA* palette;            /* 0x18 */
} RwImage;

#define RwImageGetWidth(_image) ((_image)->width)
#define RwImageGetHeight(_image) ((_image)->height)
#define RwImageGetDepth(_image) ((_image)->depth)
#define RwImageGetStride(_image) ((_image)->stride)
#define RwImageGetPixels(_image) ((_image)->cpPixels)
#define RwImageGetPalette(_image) ((_image)->palette)

typedef RwImage* (*RwImageCallBackRead)(const RwChar* imageName);
typedef RwImage* (*RwImageCallBackWrite)(RwImage* image, const RwChar* imageName);

struct RwModuleInfo;
extern RwPluginRegistry lbl_8044A7A8;
extern struct RwModuleInfo lbl_8052B178;

/* Defined in baimage.c. */
void* _rwImageOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwImageClose(void* instance, RwInt32 offset, RwInt32 size);
RwImage* RwImageCreate(RwInt32 width, RwInt32 height, RwInt32 depth);
RwBool RwImageDestroy(RwImage* image);
RwImage* RwImageAllocatePixels(RwImage* image);
RwImage* RwImageFreePixels(RwImage* image);
RwImage* RwImageMakeMask(RwImage* image);
RwImage* RwImageApplyMask(RwImage* image, const RwImage* mask);
const RwChar* RwImageSetPath(const RwChar* path);
RwChar* fn_80295B94(const RwChar* filename, RwInt32 extraBytes,
                    RwChar* (*callBack)(const RwChar*, void*), void* data);
RwChar* ImageAttempRead(const RwChar* pathname, void* data);
RwImage* RwImageRead(const RwChar* imageName);
RwChar* ImageDetermineExtender(const RwChar* pathname, void* data);
const RwChar* RwImageFindFileType(const RwChar* imageName);
RwBool RwImageRegisterImageFormat(const RwChar* extension,
                                   RwImageCallBackRead imageRead,
                                   RwImageCallBackWrite imageWrite);
RwImage* RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname);
RwRGBA* RwRGBASetFromPixel(RwRGBA* rgbOut, RwUInt32 pixelValue, RwInt32 rasterFormat);
RwBool fn_802966AC(RwImage* ipDestin, const RwImage* ipSource);
RwImage* RwImageCopy(RwImage* destImage, const RwImage* sourceImage);
RwImage* RwImageGammaCorrect(RwImage* image);
RwBool RwImageSetGamma(RwReal gammaValue);

/*
 * A path test (drive letter or leading backslash) called by the image path search. Its unit is
 * unidentified (it starts the unit after rwstring.c, docs/exceptions.md), so it has no owner header yet.
 */
RwBool fn_802A75A0(const RwChar* path);

#ifdef __cplusplus
}
#endif

#endif
