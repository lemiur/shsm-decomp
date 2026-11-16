// Reference: tgsm/pbg include/rwsdk/baimras.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
/*
 * baimras.c: conversion between images and rasters, done by the device driver's standard
 * functions. The file name and the function names are tgsm/pbg's (its GameCube symbols); pbg has
 * no source for this file, so the bodies are reconstructed from retail. RwRasterRead and
 * RwRasterReadMaskedRaster have no copy in retail (dead-stripped).
 */
#include <rwsdk/badevice.h>
#include <rwsdk/baimras.h>

/* Fills the image from the raster's pixels, keeping its gamma correction. */
RwImage* RwImageSetFromRaster(RwImage* image, RwRaster* raster) {
    if (RWSRCGLOBAL(stdFunc[rwSTANDARDIMAGEGETRASTER])(image, raster, 0)) {
        if (raster->privateFlags & rwRASTERGAMMACORRECTED) {
            image->flags |= rwIMAGEGAMMACORRECTED;
        }
        return image;
    }
    return NULL;
} // 0x80296EBC

/* Fills the raster from the image's pixels, keeping its gamma correction. */
RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image) {
    if (RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERSETIMAGE])(raster, image, 0)) {
        if (image->flags & rwIMAGEGAMMACORRECTED) {
            raster->privateFlags |= rwRASTERGAMMACORRECTED;
        }
        return raster;
    }
    return NULL;
} // 0x80296F30

/*
 * Asks the driver for the raster of type nRasterType that best holds the image: its width,
 * height, depth and format (type and format flags, as RwRasterCreate takes them).
 */
RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType, RwInt32* npWidth,
                                 RwInt32* npHeight, RwInt32* npDepth, RwInt32* npFormat) {
    RwRaster raster;

    if (!RWSRCGLOBAL(stdFunc[rwSTANDARDIMAGEFINDRASTERFORMAT])(&raster, ipImage, nRasterType)) {
        return NULL;
    }

    *npFormat = (raster.cFormat << 8) | raster.cType;
    *npWidth = raster.width;
    *npHeight = raster.height;
    *npDepth = raster.depth;
    return ipImage;
} // 0x80296FA4
