#ifndef GAME_CFADETRIGGER_H
#define GAME_CFADETRIGGER_H

#include <climax/Colour.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Starts a screen fade towards its colour when its start message arrives and sends its done
 * message once the fade has finished. The messages, the direction, the duration and the colour
 * are named by the attribute packet. vtable __vt__12CFadeTrigger, size 0x54. Member names invented.
 */
class CFadeTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented; command 0 is not handled. */
    enum {
        CMD_SetDirection = 1,   // integer: 0 fades the level down, otherwise up
        CMD_SetDuration,        // float: seconds the fade takes
        CMD_SetStartMsg,        // text: the name of the message that starts the fade
        CMD_SetDoneMsg,         // text: the name of the message sent when it has finished
        CMD_SetField4C,         // integer: stored in field_0x4C
        CMD_SetColour,          // colour: the fade colour (its alpha is ignored)
        CMD_SetFadeFlag         // integer: passed with the fade to fn_801E31C4
    };

                    CFadeTrigger() : field_0x4C( true ), fadeFlag( false ) {
                        colour.red = 0;
                        colour.green = 0;
                        colour.blue = 0;
                        colour.alpha = 255;
                    }
    virtual         ~CFadeTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C9C00( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CFadeTrigger;
    }

    u32             direction;  // 0x34
    float           duration;   // 0x38
    unk_801A92A8    startMsg;   // 0x3C
    unk_801A92A8    doneMsg;    // 0x44
    bool            field_0x4C; // set by the packet, never read by the unit
    bool            fadeFlag;   // 0x4D
    unk_RGBA        colour;     // 0x4E
};

extern u32 lbl_80529B00;            // class-name hash of "CFadeTrigger"
extern unk_801A8F6C lbl_80529B04;   // type-registry entry of CFadeTrigger

#endif
