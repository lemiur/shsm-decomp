// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80009DD0.h>
#include <game/unk_8000A040.h>
#include <climax/Time.h>
#include <revolution/kpad.h>
#include <revolution/wpad/__wpad.h>
#include <string.h>

u32 lbl_80526720 = 0xFFFFF060;

/*
====================
fn_80009DD0

Reads pad channel 0 while the channel is read. KPADRead fills the newest samples of the queue, the older
ones move back to make room, and every 2000 ms the WPAD device info is requested asynchronously. Name invented.

Open: register/schedule wall. Retail keeps the constant 16 and the callback address in registers from entry
(five callee-saved registers, stmw r27); this body keeps three. Matches the bytes but for the register choice.
====================
*/
// 0x80009DD0
void fn_80009DD0() {
    unk_PadChannel& pad = lbl_804579C0[0];
    if ( !pad.field_0x0 ) {
        return;
    }

    KPADStatus samples[16];
    pad.sampleCount = KPADRead( 0, samples, 16 );
    if ( pad.sampleCount == 0 ) {
        return;
    }
    if ( pad.sampleCount < 16 ) {
        for ( int i = 15; i > pad.sampleCount; i-- ) {
            memcpy( &pad.samples[i], &pad.samples[i - pad.sampleCount], sizeof( KPADStatus ) );
        }
    }
    memcpy( pad.samples, samples, pad.sampleCount * sizeof( KPADStatus ) );
    pad.field_0xB08 += pad.sampleCount;
    if ( pad.field_0xB08 > 16 ) {
        pad.field_0xB08 = 16;
    }

    u32 now = RWS::Time::GetTime();
    if ( now - lbl_80526720 > 2000 ) {
        WPADGetInfoAsync( 0, &pad.info, fn_80009DB4 );
        lbl_80526720 = RWS::Time::GetTime();
    }
}
