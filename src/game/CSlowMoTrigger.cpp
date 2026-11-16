// CSlowMoTrigger: class name from the registration strings lbl_803ECFE0 and lbl_803ECFF0.
// The inline factory fn_800CF8B8 and the two thunks follow the static initialiser in .text.

#include <game/CSlowMoTrigger.h>
#include <game/Messages.h>
#include <game/Time.h>
#include <game/unk_80491B38.h>

u32 lbl_80529BD0 = fn_801A7A84( "CSlowMoTrigger" );
unk_801A8F6C lbl_80529BD4( "CSlowMoTrigger", CSlowMoTrigger::fn_800CF8B8, sizeof( CSlowMoTrigger ) );

/*
====================
CSlowMoTrigger::Start

Name invented.
====================
*/
inline void CSlowMoTrigger::Start() {
    fn_80202384( timeScale, blendInTime );
    if ( duration > 0.0f ) {
        timeLeft = duration;
        fn_801A9DE8( lbl_8052A788 );
    }
}

/*
====================
CSlowMoTrigger::Stop

Name invented.
====================
*/
inline void CSlowMoTrigger::Stop() {
    fn_80202384( 1.0f, blendOutTime );
    if ( oneShot ) {
        fn_801A9E00( startMsg );
        fn_801A9E00( stopMsg );
    }
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CSlowMoTrigger::~CSlowMoTrigger
====================
*/
// 0x800CF530
CSlowMoTrigger::~CSlowMoTrigger() {
    fn_801A9E00( startMsg );
    fn_801A9940( startMsg );
    fn_801A9E00( stopMsg );
    fn_801A9940( stopMsg );
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CSlowMoTrigger::fn_8009CB1C

Starts the slow motion on the start message and stops it on the stop message; each frame
while a duration runs, counts it down.
====================
*/
// 0x800CF5F0
void CSlowMoTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == startMsg.record ) {
        Start();
    } else if ( msg.record == stopMsg.record ) {
        Stop();
    } else if ( msg.record == lbl_8052A788.record ) {
        timeLeft -= lbl_80491B38.field_0x4;
        if ( timeLeft <= 0.0f ) {
            Stop();
        }
    }
}

/*
====================
CSlowMoTrigger::fn_801A73A8

Takes the names of the two messages and the slow-motion values from the attribute packet.
====================
*/
// 0x800CF734
void CSlowMoTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BD0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetStartMsg:
            fn_801AA218( startMsg, it->GetString(), 0 );
            break;
        case CMD_SetStopMsg:
            fn_801AA218( stopMsg, it->GetString(), 0 );
            break;
        case CMD_SetBlendInTime:
            blendInTime = it->GetFloat();
            break;
        case CMD_SetTimeScale:
            timeScale = it->GetFloat();
            break;
        case CMD_SetBlendOutTime:
            blendOutTime = it->GetFloat();
            break;
        case CMD_SetDuration:
            duration = it->GetFloat();
            break;
        case CMD_SetOneShot:
            oneShot = it->GetU32();
            break;
        }
        ++it;
    }
}
