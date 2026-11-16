// CFadeTrigger: class name from the registration strings lbl_803EC220 and lbl_803EC230.
// The inline factory fn_800C9C00 and the two thunks follow the static initialiser in .text.

#include <game/CFadeTrigger.h>
#include <climax/Fade.h>
#include <game/Messages.h>

/*
====================
CFadeTrigger::~CFadeTrigger
====================
*/
// 0x800C984C
CFadeTrigger::~CFadeTrigger() {
    fn_801A9E00( startMsg );
    fn_801A9940( startMsg );
    fn_801A9940( doneMsg );
    fn_801A9E00( lbl_8052A7A0 );
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CFadeTrigger::fn_8009CB1C

Starts the fade when the start message arrives; on the frame messages, sends the done message
once the fade has finished. The base class does not see the messages.
====================
*/
// 0x800C990C
void CFadeTrigger::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == startMsg.record ) {
        fn_801A9E00( startMsg );
        fn_801A9DE8( lbl_8052A788 );
        fn_801A9DE8( lbl_8052A7A0 );
        if ( direction == 0 ) {
            fn_801E31C4( lbl_8052A678, -1.0f / duration, &colour, fadeFlag );
        } else {
            fn_801E31C4( lbl_8052A678, 1.0f / duration, &colour, fadeFlag );
        }
    } else if ( msg.record == lbl_8052A7A0.record || msg.record == lbl_8052A788.record ) {
        if ( !fn_801E2DA0( 1 ) ) {
            fn_801A9E00( lbl_8052A7A0 );
            fn_801A9E00( lbl_8052A788 );
            PostMsg( doneMsg, NULL );
        }
    }
}

/*
====================
CFadeTrigger::fn_801A73A8

Takes the messages and the fade values from the attribute packet.
====================
*/
// 0x800C9A48
void CFadeTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B00 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetDirection:
            direction = it->GetU32();
            break;
        case CMD_SetDuration:
            duration = it->GetFloat();
            break;
        case CMD_SetStartMsg:
            fn_801AA218( startMsg, it->GetString(), 0 );
            break;
        case CMD_SetDoneMsg:
            fn_801AA1C4( doneMsg, it->GetString(), 0 );
            break;
        case CMD_SetField4C:
            field_0x4C = it->GetU32() != 0;
            break;
        case CMD_SetFadeFlag:
            fadeFlag = it->GetU32() != 0;
            break;
        case CMD_SetColour: {
            unk_RGBA value = it->GetRGBA();
            colour.red = value.red;
            colour.green = value.green;
            colour.blue = value.blue;
            colour.alpha = 255;
            break;
        }
        }
        ++it;
    }
}

u32 lbl_80529B00 = fn_801A7A84( "CFadeTrigger" );
unk_801A8F6C lbl_80529B04( "CFadeTrigger", CFadeTrigger::fn_800C9C00, sizeof( CFadeTrigger ) );
