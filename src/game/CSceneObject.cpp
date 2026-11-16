// CSceneObject: class name from the registration strings lbl_803E95FC and lbl_803E960C.
// The inline factory fn_800A5860 and the thunks follow the static initialiser in .text.

#include <game/CSceneObject.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>

u32 lbl_80529810 = fn_801A7A84( "CSceneObject" );
unk_801A8F6C lbl_80529814( "CSceneObject", CSceneObject::fn_800A5860, sizeof( CSceneObject ) );

/*
====================
CSceneObject::CSceneObject
====================
*/
// 0x800A5670
CSceneObject::CSceneObject( const unk_AttributePacket& packet ) : unk_8009D2E4( packet ) {
    fn_80024C74( 0.0f );
    bounds = fn_80025674( fn_80024A14() );
}

/*
====================
CSceneObject::~CSceneObject
====================
*/
// 0x800A56E8
CSceneObject::~CSceneObject() {
    if ( bounds != NULL ) {
        delete bounds;
        bounds = NULL;
    }
}

/*
====================
CSceneObject::fn_8009CB1C

Passes the message on to the unk_80024158 base, then refits the bounds on the frame message.
====================
*/
// 0x800A5790
void CSceneObject::fn_8009CB1C( unk_Msg& msg ) {
    unk_80024158::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        unk_80491B38& playerRecord = GetPlayerRecord();
        if ( bounds != NULL && playerRecord.field_0x1C != 0 ) {
            bounds->fn_80025750();
        }
    }
}
