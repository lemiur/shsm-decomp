#ifndef GAME_CMAPWAYPOINT_H
#define GAME_CMAPWAYPOINT_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Map.h>

/*
 * Adds its waypoint to the map when its message arrives; the message and the waypoint are set
 * by the attribute packet. vtable __vt__12CMapWaypoint, size 0x68.
 */
class CMapWaypoint : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetAddMsg,      // text: the name of the message that adds the waypoint
        CMD_SetName,        // text: the waypoint's name
        CMD_SetField4,      // text: the waypoint's second name
        CMD_SetField10,     // integer
        CMD_SetField14,     // integer
        CMD_SetField19      // flag
    };

                    CMapWaypoint() : field_0x3C( 0 ), field_0x40( 0 ), field_0x44( 0 ), field_0x48( 0 ) {}
    virtual         ~CMapWaypoint();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_8018D854( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CMapWaypoint;
    }

    unk_801A92A8    addMsg;         // 0x34
    u32             field_0x3C;     // 0x3C, unused here
    u32             field_0x40;     // 0x40, unused here
    u32             field_0x44;     // 0x44, unused here
    u32             field_0x48;     // 0x48, unused here
    unk_MapWaypoint waypoint;       // 0x4C
};

/*
 * Removes a waypoint, named by the attribute packet, from the map when its message arrives.
 * vtable __vt__18CRemoveMapWaypoint, size 0x40.
 */
class CRemoveMapWaypoint : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetRemoveMsg,   // text: the name of the message that removes the waypoint
        CMD_SetName         // text: the name of the waypoint to remove
    };

                    CRemoveMapWaypoint() : name( NULL ) {}
    virtual         ~CRemoveMapWaypoint();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_8018D8F8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CRemoveMapWaypoint;
    }

    unk_801A92A8    removeMsg;      // 0x34
    const char*     name;           // 0x3C, points into the attribute packet
};

extern u32 lbl_8052A3F8;            // class-name hash of "CMapWaypoint"
extern unk_801A8F6C lbl_8052A3FC;   // type-registry entry of CMapWaypoint
extern u32 lbl_8052A400;            // class-name hash of "CRemoveMapWaypoint"
extern unk_801A8F6C lbl_8052A404;   // type-registry entry of CRemoveMapWaypoint

#endif
