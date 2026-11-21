// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80016E84.h>
#include <game/unk_8000A040.h>

/*
====================
fn_80016E84

Sets the flag at 0xAC of the pad input sink lbl_805291E8. The reader of that flag is not
yet known.
====================
*/
// 0x80016E84
extern "C" void fn_80016E84() {
    lbl_805291E8->field_0xAC = true;
}
