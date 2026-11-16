// Reference: tgsm/pbg src/rwsdk/src/babbox.c
/*
 * babbox.c: RenderWare bounding boxes. Only RwBBoxCalculate has a copy in retail: RwBBoxInitialize
 * and RwBBoxAddPoint are inlined into it and their own copies are dead-stripped.
 */
#include <rwsdk/babbox.h>

/* Adds a point to the box, growing it to contain the point. */
RwBBox* RwBBoxAddPoint(RwBBox* boundBox, const RwV3d* vertex) {
    if (boundBox->inf.x > vertex->x) {
        boundBox->inf.x = vertex->x;
    }
    if (boundBox->inf.y > vertex->y) {
        boundBox->inf.y = vertex->y;
    }
    if (boundBox->inf.z > vertex->z) {
        boundBox->inf.z = vertex->z;
    }
    if (boundBox->sup.x < vertex->x) {
        boundBox->sup.x = vertex->x;
    }
    if (boundBox->sup.y < vertex->y) {
        boundBox->sup.y = vertex->y;
    }
    if (boundBox->sup.z < vertex->z) {
        boundBox->sup.z = vertex->z;
    }

    return boundBox;
} // no copy in retail (dead-stripped or inlined)

/* Sets the box to the single point vertex. */
RwBBox* RwBBoxInitialize(RwBBox* boundBox, const RwV3d* vertex) {
    boundBox->inf = *vertex;
    boundBox->sup = *vertex;
    return boundBox;
} // no copy in retail (dead-stripped or inlined)

/* Sets the box to the bounds of numVerts points. */
// 0x80290F00
RwBBox* RwBBoxCalculate(RwBBox* boundBox, const RwV3d* verts, RwInt32 numVerts) {
    RwInt32 i;

    RwBBoxInitialize(boundBox, verts++);
    for (i = numVerts - 1; i != 0; i--, verts++) {
        RwBBoxAddPoint(boundBox, verts);
    }

    return boundBox;
}
