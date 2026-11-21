// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80009CF8.h>
#include <game/unk_8000A040.h>
#include <climax/Audio/Manager.h>
#include <revolution/wpad.h>
#include <string.h>

/*
====================
fn_80009CF8
====================
*/
// Name and role invented. The arguments are the WPADConnectCallback pair (chan, result): channel 0 is reset
// and enabled on WPAD_ERR_OK, reset and disabled on WPAD_ERR_NO_CONTROLLER; the other channels are disconnected.
// 0x80009CF8
extern "C" void fn_80009CF8(s32 chan, s32 result) {
    unk_PadChannel* pad;

    if (chan < 0 || chan >= WPAD_MAX_CONTROLLERS) {
        return;
    }
    if (chan == 0) {
        if (result == WPAD_ERR_OK) {
            pad = &lbl_804579C0[chan];
            memset(pad, 0, sizeof(unk_PadChannel));
            pad->field_0x0 = true;
            Audio::Manager::InitWiiController(chan);
        }
        if (result == WPAD_ERR_NO_CONTROLLER) {
            pad = &lbl_804579C0[chan];
            memset(pad, 0, sizeof(unk_PadChannel));
            pad->field_0x0 = false;
        }
    } else {
        WPADDisconnect(chan);
    }
}
