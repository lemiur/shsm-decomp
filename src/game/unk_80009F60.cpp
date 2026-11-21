// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80009F60.h>
#include <game/PadRumble.h>
#include <game/unk_8000A040.h>
#include <game/unk_MessageInbox.h>
#include <revolution/wpad.h>

/*
====================
fn_80009F60
====================
Sets the pointer flags from pad channel 0: 0x10 while the channel is not read, and 0x20 while its
device is not a Freestyle pointer (the owner's 0x1FEC field follows 0x20). Name invented.
*/
// 0x80009F60
extern "C" void fn_80009F60() {
    unk_PadChannel& pad = lbl_804579C0[0];

    if ( !fn_80006A20() ) {
        return;
    }
    if ( fn_8018E09C() ) {
        return;
    }

    fn_80202FEC( 0x10, !pad.field_0x0 );
    if ( !pad.field_0x0 ) {
        return;
    }
    // The second read of field_0x0 is redundant. Retail keeps it: the compare of the first test
    // stays in cr1 and the branch reuses it after the sample count test.
    if ( pad.sampleCount > 0 && pad.field_0x0 ) {
        if ( pad.samples[0].wpad_err != 0 ) {
            return;
        }
        if ( pad.samples[0].dev_type == WPAD_DEV_INITIALIZING ) {
            return;
        }
        if ( pad.samples[0].dev_type != WPAD_DEV_FS ) {
            fn_80202FEC( 0x20, true );
            lbl_80529D20->field_0x1FEC = 1;
        } else {
            fn_80202FEC( 0x20, false );
            lbl_80529D20->field_0x1FEC = 0;
        }
    }
}
