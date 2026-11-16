#ifndef GAME_CMAPWAYPOINTPURGE_H
#define GAME_CMAPWAYPOINTPURGE_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Map.h>

/*
 * Purges the map waypoints when its message arrives; the message is named by the attribute
 * packet. vtable lbl_803FD048, size 0x3C.
 */
class CMapWaypointPurge : public CBaseBehaviour {
public:
    /* Attribute commands. Name invented. */
    enum {
        CMD_SetPurgeMsg     // text: the name of the message that triggers the purge
    };

                    CMapWaypointPurge() {}
    virtual         ~CMapWaypointPurge();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_8018DB6C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CMapWaypointPurge;
    }

    unk_801A92A8    purgeMsg;   // 0x34
};

extern u32 lbl_8052A408;            // class-name hash of "CMapWaypointPurge"
extern unk_801A8F6C lbl_8052A40C;   // type-registry entry of CMapWaypointPurge

#endif
