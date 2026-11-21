// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Parked NonMatching: the body is exact, but the compiled .sdata2 is 8-aligned and the retail
// constant lbl_8052B8C4 sits at 4 mod 8, so the link pads it (gate red at 0x8052B8C4).

#include <game/unk_8002C6AC.h>
#include <game/CPreCache.h>

/*
====================
fn_8002C6AC
====================
*/
// 0x8002C6AC
extern "C" void fn_8002C6AC( unk_8002C6AC* self ) {
    if ( self->slotHandle == NULL && self->index >= 0 && self->slots[self->index].active ) {
        self->slotHandle = fn_801B05C0( lbl_8052A5C0, self->CurrentName(),
                                        self, 0, NULL, NULL );
        fn_801B2380( lbl_8052A5C0, self->slotHandle, 1024.0f );
        if ( self->name != NULL ) {
            self->nameHandle = fn_801B05C0( lbl_8052A5C0, self->name, self, 0, NULL, NULL );
            fn_801B2380( lbl_8052A5C0, self->nameHandle, 1024.0f );
        }
    }
}
