#ifndef GAME_CSCALINGOBJECT_H
#define GAME_CSCALINGOBJECT_H

#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/CPlayer.h>
#include <game/CSceneObject.h>

/*
 * A scene object that grows with the player's distance: on each frame message, while the player
 * is the one it belongs to (same field_0xC), it scales its frame's matrix by a factor taken from
 * how far the player is outside field_0x150 (0 inside it), and, the first time that distance passes
 * field_0x1A4, posts its message field_0x1A8 once. vtable __vt__14CScalingObject, size 0x1B4.
 * Member names invented.
 */
class CScalingObject : public CSceneObject {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetField0x148,      // float
        CMD_SetField0x14C,      // float
        CMD_SetField0x150,      // float
        CMD_SetField0x15C,      // float
        CMD_SetField0x1A4,      // float
        CMD_SetMsg              // text: the name of the message posted once
    };

                    CScalingObject( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CScalingObject();  // override
    virtual void    fn_800A52C4( float frameTime, CPlayer* player );
    virtual void    fn_8000CFD0();  // override

    static CAttributeHandler* fn_800A55F8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CScalingObject( packet );
    }

    float           field_0x148;    // 0x148
    float           field_0x14C;    // 0x14C
    float           field_0x150;    // 0x150, distance inside which the factor stays 0
    float           field_0x154;    // 0x154
    bool            field_0x158;    // 0x158, nonzero: the scale is field_0x154
    float           field_0x15C;    // 0x15C
    unk_RwMatrix    field_0x160;    // 0x160, the frame's matrix as it was
    float           field_0x1A0;    // 0x1A0, the current scale
    float           field_0x1A4;    // 0x1A4
    unk_801A92A8    field_0x1A8;    // 0x1A8
    bool            field_0x1B0;    // 0x1B0, field_0x1A8 was posted
};

extern u32 lbl_80529808;            // class-name hash of "CScalingObject"
extern unk_801A8F6C lbl_8052980C;   // type-registry entry of CScalingObject

#endif
