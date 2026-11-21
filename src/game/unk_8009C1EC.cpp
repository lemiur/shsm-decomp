// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C1EC.h>

/*
====================
fn_8009C1EC
====================
*/
// 0x8009C1EC
extern "C" OSMessage fn_8009C1EC() {
    OSMessage msg;

    OSReceiveMessage(&lbl_8046C120, &msg, OS_MESSAGE_BLOCK);
    return msg;
}
