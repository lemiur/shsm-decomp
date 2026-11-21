#ifndef GAME_UNK_80009DD0_H
#define GAME_UNK_80009DD0_H

#include <revolution/types.h>

extern "C" {
void fn_80009DD0();
void fn_80009DB4( s32 chan, s32 result );   // WPAD info callback, defined elsewhere (unit not yet reconstructed)
}

/* The time of the last WPAD info request (milliseconds). Name invented. */
extern u32 lbl_80526720;

#endif
