// Unit name: class registry string "CAudioReverb" (fn_8003BB84); boundary: unit_plan

#include <game/CAudioReverb.h>

u32 lbl_80529310 = fn_801A7A84( "CAudioReverb" );
unk_801A8F6C lbl_80529314( "CAudioReverb", CAudioReverb::fn_8003BBE4, sizeof( CAudioReverb ) );

/*
====================
CAudioReverb::~CAudioReverb
====================
*/
// 0x8003B898
CAudioReverb::~CAudioReverb() {
    fn_801A9E00( enterMsg );
    fn_801A9E00( exitMsg );
    fn_801A9940( enterMsg );
    fn_801A9940( exitMsg );
}

/*
====================
CAudioReverb::fn_801A73A8

Reads the enter and exit messages, the preset name and the enabled flag from the packet.
====================
*/
// 0x8003B94C
void CAudioReverb::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529310 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetReverbName:
            reverbName = it->GetString();
            break;
        case CMD_SetEnterMsg:
            fn_801AA218( enterMsg, it->GetString(), NULL );
            break;
        case CMD_SetExitMsg:
            fn_801AA218( exitMsg, it->GetString(), NULL );
            break;
        case CMD_SetEnabled:
            enabled = it->GetU32() != 0;
            break;
        }
        ++it;
    }
}

/*
====================
CAudioReverb::fn_8000CFD0

Switches the reverb at load when enabled.
====================
*/
// 0x8003BA40
void CAudioReverb::fn_8000CFD0() {
    if ( enabled == true ) {
        fn_8003BAE8( false );
    }
}

/*
====================
CAudioReverb::fn_8009CB1C

The enter message switches the reverb, the exit message restores it.
====================
*/
// 0x8003BA64
void CAudioReverb::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == enterMsg.record ) {
        fn_8003BAE8( false );
    } else if ( msg.record == exitMsg.record ) {
        fn_8003BB5C();
    }
}

/*
====================
CAudioReverb::fn_8003BAE8
====================
*/
// 0x8003BAE8
void CAudioReverb::fn_8003BAE8( bool apply ) {
    Audio::System::GetInstance()->m_fmodSystem->getReverbProperties( &reverbProps );
    savedReverb = Audio::System::GetInstance()->m_currReverb;
    if ( !apply ) {
        fn_80037018( lbl_805292F8, reverbName );
    } else {
        Audio::System::GetInstance()->SetReverb( reverbName );
    }
}

/*
====================
CAudioReverb::fn_8003BB5C
====================
*/
// 0x8003BB5C
void CAudioReverb::fn_8003BB5C() {
    if ( savedReverb != NULL ) {
        Audio::System::GetInstance()->SetReverbPreset( savedReverb, true );
    } else {
        Audio::System::GetInstance()->m_fmodSystem->setReverbProperties( &reverbProps );
    }
}
