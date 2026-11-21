// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80006C7C.h>
#include <string.h>

/*
====================
unk_80006C7C::unk_80006C7C
====================
*/
// 0x80006C7C
unk_80006C7C::unk_80006C7C() {
    memset(field_0x78, 0, sizeof(field_0x78));
    field_0x00 = 0;
    field_0x04 = 0;
    field_0x08 = 0;
    field_0x0C = 0.0f;
    field_0x10 = 0.0f;
    field_0x14 = 0.0f;
    field_0x18 = 0.0f;
    field_0x1C = 0;
    field_0x24 = 0;
    field_0x28 = 0;
    field_0x2C = 0;
    field_0x30 = 0;
    field_0x34 = 0;
    field_0x38 = 0.0f;
    field_0x3C = 0.0f;
    field_0x40 = 0.0f;
    field_0x44 = 0.0f;
    field_0x48 = 0;
    field_0x50 = 0;
    field_0x54 = 0;
    field_0x68 = 0;
    field_0x6C = 0;
    field_0x70 = 0;
    field_0x60 = 0.0f;
    field_0x64 = 0.0f;
    field_0x58 = 0.0f;
    field_0x5C = 0.0f;
    field_0x74 = false;
    field_0x75 = false;
}

/*
====================
unk_80006C7C::~unk_80006C7C
====================
*/
// 0x80006D34
unk_80006C7C::~unk_80006C7C() {
}

/*
====================
fn_80006D74
====================
*/
// 0x80006D74
// Wall (structure): a 0x4FC-byte unrolled float-table walk (indexed lfsx/stfsx from r3 with
// several strides, int-to-float conversion through the 0x4330 magic); the table's element type
// and owner are not recovered, so no source body is written.
extern "C" void fn_80006D74() {
    // TODO
}

/*
====================
unk_80006C7C::fn_80007270
====================
*/
// 0x80007270
// Wall (structure): the body walks a ring of 0x50-byte samples from this+0x50 (x at +0x2C, y at
// +0x30, stride 0x50, 15 steps). That ring overlaps the header fields 0x50-0x78 and the 0x78
// table, and no sample type is recovered yet; without it the reads are offset arithmetic (R4).
// Body, for the record: the flags are the mean sample step against 0.1f; when both are set
// both are cleared.
void unk_80006C7C::fn_80007270( bool* a, bool* b ) {
    // TODO
}

/*
====================
unk_80006C7C::fn_80007384
====================
*/
// 0x80007384
// Wall (structure): 1151 instructions, 10 loops, six calls to fn_80006D74 and two to RwV3dLength.
// The body indexes a 0xB30-byte record of lbl_804579C0 by its argument and shifts a 256-entry
// ring of 0x50-byte samples inside this+0x78 (the field_0x78 table); neither the record type
// nor the sample type is recovered, so no source body is written.
void unk_80006C7C::fn_80007384( int ) {
    // TODO
}

/*
====================
unk_80006C7C::fn_80008580
====================
*/
// Test-and-clear of one state flag selected by index: returns 1.0f when the flag was set (and
// clears it), else 0.0f. Indices 12 and 13 return the floats at 0x60 and 0x64.
// 0x80008580
float unk_80006C7C::fn_80008580( int index ) {
    switch ( index ) {
    case 0:
        if ( field_0x70 & 0x2 ) {
            field_0x70 &= ~0x2;
            return 1.0f;
        }
        if ( field_0x70 & 0x20 ) {
            field_0x70 &= ~0x20;
            return 1.0f;
        }
        break;
    case 1:
        if ( field_0x70 & 0x8 ) {
            field_0x70 &= ~0x8;
            return 1.0f;
        }
        if ( field_0x70 & 0x80 ) {
            field_0x70 &= ~0x80;
            return 1.0f;
        }
        break;
    case 2:
        if ( field_0x70 & 0x1 ) {
            field_0x70 &= ~0x1;
            return 1.0f;
        }
        break;
    case 3:
        if ( field_0x70 & 0x4 ) {
            field_0x70 &= ~0x4;
            return 1.0f;
        }
        break;
    case 4:
        if ( field_0x28 == 1 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        if ( field_0x28 == 5 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        break;
    case 5:
        if ( field_0x28 == 3 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        if ( field_0x28 == 7 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        break;
    case 6:
        if ( field_0x28 == 0 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        break;
    case 7:
        if ( field_0x28 == 2 && field_0x74 ) {
            field_0x74 = false;
            return 1.0f;
        }
        break;
    case 8:
        if ( field_0x54 == 1 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        if ( field_0x54 == 5 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        break;
    case 9:
        if ( field_0x54 == 3 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        if ( field_0x54 == 7 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        break;
    case 10:
        if ( field_0x54 == 0 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        break;
    case 11:
        if ( field_0x54 == 2 && field_0x75 ) {
            field_0x75 = false;
            return 1.0f;
        }
        break;
    case 12:
        return field_0x60;
    case 13:
        return field_0x64;
    }
    return 0.0f;
}
