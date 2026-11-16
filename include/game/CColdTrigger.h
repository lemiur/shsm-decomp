#ifndef GAME_CCOLDTRIGGER_H
#define GAME_CCOLDTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Sets the cold level lbl_80529AC8 when its message arrives; the message and the level are
 * named by the attribute packet. vtable __vt__12CColdTrigger, size 0x58. Member names invented.
 */
class CColdTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetColdMsg,     // text: the name of the message that sets the level
        CMD_SetColdLevel    // integer: the level, clamped to [0, 3]
    };

                    CColdTrigger() : coldLevel( 0 ) {}
    virtual         ~CColdTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C8988( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CColdTrigger;
    }

    unk_801A92A8    coldMsg;    // 0x34
    u32             coldLevel;  // 0x3C
};

extern u32 lbl_80529AC8;            // the current cold level
extern u32 lbl_80529ACC;            // class-name hash of "CColdTrigger"
extern unk_801A8F6C lbl_80529AD0;   // type-registry entry of CColdTrigger

#endif
