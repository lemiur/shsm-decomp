// unit name unconfirmed: the class is named after its vtable (invented); rename it when evidence
// appears and keep the open item in docs/exceptions.md.

#include <game/unk_803E0880.h>

// Defined here, read only by fn_800175A4 (the list the constructor leaves in the global).
unk_803E0880* lbl_805291F8;

/*
====================
fn_800175A4
====================
*/
// Not exact: the retail constructor computes the vtable address once and stores that register at
// both 0x0 and 0x10; MWCC computes it twice (the vptr store and the 0x10 field), so the body is
// 93.58%. Wall: referent (common subexpression of the vtable address).
// 0x800175A4
extern "C" void fn_800175A4() {
    lbl_805291F8 = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E0880( 32 );
}
