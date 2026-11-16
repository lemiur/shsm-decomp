#ifndef GAME_CSCREENSHAKETRIGGER_H
#define GAME_CSCREENSHAKETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/* The payload CScreenShakeTrigger posts to lbl_80529E70; the meaning of the values is unknown. Name invented. */
struct unk_ScreenShakeData {
    float   field_0x0;      // from CScreenShakeTrigger::field_0x40
    float   field_0x4;      // from field_0x44
    float   field_0x8;      // from field_0x3C
    float   field_0xC;      // from field_0x50
    u32     field_0x10;     // from field_0x4C
};

/*
 * Posts a screen shake to lbl_80529E70 when its message arrives; a one-shot trigger then stops
 * listening. The message and the shake values are named by the attribute packet.
 * vtable __vt__19CScreenShakeTrigger, size 0x54. Member names invented.
 */
class CScreenShakeTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetShakeMsg,    // text: the name of the message that starts the shake
        CMD_SetField3C,     // float
        CMD_SetField40,     // float
        CMD_SetField44,     // float
        CMD_SetOneShot,     // integer: nonzero stops listening after the first shake
        CMD_SetField4C,     // integer
        CMD_SetField50      // float
    };

                    CScreenShakeTrigger()
                        : field_0x3C( 0.0f ), field_0x40( 0.0f ), field_0x44( 0.0f ), oneShot( 0 ) {}
    virtual         ~CScreenShakeTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CF49C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CScreenShakeTrigger;
    }

    unk_801A92A8    shakeMsg;   // 0x34
    float           field_0x3C;
    float           field_0x40;
    float           field_0x44;
    u32             oneShot;    // 0x48
    u32             field_0x4C;
    float           field_0x50;
};

extern u32 lbl_80529BC8;            // class-name hash of "CScreenShakeTrigger"
extern unk_801A8F6C lbl_80529BCC;   // type-registry entry of CScreenShakeTrigger

#endif
