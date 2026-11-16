// CCounter: class name from the registration strings lbl_803EC010 and lbl_803EC01C.
// The inline factory fn_800C9034 and the two thunks follow the static initialiser in .text.

#include <game/CCounter.h>
#include <game/Log.h>

u32 lbl_80529AE0 = fn_801A7A84( "CCounter" );
unk_801A8F6C lbl_80529AE4( "CCounter", CCounter::fn_800C9034, sizeof( CCounter ) );

/*
====================
CCounter::Reset

Name invented.
====================
*/
inline void CCounter::Reset() {
    count = 0;
    counting = true;
    fn_800A65B0( "CCounter[%s]: Reset", NULL );
}

/*
====================
CCounter::Count

Name invented.
====================
*/
inline void CCounter::Count() {
    if ( counting ) {
        fn_800A65B0( "CCounter[%s]: Count %i/%i", NULL, ++count, target );
        if ( count >= target ) {
            CountReached();
        }
    }
}

/*
====================
CCounter::CountReached

Name invented.
====================
*/
inline void CCounter::CountReached() {
    fn_800A65B0( "CCounter[%s]: CountReached", NULL );
    Send();
    if ( oneShot ) {
        counting = false;
    }
    count = 0;
}

/*
====================
CCounter::Send

Name invented.
====================
*/
inline void CCounter::Send() {
    if ( outputMsg.registrations > 0 ) {
        PostMsg( outputMsg, NULL );
        fn_80028D0C( 4, "CCounter[%s]: Sends %s", NULL, outputMsg.record != NULL ? outputMsg.record->name : NULL );
    }
}

/*
====================
CCounter::~CCounter
====================
*/
// 0x800C8CB0
CCounter::~CCounter() {
    fn_801A9E00( countMsg );
    fn_801A9940( countMsg );
    fn_801A9940( outputMsg );
    fn_801A9E00( resetMsg );
    fn_801A9940( resetMsg );
}

/*
====================
CCounter::fn_8009CB1C

Counts the count message while counting, sending the output message when the target is
reached; the reset message clears the count and resumes counting.
====================
*/
// 0x800C8D78
void CCounter::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == countMsg.record ) {
        Count();
    } else if ( msg.record == resetMsg.record ) {
        Reset();
    }
}

/*
====================
CCounter::fn_801A73A8

Takes the names of the three messages, the target and the one-shot flag from the attribute packet.
====================
*/
// 0x800C8ED0
void CCounter::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AE0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetCountMsg:
            fn_801AA218( countMsg, it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg:
            fn_801AA1C4( outputMsg, it->GetString(), 0 );
            break;
        case CMD_SetResetMsg:
            fn_801AA218( resetMsg, it->GetString(), 0 );
            break;
        case CMD_SetTarget:
            target = it->GetU32();
            break;
        case CMD_SetOneShot:
            oneShot = it->GetU32();
            break;
        }
        ++it;
    }
}
