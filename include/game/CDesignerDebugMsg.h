#ifndef GAME_CDESIGNERDEBUGMSG_H
#define GAME_CDESIGNERDEBUGMSG_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Shows a line of designer debug text near the bottom of the screen for 100 frames when its
 * message arrives, fading it out over the last 25; a newer message replaces the one shown.
 * The message and the text are named by the attribute packet.
 * vtable __vt__17CDesignerDebugMsg, size 0x44. Member names invented.
 */
class CDesignerDebugMsg : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetShowMsg,     // text: the name of the message that shows the text
        CMD_SetText         // text: the text shown
    };

    enum {
        SHOW_FRAMES = 100,  // frames the text is shown for
        FADE_FRAMES = 25    // the last frames, in which it fades out
    };

                    CDesignerDebugMsg() : text( NULL ), framesLeft( SHOW_FRAMES ) {}
    virtual         ~CDesignerDebugMsg();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C97C0( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CDesignerDebugMsg;
    }

    unk_801A92A8    showMsg;    // 0x34
    char*           text;       // 0x3C, owned copy
    int             framesLeft; // 0x40

private:
    void            Show();
    void            Hide();
    void            Draw();
};

extern CDesignerDebugMsg* lbl_80529AF0; // the message shown, NULL if none
extern u32 lbl_80529AF4;                // class-name hash of "CDesignerDebugMsg"
extern unk_801A8F6C lbl_80529AF8;       // type-registry entry of CDesignerDebugMsg

#endif
