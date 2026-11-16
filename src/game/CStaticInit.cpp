// CStaticInit: class name from the registration strings lbl_803ED0E0 and lbl_803ED0EC.
// The inline factory fn_800D0354 and the two thunks follow the static initialiser in .text.

#include <game/CStaticInit.h>
#include <game/Messages.h>

u32 lbl_80529BE0 = fn_801A7A84( "CStaticInit" );
unk_801A8F6C lbl_80529BE4( "CStaticInit", CStaticInit::fn_800D0354, sizeof( CStaticInit ) );

/*
====================
CStaticInit::CStaticInit

Inlined into the factory fn_800D0354; the out-of-line copy is dead-stripped by the linker (no address).
====================
*/
CStaticInit::CStaticInit() {
    fn_801A9DE8( lbl_8052A788 );
}

/*
====================
CStaticInit::~CStaticInit
====================
*/
// 0x800D0144
CStaticInit::~CStaticInit() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9940( outputMsg );
}

/*
====================
CStaticInit::fn_8009CB1C

Sends the output message on the first frame message, then stops listening to the frame.
====================
*/
// 0x800D01D8
void CStaticInit::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        unk_Msg output( outputMsg, NULL );
        if ( output.record != lbl_8052A530.record && output.record->head.next != NULL ) {
            fn_801AA2AC( output );
        }
        fn_801A9E00( lbl_8052A788 );
    }
}

/*
====================
CStaticInit::fn_801A73A8

Takes the name of the output message from the attribute packet.
====================
*/
// 0x800D025C
void CStaticInit::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BE0 );
    while ( !it.IsFinished() ) {
        if ( it->id == CMD_SetOutputMsg ) {
            fn_801AA1C4( outputMsg, it->GetString(), 0 );
        }
        ++it;
    }
}
