// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80016644.h>

/*
====================
fn_80016644
====================
*/
// 0x80016644
extern "C" void fn_80016644( void* self, void* msg, s32 value ) {
    switch ( lbl_805291D0 ) {
    case 0:
        fn_80016678( self, msg, value );
        return;
    case 1:
        return;
    case 2:
        fn_800168F8( self, msg, lbl_805291D4 );
        return;
    }
}
