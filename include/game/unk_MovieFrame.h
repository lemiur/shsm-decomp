#ifndef GAME_UNK_MOVIEFRAME_H
#define GAME_UNK_MOVIEFRAME_H

#include <revolution/types.h>
#include <game/unk_80099CC8.h>

/*
 * The frame that lbl_8046BB40 points at (its field 0xF8, see unk_80099CC8.h). Three words are handed to
 * the draw routine fn_8009A4CC; field 0xC is added to field 0xC0 of the state before the
 * modulo. Member names invented.
 */
struct unk_MovieFrame {
    u32 field_0x0;
    u32 field_0x4;
    u32 field_0x8;
    u32 field_0xC;
};

/* The object the draw routine is given first. Its layout is not modelled here. Name invented. */
struct unk_MovieSurface;

extern "C" {
void fn_8009A024( unk_MovieSurface* surface );
void fn_8009A4CC( u32 a, u32 b, u32 c, float depth, s16 x, s16 y, s16 p7, s16 p8, s16 width, s16 height );
int  fn_8009B840( unk_MovieSurface* surface, int x, int y, int width, int height, float depth );
}

void RestoreGXState();

#endif
