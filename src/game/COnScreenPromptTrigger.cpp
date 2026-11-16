// COnScreenPromptTrigger: class name from the registration strings lbl_803EC99C and
// lbl_803EC9B4. The inline factory fn_800CCE68 and the two thunks follow the static initialiser
// in .text.

#include <game/COnScreenPromptTrigger.h>
#include <game/DisplayHeight.h>
#include <game/unk_MessageInbox.h>
#include <game/unk_Settings.h>

/* The lifetime of a prompt that stays until it is let go. Name invented. */
#define PROMPT_LIFETIME_UNLIMITED   3.4028235e+38f

const char* lbl_80527560 = "SaveData";
u32 lbl_80529B70 = fn_801A7A84( "COnScreenPromptTrigger" );
unk_801A8F6C lbl_80529B74( "COnScreenPromptTrigger", COnScreenPromptTrigger::fn_800CCE68,
                           sizeof( COnScreenPromptTrigger ) );

/*
====================
unk_OnScreenPromptRequest::fn_80072D1C
====================
*/
// 0x800CC914
bool unk_OnScreenPromptRequest::fn_80072D1C() const {
    bool done = unk_HUDRequest::fn_80072D1C();
    if ( !done ) {
        if ( owner != NULL ) {
            done = owner->IsSaveDataSet();
        } else {
            done = true;
        }
    }
    return done;
}

/*
====================
COnScreenPromptTrigger::~COnScreenPromptTrigger
====================
*/
// 0x800CC9B0
COnScreenPromptTrigger::~COnScreenPromptTrigger() {
    fn_801A9E00( showMsg );
    fn_801A9E00( hideMsg );
    fn_801A9940( showMsg );
    fn_801A9940( hideMsg );
    if ( prompt != NULL ) {
        prompt->owner = NULL;
    }
    if ( text != NULL ) {
        delete[] text;
        text = NULL;
    }
    if ( saveDataA != NULL ) {
        delete[] saveDataA;
        saveDataA = NULL;
    }
    if ( saveDataB != NULL ) {
        delete[] saveDataB;
        saveDataB = NULL;
    }
}

/*
====================
COnScreenPromptTrigger::fn_8009CB1C

Hands a new prompt request to the HUD when the show message arrives and the setting allows it;
lets the request go when the hide message arrives.
====================
*/
// 0x800CCAFC
void COnScreenPromptTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == showMsg.record ) {
        if ( fn_800C770C( 11 ) ) {
            prompt = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_OnScreenPromptRequest(
                lifetime > 0.0f ? lifetime : PROMPT_LIFETIME_UNLIMITED, this );
            lbl_80529D20->field_0x6C4.fn_800E50E8( text, PROMPT_KIND, prompt );
        }
    } else if ( msg.record == hideMsg.record ) {
        if ( prompt != NULL ) {
            prompt->owner = NULL;
        }
    }
}

/*
====================
COnScreenPromptTrigger::fn_801A73A8

Takes the messages, the text, the lifetime and the save-data keys from the attribute packet.
====================
*/
// 0x800CCC74
void COnScreenPromptTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B70 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetShowMsg:
            fn_801AA218( showMsg, it->GetString(), 0 );
            break;
        case CMD_SetHideMsg:
            fn_801AA218( hideMsg, it->GetString(), 0 );
            break;
        case CMD_SetText:
            text = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetLifetime:
            lifetime = it->GetFloat();
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}
