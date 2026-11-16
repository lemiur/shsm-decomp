#ifndef CLIMAX_UNK_801F7E80_H
#define CLIMAX_UNK_801F7E80_H

// unit name unconfirmed: header named after the function's address (unit 0x801E3658, not yet
// reconstructed).

#include <climax/Camera.h>
#include <game/VectorPS.h>

/* A rectangle in the world: its centre and its size. Name invented. */
struct unk_WorldRect {
    unk_Vector3     centre;     // 0x0
    float           width;      // 0xC
    float           height;     // 0x10
};

/*
 * The screen rectangle fn_801F7E80 gives a world rectangle: position and size as fractions of
 * the screen. Name invented.
 */
struct unk_ScreenRect {
    float   x;          // 0x0, 0 left to 1 right
    float   y;          // 0x4, 0 top to 1 bottom
    float   width;      // 0x8
    float   height;     // 0xC
};

// defined in 0x801F7E80, unit not yet reconstructed: projects rect through camera (its frame and
// view matrix) into out; false when it is not on the screen
bool    fn_801F7E80( const unk_WorldRect* rect, unk_RwCamera* camera, unk_ScreenRect* out );

#endif
