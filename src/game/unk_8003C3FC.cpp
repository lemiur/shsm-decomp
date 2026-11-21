// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8003C3FC.h>

/*
====================
fn_8003C3FC

Callback: when the slot's source word at 0x38 is zero, sets the slot's word at 0x20.
====================
*/
// 0x8003C3FC
extern "C" void fn_8003C3FC( void* context, unk_8003C454* who ) {
    if ( who == NULL ) {
        return;
    }
    unk_ActorSlot* slot = who->field_0x220;
    if ( slot->source->field_0x38 == 0 ) {
        slot->field_0x20 = 1;
    }
}

/*
====================
fn_8003C424

Callback with no work.
====================
*/
// 0x8003C424
extern "C" void fn_8003C424( void* context, unk_8003C454* who ) {
}

/*
====================
fn_8003C428

Callback: when the slot's source word at 0x38 is not zero, sets the slot's word at 0x20.
====================
*/
// 0x8003C428
extern "C" void fn_8003C428( void* context, unk_8003C454* who ) {
    if ( who == NULL ) {
        return;
    }
    unk_ActorSlot* slot = who->field_0x220;
    if ( slot->source->field_0x38 != 0 ) {
        slot->field_0x20 = 1;
    }
}

/*
====================
fn_8003C450

Callback with no work.
====================
*/
// 0x8003C450
extern "C" void fn_8003C450( void* context, unk_8003C454* who ) {
}
