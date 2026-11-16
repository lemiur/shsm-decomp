#ifndef GAME_RESPOTZONETRIGGER_H
#define GAME_RESPOTZONETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Sets the respot zone when its message arrives ("RespotZoneTrigger[%s]: RespotZone set as
 * %s / %s / %s"); the message and the three names are set by the attribute packet. The class
 * name has no C prefix in the registration strings. vtable __vt__17RespotZoneTrigger, size
 * 0x48. Member names invented.
 */
class RespotZoneTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that sets the zone
        CMD_SetField3C,     // text: field_0x3C
        CMD_SetField40,     // text: field_0x40
        CMD_SetField44      // text: field_0x44
    };

                    RespotZoneTrigger() : field_0x3C( NULL ), field_0x40( NULL ), field_0x44( NULL ) {}
    virtual         ~RespotZoneTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CF120( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) RespotZoneTrigger;
    }

    unk_801A92A8    triggerMsg;     // 0x34
    const char*     field_0x3C;     // 0x3C, the three names of the respot zone, in the packet's text
    const char*     field_0x40;     // 0x40, handed to the respot manager third
    const char*     field_0x44;     // 0x44, handed to the respot manager second
};

extern u32 lbl_80529BC0;            // class-name hash of "RespotZoneTrigger"
extern unk_801A8F6C lbl_80529BC4;   // type-registry entry of RespotZoneTrigger

#endif
