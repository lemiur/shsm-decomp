// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF050.h>

#include <game/CGenericSpawner.h>
#include <game/unk_ResourceManager.h>

/*
====================
fn_800AF050

Releases the handle it is given through the resource manager, filed under the shared owner
object lbl_80529888 with flags 1. Name invented.
====================
*/
// 0x800AF050
extern "C" void fn_800AF050( void* handle ) {
    fn_801B0B8C( lbl_8052A5C0, handle, reinterpret_cast<unk_SpawnerResource*>( lbl_80529888 ), 1 );
}
