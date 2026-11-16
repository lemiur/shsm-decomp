// Unit name: Gormiti link map (cattributehandler.obj); boundary: first function GetInstanceId
// (0x801A74FC, exact pair) after the attributehandlerstrfuncs members; the end 0x801A7C08 is where
// cclassfactory's container destructor starts (judgement for 0x801A7A84-0x801A7C08: the
// registry thunk and the three CAttributeHandler slot defaults sit between FindRaster and that
// destructor, where Gormiti's object ends with the class-name functions).

#include <climax/AttributeHandler.h>
#include <climax/Debug.h>
#include <climax/EventHandler.h>
#include <game/CZoneChangeInfo.h>
#include <rwsdk/batextur.h>
#include <string.h>

/*
====================
unk_AttributePacket::GetInstanceId

The RWSGUID of the first kChunkInstanceId chunk. The loop has no end test: a packet always
holds one.
Gormiti: RWS::CAttributePacket::GetInstanceId, same body.
====================
*/
// 0x801A74FC
RWS::RWSGUID unk_AttributePacket::GetInstanceId() const {
    bool found = false;
    RWS::RWSGUID result;
    unk_AttributeChunkIterator chunkIt( first );
    while ( !found ) {
        if ( ( *chunkIt ).id == kChunkInstanceId ) {
            result = ( *chunkIt ).GetGUID();
            found = true;
        }
        ++chunkIt;
    }
    return result;
}

/*
====================
unk_AttributePacket::ReprocessAttributePacket

Replaces the class name of each class chunk by its registered id and links the class chunks
through their nextClass offsets (the last one links to the terminator).
Name from Gormiti (RWS::CAttributePacket::ReprocessAttributePacket, which has the same locals);
the body here is retail's.
====================
*/
// 0x801A7574
void unk_AttributePacket::ReprocessAttributePacket() {
    unk_AttributeCommand* prevClass = NULL;
    unk_AttributeCommand* chunk = &first;
    while ( chunk->next != 0 ) {
        char* data = chunk->data;
        if ( chunk->id == kChunkClassId ) {
            if ( prevClass != NULL ) {
                prevClass->nextClass = ( char* )chunk - ( char* )prevClass;
            }
            const char* refName = fn_801B63CC( data );
            chunk->classID = ( u32 )( refName != NULL ? refName : "UnRegisterd Class" );
            prevClass = chunk;
        }
        chunk = ( unk_AttributeCommand* )( ( char* )chunk + chunk->next );
    }
    if ( prevClass != NULL ) {
        prevClass->nextClass = ( char* )chunk - ( char* )prevClass;
    }
}

/*
====================
unk_AttributeCommandIterator::unk_AttributeCommandIterator

Starts at the first command addressed to classID.
Gormiti: RWS::CAttributeCommandIterator::CAttributeCommandIterator (it takes the class name).
====================
*/
// 0x801A7614
unk_AttributeCommandIterator::unk_AttributeCommandIterator( const unk_AttributePacket& packet, u32 classID ) {
    current = &packet.first;
    this->classID = classID;
    SkipToNextAttributeCommand( false );
}

/*
====================
unk_AttributeCommandIterator::SkipToNextAttributeCommand

Moves current to the next command of the target class. Chunks of the other types are stepped
over; a class chunk of another class is skipped whole through its nextClass link.
Gormiti: RWS::CAttributeCommandIterator::SkipToNextAttributeCommand (a few instructions shorter,
unpaired).
====================
*/
// 0x801A7650
void unk_AttributeCommandIterator::SkipToNextAttributeCommand( bool lastClassIdChunkWasForTargetClass ) {
    while ( current->next != 0 ) {
        const unk_AttributeCommand* chunk = current;
        switch ( chunk->id ) {
        case kChunkInstanceId:
        case kChunkCreateClassId:
            current = chunk->Next();
            break;
        case kChunkClassId:
            if ( classID == chunk->classID ) {
                lastClassIdChunkWasForTargetClass = true;
                current = chunk->Next();
            } else {
                lastClassIdChunkWasForTargetClass = false;
                current = ( const unk_AttributeCommand* )( ( const char* )current + current->nextClass );
            }
            break;
        default:
            if ( lastClassIdChunkWasForTargetClass ) {
                return;
            }
            current = chunk->Next();
            break;
        }
    }
}

/*
====================
unk_AttributePacket::Clone

A copy of the packet, up to and including its terminator chunk, in a block of its own.
Gormiti: RWS::CAttributePacket::Clone, same body.
====================
*/
// 0x801A76F4
unk_AttributePacket* unk_AttributePacket::Clone() const {
    unk_AttributeChunkIterator chunkIt( first );
    const void* pStart = &*chunkIt;
    while ( !chunkIt.IsFinished() ) {
        if ( ( *chunkIt ).id == kChunkClassId ) {
            chunkIt.ClassIdNextClassID();
        } else {
            ++chunkIt;
        }
    }
    const void* pEnd = &*chunkIt;
    u32 size = ( const char* )pEnd - ( const char* )pStart;
    unk_AttributePacket* pAttr = ( unk_AttributePacket* )RwMalloc( size + sizeof( u32 ), rwMEMHINTDUR_GLOBAL );
    memcpy( pAttr, pStart, size + sizeof( u32 ) );
    return pAttr;
}

