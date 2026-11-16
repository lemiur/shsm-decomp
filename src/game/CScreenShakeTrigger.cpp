// CScreenShakeTrigger: class name from the registration strings lbl_803ECF60 and lbl_803ECF74.
// The inline factory fn_800CF49C and the two thunks follow the static initialiser in .text.

#include <game/CScreenShakeTrigger.h>
#include <game/Log.h>
#include <game/Messages.h>

/*
====================
CScreenShakeTrigger::~CScreenShakeTrigger
====================
*/
// 0x800CF1AC
CScreenShakeTrigger::~CScreenShakeTrigger() {
    fn_801A9E00( shakeMsg );
    fn_801A9940( shakeMsg );
}

/*
====================
CScreenShakeTrigger::fn_8009CB1C

Posts the shake when the message arrives.
====================
*/
// 0x800CF240
void CScreenShakeTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == shakeMsg.record ) {
        fn_80028D0C( 2, "CScreenShakeTrigger[%s]: Recieved %s", NULL, shakeMsg.record != NULL ? shakeMsg.record->name : NULL );
        unk_ScreenShakeData data;
        data.field_0x4 = field_0x44;
        data.field_0x0 = field_0x40;
        data.field_0x8 = field_0x3C;
        data.field_0x10 = field_0x4C;
        data.field_0xC = field_0x50;
        PostMsg( lbl_80529E70, &data );
        if ( oneShot ) {
            fn_801A9E00( shakeMsg );
        }
    }
}

/*
====================
CScreenShakeTrigger::fn_801A73A8

Takes the name of the message and the shake values from the attribute packet.
====================
*/
// 0x800CF324
void CScreenShakeTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BC8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetShakeMsg:
            fn_801AA218( shakeMsg, it->GetString(), 0 );
            break;
        case CMD_SetField3C:
            field_0x3C = it->GetFloat();
            break;
        case CMD_SetField40:
            field_0x40 = it->GetFloat();
            break;
        case CMD_SetField44:
            field_0x44 = it->GetFloat();
            break;
        case CMD_SetOneShot:
            oneShot = it->GetU32();
            break;
        case CMD_SetField4C:
            field_0x4C = it->GetU32();
            break;
        case CMD_SetField50:
            field_0x50 = it->GetFloat();
            break;
        }
        ++it;
    }
}

u32 lbl_80529BC8 = fn_801A7A84( "CScreenShakeTrigger" );
unk_801A8F6C lbl_80529BCC( "CScreenShakeTrigger", CScreenShakeTrigger::fn_800CF49C, sizeof( CScreenShakeTrigger ) );
