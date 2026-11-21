// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Parked (NonMatching): the body matches, but this object's .sdata section is 8-aligned while retail
// places lbl_8052693C at a 4-aligned address between other units' small-data words, so the layout
// gate is red when the unit is linked.

#include <game/unk_800161DC.h>

// A pointer to the string literal "LoadSave.arc" (the literal is the compiler's own string).
const char* lbl_8052693C = "LoadSave.arc";

/*
====================
fn_800161DC
====================
*/
// Identity TODO: the branch tests the address of lbl_805291D8, which is never null; retail keeps
// the test, so it is kept. The parameters of fn_801B23A0 are guessed from this one call.
// 0x800161DC
extern "C" void fn_800161DC() {
    void** b = lbl_805291D8;
    if (lbl_805291D8) {
        b += 2;
    }
    fn_801B23A0(lbl_805291D8, lbl_8052693C, b, 0.5f);
}
