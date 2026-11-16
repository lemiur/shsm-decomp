// CPsyTrigger: class name from the registration strings lbl_803ECDE8 and lbl_803ECDF4.
// The inline factory fn_800CEE18 and the two thunks follow the static initialiser in .text.

#include <game/CPsyTrigger.h>

u32 lbl_80529BB8 = fn_801A7A84( "CPsyTrigger" );
unk_801A8F6C lbl_80529BBC( "CPsyTrigger", CPsyTrigger::fn_800CEE18, sizeof( CPsyTrigger ) );

/*
====================
CPsyTrigger::~CPsyTrigger
====================
*/
// 0x800CEBF0
CPsyTrigger::~CPsyTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
CPsyTrigger::fn_801A73A8

Takes the name, the message and the flag from the attribute packet.
====================
*/
// 0x800CEC84
void CPsyTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BB8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetFlag:
            flag = it->GetU32() == 0;
            break;
        }
        ++it;
    }
}

/*
====================
CPsyTrigger::fn_8009CB1C

Hands the name to the player when the message arrives.
====================
*/
// 0x800CED54
void CPsyTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record && name != NULL ) {
        lbl_8052A0D0->fn_8014EC98( name, flag, 0 );
    }
}
