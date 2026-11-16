// CCombatZoneSetEnemyCountTrigger: class name from the registration strings lbl_803EBF78 and lbl_803EBF98.
// The inline factory fn_800C8C2C and the two thunks follow the static initialiser in .text.

#include <game/CCombatZoneSetEnemyCountTrigger.h>

/*
====================
CCombatZoneSetEnemyCountTrigger::~CCombatZoneSetEnemyCountTrigger
====================
*/
// 0x800C8A0C
CCombatZoneSetEnemyCountTrigger::~CCombatZoneSetEnemyCountTrigger() {
    fn_801A9E00( enemyCountMsg );
    fn_801A9940( enemyCountMsg );
}

/*
====================
CCombatZoneSetEnemyCountTrigger::fn_8009CB1C

Posts the enemy count to the combat zone when the message arrives.
====================
*/
// 0x800C8AA0
void CCombatZoneSetEnemyCountTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == enemyCountMsg.record ) {
        PostMsg( lbl_80529F18, &enemyCount );
    }
}

/*
====================
CCombatZoneSetEnemyCountTrigger::fn_801A73A8

Takes the name of the message and the enemy count from the attribute packet.
====================
*/
// 0x800C8B18
void CCombatZoneSetEnemyCountTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AD8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetEnemyCountMsg:
            fn_801AA218( enemyCountMsg, it->GetString(), 0 );
            break;
        case CMD_SetEnemyCount:
            enemyCount = it->GetU32();
            break;
        }
        ++it;
    }
}

u32 lbl_80529AD8 = fn_801A7A84( "CCombatZoneSetEnemyCountTrigger" );
unk_801A8F6C lbl_80529ADC( "CCombatZoneSetEnemyCountTrigger", CCombatZoneSetEnemyCountTrigger::fn_800C8C2C, sizeof( CCombatZoneSetEnemyCountTrigger ) );
