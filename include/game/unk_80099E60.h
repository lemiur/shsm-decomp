#ifndef GAME_UNK_80099E60_H
#define GAME_UNK_80099E60_H

#include <revolution/os/OSMessage.h>

// Message queue in .bss; owner not yet reconstructed (the AudioDecode unit posts to it too). Role name invented.
extern OSMessageQueue lbl_8046BAC8;

extern "C" {
void fn_80099E60(OSMessage msg);
}

#endif
