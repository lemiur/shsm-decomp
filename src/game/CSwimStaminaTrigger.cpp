// CSwimStaminaTrigger: class name from the registration strings lbl_803ED168 and lbl_803ED17C;
// the message it posts is named by lbl_803ED0F8. The inline factory fn_800D065C and the two
// thunks follow the static initialiser in .text.

#include <game/CSwimStaminaTrigger.h>

extern const char* const lbl_8052C8C0 = "SetSwimStaminaDecay";

unk_801A92A8 lbl_80529BE8;
u32 lbl_80529BF0 = fn_801A7A84( "CSwimStaminaTrigger" );
unk_801A8F6C lbl_80529BF4( "CSwimStaminaTrigger", CSwimStaminaTrigger::fn_800D065C, sizeof( CSwimStaminaTrigger ) );

/*
====================
CSwimStaminaTrigger::~CSwimStaminaTrigger
====================
*/
// 0x800D03EC
CSwimStaminaTrigger::~CSwimStaminaTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    fn_801A9940( lbl_80529BE8 );
}

/*
====================
CSwimStaminaTrigger::fn_801A73A8

Takes the name of the message and the three decay values from the attribute packet.
====================
*/
// 0x800D0488
void CSwimStaminaTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BF0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetDecay0:
            decay[0] = it->GetFloat();
            break;
        case CMD_SetDecay1:
            decay[1] = it->GetFloat();
            break;
        case CMD_SetDecay2:
            decay[2] = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
CSwimStaminaTrigger::fn_8009CB1C

Posts the decay values when the message arrives.
====================
*/
// 0x800D0564
void CSwimStaminaTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        PostMsg( lbl_80529BE8, decay );
    }
}
