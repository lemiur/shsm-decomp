#ifndef GAME_CPSYTRIGGER_H
#define GAME_CPSYTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/unk_NamedEntryPlayer.h>

/*
 * Hands a name to the unk_NamedEntryPlayer when its message arrives; the name, a flag and
 * the message are set by the attribute packet. vtable __vt__11CPsyTrigger, size 0x44. Member names invented.
 */
class CPsyTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName,    // text: the name handed to the player
        CMD_SetMsg,     // text: the name of the message that triggers it
        CMD_SetFlag     // integer: the flag is set when it is zero
    };

                    CPsyTrigger() {
                        name = NULL;
                        flag = true;
                    }
    virtual         ~CPsyTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CEE18( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPsyTrigger;
    }

    const char*     name;       // 0x34
    bool            flag;       // 0x38
    unk_801A92A8    triggerMsg; // 0x3C
};

extern u32 lbl_80529BB8;            // class-name hash of "CPsyTrigger"
extern unk_801A8F6C lbl_80529BBC;   // type-registry entry of CPsyTrigger

#endif
