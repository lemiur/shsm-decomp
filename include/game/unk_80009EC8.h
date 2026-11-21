#ifndef GAME_UNK_80009EC8_H
#define GAME_UNK_80009EC8_H

#include <revolution/types.h>

extern "C" {
void fn_80009EC8();

// defined elsewhere, unit not yet reconstructed: the WPAD allocator and free callbacks
// registered by fn_80009EC8, and the KPAD connect callback it installs on every channel.
void* fn_80009CCC(u32 size);
BOOL fn_80009CD4(void* ptr);
void fn_80009CF8(s32 chan, s32 result);
}

#endif
