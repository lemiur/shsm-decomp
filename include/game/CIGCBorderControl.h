#ifndef GAME_CIGCBORDERCONTROL_H
#define GAME_CIGCBORDERCONTROL_H

#include <climax/EventHandler.h>
#include <game/CBaseBehaviour.h>

extern u32 lbl_80529850;                // class-name hash of "CIGCBorderControl"
extern unk_801A8F6C lbl_80529854;       // type-registry entry of CIGCBorderControl
extern unk_801A92A8 lbl_8052A828;       // the per-frame message the border state fades on (defined elsewhere)

/*
 * The border state: one per game, created on first use and kept in lbl_8052984C. It is an
 * event handler of its own (a 0x10-byte object, not a behaviour) and fades the black border
 * in (direction 1.0) or out (direction -1.0) on the per-frame message. Name invented.
 * vtable lbl_803E9BA0.
 */
class unk_BorderState : public CEventHandler {
public:
                    unk_BorderState() : direction( 0.0f ), alpha( 0.0f ) {}
    virtual         ~unk_BorderState();
    virtual void    fn_8009CB1C( unk_Msg& msg );

    float           direction;      // 0x8: 1.0 while the border is wanted on, -1.0 off, 0.0 when settled
    float           alpha;          // 0xC: how far the border has come in, 0.0 to 1.0
};

extern unk_BorderState* lbl_8052984C;   // the border state, NULL until first created

/* Creates the border state if there is none yet. Name invented. */
void    fn_800A8644();

/*
 * The border behaviour: keeps the ON and OFF messages named by its attributes and turns the
 * border on or off when they arrive. vtable __vt__15CIGCBorderControl, size 0x44. Names invented.
 */
class CIGCBorderControl : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetOnMsg,           // text: the name of the message that turns the border on
        CMD_SetOffMsg           // text: the name of the message that turns the border off
    };

                    CIGCBorderControl() {
                        fn_800A8644();
                    }
    virtual         ~CIGCBorderControl();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800A8F88( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CIGCBorderControl;
    }

    unk_801A92A8    ids[2];         // 0x34: the ON message, then the OFF message
};

/* Sets the border direction: on, or off. Name invented. */
void    fn_800A86A8( bool on );

#endif
