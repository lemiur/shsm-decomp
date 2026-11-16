// Reference: tgsm/pbg config/GPLE9G/symbols.txt (baresamp: RwImageResample, RwImageCreateResample)
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BARESAMP_H
#define RWSDK_BARESAMP_H

#include <rwsdk/baimage.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Defined in baresamp.c. */
RwImage* RwImageResample(RwImage* dstImage, const RwImage* srcImage);
RwImage* RwImageCreateResample(const RwImage* srcImage, RwInt32 width, RwInt32 height);

#ifdef __cplusplus
}
#endif

#endif
