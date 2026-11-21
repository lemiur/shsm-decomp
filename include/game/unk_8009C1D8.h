#ifndef GAME_UNK_8009C1D8_H
#define GAME_UNK_8009C1D8_H

#include <revolution/types.h>
#include <revolution/os/OSMessage.h>

// Message queue shared with the read thread's unit (lbl_8046C0E0, unnamed).
extern OSMessageQueue lbl_8046C0E0;

extern "C" {
BOOL fn_8009C1D8(OSMessage msg);
}

#endif
