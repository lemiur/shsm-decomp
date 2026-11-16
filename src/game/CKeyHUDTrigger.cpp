// CKeyHUDTrigger: class name from the registration strings lbl_803EC580 and lbl_803EC590.
// The inline factory fn_800CB474 and the two thunks follow the static initialiser in .text.

#include <game/CKeyHUDTrigger.h>
#include <game/DisplayHeight.h>

const char* lbl_80527548 = "UIKeyHUDPromptRoot";
const char* lbl_8052754C = "KeyHUDPromptActive";

u32 lbl_80529B38 = fn_801A7A84( "CKeyHUDTrigger" );
unk_801A8F6C lbl_80529B3C( "CKeyHUDTrigger", CKeyHUDTrigger::fn_800CB474, sizeof( CKeyHUDTrigger ) );

/*
====================
CKeyHUDTrigger::~CKeyHUDTrigger
====================
*/
// 0x800CB1C8
CKeyHUDTrigger::~CKeyHUDTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
CKeyHUDTrigger::fn_8009CB1C

Saves whether the prompt is shown, then shows or hides it, when the message arrives.
====================
*/
// 0x800CB25C
void CKeyHUDTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        void* save = fn_802050AC( fn_80204C04(), lbl_80527548, 0, 0, 0, 0 );
        if ( save != NULL ) {
            u32 active = promptActive;
            fn_80205354( save, lbl_8052754C, 2, &active, sizeof( active ) );
        }
        if ( lbl_80529D20 != NULL ) {
            if ( promptActive ) {
                lbl_80529D20->field_0x6C4.fn_800E4148( HUD_KEY_PROMPT, lbl_803E6400, 0, 0, 1.0f );
            } else {
                lbl_80529D20->field_0x6C4.fn_800E588C( HUD_KEY_PROMPT, 1, 0 );
            }
        }
    }
}

/*
====================
CKeyHUDTrigger::fn_801A73A8

Takes the name of the message and whether it shows the prompt from the attribute packet.
====================
*/
// 0x800CB334
void CKeyHUDTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B38 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetPromptActive:
            promptActive = it->GetU32();
            break;
        }
        ++it;
    }
}
