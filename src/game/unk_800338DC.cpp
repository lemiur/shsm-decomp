// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800338DC.h>

#include <climax/Utils.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>

#include <math.h>

/*
====================
unk_800338DC::unk_800338DC
====================
*/
// 0x800338DC
unk_800338DC::unk_800338DC() {
    startEvent.fn_801A92FC( NULL );
    stopEvent.fn_801A92FC( NULL );
    soundName = NULL;
    startEvent.fn_801A92FC( NULL );
    stopEvent.fn_801A92FC( NULL );
    nearDistance = 0.0f;
    farDistance = 0.0f;
    sound = 0;
    fn_801A9DE8( lbl_8052A788 );
}

/*
====================
unk_800338DC::~unk_800338DC
====================
*/
// 0x800339A4
unk_800338DC::~unk_800338DC() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( startEvent );
    fn_801A9E00( stopEvent );
    fn_801A9940( startEvent );
    fn_801A9940( stopEvent );
    delete soundName;
}

/*
====================
unk_800338DC::fn_801A73A8
====================
*/
// 0x80033A6C
void unk_800338DC::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529290 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetSoundName:
            soundName = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetStartEvent:
            fn_801AA218( startEvent, it->GetString(), NULL );
            break;
        case CMD_SetStopEvent:
            fn_801AA218( stopEvent, it->GetString(), NULL );
            break;
        case CMD_SetNearDistance:
            nearDistance = it->GetFloat();
            break;
        case CMD_SetFarDistance:
            farDistance = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
unk_800338DC::fn_8009CB1C
====================
*/
// Wall (referent, then control flow): retail reads the zone's relay at 0x34 inline, where this
// unit calls GetZoneAudioRelay (CZone is not reconstructed); the PlaySound signature also differs
// in its RenderWare type name.
// 0x80033B70
void unk_800338DC::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == startEvent ) {
        sound = GetZoneAudioRelay( GetZone() )->PlaySound( soundName, fn_8009CD08(), (Audio::Channel::Type)-1, false, true );
        return;
    }
    if ( msg.record == stopEvent ) {
        if ( sound != 0 ) {
            fn_8003AFDC( GetZoneAudioRelay( GetZone() ) );
            sound = 0;
        }
        return;
    }
    if ( msg.record != lbl_8052A788 || sound == 0 ) {
        return;
    }
    unk_80491B38& record = GetPlayerRecord();
    CPlayer* player = record.field_0x14;
    if ( player == NULL ) {
        return;
    }
    const unk_RwV3d* self = fn_8009CD08();
    const unk_RwV3d* target = player->fn_8009CD08();
    float dx = self->x - target->x;
    float dy = self->y - target->y;
    float dz = self->z - target->z;
    float distance = sqrtf( dx * dx + dy * dy + dz * dz );
    float volume = ( distance - nearDistance ) / ( farDistance - nearDistance );
    if ( volume < 0.0f ) {
        volume = 0.0f;
    } else if ( volume > 1.0f ) {
        volume = 1.0f;
    }
    fn_8003B1B0( GetZoneAudioRelay( GetZone() ), sound, true, volume );
}
