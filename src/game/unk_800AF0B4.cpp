// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF0B4.h>
#include <game/unk_ResourceManager.h>

/*
====================
fn_800AF0B4
====================
*/
// Reads the word at 0x18 of obj through the getter fn_801B0F5C on the resource manager and
// reports whether it is at least limit (unsigned). Name invented.
// 0x800AF0B4
extern "C" bool fn_800AF0B4( void* obj, u32 limit ) {
    return fn_801B0F5C( lbl_8052A5C0, obj ) >= limit;
}
