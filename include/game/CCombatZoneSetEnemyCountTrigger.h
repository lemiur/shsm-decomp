#ifndef GAME_CCOMBATZONESETENEMYCOUNTTRIGGER_H
#define GAME_CCOMBATZONESETENEMYCOUNTTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Messages.h>

/*
 * Posts its enemy count to the combat zone message lbl_80529F18 when its own message arrives;
 * the message and the count are named by the attribute packet.
 * vtable __vt__31CCombatZoneSetEnemyCountTrigger, size 0x40. Member names invented.
 */
class CCombatZoneSetEnemyCountTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetEnemyCountMsg,   // text: the name of the message that triggers the post
        CMD_SetEnemyCount       // integer: the enemy count
    };

                    CCombatZoneSetEnemyCountTrigger() : enemyCount( 0 ) {}
    virtual         ~CCombatZoneSetEnemyCountTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C8C2C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CCombatZoneSetEnemyCountTrigger;
    }

    unk_801A92A8    enemyCountMsg;  // 0x34
    u32             enemyCount;     // 0x3C
};

extern u32 lbl_80529AD8;            // class-name hash of "CCombatZoneSetEnemyCountTrigger"
extern unk_801A8F6C lbl_80529ADC;   // type-registry entry of CCombatZoneSetEnemyCountTrigger

#endif
