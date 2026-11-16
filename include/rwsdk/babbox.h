// Reference: tgsm/pbg include/rwsdk/babbox.h
#ifndef RWSDK_BABBOX_H
#define RWSDK_BABBOX_H

#include <rwsdk/plcore/bavector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwBBox: an axis-aligned bounding box, its supremum (maximum) corner first. */
typedef struct RwBBox {
    RwV3d   sup;    /* 0x0 */
    RwV3d   inf;    /* 0xC */
} RwBBox;

RwBBox* RwBBoxCalculate(RwBBox* boundBox, const RwV3d* verts, RwInt32 numVerts);
RwBBox* RwBBoxInitialize(RwBBox* boundBox, const RwV3d* vertex);
RwBBox* RwBBoxAddPoint(RwBBox* boundBox, const RwV3d* vertex);

#ifdef __cplusplus
}
#endif

#endif
