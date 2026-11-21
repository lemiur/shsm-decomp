// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C6A8.h>

/* Data owned by this unit: the message queue lbl_8046E7E8 (zero-initialised). */
OSMessageQueue lbl_8046E7E8;

/*
====================
fn_8009C6A8
====================
*/
// 0x8009C6A8
extern "C" BOOL fn_8009C6A8(OSMessage msg) {
    return OSSendMessage(&lbl_8046E7E8, msg, OS_MESSAGE_NOBLOCK);
}
