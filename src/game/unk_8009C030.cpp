// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C030.h>

THPReadThread lbl_8046C1B8;
s32           lbl_80529798;

/*
====================
fn_8009C030
====================
*/
// Cancels the read thread when it was created, then clears the flag.
// 0x8009C030
extern "C" void fn_8009C030() {
    if ( lbl_80529798 ) {
        OSCancelThread( &lbl_8046C1B8.thread );
        lbl_80529798 = FALSE;
    }
}
