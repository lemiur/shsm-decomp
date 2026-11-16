#ifndef GAME_CPLAYERSTATETRIGGER_H
#define GAME_CPLAYERSTATETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Sends the player to a named state when its message arrives ("Player State Trigger (%s)
 * jumps player to %s "): it posts its state payload, the state name and a flag, to
 * lbl_805294E0. The message, the state name and the flag are set by the attribute packet.
 * vtable __vt__19CPlayerStateTrigger, size 0x44. Member names invented.
 */
class CPlayerStateTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that triggers the jump
        CMD_SetStateName,   // text: the name of the player state
        CMD_SetFlag         // integer: nonzero sets field_0x40
    };

                    CPlayerStateTrigger() : stateName( NULL ), field_0x40( false ) {}
    virtual         ~CPlayerStateTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CE900( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPlayerStateTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    const char*     stateName;      // 0x3C, the payload posted to lbl_805294E0 begins here
    bool            field_0x40;     // 0x40, posted with the state name; purpose unknown
};

extern u32 lbl_80529BA8;            // class-name hash of "CPlayerStateTrigger"
extern unk_801A8F6C lbl_80529BAC;   // type-registry entry of CPlayerStateTrigger

#endif
