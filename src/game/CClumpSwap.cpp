// CClumpSwap: class name from the registration strings lbl_803E8F28 and lbl_803E8F34.
// The inline factory fn_8009EF28 and the destructor thunk follow the static initialiser in .text.

#include <game/CClumpSwap.h>
#include <game/unk_ClumpSwapList.h>

u32 lbl_805297C8 = fn_801A7A84( "CClumpSwap" );
unk_801A8F6C lbl_805297CC( "CClumpSwap", CClumpSwap::fn_8009EF28, sizeof( CClumpSwap ) );

/*
====================
CClumpSwap::CClumpSwap
====================
*/
// 0x8009ED10
CClumpSwap::CClumpSwap( const unk_AttributePacket& packet ) : clump( NULL ), name( NULL ) {
}

/*
====================
CClumpSwap::~CClumpSwap

Drops the swap from the list and destroys the copy of the clump.
====================
*/
// 0x8009ED68
CClumpSwap::~CClumpSwap() {
    lbl_805297D0->fn_8009F1A0( name );
    if ( clump != NULL ) {
        RpClumpDestroy( clump );
        clump = NULL;
    }
}

/*
====================
CClumpSwap::fn_801A73A8

Copies the packet's clump and takes the name from the attribute packet.
====================
*/
// 0x8009EE04
void CClumpSwap::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_RpClump* source = static_cast< unk_RpClump* >( fn_800A275C( packet, "rwID_CLUMP" ) );
    if ( source != NULL ) {
        clump = RpClumpClone( source );
    }
    unk_AttributeCommandIterator it( packet, lbl_805297C8 );
    while ( !it.IsFinished() ) {
        if ( it->id == CMD_SetName ) {
            name = it->GetString();
        }
        ++it;
    }
}

/*
====================
CClumpSwap::fn_801A7A88

Files the clump in the clump swap list under its name.
====================
*/
// 0x8009EEB4
void CClumpSwap::fn_801A7A88() {
    lbl_805297D0->fn_8009EFE8( clump, name );
}
