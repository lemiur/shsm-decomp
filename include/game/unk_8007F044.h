#ifndef GAME_UNK_8007F044_H
#define GAME_UNK_8007F044_H

#include <revolution/types.h>
#include <climax/Colour.h>
#include <game/Vec3.h>

struct RwRaster;

/*
 * One textured, coloured quad of a batch. Name invented. The four corners are in winding order
 * and take the texture rectangle (u0,v0)-(u1,v1). The bytes from 0x44 are not read by the drawing.
 */
struct unk_Quad {
    unk_Vec3    corner[ 4 ];    // 0x00
    float       u0;             // 0x30
    float       v0;             // 0x34
    float       u1;             // 0x38
    float       v1;             // 0x3C
    unk_RGBA    color;          // 0x40; a zero alpha hides the quad
    u8          field_0x44[ 0xC ];
};

/*
 * A node of the list of quad batches. Name invented. The list head is lbl_80529608.
 */
struct unk_QuadBatch {
    unk_QuadBatch*  next;       // 0x00
    s32             count;      // 0x04
    unk_Quad*       quads;      // 0x08
    RwRaster*       raster;     // 0x0C
};

extern "C" {
void fn_8007F044( unk_QuadBatch* batch, u32 unused1, u32 unused2 );
}

#endif
