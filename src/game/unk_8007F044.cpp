// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8007F044.h>
#include <climax/Image.h>
#include <climax/RenderState.h>

// Quads one call can draw: the index and vertex buffers are sized for them on the stack.
enum {
    UNK_QUAD_MAX = 32
};

/*
====================
fn_8007F044

Draws the visible quads of one batch in one indexed triangle list, textured with the batch's
raster. The caller passes two more words that the drawing does not read.

NonMatching, register/schedule wall (wall.exe): control flow is isomorphic; the two SetPos
argument copies take two stack slots where retail shares one, and the register numbering and
the colour copy schedule differ.
====================
*/
// 0x8007F044
extern "C" void fn_8007F044( unk_QuadBatch* batch, u32 unused1, u32 unused2 ) {
    RwRenderStateSet( rwRENDERSTATETEXTURERASTER, batch->raster );

    RwImVertexIndex indices[ UNK_QUAD_MAX * 6 ];
    unk_RwIm3DVertex vertices[ UNK_QUAD_MAX * 4 ];
    unk_RwIm3DVertex* vertex = vertices;
    RwImVertexIndex* index = indices;
    unk_Quad* quad = batch->quads;
    s32 drawn = 0;
    s32 firstVertex = 0;

    for ( s32 remaining = batch->count; remaining > 0; remaining--, quad++ ) {
        if ( quad->color.alpha != 0 ) {
            drawn++;
            unk_Vec3* corner = quad->corner;
            unk_RwIm3DVertex* v = vertex;
            for ( s32 pair = 0; pair < 2; pair++ ) {
                v[0].SetPos( corner[0] );
                v[0].color = quad->color;
                v[1].SetPos( corner[1] );
                v[1].color = quad->color;
                corner += 2;
                v += 2;
            }

            vertex[0].u = quad->u0;
            vertex[0].v = quad->v0;
            vertex[1].u = quad->u1;
            vertex[1].v = quad->v0;
            vertex[2].u = quad->u1;
            vertex[2].v = quad->v1;
            vertex[3].u = quad->u0;
            vertex[3].v = quad->v1;

            index[0] = firstVertex;
            index[1] = firstVertex + 1;
            index[2] = firstVertex + 2;
            index[3] = firstVertex + 2;
            index[4] = firstVertex + 3;
            index[5] = firstVertex;
            index += 6;
            vertex += 4;
            firstVertex += 4;
        }
    }

    if ( drawn > 0 ) {
        if ( RwIm3DTransform( vertices, drawn * 4, NULL, IM3D_VERTEXXYZ | IM3D_VERTEXRGBA | IM3D_VERTEXUV ) ) {
            RwIm3DRenderIndexedPrimitive( rwPRIMTYPETRILIST, indices, drawn * 6 );
            RwIm3DEnd();
        }
    }
}
