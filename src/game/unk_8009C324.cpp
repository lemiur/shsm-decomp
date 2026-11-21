// unit name unconfirmed

#include <game/unk_8009C324.h>

/*
====================
fn_8009C324

Cancels the video decode thread if it was started, then clears the started flag.
====================
*/
// 0x8009C324
extern "C" void fn_8009C324() {
    if (lbl_805297A0) {
        OSCancelThread(&lbl_8046D4D0.thread);
        lbl_805297A0 = FALSE;
    }
}
