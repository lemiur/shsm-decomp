// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C6BC.h>

// Data owned by this unit: the message queue this function receives from. It is the member at
// lbl_8046D4D0+0x1338 of the video-decode state that CreateVideoDecodeThread initialises; the
// 0x18 bytes after the queue hold the message arrays of that state, so the unit cannot be
// flipped to Matching until the owning unit defines the state.
OSMessageQueue lbl_8046E808;

/*
====================
fn_8009C6BC
====================
*/
// 0x8009C6BC
extern "C" OSMessage fn_8009C6BC(s32 flags) {
    OSMessage msg;

    if (OSReceiveMessage(&lbl_8046E808, &msg, flags) == TRUE) {
        return msg;
    }
    return NULL;
}
