// CGenericSpawner: class name from the registration strings lbl_803EBE78 and lbl_803EBE88.
// The inline factory fn_800C8548 and the one thunk follow the static initialiser in .text.

#include <game/CGenericSpawner.h>
#include <game/Log.h>

u32 lbl_80529AC0 = fn_801A7A84( "CGenericSpawner" );
unk_801A8F6C lbl_80529AC4( "CGenericSpawner", CGenericSpawner::fn_800C8548, sizeof( CGenericSpawner ) );

/*
====================
CGenericSpawner::CGenericSpawner

Warns when the packet carries a name command.
====================
*/
// 0x800C8108
CGenericSpawner::CGenericSpawner( const unk_AttributePacket& packet ) {
    name = NULL;
    spawnArg = 0;
    this->packet = &packet;
    if ( fn_800A26D8( packet ) ) {
        const char* spawnerName = "Unknown";
        unk_AttributeCommandIterator it( packet, lbl_80529AC0 );
        while ( !it.IsFinished() ) {
            switch ( it->id ) {
            case CMD_SetName:
                spawnerName = it->GetString();
                break;
            }
            ++it;
        }
        fn_8002438C( "CGenericSpawner::%s has data attached", spawnerName );
    }
}

/*
====================
CGenericSpawner::~CGenericSpawner
====================
*/
// 0x800C81E8
CGenericSpawner::~CGenericSpawner() {
    fn_800C7F54( packet );
}

/*
====================
CGenericSpawner::fn_800C8268

The asset is loaded: releases the request's reference and spawns if the spawner was enabled.
====================
*/
// 0x800C8268
void CGenericSpawner::fn_800C8268( u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, CGenericSpawner* spawner ) {
    void* resource = fn_801B1E3C( lbl_8052A5C0, spawner->name, spawner, 0 );
    fn_801A8554( spawner->name, resource );
    if ( resource != NULL ) {
        fn_801B2004( lbl_8052A5C0, resource );
    }
    if ( spawner->wasEnabled ) {
        spawner->fn_800C7C04();
    }
}

/*
====================
CGenericSpawner::fn_8000CFD0

Starts caching the asset and holds the spawn back until it is loaded.
====================
*/
// 0x800C82F0
void CGenericSpawner::fn_8000CFD0() {
    if ( name != NULL && *name != '\0' ) {
        wasEnabled = enabled;
        enabled = false;
        fn_80028D0C( 2, "CGenericSpawner[%s]: Caching %s", NULL, name );
        cacheHandle = fn_801B05C0( lbl_8052A5C0, name, this, 0, ( unk_LoadCallback )fn_800C8268, this );
    }
    CBaseSpawner::fn_8000CFD0();
}

/*
====================
CGenericSpawner::fn_8000CFCC

Gives the cached asset back.
====================
*/
// 0x800C8390
void CGenericSpawner::fn_8000CFCC() {
    if ( cacheHandle != NULL ) {
        fn_80028D0C( 2, "CGenericSpawner[%s]: UnCaching %s", NULL, name );
        fn_801B0B8C( lbl_8052A5C0, cacheHandle, this, 0 );
        cacheHandle = NULL;
    }
    if ( name != NULL && *name != '\0' ) {
        fn_801A89A0( name );
    }
}

/*
====================
CGenericSpawner::fn_801A73A8

Takes the asset name from the attribute packet.
====================
*/
// 0x800C8424
void CGenericSpawner::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseSpawner::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AC0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            if ( *name == '\0' ) {
                name = NULL;
            }
            break;
        }
        ++it;
    }
}

/*
====================
CGenericSpawner::fn_800C84C8

Queues the spawn request.
====================
*/
// 0x800C84C8
void CGenericSpawner::fn_800C84C8() {
    fn_800C7E84( mZone, name, packet, spawnArg, data, 0 );
}
