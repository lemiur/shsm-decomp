#ifndef GAME_CVOICEMAILMESSAGETRIGGER_H
#define GAME_CVOICEMAILMESSAGETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/unk_MessageInbox.h>

/*
 * Hands a voicemail to the phone's inbox when its message arrives, the voicemail twin of
 * CPhoneMessageTrigger; the message, its four texts and its fifth word are set by the
 * attribute packet (the texts are copied and owned). vtable __vt__24CVoicemailMessageTrigger,
 * size 0x50. Member names invented.
 */
class CVoicemailMessageTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that sends the voicemail
        CMD_SetField3C,     // text: copied to field_0x3C
        CMD_SetField40,     // text: copied to field_0x40
        CMD_SetField44,     // text: copied to field_0x44
        CMD_SetField48,     // text: copied to field_0x48
        CMD_SetField4C      // integer: field_0x4C
    };

                    CVoicemailMessageTrigger() :
                        field_0x3C( NULL ),
                        field_0x40( NULL ),
                        field_0x44( NULL ),
                        field_0x48( NULL ),
                        field_0x4C( 0 ) {}
    /* Hands the voicemail to the phone's inbox. Name invented. */
    void            SendVoicemail() {
                        unk_MessageInfo info( field_0x3C, field_0x40, field_0x44, field_0x48, field_0x4C );
                        lbl_80529D20->field_0x9C.fn_800F5AE8( info );
                    }
    virtual         ~CVoicemailMessageTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800D0A1C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CVoicemailMessageTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    char*           field_0x3C;     // 0x3C, the four texts of the unk_MessageInfo, owned copies
    char*           field_0x40;     // 0x40
    char*           field_0x44;     // 0x44
    char*           field_0x48;     // 0x48
    u32             field_0x4C;     // 0x4C, the fifth word of the unk_MessageInfo
};

extern u32 lbl_80529C00;            // class-name hash of "CVoicemailMessageTrigger"
extern unk_801A8F6C lbl_80529C04;   // type-registry entry of CVoicemailMessageTrigger

#endif
