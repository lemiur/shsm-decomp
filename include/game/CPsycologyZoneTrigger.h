#ifndef GAME_CPSYCOLOGYZONETRIGGER_H
#define GAME_CPSYCOLOGYZONETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

// defined in 0x8005B444, unit not yet reconstructed: the first argument is not read; the name
// is looked up in the save data. Signature and name invented.
void fn_8005B444( CBaseBehaviour* player, const char* name );

/*
 * Passes its zone name on to fn_8005B444 when its message arrives; the zone name and the
 * message are named by the attribute packet. The class name keeps the original spelling
 * ("Psycology"). vtable __vt__21CPsycologyZoneTrigger, size 0x40. Member names invented.
 */
class CPsycologyZoneTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetZoneName,    // text: the zone name
        CMD_SetZoneMsg      // text: the name of the message that triggers the zone
    };

                    CPsycologyZoneTrigger() : zoneName( NULL ) {}
    virtual         ~CPsycologyZoneTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CEB6C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPsycologyZoneTrigger;
    }

    char*           zoneName;   // 0x34, owned copy
    unk_801A92A8    zoneMsg;    // 0x38
};

extern u32 lbl_80529BB0;            // class-name hash of "CPsycologyZoneTrigger"
extern unk_801A8F6C lbl_80529BB4;   // type-registry entry of CPsycologyZoneTrigger

#endif
