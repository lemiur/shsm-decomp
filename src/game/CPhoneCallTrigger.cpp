// CPhoneCallTrigger: class name from the registration strings lbl_803ECAE0 and lbl_803ECAF4.
// The inline factory fn_800CD940 and the two thunks follow the static initialiser in .text.

#include <game/CPhoneCallTrigger.h>
#include <game/DisplayHeight.h>

u32 lbl_80529B88 = fn_801A7A84( "CPhoneCallTrigger" );
unk_801A8F6C lbl_80529B8C( "CPhoneCallTrigger", CPhoneCallTrigger::fn_800CD940, sizeof( CPhoneCallTrigger ) );

/*
====================
CPhoneCallTrigger::CPhoneCallTrigger
====================
*/
// 0x800CD374
// quirk: the attribute packet is taken and not used (the members are set later by
// fn_801A73A8); the factory fn_800CD940 passes it, and this body stays out of line, as retail
// calls it from the factory rather than folding it in
CPhoneCallTrigger::CPhoneCallTrigger( const unk_AttributePacket& packet ) :
    field_0x54( NULL ),
    field_0x58( NULL ),
    field_0x5C( NULL ),
    field_0x60( 15 ),
    field_0x64( 0 ),
    field_0x68( 0 ),
    field_0x6C( 0 ),
    callStarted( 0 ),
    callEnded( 0 ),
    field_0x78( NULL ),
    field_0x7C( 0 ),
    field_0x80( false ) {}

/*
====================
CPhoneCallTrigger::~CPhoneCallTrigger

Ends a call that was started and not ended.
====================
*/
// 0x800CD418
CPhoneCallTrigger::~CPhoneCallTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    fn_801A9E00( field_0x4C );
    fn_801A9940( field_0x4C );
    fn_801A9E00( field_0x44 );
    fn_801A9940( field_0x44 );
    fn_801A9940( field_0x3C );
    // quirk: the three texts are deleted in the order 0x54, 0x5C, 0x58, not in field order, the
    // same order StartCall hands them to unk_CallInfo; retail's deletes run in this order
    if ( field_0x54 ) {
        delete field_0x54;
        field_0x54 = NULL;
    }
    if ( field_0x5C ) {
        delete field_0x5C;
        field_0x5C = NULL;
    }
    if ( field_0x58 ) {
        delete field_0x58;
        field_0x58 = NULL;
    }
    if ( field_0x78 ) {
        delete field_0x78;
        field_0x78 = NULL;
    }
    if ( callStarted && !callEnded ) {
        lbl_80529D20->field_0x9C.fn_800F6158();
    }
}

/*
====================
CPhoneCallTrigger::fn_8009CB1C

Starts the call when its message arrives; the other two messages act on the inbox.
====================
*/
// 0x800CD584
void CPhoneCallTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        StartCall();
        callEnded = 0;
        callStarted = 1;
    } else if ( msg.record == field_0x44.record ) {
        lbl_80529D20->field_0x9C.fn_800F6B6C( field_0x80 );
    } else if ( msg.record == field_0x4C.record ) {
        unk_MessageInbox* inbox = &lbl_80529D20->field_0x9C;
        // quirk: a member's address is never null, but retail tests it (the original's
        // defensive check, kept)
        if ( inbox ) {
            inbox->fn_800F67E4();
        }
    }
}

/*
====================
CPhoneCallTrigger::fn_801A73A8

Takes the messages, the call's texts and numbers and the flag from the attribute packet.
====================
*/
// 0x800CD678
void CPhoneCallTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B88 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetField3C:
            fn_801AA1C4( field_0x3C, it->GetString(), 0 );
            break;
        case CMD_SetField44:
            fn_801AA218( field_0x44, it->GetString(), 0 );
            break;
        case CMD_SetField54:
            field_0x54 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField58:
            field_0x58 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField5C:
            field_0x5C = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField60:
            field_0x60 = it->GetU32();
            break;
        case CMD_SetField64:
            field_0x64 = it->GetU32();
            break;
        case CMD_SetField68:
            field_0x68 = it->GetU32();
            break;
        case CMD_SetField6C:
            field_0x6C = it->GetU32();
            break;
        case CMD_SetField4C:
            fn_801AA218( field_0x4C, it->GetString(), 0 );
            break;
        case CMD_SetField78:
            field_0x78 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField7C:
            field_0x7C = it->GetU32();
            break;
        case CMD_SetField80:
            if ( it->GetU32() ) {
                field_0x80 = true;
            } else {
                field_0x80 = false;
            }
            break;
        }
        ++it;
    }
}

/*
====================
CPhoneCallTrigger::fn_800CD830

Ends the call: restores the player, posts the end message and marks the call ended.
====================
*/
// 0x800CD830
void CPhoneCallTrigger::fn_800CD830() {
    lbl_80491B38.field_0x14->fn_80040D4C( 0.0f, 255 );
    PostMsg( field_0x3C, NULL );
    callEnded = 1;
}
