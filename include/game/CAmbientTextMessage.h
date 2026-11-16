#ifndef GAME_CAMBIENTTEXTMESSAGE_H
#define GAME_CAMBIENTTEXTMESSAGE_H

#include <game/CAmbientInterferencePoint.h>
#include <game/unk_MessageInbox.h>

/*
 * An ambient interference point that delivers a text message (vtable lbl_803EB3C8,
 * registered as "CAmbientTextMessage").
 */
class CAmbientTextMessage : public CAmbientInterferencePoint {
public:
    /* Attribute commands, one per message member. Names invented. */
    enum {
        CMD_SetFieldE8,
        CMD_SetFieldEC,
        CMD_SetFieldF0,
        CMD_SetFieldF4,
        CMD_SetFieldF8
    };

                    CAmbientTextMessage( const unk_AttributePacket& packet );
    virtual         ~CAmbientTextMessage();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual int     fn_800C2790();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C2F54( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CAmbientTextMessage( packet );
    }

    char*   field_0xE8;     // the message, set by the CMD_ commands
    char*   field_0xEC;     // also handed to CAmbientInterferencePoint::fn_800C2A0C
    char*   field_0xF0;
    char*   field_0xF4;
    u32     field_0xF8;
};

extern u32 lbl_80529A50;            // class id of "CAmbientTextMessage"
extern unk_801A8F6C lbl_80529A54;

#endif
