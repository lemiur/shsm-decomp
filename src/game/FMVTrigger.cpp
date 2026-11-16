// FMVTrigger: class name from the registration strings lbl_803EC3A8 and lbl_803EC3B4.
// The inline factory fn_800CA280 and the two thunks follow the static initialiser in .text.

#include <game/FMVTrigger.h>
#include <game/DisplayHeight.h>

/*
====================
FMVTrigger::~FMVTrigger
====================
*/
// 0x800CA038
FMVTrigger::~FMVTrigger() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    if ( film != NULL ) {
        delete film;
        film = NULL;
    }
}

/*
====================
FMVTrigger::fn_8009CB1C

Passes the film name on when the message arrives.
====================
*/
// 0x800CA0E4
void FMVTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        lbl_80529260->fn_8002F808( film );
    }
}

/*
====================
FMVTrigger::fn_801A73A8

Takes the name of the message and of the film from the attribute packet.
====================
*/
// 0x800CA138
void FMVTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B18 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetFilm:
            film = fn_801B67FC( it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

u32 lbl_80529B18 = fn_801A7A84( "FMVTrigger" );
unk_801A8F6C lbl_80529B1C( "FMVTrigger", FMVTrigger::fn_800CA280, sizeof( FMVTrigger ) );
