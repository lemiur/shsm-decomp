// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C1D8.h>

/*
====================
fn_8009C1D8
====================
*/
// 0x8009C1D8
extern "C" BOOL fn_8009C1D8(OSMessage msg) {
    return OSSendMessage(&lbl_8046C0E0, msg, OS_MESSAGE_BLOCK);
}
