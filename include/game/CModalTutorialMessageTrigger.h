#ifndef GAME_CMODALTUTORIALMESSAGETRIGGER_H
#define GAME_CMODALTUTORIALMESSAGETRIGGER_H

#include <climax/Memory.h>
#include <climax/unk_FileLoadCallback.h>
#include <game/CBaseBehaviour.h>
#include <game/unk_80491B38.h>
#include <game/unk_MessageInbox.h>

/*
 * The three texts of a modal tutorial message: owned copies, deleted with the holder. Name and
 * member names invented.
 */
struct unk_TutorialText {
                    unk_TutorialText() :
                        title( NULL ),
                        body( NULL ),
                        image( NULL ) {}
                    ~unk_TutorialText() {
                        FreeText( title );
                        FreeText( body );
                        FreeText( image );
                    }

    /* Replaces each text with a copy of the other holder's. */
    unk_TutorialText& operator=( const unk_TutorialText& other ) {
                        FreeText( title );
                        title = fn_801B67FC( other.title, 0 );
                        FreeText( body );
                        body = fn_801B67FC( other.body, 0 );
                        FreeText( image );
                        image = fn_801B67FC( other.image, 0 );
                        return *this;
                    }

    static void     FreeText( char*& text ) {
                        if ( text != NULL ) {
                            delete[] text;
                            text = NULL;
                        }
                    }

    char*           title;      // 0x0
    char*           body;       // 0x4
    char*           image;      // 0x8, the name of a "Jpg" image, or NULL
};

/*
 * The one modal tutorial message box (lbl_80529B50, created by fn_800CC25C): it holds the texts
 * of the last message and, when the message has an image, is told when the image has been read.
 * vtable __vt__22unk_TutorialMessageBox, size 0x18. Name invented.
 */
class unk_TutorialMessageBox : public unk_BehaviourBase, public unk_FileLoadCallback {
public:
    virtual         ~unk_TutorialMessageBox();
    /* Shows the message with the image just read. */
    virtual void    fn_800CC364( void* data, int a, const char* name, int b, u32 size );

    /* Shows the message, if the player can see it, and ends the pending state. Name invented. */
    void            Show( void* image );

    unk_TutorialText text;      // 0xC
};

/*
 * Shows a modal tutorial message, its texts set by the attribute packet, when its message
 * arrives and the setting allows it. vtable __vt__28CModalTutorialMessageTrigger, size 0x48.
 * Member names invented.
 */
class CModalTutorialMessageTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented; command 1 is not handled. */
    enum {
        CMD_SetTriggerMsg = 0,  // text: the name of the message that shows the tutorial message
        CMD_SetTitle = 2,       // text: the title
        CMD_SetBody = 3,        // text: the body
        CMD_SetImage = 4        // text: the name of the image
    };

    virtual         ~CModalTutorialMessageTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CC458( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CModalTutorialMessageTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    unk_TutorialText text;          // 0x3C
};

// defined in 0x801D207C, unit not yet reconstructed: the path of the file name with extension
// ext, built in buffer; NULL when there is none. Parameter names invented.
char*   fn_801D207C( const char* name, char* buffer, const char* ext, int a );

/* Nonzero while a modal tutorial message is being shown. */
bool    fn_800CBEF8();
/* Creates the modal tutorial message box once. */
void    fn_800CC25C();

extern unk_TutorialMessageBox* lbl_80529B50;    // the modal tutorial message box
extern u32 lbl_80529B54;                        // class-name hash of "CModalTutorialMessageTrigger"
extern unk_801A8F6C lbl_80529B58;               // type-registry entry of CModalTutorialMessageTrigger
extern bool lbl_80529B59;                       // a modal tutorial message is being shown

#endif
