// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C220.h>

/*
====================
fn_8009C220
====================
*/
// 0x8009C220
extern "C" BOOL fn_8009C220(OSMessage msg) {
    return OSSendMessage( &lbl_8046C120, msg, OS_MESSAGE_BLOCK );
}
