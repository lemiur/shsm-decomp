// CMapWaypoint: class names from the registration strings lbl_803FD000 and lbl_803FD010
// ("CMapWaypoint"), lbl_803FD020 and lbl_803FD034 ("CRemoveMapWaypoint"). Both classes share
// this unit; their inline factories and the four thunks follow the static initialiser in .text.

#include <game/CMapWaypoint.h>
#include <string.h>

u32 lbl_8052A3F8 = fn_801A7A84( "CMapWaypoint" );
unk_801A8F6C lbl_8052A3FC( "CMapWaypoint", CMapWaypoint::fn_8018D854, sizeof( CMapWaypoint ) );
u32 lbl_8052A400 = fn_801A7A84( "CRemoveMapWaypoint" );
unk_801A8F6C lbl_8052A404( "CRemoveMapWaypoint", CRemoveMapWaypoint::fn_8018D8F8, sizeof( CRemoveMapWaypoint ) );

/*
====================
CMapWaypoint::~CMapWaypoint
====================
*/
// The original frees the waypoint's names here with scalar delete, although they come from
// fn_801B67FC's array allocation; the waypoint's own destructor then finds them NULL.
// 0x8018D374
CMapWaypoint::~CMapWaypoint() {
    fn_801A9E00( addMsg );
    fn_801A9940( addMsg );
    if ( waypoint.name != NULL ) {
        delete waypoint.name;
        waypoint.name = NULL;
    }
    if ( waypoint.field_0x4 != NULL ) {
        delete waypoint.field_0x4;
        waypoint.field_0x4 = NULL;
    }
}

/*
====================
CMapWaypoint::fn_8009CB1C

Adds the waypoint to the map when the add message arrives.
====================
*/
// 0x8018D470
void CMapWaypoint::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == addMsg.record ) {
        lbl_8052A3F0->fn_8018C524( &waypoint );
    }
}

/*
====================
CMapWaypoint::fn_801A73A8

Takes the add message and the waypoint from the attribute packet.
====================
*/
// 0x8018D4C4
void CMapWaypoint::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_8052A3F8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetAddMsg:
            fn_801AA218( addMsg, it->GetString(), 0 );
            break;
        case CMD_SetName:
            waypoint.name = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField4:
            waypoint.field_0x4 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField10:
            waypoint.field_0x10 = it->GetU32();
            break;
        case CMD_SetField14:
            waypoint.field_0x14 = it->GetU32();
            break;
        case CMD_SetField19:
            waypoint.field_0x19 = it->GetU32() != 0;
            break;
        }
        ++it;
    }
}

/*
====================
CRemoveMapWaypoint::~CRemoveMapWaypoint
====================
*/
// 0x8018D5E4
CRemoveMapWaypoint::~CRemoveMapWaypoint() {
    fn_801A9E00( removeMsg );
    fn_801A9940( removeMsg );
}

/*
====================
CRemoveMapWaypoint::fn_801A73A8

Takes the remove message and the waypoint's name from the attribute packet.
====================
*/
// 0x8018D678
void CRemoveMapWaypoint::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_8052A400 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetRemoveMsg:
            fn_801AA218( removeMsg, it->GetString(), 0 );
            break;
        case CMD_SetName:
            name = it->GetString();
            break;
        }
        ++it;
    }
}

/*
====================
CRemoveMapWaypoint::fn_8009CB1C

Removes the named waypoint from the map when the remove message arrives.
====================
*/
// 0x8018D72C
void CRemoveMapWaypoint::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == removeMsg.record ) {
        const char* waypointName = name;
        if ( waypointName != NULL && strlen( waypointName ) != 0 ) {
            lbl_8052A3F0->fn_8018CD44( waypointName );
        }
    }
    CBaseBehaviour::fn_8009CB1C( msg );
}
