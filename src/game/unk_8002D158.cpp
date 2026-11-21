// unit name unconfirmed: no class-name or string evidence; the file name is the first function's address
// Parked (NonMatching): both functions match, but the flip gate is red. Retail pools this float at 0x8052B8CC
// (4-byte aligned, after the constants 0x8052B8C8, 0x8052B8D0 and 0x8052B8D4 of other functions of the same
// translation unit), and a unit of its own gets an 8-byte aligned pool, so the boundary must be re-planned.

#include <game/unk_8002D158.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata2 0x8052B8CC size 0x4 lbl_8052B8CC

/*
====================
fn_8002D158
====================
*/
// 0x8002D158
extern "C" float fn_8002D158() {
    return 0.1f;
}

/*
====================
fn_8002D160
====================
*/
// 0x8002D160
extern "C" bool fn_8002D160( unk_8002BC7C* self ) {
    bool result = false;
    if ( self->field_0x394 >= 0 && fn_80183114( self ) ) {
        result = true;
    }
    return result;
}
