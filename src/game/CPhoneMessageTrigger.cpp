// CPhoneMessageTrigger: class name from the registration strings lbl_803ECB60 and lbl_803ECB78.
// The inline factory fn_800CDCBC and the two thunks follow the static initialiser in .text.

#include <game/CPhoneMessageTrigger.h>
#include <game/DisplayHeight.h>

u32 lbl_80529B98 = fn_801A7A84( "CPhoneMessageTrigger" );
unk_801A8F6C lbl_80529B9C( "CPhoneMessageTrigger", CPhoneMessageTrigger::fn_800CDCBC, sizeof( CPhoneMessageTrigger ) );

/*
====================
CPhoneMessageTrigger::~CPhoneMessageTrigger
====================
*/
// 0x800CD998
CPhoneMessageTrigger::~CPhoneMessageTrigger() {
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
CPhoneMessageTrigger::fn_8009CB1C

Hands the text message to the phone's inbox when the message arrives.
====================
*/
// 0x800CDA8C
void CPhoneMessageTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        SendTextMessage();
    }
}

/*
====================
CPhoneMessageTrigger::fn_801A73A8

Takes the name of the message and the text message from the attribute packet.
====================
*/
// 0x800CDB0C
void CPhoneMessageTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B98 );
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
