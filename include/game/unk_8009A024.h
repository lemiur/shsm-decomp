#ifndef GAME_UNK_8009A024_H
#define GAME_UNK_8009A024_H

#include <revolution/types.h>

// Name invented: the screen rectangle fn_8009A024 reads. Only the two halfwords at 0x4 and 0x6 are read.
struct unk_ScreenSize {
    u16 field_0x0;  // 0x0: not read by this unit
    u16 field_0x2;  // 0x2: not read by this unit
    u16 width;      // 0x4
    u16 height;     // 0x6
};

extern "C" {
void fn_8009A024(const unk_ScreenSize* screen);
void fn_8009A4CC(f32 z, void* texture0, void* texture1, void* texture2, s32 x, s32 y, u16 width, u16 height, s32 dx, s16 dy);
}

#endif
