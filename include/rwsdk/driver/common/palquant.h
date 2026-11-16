// Reference: tgsm/pbg include/rwsdk/driver/common/palquant.h
#ifndef RWSDK_DRIVER_COMMON_PALQUANT_H
#define RWSDK_DRIVER_COMMON_PALQUANT_H

#include <rwsdk/baimage.h>
#include <rwsdk/plcore/bamemory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RwPalQuant: the palette quantiser's state (an octree of colour cubes over the images added to
 * it), kept on the caller's stack. Only the size (0x4008) and the last two members are known.
 */
typedef struct RwPalQuant {
    RwUInt8 field_0x0[0x4000];  /* 0x0: the quantiser's tables; layout not yet recovered */
    void* field_0x4000;         /* 0x4000: the octree's root node, from field_0x4004 */
    RwFreeList* field_0x4004;   /* 0x4004: the free list of octree nodes (0x5C bytes each) */
} RwPalQuant;

/* Defined in palquant.c, unit not yet reconstructed. */
RwBool _rwPalQuantInit(RwPalQuant* pq);
RwInt32 _rwPalQuantResolvePalette(RwRGBA* palette, RwInt32 maxcols, RwPalQuant* pq);
void _rwPalQuantMatchImage(RwUInt8* dstPixels, RwInt32 dstStride, RwInt32 dstDepth, RwInt32 dstPacked,
                           RwPalQuant* pq, RwImage* img);
void _rwPalQuantAddImage(RwPalQuant* pq, RwImage* img, RwReal weight);
void _rwPalQuantTerm(RwPalQuant* pq);

#ifdef __cplusplus
}
#endif

#endif
