#ifndef GAME_UNK_80099A7C_H
#define GAME_UNK_80099A7C_H

// unit name unconfirmed: header name invented.

#include <revolution/types.h>

// THP movie player entry points (revolution/thp, not yet declared in revolution/thp.h).
extern "C" {
void THPPlayerClose();
void THPPlayerQuit();
void fn_8009B37C();
}

// Work buffer of the movie player: allocated by fn_80099AB4, released by fn_80099A7C.
// Defined by its owning unit; name and type invented.
extern u8* lbl_80529768;

extern "C" {
void fn_80099A7C();
}

#endif
