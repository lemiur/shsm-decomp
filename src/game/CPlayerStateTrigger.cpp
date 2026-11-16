// CPlayerStateTrigger: class name from the registration strings lbl_803ECCE0 and lbl_803ECCF4.
// The inline factory fn_800CE900 and the two thunks follow the static initialiser in .text.

#include <game/CPlayerStateTrigger.h>
#include <game/Log.h>
#include <game/Messages.h>

/*
====================
CPlayerStateTrigger::~CPlayerStateTrigger
====================
*/
// 0x800CE684
CPlayerStateTrigger::~CPlayerStateTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
CPlayerStateTrigger::fn_801A73A8

Takes the name of the message, the state name and the flag from the attribute packet.
====================
*/
// 0x800CE718
void CPlayerStateTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BA8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetStateName:
            stateName = it->GetString();
            break;
        case CMD_SetFlag:
            if ( it->GetU32() ) {
                field_0x40 = true;
            } else {
                field_0x40 = false;
            }
            break;
        }
        ++it;
    }
}

/*
====================
CPlayerStateTrigger::fn_8009CB1C

Posts the state payload when the message arrives.
====================
*/
// 0x800CE800
void CPlayerStateTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        fn_800A65B0( "Player State Trigger (%s) jumps player to %s ",
                     triggerMsg.record != NULL ? triggerMsg.record->name : NULL, stateName );
        PostMsg( lbl_805294E0, &stateName );
    }
}

u32 lbl_80529BA8 = fn_801A7A84( "CPlayerStateTrigger" );
unk_801A8F6C lbl_80529BAC( "CPlayerStateTrigger", CPlayerStateTrigger::fn_800CE900, sizeof( CPlayerStateTrigger ) );
