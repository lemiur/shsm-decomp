// MusicTrigger: class name from the registration strings lbl_803EC900 and lbl_803EC910.
// The inline factory fn_800CC888 and the two thunks follow the static initialiser in .text.

#include <game/MusicTrigger.h>
#include <game/Log.h>

/*
====================
MusicTrigger::MusicTrigger

Inlined into the factory fn_800CC888; the out-of-line copy is dead-stripped by the linker (no address).
Compiled first, it creates the unit's first .sdata2 constant (1.0f), ahead of the message handler's 0.0f.
====================
*/
MusicTrigger::MusicTrigger() {
    track = NULL;
    field_0x3C = 1.0f;
}

/*
====================
MusicTrigger::~MusicTrigger
====================
*/
// 0x800CC4E4
MusicTrigger::~MusicTrigger() {
    delete track;
    fn_801A9E00( playMsg );
    fn_801A9940( playMsg );
}

/*
====================
MusicTrigger::fn_8000CFD0

Starts caching the track.
====================
*/
// 0x800CC584
void MusicTrigger::fn_8000CFD0() {
    if ( track != NULL && *track != '\0' ) {
        fn_80028D0C( 2, "MusicTrigger[%s]: Caching %s", NULL, track );
        cacheHandle = fn_801B05C0( lbl_8052A5C0, track, this, 0, NULL, NULL );
    }
}

/*
====================
MusicTrigger::fn_8000CFCC

Gives the cached track back.
====================
*/
// 0x800CC608
void MusicTrigger::fn_8000CFCC() {
    if ( cacheHandle != NULL ) {
        fn_80028D0C( 2, "MusicTrigger[%s]: UnCaching %s", NULL, track );
        fn_801B0B8C( lbl_8052A5C0, cacheHandle, this, 0 );
        cacheHandle = NULL;
    }
}

/*
====================
MusicTrigger::fn_801A73A8

Takes the track, the message name and the float from the attribute packet.
====================
*/
// 0x800CC680
void MusicTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B60 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTrack:
            track = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetPlayMsg:
            fn_801AA218( playMsg, it->GetString(), 0 );
            break;
        case CMD_SetField3C:
            field_0x3C = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
MusicTrigger::fn_8009CB1C

Plays the track when the message arrives, or stops the music when there is none.
====================
*/
// 0x800CC750
void MusicTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == playMsg.record ) {
        fn_80028D0C( 2, "MusicTrigger[%s]: Recieve %s", NULL, playMsg.record != NULL ? playMsg.record->name : NULL );
        fn_80028D0C( 1, "MusicTrigger[%s]: Play %s", NULL, track );
        if ( track != NULL && *track != '\0' ) {
            fn_80038DB8( lbl_805292F8, track, field_0x3C );
        } else {
            fn_80038FF4( lbl_805292F8, NULL, 0.0f, 1, 1, 1, 1 );
        }
    }
}

u32 lbl_80529B60 = fn_801A7A84( "MusicTrigger" );
unk_801A8F6C lbl_80529B64( "MusicTrigger", MusicTrigger::fn_800CC888, sizeof( MusicTrigger ) );
