#ifndef GAME_CONSCREENPROMPTTRIGGER_H
#define GAME_CONSCREENPROMPTTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/SaveData.h>
#include <game/unk_HUDRequest.h>

class COnScreenPromptTrigger;

extern const char* lbl_80527560;    // "SaveData", the group of the save-data value

/*
 * The request behind an on-screen prompt: done when its lifetime has run out, when its trigger
 * has let it go, or once the trigger's save-data value is set. vtable
 * __vt__25unk_OnScreenPromptRequest, size 0x14. Name invented.
 */
class unk_OnScreenPromptRequest : public unk_HUDRequest {
public:
                    unk_OnScreenPromptRequest( float lifetime, COnScreenPromptTrigger* trigger ) :
                        unk_HUDRequest( lifetime ),
                        owner( trigger ) {}
    virtual bool    fn_80072D1C() const;

    COnScreenPromptTrigger* owner;  // 0xC, the trigger that showed the prompt; NULL once let go
};

/*
 * Shows a text prompt on the HUD when its show message arrives, if the setting allows it, until
 * its hide message arrives, its lifetime runs out or a save-data value (fn_80206348) is set; the
 * messages, the text, the lifetime and the save-data value are set by the attribute packet.
 * vtable __vt__22COnScreenPromptTrigger, size 0x58. Member names invented.
 */
class COnScreenPromptTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetShowMsg,         // text: the name of the message that shows the prompt
        CMD_SetHideMsg,         // text: the name of the message that lets the prompt go
        CMD_SetText,            // text: the prompt text
        CMD_SetLifetime,        // float: seconds the prompt stays; 0 or less for no limit
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB        // text: copied to saveDataB
    };

    /* The kind of prompt passed to the HUD controller. Name invented. */
    enum { PROMPT_KIND = 2 };

                    COnScreenPromptTrigger() :
                        text( NULL ),
                        lifetime( 0.0f ),
                        saveDataA( NULL ),
                        saveDataB( NULL ) {}
    virtual         ~COnScreenPromptTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CCE68( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) COnScreenPromptTrigger;
    }

    /* True when both save-data keys are set. Name invented. */
    bool            HasSaveData() const {
                        return saveDataA != NULL && saveDataB != NULL;
                    }
    /* True when the save-data value is set. Name invented. */
    bool            IsSaveDataSet() const {
                        return HasSaveData() && fn_80206348( saveDataA, saveDataB, lbl_80527560 );
                    }

    unk_801A92A8    showMsg;        // 0x34
    unk_801A92A8    hideMsg;        // 0x3C
    char*           text;           // 0x44, owned copy
    float           lifetime;       // 0x48, seconds
    char*           saveDataA;      // 0x4C, owned copy; first argument of fn_80206348
    char*           saveDataB;      // 0x50, owned copy; second argument of fn_80206348
    unk_RefPtr< unk_OnScreenPromptRequest > prompt; // 0x54, the request of the prompt shown last
};

extern u32 lbl_80529B70;            // class-name hash of "COnScreenPromptTrigger"
extern unk_801A8F6C lbl_80529B74;   // type-registry entry of COnScreenPromptTrigger

#endif
