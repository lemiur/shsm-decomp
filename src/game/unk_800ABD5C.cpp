// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800ABD5C.h>

/*
====================
fn_800ABD5C
====================
*/
// 0x800ABD5C
extern "C" void fn_800ABD5C( void* handle ) {
    // Original: no check for a missing entry; with no match this writes at address 0x104.
    unk_FindRegistryEntry( handle )->refCount++;
}
