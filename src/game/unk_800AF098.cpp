// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF098.h>

/*
====================
fn_800AF098
====================
*/
// 0x800AF098
// Forwards its three arguments to fn_801B10AC with the resource manager lbl_8052A5C0 first.
// Argument names and types invented: the callers pass a pointer, a pointer offset from it and a size.
extern "C" int fn_800AF098( void* a, void* b, s32 c ) {
    return fn_801B10AC( lbl_8052A5C0, a, b, c );
}
