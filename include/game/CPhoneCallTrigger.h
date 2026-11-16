#ifndef GAME_CPHONECALLTRIGGER_H
#define GAME_CPHONECALLTRIGGER_H

// unit name unconfirmed: the class name is evidenced by the registration strings lbl_803ECAE0 and
// lbl_803ECAF4; the file name is invented.

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CPlayer.h>
#include <game/unk_80491B38.h>
#include <game/unk_MessageInbox.h>

/*
 * Starts a phone call on the phone's inbox when its message arrives, like CPhoneMessageTrigger
 * does for a text message; the call's texts and numbers are set by the attribute packet (the
 * texts are copied and owned). Two more messages act on the inbox, and fn_800CD830 (called from
 * outside) ends the call. vtable __vt__17CPhoneCallTrigger, size 0x58. Member names invented.
 */
class CPhoneCallTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that starts the call
        CMD_SetField3C,     // text: the name of the message posted when the call is ended
        CMD_SetField44,     // text: the name of a message that passes field_0x80 to the inbox
        CMD_SetField54,     // text: copied to field_0x54
        CMD_SetField58,     // text: copied to field_0x58
        CMD_SetField5C,     // text: copied to field_0x5C
        CMD_SetField60,     // integer: field_0x60
        CMD_SetField64,     // integer: field_0x64
        CMD_SetField68,     // integer: field_0x68
        CMD_SetField6C,     // integer: field_0x6C
        CMD_SetField4C,     // text: the name of a message that acts on the inbox
        CMD_SetField78,     // text: copied to field_0x78
        CMD_SetField7C,     // integer: field_0x7C
        CMD_SetField80      // integer: nonzero sets field_0x80
    };

                    CPhoneCallTrigger( const unk_AttributePacket& packet );
    /* Hands the call to the phone's inbox. Name invented. */
    void            StartCall() {
                        unk_CallInfo info( field_0x54, field_0x5C, field_0x58, field_0x60, field_0x64,
                                           field_0x68, field_0x6C, field_0x78, field_0x7C );
                        lbl_80529D20->field_0x9C.fn_800F5EE0( info, this );
                    }
    virtual         ~CPhoneCallTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    void            fn_800CD830();

    static CAttributeHandler* fn_800CD940( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPhoneCallTrigger( packet );
    }

    unk_801A92A8    triggerMsg;     // 0x34
    unk_801A92A8    field_0x3C;     // 0x3C
    unk_801A92A8    field_0x44;     // 0x44
    unk_801A92A8    field_0x4C;     // 0x4C
    char*           field_0x54;     // 0x54, owned copy of a text of the unk_CallInfo
    char*           field_0x58;     // 0x58, owned copy
    char*           field_0x5C;     // 0x5C, owned copy
    u32             field_0x60;     // 0x60, 15 until set
    u32             field_0x64;     // 0x64
    u32             field_0x68;     // 0x68
    u32             field_0x6C;     // 0x6C
    int             callStarted;    // 0x70, set once the inbox has the call
    int             callEnded;      // 0x74, set when the call is ended
    char*           field_0x78;     // 0x78, owned copy
    u32             field_0x7C;     // 0x7C
    bool            field_0x80;     // 0x80
};

extern u32 lbl_80529B88;            // class-name hash of "CPhoneCallTrigger"
extern unk_801A8F6C lbl_80529B8C;   // type-registry entry of CPhoneCallTrigger

#endif
