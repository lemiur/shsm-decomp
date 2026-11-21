#ifndef GAME_UNK_800999A8_H
#define GAME_UNK_800999A8_H

// unit name unconfirmed: header name invented; it declares the movie-frame setup and its globals.

#include <revolution/types.h>

/* The 0x40-byte object that fn_800999A8 passes to fn_8009B840 as the frame's target. It is shared
 * with fn_8018EF3C; the unit that owns it is not yet reconstructed. */
extern u8 lbl_80478E78[0x40];

/* Result of the last fn_8009B840 call; defined in game/unk_800999A8.cpp. */
extern s32 lbl_8052976C;

extern "C" {
/* Owned by the unit that defines it (not yet reconstructed). Returns -1 on failure. */
s32 fn_8009B840(void* target, s32 x, s32 y, s32 width, s32 height, f32 z);

s32 fn_800999A8(void);
void fn_80099A78(void);
}

#endif
