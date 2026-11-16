#ifndef GAME_CSWIMSTAMINATRIGGER_H
#define GAME_CSWIMSTAMINATRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

extern unk_801A92A8 lbl_80529BE8;           // the message SetSwimStaminaDecay; each instance registers it
extern u32 lbl_80529BF0;                    // class-name hash of "CSwimStaminaTrigger"
extern unk_801A8F6C lbl_80529BF4;           // type-registry entry of CSwimStaminaTrigger
extern const char* const lbl_8052C8C0;      // "SetSwimStaminaDecay"

/*
 * Posts the three swim-stamina decay values it holds as SetSwimStaminaDecay when its message
 * arrives; the message and the values are set by the attribute packet. vtable __vt__19CSwimStaminaTrigger,
 * size 0x48. Member names invented.
 */
class CSwimStaminaTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that posts the decay
        CMD_SetDecay0,      // float: decay[0]
        CMD_SetDecay1,      // float: decay[1]
        CMD_SetDecay2       // float: decay[2]
    };

                    CSwimStaminaTrigger() {
                        decay[0] = 0.0f;
                        decay[1] = 0.0f;
                        decay[2] = 0.0f;
                        fn_801A93B4( lbl_80529BE8, lbl_8052C8C0, NULL );
                    }
    virtual         ~CSwimStaminaTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800D065C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSwimStaminaTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    float           decay[3];       // 0x3C, the payload of lbl_80529BE8; names invented
};

#endif
