// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800A8100.h>
#include <game/unk_ResourceManager.h>

/*
====================
unk_800A8100::unk_800A8100
====================
*/
// 0x800A8100
unk_800A8100::unk_800A8100( const char* name, unk_RpClump* clump, unk_RpWorld* world, void* attachment ) {
    this->clump = NULL;
    this->world = world;
    this->cacheHandle = NULL;
    this->stream = NULL;
    this->attachment = attachment;
    this->assets = NULL;
    this->name = fn_801B67FC( name, 0 );
    if ( name != NULL && name[0] != '\0' ) {
        this->cacheHandle = fn_801B05C0( lbl_8052A5C0, name, this, 0, NULL, NULL );
    }
    if ( clump != NULL ) {
        this->clump = RpClumpClone( clump );
        if ( RpClumpGetWorld( this->clump ) != NULL ) {
            unk_RpWorld* clumpWorld = RpClumpGetWorld( this->clump );
            RpWorldRemoveClump( clumpWorld, this->clump );
        }
        if ( this->world != NULL ) {
            RpWorldAddClump( this->world, this->clump );
        }
        if ( this->attachment != NULL ) {
            fn_8009EB2C( this->attachment, this->clump );
        }
    }
}
