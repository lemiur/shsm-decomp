#ifndef GAME_UNK_8009C1EC_H
#define GAME_UNK_8009C1EC_H

#include <revolution/os/OSMessage.h>

// The message queue object (symbols.txt lbl_8046C120). Its first member is the OSMessageQueue;
// symbols.txt sizes the object at 0x98 bytes, and the layout past the queue is not yet known.
// Name invented.
extern OSMessageQueue lbl_8046C120;

extern "C" {
// Receives the next message from lbl_8046C120, blocking until one arrives. Name invented.
OSMessage fn_8009C1EC();
}

#endif
