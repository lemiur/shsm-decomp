// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002D384.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata 0x80526A24 size 0x4 lbl_80526A24

int lbl_80526A24 = 8;

/*
====================
fn_8002D384

Replaces the cached asset of a request: loads the resource named by name, shares its textures
while the loader registers it, releases the handle the caller holds, and records the respot
entry in the state words. The changed entry's name is freed from the table.

Wall (NonMatching): the owner is the unk_SpawnerResource base of unk_RespotManager, which retail
reaches through a null-checked +8 base conversion; the base relation is not yet recovered, so the
owner is a reinterpret_cast here. The second argument is never read.
====================
*/
// 0x8002D384
extern "C" void fn_8002D384( void* handle, void* unusedArg, const char* name ) {
    void* resource = fn_801B1E3C( lbl_8052A5C0, name,
                                  reinterpret_cast<unk_SpawnerResource*>( lbl_80529260 ), 0 );
    if ( resource != NULL ) {
        ResourceLoader::ShareTextures( true );
        lbl_80529268 = fn_801A8554( name, resource );
        ResourceLoader::ShareTextures( false );
        fn_801B2004( lbl_8052A5C0, resource );
        fn_801B0B8C( lbl_8052A5C0, handle, reinterpret_cast<unk_SpawnerResource*>( lbl_80529260 ), 0 );

        unk_RespotManager* manager = lbl_80529260;
        int previous = lbl_80526A24;
        int current = manager->field_0x64;
        lbl_80526A20 = current;
        if ( previous != 8 && previous != current ) {
            fn_801A89A0( lbl_803E1A90[previous] );
        }
        lbl_80526A24 = lbl_80526A20;
        lbl_80526A1C = 8;
    }
}
