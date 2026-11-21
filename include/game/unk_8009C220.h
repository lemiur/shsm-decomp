#ifndef GAME_UNK_8009C220_H
#define GAME_UNK_8009C220_H

#include <revolution/types.h>
#include <revolution/os/OSMessage.h>

extern "C" {
BOOL fn_8009C220(OSMessage msg);
}

// defined elsewhere, owner not yet reconstructed: the 0x98-byte object whose leading OSMessageQueue
// this unit sends to and fn_8009C1EC receives from (the first 0x20 bytes are the queue).
extern OSMessageQueue lbl_8046C120;

#endif
