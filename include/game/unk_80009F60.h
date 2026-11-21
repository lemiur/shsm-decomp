#ifndef GAME_UNK_80009F60_H
#define GAME_UNK_80009F60_H

#include <revolution/types.h>

extern "C" {
void fn_80009F60();

/* Sets or clears the bits of the flag word lbl_8052A83C. Defined elsewhere, unit not yet reconstructed. Name invented. */
void fn_80202FEC( u32 bits, bool on );

/* Returns the flag byte lbl_8052A440. Defined elsewhere, unit not yet reconstructed. Name invented. */
bool fn_8018E09C();
}

#endif
