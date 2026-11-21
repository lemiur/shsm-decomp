// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800ABDA0.h>
#include <game/unk_Registry.h>

/*
====================
fn_800ABDA0
====================
*/
// 0x800ABDA0
extern "C" void fn_800ABDA0( void* handle ) {
    unk_RegistryEntry* entry = unk_FindRegistryEntry( handle );

    entry->refCount--;
}
