// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8001A848.h>

/*
====================
fn_8001A848
====================
*/
// 0x8001A848
extern "C" float fn_8001A848( unk_SlotTable* table, u32 index ) {
    unk_Target* target;
    u32 valid;
    if ( index == 0xFFFFFFFF ) {
        valid = 0;
    } else if ( index >= table->count ) {
        valid = 0;
    } else {
        valid = table->slots[ index ] != NULL;
    }
    if ( valid ) {
        target = table->slots[ index ]->target;
    } else {
        target = NULL;
    }
    if ( target != NULL ) {
        return target->field_0xC;
    }
    return 0.0f;
}

/*
====================
fn_8001A8C0
====================
*/
// Wall: control flow. The retail body is a virtual call (vtable slot 21) on the second argument,
// the loop callback that the RtAnimInterpolator setters in fn_8001B7AC and fn_8001BA3C install;
// the class of that argument is not recovered yet, so this stays a stub.
// 0x8001A8C0
extern "C" void fn_8001A8C0() {
    // TODO
}
