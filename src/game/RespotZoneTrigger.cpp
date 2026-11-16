// RespotZoneTrigger: class name from the registration strings lbl_803ECEB8 and lbl_803ECECC.
// The inline factory fn_800CF120 and the two thunks follow the static initialiser in .text.

#include <game/RespotZoneTrigger.h>
#include <game/Log.h>
#include <game/unk_RespotManager.h>

/*
====================
RespotZoneTrigger::~RespotZoneTrigger
====================
*/
// 0x800CEEA4
RespotZoneTrigger::~RespotZoneTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
RespotZoneTrigger::fn_801A73A8

Takes the name of the message and the three names of the zone from the attribute packet.
====================
*/
// 0x800CEF38
void RespotZoneTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BC0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetField3C:
            field_0x3C = it->GetString();
            break;
        case CMD_SetField40:
            field_0x40 = it->GetString();
            break;
        case CMD_SetField44:
            field_0x44 = it->GetString();
            break;
        }
        ++it;
    }
}

/*
====================
RespotZoneTrigger::fn_8009CB1C

Hands the three names to the respot manager when the message arrives.
====================
*/
// 0x800CF014
void RespotZoneTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        // quirk: both log lines pass a literal NULL for the first %s (the trigger's own name);
        // retail loads li r5, 0 before each fn_80028D0C call
        fn_80028D0C( 2, "RespotZoneTrigger[%s]: Recieve %s", NULL, triggerMsg.record != NULL ? triggerMsg.record->name : NULL );
        lbl_80529260->fn_80030F34( field_0x3C, field_0x44, field_0x40 );
        fn_80028D0C( 1, "RespotZoneTrigger[%s]: RespotZone set as %s / %s / %s", NULL, field_0x3C, field_0x40, field_0x44 );
    }
}

u32 lbl_80529BC0 = fn_801A7A84( "RespotZoneTrigger" );
unk_801A8F6C lbl_80529BC4( "RespotZoneTrigger", RespotZoneTrigger::fn_800CF120, sizeof( RespotZoneTrigger ) );
