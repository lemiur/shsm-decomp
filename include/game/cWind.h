#ifndef GAME_CWIND_H
#define GAME_CWIND_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * A wind: a force that the objects of a zone sample with fn_800917C8. Winds are kept in the list
 * lbl_80529730, linked through next. A directional wind pushes along the at vector of its frame;
 * a point wind blends toward the at vector inside its radius. The message ids onMessage and
 * offMessage ramp the wind up and down over the frame time. The class is registered as "cWind"
 * (lbl_805272A0). Member names invented.
 */
class cWind : public CBaseBehaviour {
public:
    /* Attribute commands, by command id. Names invented. */
    enum {
        CMD_SetMode,            // integer: MODE_Directional or MODE_Point
        CMD_SetRadius,          // float: the radius of a point wind
        CMD_SetStrength,        // float: the maximum strength
        CMD_SetActive,          // integer: nonzero starts the wind at full level
        CMD_SetOnMessage,       // name: the message that ramps the wind up
        CMD_SetOffMessage,      // name: the message that ramps the wind down
        CMD_SetDuration         // float: seconds for a ramp to complete
    };

    /* Wind modes. Names invented. */
    enum {
        MODE_Directional,       // pushes along the at vector of the frame
        MODE_Point              // blends toward the at vector inside the radius
    };

                    cWind( const unk_AttributePacket& packet );
    virtual         ~cWind();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_80091BA4( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) cWind( packet );
    }

    bool            active;         // 0x34
    unk_801A92A8    onMessage;      // 0x38, ramps the wind up
    unk_801A92A8    offMessage;     // 0x40, ramps the wind down
    float           maxStrength;    // 0x48
    bool            rampingUp;      // 0x4C
    bool            rampingDown;    // 0x4D
    int             mode;           // 0x50
    float           level;          // 0x54, the fraction of maxStrength in force
    float           radius;         // 0x58, the falloff distance of a point wind
    float           radiusSq;       // 0x5C, radius squared
    float           duration;       // 0x60, seconds for a ramp
    float           strength;       // 0x64, the strength applied this frame
    cWind*          next;           // 0x68, the next wind in lbl_80529730
};

// Sums the force of the active winds of zone at pos into out.
void fn_800917C8( CZone* zone, const unk_RwV3d* pos, unk_RwV3d* out );

extern cWind*           lbl_80529730;   // the list of winds (8 bytes in retail)
extern u32              lbl_80529728;   // class-name hash of "cWind"
extern unk_801A8F6C     lbl_8052972C;   // type-registry entry of cWind

#endif
