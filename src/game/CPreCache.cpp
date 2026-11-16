// CPreCache: class name from the registration strings lbl_803E16B4 and lbl_803E16C0.
// The inline factory fn_80029004 and the two thunks follow the static initialiser in .text.

#include <game/CPreCache.h>
#include <game/Log.h>
#include <game/SaveData.h>

const char* lbl_80526A00 = "SaveData";

u32 lbl_80529248 = fn_801A7A84( "CPreCache" );
unk_801A8F6C lbl_8052924C( "CPreCache", CPreCache::fn_80029004, sizeof( CPreCache ) );

/*
====================
CPreCache::CPreCache
====================
*/
// 0x800288F4
CPreCache::CPreCache( const unk_AttributePacket& packet ) {
    name = NULL;
    field_0x38 = NULL;
    field_0x3C = 0;
    cacheOnLoad = true;
    cacheTxd = false;
    saveDataA = NULL;
    saveDataB = NULL;
    cacheHandle = NULL;
    txdHandle = NULL;
}

/*
====================
CPreCache::~CPreCache
====================
*/
// 0x8002897C
CPreCache::~CPreCache() {
    fn_801A9E00( cacheMsg );
    fn_801A9940( cacheMsg );
    fn_801A9E00( uncacheMsg );
    fn_801A9940( uncacheMsg );
    delete saveDataA;
    delete saveDataB;
}

/*
====================
CPreCache::fn_801A73A8

Takes the asset name, the messages, the save-data value and the modes from the attribute packet.
====================
*/
// 0x80028A40
void CPreCache::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529248 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetCacheOnLoad:
            if ( it->GetU32() != 0 ) {
                cacheOnLoad = true;
            } else {
                cacheOnLoad = false;
            }
            break;
        case CMD_SetCacheMsg:
            fn_801AA218( cacheMsg, it->GetString(), 0 );
            break;
        case CMD_SetUncacheMsg:
            fn_801AA218( uncacheMsg, it->GetString(), 0 );
            break;
        case CMD_SetField0x38:
            field_0x38 = it->GetString();
            break;
        case CMD_SetCacheTxd:
            if ( it->GetU32() != 0 ) {
                cacheTxd = true;
            } else {
                cacheTxd = false;
            }
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
    fn_80028BB0();
}

/*
====================
CPreCache::fn_8009CB1C

Caches on the cache message and drops the requests on the uncache message.
====================
*/
// 0x80028BB4
void CPreCache::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == cacheMsg.record ) {
        fn_80028D0C( 2, "PreCache[%s] CACHE Event: Recieve %s ", NULL,
                     cacheMsg.record != NULL ? cacheMsg.record->name : NULL );
        fn_80028E38();
    } else if ( msg.record == uncacheMsg.record ) {
        fn_80028D0C( 2, "PreCache[%s] UNCACHE Event: Recieve %s", NULL,
                     uncacheMsg.record != NULL ? uncacheMsg.record->name : NULL );
        UnCache();
    }
}

/*
====================
CPreCache::fn_8000CFD0

Caches at load when asked to.
====================
*/
// 0x80028D5C
void CPreCache::fn_8000CFD0() {
    if ( cacheOnLoad ) {
        fn_80028E38();
    }
}

/*
====================
CPreCache::fn_8000CFCC
====================
*/
// 0x80028D70
void CPreCache::fn_8000CFCC() {
    UnCache();
}

/*
====================
CPreCache::fn_80028E38

Requests the asset, and with cacheTxd its texture dictionary, unless it is requested already or
the save-data value is not set.
====================
*/
// 0x80028E38
void CPreCache::fn_80028E38() {
    if ( name != NULL && cacheHandle == NULL && fn_80206348( saveDataA, saveDataB, lbl_80526A00 ) ) {
        fn_80028D0C( 2, "PreCache[%s]: Caching %s", NULL, name );
        cacheHandle = fn_801B05C0( lbl_8052A5C0, name, this, field_0x3C, NULL, NULL );
        if ( cacheHandle != NULL ) {
            fn_801B2380( lbl_8052A5C0, cacheHandle, 0.5f );
            if ( field_0x38 != NULL && *field_0x38 != '\0' ) {
                fn_801B0588( lbl_8052A5C0, cacheHandle, field_0x38 );
            }
        }
        if ( cacheTxd ) {
            const char* txd = lbl_80529260->fn_80030314( lbl_80529260->fn_80030254(), name );
            if ( txd != NULL ) {
                fn_80028D0C( 2, "PreCache[%s]: Caching TXD %s", NULL, txd );
                txdHandle = fn_801B05C0( lbl_8052A5C0, txd, this, 0, NULL, NULL );
                fn_801B2380( lbl_8052A5C0, txdHandle, 0.4f );
            }
        }
    }
}

/*
====================
CPreCache::UnCache

Drops the requests for the asset and the texture dictionary. Name invented.
====================
*/
void CPreCache::UnCache() {
    if ( cacheHandle != NULL ) {
        fn_80028D0C( 2, "PreCache[%s]: UnCaching %s", NULL, name );
        fn_801B0B8C( lbl_8052A5C0, cacheHandle, this, 0 );
        cacheHandle = NULL;
    }
    if ( txdHandle != NULL ) {
        fn_80028D0C( 2, "PreCache[%s]: UnCaching %s", NULL, name );
        fn_801B0B8C( lbl_8052A5C0, txdHandle, this, 0 );
        txdHandle = NULL;
    }
}
