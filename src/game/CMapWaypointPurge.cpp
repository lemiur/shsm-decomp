// CMapWaypointPurge: class name from the registration strings lbl_803FD0A0 and lbl_803FD0B4.

#include <game/CMapWaypointPurge.h>

u32 lbl_8052A408 = fn_801A7A84( "CMapWaypointPurge" );
unk_801A8F6C lbl_8052A40C( "CMapWaypointPurge", CMapWaypointPurge::fn_8018DB6C, sizeof( CMapWaypointPurge ) );

/*
====================
CMapWaypointPurge::~CMapWaypointPurge
====================
*/
// 0x8018D98C
CMapWaypointPurge::~CMapWaypointPurge() {
    fn_801A9E00( purgeMsg );
    fn_801A9940( purgeMsg );
}

/*
====================
CMapWaypointPurge::fn_8009CB1C

Purges the map waypoints when the purge message arrives.
====================
*/
// 0x8018DA20
void CMapWaypointPurge::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == purgeMsg.record ) {
        lbl_8052A3F0->fn_8018CA04();
    }
}

/*
====================
CMapWaypointPurge::fn_801A73A8

Takes the name of the purge message from the attribute packet.
====================
*/
// 0x8018DA70
void CMapWaypointPurge::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_8052A408 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetPurgeMsg:
            fn_801AA218( purgeMsg, it->GetString(), 0 );
            break;
        }
        ++it;
    }
}
