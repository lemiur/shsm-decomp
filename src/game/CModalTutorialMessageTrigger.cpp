// CModalTutorialMessageTrigger: class name from the registration strings lbl_803EC7DC and
// lbl_803EC7FC. The inline factory fn_800CC458 and the two thunks follow the static initialiser
// in .text.

#include <game/CModalTutorialMessageTrigger.h>
#include <game/DisplayHeight.h>
#include <game/unk_Settings.h>

unk_TutorialMessageBox* lbl_80529B50;
u32 lbl_80529B54 = fn_801A7A84( "CModalTutorialMessageTrigger" );
unk_801A8F6C lbl_80529B58( "CModalTutorialMessageTrigger", CModalTutorialMessageTrigger::fn_800CC458,
                           sizeof( CModalTutorialMessageTrigger ) );
bool lbl_80529B59;

/*
====================
fn_800CBEF8
====================
*/
// 0x800CBEF8
bool fn_800CBEF8() {
    return lbl_80529B59;
}

/*
====================
CModalTutorialMessageTrigger::~CModalTutorialMessageTrigger
====================
*/
// 0x800CBF00
CModalTutorialMessageTrigger::~CModalTutorialMessageTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
unk_TutorialMessageBox::Show
====================
*/
inline void unk_TutorialMessageBox::Show( void* image ) {
    CPlayer* player = lbl_80491B38.field_0x14;
    if ( player != NULL && player->mZone != NULL && !player->field_0x2366 ) {
        lbl_80529D20->field_0x6C4.fn_800E611C( text.title, text.body, image );
    }
    lbl_80529B59 = false;
}

/*
====================
CModalTutorialMessageTrigger::fn_8009CB1C

Hands the texts to the message box when the message arrives and shows them, once the image has
been read when there is one.
====================
*/
// 0x800CBFE4
void CModalTutorialMessageTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        unk_TutorialMessageBox* box = lbl_80529B50;
        box->text = text;
        if ( fn_800C770C( 11 ) ) {
            if ( box->text.image != NULL ) {
                char buffer[256];
                char* path = fn_801D207C( box->text.image, buffer, "Jpg", 0 );
                if ( path != NULL ) {
                    lbl_80529B59 = true;
                    bool requested = box->fn_801B250C( path, box, 0.5f ) != NULL;
                    if ( !requested ) {
                        lbl_80529B59 = false;
                    }
                }
            } else {
                lbl_80529B59 = true;
                box->Show( NULL );
            }
        }
    }
}

/*
====================
CModalTutorialMessageTrigger::fn_801A73A8

Takes the name of the message and the three texts from the attribute packet.
====================
*/
// 0x800CC168
void CModalTutorialMessageTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B54 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetTitle:
            text.title = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetBody:
            text.body = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetImage:
            text.image = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

/*
====================
fn_800CC25C
====================
*/
// 0x800CC25C
void fn_800CC25C() {
    if ( lbl_80529B50 == NULL ) {
        lbl_80529B50 = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_TutorialMessageBox;
    }
}

/*
====================
unk_TutorialMessageBox::~unk_TutorialMessageBox
====================
*/
// 0x800CC2C4
unk_TutorialMessageBox::~unk_TutorialMessageBox() {
}

/*
====================
unk_TutorialMessageBox::fn_800CC364
====================
*/
// 0x800CC364
void unk_TutorialMessageBox::fn_800CC364( void* data, int a, const char* name, int b, u32 size ) {
    Show( data );
}
