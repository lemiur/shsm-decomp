// CVoicemailMessageTrigger: class name from the registration strings lbl_803ED1E8 and lbl_803ED204.
// The inline factory fn_800D0A1C and the two thunks follow the static initialiser in .text.

#include <game/CVoicemailMessageTrigger.h>
#include <game/DisplayHeight.h>

u32 lbl_80529C00 = fn_801A7A84( "CVoicemailMessageTrigger" );
unk_801A8F6C lbl_80529C04( "CVoicemailMessageTrigger", CVoicemailMessageTrigger::fn_800D0A1C, sizeof( CVoicemailMessageTrigger ) );

/*
====================
CVoicemailMessageTrigger::~CVoicemailMessageTrigger
====================
*/
// 0x800D06F8
CVoicemailMessageTrigger::~CVoicemailMessageTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    if ( field_0x3C ) {
        delete field_0x3C;
        field_0x3C = NULL;
    }
    if ( field_0x40 ) {
        delete field_0x40;
        field_0x40 = NULL;
    }
    if ( field_0x44 ) {
        delete field_0x44;
        field_0x44 = NULL;
    }
    if ( field_0x48 ) {
        delete field_0x48;
        field_0x48 = NULL;
    }
}

/*
====================
CVoicemailMessageTrigger::fn_8009CB1C

Hands the voicemail to the phone's inbox when the message arrives.
====================
*/
// 0x800D07EC
void CVoicemailMessageTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        SendVoicemail();
    }
}

/*
====================
CVoicemailMessageTrigger::fn_801A73A8

Takes the name of the message and the voicemail from the attribute packet.
====================
*/
// 0x800D086C
void CVoicemailMessageTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529C00 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetField3C:
            field_0x3C = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField40:
            field_0x40 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField44:
            field_0x44 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField48:
            field_0x48 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField4C:
            field_0x4C = it->GetU32();
            break;
        }
        ++it;
    }
}
