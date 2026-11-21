// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8004B2F0.h>

/*
====================
fn_8001A8E8
====================
*/
// Open: destructor, 557 instructions with 28 calls and 19 loops; the class layout past
// +0x8 is not recovered yet (wall: call set and control flow).
// 0x8001A8E8
extern "C" void fn_8001A8E8() {
    // TODO
}

/*
====================
unk_8004B2F0::fn_8001B19C
====================
*/
// 0x8001B19C
void unk_8004B2F0::fn_8001B19C() {
    if (field_0x8 != 0) {
        field_0x8->unk_Slot5();
    }
}

/*
====================
unk_8004B2F0::fn_8001B1BC
====================
*/
// 0x8001B1BC
void unk_8004B2F0::fn_8001B1BC() {
    if (field_0x8 != 0) {
        field_0x8->unk_Slot6();
    }
}

/*
====================
unk_8004B2F0::fn_8001B1DC
====================
*/
// 0x8001B1DC
void* unk_8004B2F0::fn_8001B1DC() {
    if (field_0x8 != 0) {
        return field_0x8->unk_Slot3();
    }
    return 0;
}
