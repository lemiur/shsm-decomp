// CMapLayer: class name from the registration strings lbl_803FCD38 and lbl_803FCD44.
// The inline factory fn_8018ADA0 and the two thunks follow the static initialiser in .text.

#include <game/CMapLayer.h>
#include <string.h>

u32 lbl_8052A3E0 = fn_801A7A84( "CMapLayer" );
unk_801A8F6C lbl_8052A3E4( "CMapLayer", CMapLayer::fn_8018ADA0, sizeof( CMapLayer ) );

/*
====================
CMapLayer::~CMapLayer
====================
*/
// 0x8018AB64
CMapLayer::~CMapLayer() {
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
}

/*
====================
CMapLayer::fn_8009CB1C

Hands the layer's name and the flag to the map when the message arrives.
====================
*/
// 0x8018ABF8
void CMapLayer::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == triggerMsg.record ) {
        const char* layer = name;
        if ( layer != NULL && strlen( layer ) != 0 ) {
            lbl_8052A3F0->fn_8018CFCC( layer, flag );
        }
    }
}

/*
====================
CMapLayer::fn_801A73A8

Takes the message, the name and the flag from the attribute packet.
====================
*/
// 0x8018AC6C
void CMapLayer::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_8052A3E0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetFlag:
            flag = it->GetU32() != 0;
            break;
        }
        ++it;
    }
}
