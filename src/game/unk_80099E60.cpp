// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80099E60.h>

/*
====================
fn_80099E60

Posts msg to the queue lbl_8046BAC8 without blocking. The callers ignore the result.
====================
*/
// 0x80099E60
extern "C" void fn_80099E60(OSMessage msg) {
    OSSendMessage(&lbl_8046BAC8, msg, OS_MESSAGE_NOBLOCK);
}
