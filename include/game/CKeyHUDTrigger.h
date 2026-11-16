#ifndef GAME_CKEYHUDTRIGGER_H
#define GAME_CKEYHUDTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/SaveData.h>
#include <game/unk_MessageInbox.h>

/*
 * Shows or hides the key HUD prompt when its message arrives, and records the choice in the
 * "UIKeyHUDPromptRoot" save block as "KeyHUDPromptActive"; whether it shows or hides is set
 * by the attribute packet. vtable __vt__14CKeyHUDTrigger, size 0x40. Member names invented.
 */
class CKeyHUDTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that shows or hides the prompt
        CMD_SetPromptActive // integer: nonzero shows the prompt, zero hides it
    };

    /* HUD element index of the key prompt. Name invented. */
    enum {
        HUD_KEY_PROMPT = 0x40
    };

                    CKeyHUDTrigger() :
                        promptActive( 0 ) {}
    virtual         ~CKeyHUDTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CB474( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CKeyHUDTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    u32             promptActive;   // 0x3C, nonzero: the message shows the prompt
};

extern const char* lbl_80527548;    // "UIKeyHUDPromptRoot", the save block
extern const char* lbl_8052754C;    // "KeyHUDPromptActive", the saved value
extern u32 lbl_80529B38;            // class-name hash of "CKeyHUDTrigger"
extern unk_801A8F6C lbl_80529B3C;   // type-registry entry of CKeyHUDTrigger

#endif
