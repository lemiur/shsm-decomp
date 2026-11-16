#include <game/CAmbientTextMessage.h>
#include <game/DisplayHeight.h>

u32 lbl_80529A50 = fn_801A7A84( "CAmbientTextMessage" );
unk_801A8F6C lbl_80529A54( "CAmbientTextMessage", CAmbientTextMessage::fn_800C2F54, sizeof( CAmbientTextMessage ) );

/*
====================
CAmbientTextMessage::CAmbientTextMessage
====================
*/
inline CAmbientTextMessage::CAmbientTextMessage( const unk_AttributePacket& packet ) : CAmbientInterferencePoint( packet ) {
    field_0xE8 = NULL;
    field_0xEC = NULL;
    field_0xF0 = NULL;
    field_0xF4 = NULL;
    field_0xF8 = 0;
}

/*
====================
CAmbientTextMessage::~CAmbientTextMessage
====================
*/
// 0x800C2C48
CAmbientTextMessage::~CAmbientTextMessage() {
    if ( field_0xE8 ) {
        delete field_0xE8;
        field_0xE8 = NULL;
    }
    if ( field_0xEC ) {
        delete field_0xEC;
        field_0xEC = NULL;
    }
    if ( field_0xF0 ) {
        delete field_0xF0;
        field_0xF0 = NULL;
    }
    if ( field_0xF4 ) {
        delete field_0xF4;
        field_0xF4 = NULL;
    }
}

/*
====================
CAmbientTextMessage::fn_8009CB1C
====================
*/
// 0x800C2D24
void CAmbientTextMessage::fn_8009CB1C( unk_Msg& msg ) {
    CAmbientInterferencePoint::fn_8009CB1C( msg );
}

/*
====================
CAmbientTextMessage::fn_800C2790

Hands the message to the phone when the base class fires.
====================
*/
// 0x800C2D28
int CAmbientTextMessage::fn_800C2790() {
    int fired = CAmbientInterferencePoint::fn_800C2790();
    if ( fired ) {
        lbl_80529D20->field_0x9C.fn_800F56F0( unk_MessageInfo( field_0xE8, field_0xEC, field_0xF0, field_0xF4, field_0xF8 ) );
        fn_800C2A0C( field_0xEC, true );
    }
    return fired;
}

/*
====================
CAmbientTextMessage::fn_801A73A8

Takes the message from the packet's commands.
====================
*/
// 0x800C2DB4
void CAmbientTextMessage::fn_801A73A8( const unk_AttributePacket& packet ) {
    CAmbientInterferencePoint::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_80529A50 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetFieldE8:
            field_0xE8 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetFieldEC:
            field_0xEC = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetFieldF0:
            field_0xF0 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetFieldF4:
            field_0xF4 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetFieldF8:
            field_0xF8 = it->GetU32();
            break;
        }
        ++it;
    }
    fn_800C2A0C( field_0xEC, false );
}
