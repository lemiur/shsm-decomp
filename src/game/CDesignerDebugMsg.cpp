// CDesignerDebugMsg: class name from the registration strings lbl_803EC1A0 and lbl_803EC1B4.
// The inline factory fn_800C97C0 and the two thunks follow the static initialiser in .text.

#include <game/CDesignerDebugMsg.h>
#include <climax/Font.h>
#include <game/Display.h>
#include <game/Messages.h>

CDesignerDebugMsg* lbl_80529AF0;

/*
====================
CDesignerDebugMsg::Hide

Name invented.
====================
*/
inline void CDesignerDebugMsg::Hide() {
    if ( lbl_80529AF0 == this ) {
        lbl_80529AF0 = NULL;
    }
    fn_801A9E00( lbl_8052A828 );
}

/*
====================
CDesignerDebugMsg::Show

Name invented.
====================
*/
inline void CDesignerDebugMsg::Show() {
    fn_801A9DE8( lbl_8052A828 );
    framesLeft = SHOW_FRAMES;
    if ( lbl_80529AF0 != NULL ) {
        lbl_80529AF0->Hide();
    }
    lbl_80529AF0 = this;
}

/*
====================
CDesignerDebugMsg::Draw

Name invented. The fade is lost: 0.04 times the frames left is below 1 throughout the fade, so
the integer factor is 0 and the text vanishes when the fade should start.
====================
*/
inline void CDesignerDebugMsg::Draw() {
    unk_Font* font = fn_801CE59C( NULL );
    framesLeft--;
    if ( font != NULL ) {
        unk_TextBox box;
        box.x = 0.0f;
        box.y = (float)lbl_8052861C - 42.0f;
        box.width = 640.0f;
        box.height = 28.0f;
        unk_RGBA colour = { 255, 255, 255, 255 };
        if ( framesLeft < FADE_FRAMES ) {
            float fade = 0.04f * framesLeft;
            // Blue before green, as retail stores them.
            colour.red *= (u8)(int)fade;
            colour.blue *= (u8)(int)fade;
            colour.green *= (u8)(int)fade;
            colour.alpha *= (u8)(int)fade;
        }
        fn_801CF248( font, text, &box, &colour, 1, 0, 3, 14.0f );
    }
    if ( framesLeft == 0 ) {
        Hide();
    }
}

/*
====================
CDesignerDebugMsg::~CDesignerDebugMsg
====================
*/
// 0x800C9400
CDesignerDebugMsg::~CDesignerDebugMsg() {
    Hide();
    if ( text != NULL ) {
        delete text;
    }
    fn_801A9E00( showMsg );
    fn_801A9940( showMsg );
}

/*
====================
CDesignerDebugMsg::fn_8009CB1C

Shows the text when the show message arrives and draws it on each frame message.
====================
*/
// 0x800C94C8
void CDesignerDebugMsg::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == showMsg.record ) {
        Show();
    } else if ( msg.record == lbl_8052A828.record ) {
        Draw();
    }
}

/*
====================
CDesignerDebugMsg::fn_801A73A8

Takes the name of the show message and the text from the attribute packet.
====================
*/
// 0x800C96A4
void CDesignerDebugMsg::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AF4 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetShowMsg:
            fn_801AA218( showMsg, it->GetString(), 0 );
            break;
        case CMD_SetText:
            text = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

u32 lbl_80529AF4 = fn_801A7A84( "CDesignerDebugMsg" );
unk_801A8F6C lbl_80529AF8( "CDesignerDebugMsg", CDesignerDebugMsg::fn_800C97C0, sizeof( CDesignerDebugMsg ) );
