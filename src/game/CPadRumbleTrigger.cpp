// CPadRumbleTrigger: class name from the registration strings lbl_803ECA28 and lbl_803ECA3C.
// The inline factory fn_800CD2CC and the two thunks follow the static initialiser in .text.

#include <game/CPadRumbleTrigger.h>
#include <game/Messages.h>
#include <game/PadRumble.h>
#include <game/unk_80491B38.h>

u32 lbl_80529B78 = fn_801A7A84( "CPadRumbleTrigger" );
unk_801A8F6C lbl_80529B7C( "CPadRumbleTrigger", CPadRumbleTrigger::fn_800CD2CC, sizeof( CPadRumbleTrigger ) );

/*
====================
CPadRumbleTrigger::~CPadRumbleTrigger
====================
*/
// 0x800CCF10
CPadRumbleTrigger::~CPadRumbleTrigger() {
    fn_801A9E00( rumbleMsg );
    fn_801A9940( rumbleMsg );
    if ( active ) {
        fn_801A9E00( lbl_8052A788 );
    }
}

/*
====================
CPadRumbleTrigger::fn_8009CB1C

Starts the rumble when the message arrives and counts the frame time down until it stops.
====================
*/
// 0x800CCFBC
void CPadRumbleTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == rumbleMsg.record ) {
        if ( isSmall ) {
            PostMsg( MsgStartSmallRumble, &startPad );
        } else {
            PostMsg( MsgStartBigRumble, &startPad );
        }
        timeLeft = startLifetime;
        if ( !active ) {
            fn_801A9DE8( lbl_8052A788 );
            active = true;
        }
    } else if ( msg.record == lbl_8052A788.record ) {
        timeLeft -= lbl_80491B38.field_0x8;
        if ( timeLeft <= 0.0f ) {
            if ( isSmall ) {
                PostMsg( MsgStopSmallRumble, &stopPad );
            } else {
                PostMsg( MsgStopBigRumble, &stopPad );
            }
            fn_801A9E00( lbl_8052A788 );
            active = false;
        }
    }
}

/*
====================
CPadRumbleTrigger::fn_801A73A8

Takes the name of the message, the kind of rumble and the rumble values from the attribute packet.
====================
*/
// 0x800CD168
void CPadRumbleTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B78 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetRumbleMsg:
            fn_801AA218( rumbleMsg, it->GetString(), 0 );
            break;
        case CMD_SetSmall:
            isSmall = it->GetU32();
            break;
        case CMD_SetStartLevel:
            startLevel = it->GetFloat();
            break;
        case CMD_SetStartLifetime:
            startLifetime = it->GetFloat();
            break;
        case CMD_SetStartRamp:
            startRamp = it->GetFloat();
            break;
        case CMD_SetStopLevel:
            stopLevel = it->GetFloat();
            break;
        }
        ++it;
    }
}
