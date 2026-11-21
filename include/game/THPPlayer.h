#ifndef GAME_THPPLAYER_H
#define GAME_THPPLAYER_H

#include <revolution/types.h>
#include <revolution/os.h>
#include <revolution/ai.h>
#include <revolution/dvd.h>

// Shared with fn_8009A730 and THPPlayerPrepare, whose units are not split yet: the owning unit is open.
extern AIDCallback lbl_80529788;    // AI DMA callback saved for THPPlayerQuit to restore
extern u32         lbl_80529778;    // cleared by THPPlayerQuit

extern "C" {
void THPPlayerQuit();
s32  fn_8009A8A4(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset);

// Callee, unit not yet reconstructed: the second argument is always 0 at its call sites.
void fn_8018F788(s32 code, s32 unk_arg);
}

#endif
