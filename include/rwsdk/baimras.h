// Reference: tgsm/pbg include/rwsdk/baimras.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BAIMRAS_H
#define RWSDK_BAIMRAS_H

#include <rwsdk/baimage.h>
#include <rwsdk/baraster.h>

#ifdef __cplusplus
extern "C" {
#endif

RwImage* RwImageSetFromRaster(RwImage* image, RwRaster* raster);
RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image);
RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType, RwInt32* npWidth,
                                 RwInt32* npHeight, RwInt32* npDepth, RwInt32* npFormat);

#ifdef __cplusplus
}
#endif

#endif
