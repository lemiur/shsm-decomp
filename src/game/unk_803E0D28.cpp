// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_803E0D28.h>

/*
====================
unk_803E0D28::~unk_803E0D28

Open: register/schedule residue only (wall.exe): the field value is in r0 and the vtable
address in r4, where retail uses r4 and r5. Structure, branches and calls match.
====================
*/
// 0x800202D0
unk_803E0D28::~unk_803E0D28() {
    if ( field_0x60 ) {
        fn_8001C9B8();
        field_0x60 = NULL;
    }
}

/*
====================
fn_80020350

Virtual slot 0x50 of unk_8004C25C (vtable lbl_803E0BA0), not of unk_803E0D28. It sits between this
class's functions by address; its owner unit is not created yet, so it stays a stub here.
====================
*/
// 0x80020350
extern "C" void fn_80020350() {
    // TODO
}

/*
====================
unk_803E0D28::fn_80020358

Returns the float at 0x64.
====================
*/
// 0x80020358
float unk_803E0D28::fn_80020358() {
    return field_0x64;
}

/*
====================
unk_803E0D28::fn_80020360

Returns the byte at 0x69.
====================
*/
// 0x80020360
unsigned char unk_803E0D28::fn_80020360() {
    return field_0x69;
}

/*
====================
unk_803E0D28::fn_80020368

Returns the pointer at 0x60.
====================
*/
// 0x80020368
void* unk_803E0D28::fn_80020368() {
    return field_0x60;
}

/*
====================
unk_803E0D28::fn_80020370

Calls the slot 0x50 virtual with -1 when the value at 0x5C lies in [minValue, maxValue].
====================
*/
// 0x80020370
void unk_803E0D28::fn_80020370( u32 minValue, u32 maxValue ) {
    if ( field_0x5C >= minValue && field_0x5C <= maxValue ) {
        fn_8001B7AC( -1 );
    }
}
