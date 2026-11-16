// CPsycologyZoneTrigger: class name from the registration strings lbl_803ECD60 and lbl_803ECD78.
// The inline factory fn_800CEB6C and the two thunks follow the static initialiser in .text.

#include <game/CPsycologyZoneTrigger.h>
#include <game/unk_80491B38.h>

u32 lbl_80529BB0 = fn_801A7A84( "CPsycologyZoneTrigger" );
unk_801A8F6C lbl_80529BB4( "CPsycologyZoneTrigger", CPsycologyZoneTrigger::fn_800CEB6C, sizeof( CPsycologyZoneTrigger ) );

/*
====================
CPsycologyZoneTrigger::~CPsycologyZoneTrigger
====================
*/
// 0x800CE988
CPsycologyZoneTrigger::~CPsycologyZoneTrigger() {
    delete[] zoneName;
    fn_801A9E00( zoneMsg );
    fn_801A9940( zoneMsg );
}

/*
====================
CPsycologyZoneTrigger::fn_8009CB1C

Passes the zone name on when the message arrives.
====================
*/
// 0x800CEA28
void CPsycologyZoneTrigger::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == zoneMsg.record ) {
        fn_8005B444( lbl_80491B38.field_0x14, zoneName );
    }
}

/*
====================
CPsycologyZoneTrigger::fn_801A73A8

Takes the zone name and the name of the message from the attribute packet.
====================
*/
// 0x800CEA50
void CPsycologyZoneTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BB0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetZoneName:
            zoneName = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetZoneMsg:
            fn_801AA218( zoneMsg, it->GetString(), 0 );
            break;
        }
        ++it;
    }
}
