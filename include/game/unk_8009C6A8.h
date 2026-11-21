#ifndef GAME_UNK_8009C6A8_H
#define GAME_UNK_8009C6A8_H

#include <revolution/os/OSMessage.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The message queue sent to by fn_8009C6A8 (lbl_8046E7E8). */
extern OSMessageQueue lbl_8046E7E8;

/* Sends msg to lbl_8046E7E8 without blocking; the result is the OSSendMessage result. */
BOOL fn_8009C6A8(OSMessage msg);

#ifdef __cplusplus
}
#endif

#endif