/*
====================
unk_AttributePacket::CreateEntity

Creates the handler a creation packet describes and registers it with the asset tracker.
Gormiti: RWS::CAttributePacket::CreateEntity, same body.
====================
*/
// 0x801A778C
CAttributeHandler* unk_AttributePacket::CreateEntity( RwStream* stream ) const {
    CAttributeHandler* handler = NULL;
    RWS::RWSGUID instanceId = GetInstanceId();
    // Debug build: warning at this point (Gormiti Debug), the handler must not exist yet
    CAttributeHandler* existing = RWS::AssetTracker::Find( instanceId );
    RWS_WARN( existing != NULL, "InstanceID alraedy exists." );
    handler = fn_801A7EB4( first.GetString(), *this );
    if ( handler != NULL ) {
        fn_801AF254( stream, handler, this, instanceId );
    }
    return handler;
}

/*
====================
CAttributeHandler::~CAttributeHandler

Gormiti: RWS::CAttributeHandler::~CAttributeHandler, same body.
====================
*/
// 0x801A7870
CAttributeHandler::~CAttributeHandler() {
    RWS::AssetTracker::Remove( this );
    delete mFilter;
}

/*
====================
CAttributeHandler::RegisterForDelete

Gormiti: RWS::CAttributeHandler::RegisterForDelete, same body.
====================
*/
// 0x801A78D8
void CAttributeHandler::RegisterForDelete() {
    unk_Msg msg( OnRegisterDeleteEntity, this );
    if ( msg.record != lbl_8052A530.record && msg.record->head.next != NULL ) {
        fn_801AA2AC( msg );
    }
    RWS::AssetTracker::RegisterForDelete( this );
}

/*
====================
CAttributeHandler::FindTexture

Gormiti: RWS::CAttributeHandler::FindTexture, same body (its flag is a bool).
====================
*/
// 0x801A7934
RwTexture* CAttributeHandler::FindTexture( const char* name, int addRef ) {
    RwTexture* texture = NULL;
    if ( name != NULL ) {
        RwTexDictionary* dictionary = fn_80206718();
        if ( dictionary != NULL ) {
            texture = RwTexDictionaryFindNamedTexture( dictionary, name );
        }
        if ( texture == NULL && mAssetList != NULL ) {
            texture = fn_801AEFEC( mAssetList, name );
        }
    }
    if ( addRef && texture != NULL ) {
        RwTextureAddRef( texture );
    }
    return texture;
}

/*
====================
CAttributeHandler::FindRaster

Gormiti: RWS::CAttributeHandler::FindRaster, same body (its flag is a bool).
====================
*/
// 0x801A79E0
RwRaster* CAttributeHandler::FindRaster( const char* name, int addRef ) {
    RwRaster* raster = NULL;
    RwTexture* texture = FindTexture( name, addRef );
    if ( texture != NULL ) {
        raster = texture->raster;
    }
    return raster;
}

/*
====================
fn_801A7A84

The registered id of a class name; each class asks for its own in a static initialiser.
Gormiti: RWS::GetClassNameHash, which looks the name up in a hash table of this unit; retail
passes the name on to the registry.
====================
*/
// 0x801A7A84
u32 fn_801A7A84( const char* name ) {
    return fn_801B61E4( name );
}

/*
====================
CAttributeHandler::fn_801A7A88

Slot of the vtable: announces the handler's zone as the zone entered, through the zone-change
slot. Derived classes override it and call this first.
====================
*/
// 0x801A7A88
void CAttributeHandler::fn_801A7A88() {
    CZoneChangeInfo change;
    change.newZone = mZone;
    fn_801A7BFC( &change );
}

/*
====================
CAttributeHandler::fn_801A7B20

Slot of the vtable: announces the handler's zone as the zone left, with no zone entered.
====================
*/
// 0x801A7B20
void CAttributeHandler::fn_801A7B20() {
    CZoneChangeInfo change;
    change.newZone = NULL;
    change.oldZone = mZone;
    fn_801A7BFC( &change );
}

/*
====================
CAttributeHandler::fn_801A7BFC

Slot of the vtable: the handler's zone becomes the zone entered. Gormiti: SetZone( CZone* ),
whose argument is the zone itself.
====================
*/
// 0x801A7BFC
void CAttributeHandler::fn_801A7BFC( CZoneChangeInfo* change ) {
    mZone = change->newZone.GetZone();
}
