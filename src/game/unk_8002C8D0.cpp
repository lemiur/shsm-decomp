// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002C8D0.h>

/*
====================
unk_8002BC7C::fn_8002C8D0
====================
*/
// Register/schedule wall: this and arg sit in r30/r31 swapped, and the index is reloaded at
// different points than retail; control flow is isomorphic (wall.exe).
// 0x8002C8D0
void unk_8002BC7C::fn_8002C8D0( void* arg ) {
    if ( slots[index].flagA != 0 || slots[index].flagB != 0 || fn_80030ED4( lbl_80529260 ) != NULL ) {
        unk_80030D58* result = fn_80030D58( lbl_80529260,
                                             lbl_80491B38.field_0x14,
                                             index >= 0 ? slots[index].field_0x0 : NULL,
                                             index >= 0 ? slots[index].field_0x4 : NULL,
                                             field_0x8,
                                             1,
                                             slots[index].flagB,
                                             field_0x3C0 );
        if ( slots[index].flagA != 0 && result != NULL ) {
            result->field_0x96 = 1;
        }
    } else {
        fn_801831E4( this, arg );
    }
}
