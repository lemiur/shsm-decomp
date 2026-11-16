#ifndef CLIMAX_IMAGE_H
#define CLIMAX_IMAGE_H

// unit name unconfirmed: header name invented. RenderWare's image, raster and texture types and
// functions are in rwsdk/baimage.h, rwsdk/baraster.h and rwsdk/batextur.h; this header holds the
// immediate-mode vertex type and the Wii driver's Im3D functions, whose units are not yet
// reconstructed.

#include <revolution/types.h>
#include <climax/Colour.h>
#include <climax/Frame.h>

/*
 * A vertex of RenderWare's 3D immediate mode (RwIm3DVertex of the Wii driver): the position, the
 * normal, the vertex colour and the texture coordinates. Name invented.
 */
struct unk_RwIm3DVertex {
    void        SetPos( unk_Vec3 pos ) {
                    objVertex.x = pos.x;
                    objVertex.y = pos.y;
                    objVertex.z = pos.z;
                }

    unk_RwV3d   objVertex;  // 0x00
    unk_RwV3d   objNormal;  // 0x0C
    unk_RGBA    color;      // 0x18
    float       u;          // 0x1C
    float       v;          // 0x20
};

/* An index into an immediate-mode vertex array (RwImVertexIndex). */
typedef u16 RwImVertexIndex;

/* RwIm3DTransform flags (rwIM3D_VERTEXUV, rwIM3D_VERTEXXYZ, rwIM3D_VERTEXRGBA in rwcore.h). */
#define IM3D_VERTEXUV           0x01
#define IM3D_VERTEXXYZ          0x08
#define IM3D_VERTEXRGBA         0x10

extern "C" {

// defined in 0x8029A898 (RenderWare), unit not yet reconstructed: RwIm3DTransform
void*           RwIm3DTransform( unk_RwIm3DVertex* vertices, u32 numVertices, unk_RwMatrix* ltm, u32 flags );
// defined in 0x8029A978 (RenderWare), unit not yet reconstructed: RwIm3DEnd
int             RwIm3DEnd();
// defined in 0x8029A9C8 (RenderWare), unit not yet reconstructed: RwIm3DRenderIndexedPrimitive
int             RwIm3DRenderIndexedPrimitive( int primType, RwImVertexIndex* indices, s32 numIndices );

}

#endif
