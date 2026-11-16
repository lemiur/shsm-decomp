// CIGCPlayerMover: class name from the registration strings lbl_803E9EA0 and lbl_803E9EB0; the
// message it posts is named by lbl_803E9E38. The inline factory fn_800AB4B4 and the two thunks
// follow the static initialiser in .text.

#include <game/CIGCPlayerMover.h>

extern const char* const lbl_8052C3F0 = "RespotPlayer";

unk_801A92A8 lbl_80529860;
u32 lbl_80529868 = fn_801A7A84( "CIGCPlayerMover" );
unk_801A8F6C lbl_8052986C( "CIGCPlayerMover", CIGCPlayerMover::fn_800AB4B4, sizeof( CIGCPlayerMover ) );

/*
====================
CIGCPlayerMover::~CIGCPlayerMover
====================
*/
// 0x800AB27C
CIGCPlayerMover::~CIGCPlayerMover() {
    fn_801A9940( lbl_80529860 );
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
CIGCPlayerMover::fn_801A73A8

Takes the name of the message from the attribute packet.
====================
*/
// 0x800AB31C
void CIGCPlayerMover::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529868 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

/*
====================
CIGCPlayerMover::fn_8009CB1C

Posts RespotPlayer with the behaviour's matrix when the message arrives.
====================
*/
// 0x800AB3B8
void CIGCPlayerMover::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        PostMsg( lbl_80529860, RwFrameGetMatrix( field_0x30 ) );
    }
}
