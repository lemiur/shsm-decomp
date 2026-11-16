// CFlickerTrigger: class name from the registration strings lbl_803EC330 and lbl_803EC340.
// The inline factory fn_800C9F9C and the two thunks follow the static initialiser in .text.

#include <game/CFlickerTrigger.h>
#include <game/Log.h>

/*
====================
CFlickerTrigger::~CFlickerTrigger
====================
*/
// 0x800C9CA8
CFlickerTrigger::~CFlickerTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    fn_801A9940( sendMsg );
}

/*
====================
CFlickerTrigger::fn_801A73A8

Takes the names of the two messages and the flicker settings from the attribute packet.
====================
*/
// 0x800C9D50
void CFlickerTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B08 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetSendMsg:
            fn_801AA1C4( sendMsg, it->GetString(), 0 );
            break;
        case CMD_SetRate:
            params.rate = it->GetFloat();
            break;
        case CMD_SetIntensityFrom:
            params.intensityFrom = it->GetFloat();
            break;
        case CMD_SetIntensityTo:
            params.intensityTo = it->GetFloat();
            break;
        case CMD_SetBias:
            params.bias = it->GetFloat();
            break;
        case CMD_SetLowTime:
            params.lowTime = it->GetFloat();
            break;
        case CMD_SetLowRate:
            params.lowRate = it->GetFloat();
            break;
        case CMD_SetTime:
            params.time = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
CFlickerTrigger::fn_8009CB1C

Sends the flicker settings when the trigger message arrives.
====================
*/
// 0x800C9E6C
void CFlickerTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        const char* triggerName = triggerMsg.record != NULL ? triggerMsg.record->name : NULL;
        const char* sendName = sendMsg.record != NULL ? sendMsg.record->name : NULL;
        fn_800A65B0( "Flicker Trigger (%s) Sends %s (Rate=%0.3f, Intensity=%0.3f->%0.3f, Bias=%0.3f, Time=%0.3f, LowT/R=%0.3f/%0.3f)",
                     triggerName, sendName,
                     params.rate, params.intensityFrom, params.intensityTo, params.bias, params.time,
                     params.lowTime, params.lowRate );
        PostMsg( sendMsg, &params );
    }
}

u32 lbl_80529B08 = fn_801A7A84( "CFlickerTrigger" );
unk_801A8F6C lbl_80529B0C( "CFlickerTrigger", CFlickerTrigger::fn_800C9F9C, sizeof( CFlickerTrigger ) );
