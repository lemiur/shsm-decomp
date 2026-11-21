// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80009EC8.h>
#include <game/unk_8000A040.h>
#include <revolution/kpad.h>
#include <revolution/wpad.h>
#include <string.h>

/*
====================
fn_80009EC8
====================
*/
// 0x80009EC8
extern "C" void fn_80009EC8() {
    WPADRegisterAllocator( fn_80009CCC, fn_80009CD4 );
    WPADInit();
    KPADInit();
    KPADEnableAimingMode( 0 );
    WPADSetAutoSleepTime( 5 );
    memset( lbl_804579C0, 0, sizeof( lbl_804579C0 ) );
    KPADSetConnectCallback( 0, fn_80009CF8 );
    KPADSetConnectCallback( 1, fn_80009CF8 );
    KPADSetConnectCallback( 2, fn_80009CF8 );
    KPADSetConnectCallback( 3, fn_80009CF8 );
}
