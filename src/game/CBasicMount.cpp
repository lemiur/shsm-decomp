// CBasicMount: class name from the registration strings lbl_803E8C48 and lbl_803E8C54.
// The inline factory fn_8009D28C and the two thunks follow the static initialiser in .text.

#include <game/CBasicMount.h>
#include <game/Messages.h>

u32 lbl_805297B0 = fn_801A7A84( "CBasicMount" );
unk_801A8F6C lbl_805297B4( "CBasicMount", CBasicMount::fn_8009D28C, sizeof( CBasicMount ) );

/*
====================
CBasicMount::CBasicMount

Takes nothing from the attribute packet.
====================
*/
// 0x8009CF20
CBasicMount::CBasicMount( const unk_AttributePacket& packet ) {
    fn_801A9DE8( lbl_8052A788 );
    RwMatrixSetIdentity( &mount.offset );
    mount.field_0x80 = -1;
    mount.owner = this;
    mount.field_0x88 = 0;
    mount.field_0x8C = true;
    mount.field_0x90 = 0xFF;
    mount.field_0x94 = 0;
}

/*
====================
CBasicMount::~CBasicMount
====================
*/
// 0x8009D000
CBasicMount::~CBasicMount() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9940( mountMsg );
}

/*
====================
CBasicMount::fn_8009CB1C

On each frame message, takes the frame's matrix and sends the mount point.
====================
*/
// 0x8009D094
void CBasicMount::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        mount.matrix = *RwFrameGetMatrix( field_0x30 );
        PostMsg( mountMsg, &mount );
    }
}

/*
====================
CBasicMount::fn_801A73A8

Takes the name of the mount message from the attribute packet.
====================
*/
// 0x8009D190
void CBasicMount::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_805297B0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetMountMsg:
            fn_801AA1C4( mountMsg, it->GetString(), "s_Mount*" );
            break;
        }
        ++it;
    }
}
