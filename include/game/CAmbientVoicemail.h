#ifndef GAME_CAMBIENTVOICEMAIL_H
#define GAME_CAMBIENTVOICEMAIL_H

#include <game/CAmbientInterferencePoint.h>
#include <game/unk_MessageInbox.h>

/*
 * An ambient behaviour that delivers a voicemail to the phone (vtable lbl_803EB458,
 * registered as "CAmbientVoicemail"). Its five members are set by attribute commands.
 */
class CAmbientVoicemail : public CAmbientInterferencePoint {
public:
    /* Attribute commands, one per member. Names invented. */
    enum {
        CMD_SetFieldE8,
        CMD_SetFieldEC,
        CMD_SetFieldF0,
        CMD_SetFieldF4,
        CMD_SetFieldF8
    };

                    CAmbientVoicemail( const unk_AttributePacket& packet );
    virtual         ~CAmbientVoicemail();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual int     fn_800C2790();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler*   fn_800C32FC( const unk_AttributePacket& packet );

    char*   field_0xE8;     // the four strings are owned
    char*   field_0xEC;
    char*   field_0xF0;
    char*   field_0xF4;
    u32     field_0xF8;
};

extern u32          lbl_80529A60;   // class id of "CAmbientVoicemail"
extern unk_801A8F6C lbl_80529A64;   // registration of "CAmbientVoicemail"

/*
====================
CAmbientVoicemail::CAmbientVoicemail
====================
*/
inline CAmbientVoicemail::CAmbientVoicemail( const unk_AttributePacket& packet ) : CAmbientInterferencePoint( packet ) {
    field_0xE8 = 0;
    field_0xEC = 0;
    field_0xF0 = 0;
    field_0xF4 = 0;
    field_0xF8 = 0;
}

/*
====================
CAmbientVoicemail::fn_800C32FC

The factory registered with the class; emitted after the static initialiser, as an inline
function whose address is taken. The heap arguments are those of every behaviour factory.
====================
*/
inline CAttributeHandler* CAmbientVoicemail::fn_800C32FC( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) CAmbientVoicemail( packet );
}

#endif
