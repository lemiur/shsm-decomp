// CScalingObject: class name from the registration strings lbl_803E94EC and lbl_803E94FC.
// The inline factory fn_800A55F8 and the thunks follow the static initialiser in .text.

#include <game/CScalingObject.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>
#include <game/VectorPS.h>
#include <math.h>

u32 lbl_80529808 = fn_801A7A84( "CScalingObject" );
unk_801A8F6C lbl_8052980C( "CScalingObject", CScalingObject::fn_800A55F8, sizeof( CScalingObject ) );

/*
====================
CScalingObject::CScalingObject
====================
*/
// 0x800A4F64
CScalingObject::CScalingObject( const unk_AttributePacket& packet ) :
    CSceneObject( packet ),
    field_0x148( 0.5f ),
    field_0x14C( 1.0f ),
    field_0x150( 3.0f ),
    field_0x154( 1.0f ),
    field_0x158( false ),
    field_0x15C( 1.0f ),
    field_0x1A0( 1.0f ),
    field_0x1A4( 70.0f ),
    field_0x1B0( false )
{
    fn_801A9DE8( lbl_8052A788 );
}

/*
====================
CScalingObject::~CScalingObject
====================
*/
// 0x800A5024
CScalingObject::~CScalingObject() {
    fn_801A9940( field_0x1A8 );
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CScalingObject::fn_8000CFD0

Keeps the frame's matrix as it is.
====================
*/
// 0x800A50CC
void CScalingObject::fn_8000CFD0() {
    field_0x160 = field_0x30->modelling;
}

/*
====================
CScalingObject::fn_801A73A8
====================
*/
// 0x800A5154
void CScalingObject::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_8009D2E4::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529808 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetField0x148:
            field_0x148 = it->GetFloat();
            break;
        case CMD_SetField0x14C:
            field_0x14C = it->GetFloat();
            break;
        case CMD_SetField0x150:
            field_0x150 = it->GetFloat();
            break;
        case CMD_SetField0x15C:
            field_0x15C = it->GetFloat();
            break;
        case CMD_SetField0x1A4:
            field_0x1A4 = it->GetFloat();
            break;
        case CMD_SetMsg:
            fn_801AA1C4( field_0x1A8, it->GetString(), NULL );
            break;
        }
        ++it;
    }
}

/*
====================
CScalingObject::fn_8009CB1C

Runs the scaling on the frame message, then the scene object's own handling.
====================
*/
// 0x800A5254
void CScalingObject::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == lbl_8052A788.record ) {
        fn_800A52C4( lbl_80491B38.field_0x8, lbl_80491B38.field_0x14 );
    }
    CSceneObject::fn_8009CB1C( msg );
}

/*
====================
SubtractVec

Subtracts b from a in place. Taking b by value puts the temporary copy in the inlined-locals
region (0x10, below scale at 0x1C), matching retail's stack order. Name invented.
====================
*/
static inline void SubtractVec( unk_Vector3& a, unk_Vector3 b ) {
    V3dSub( &a, &a, &b );
}

/*
====================
CScalingObject::fn_800A52C4

Sets the scale from the distance of the player's position (not its offset from this object's: the
offset and its horizontal part, built first, are never read) beyond field_0x150, posts the message
once when that distance passes field_0x1A4, and scales the frame's matrix by it.
====================
*/
// 0x800A52C4
void CScalingObject::fn_800A52C4( float frameTime, CPlayer* player ) {
    if ( GetZone() == player->GetZone() ) {
        unk_Vector3 offset = *player->fn_8009CD08();
        SubtractVec( offset, *fn_8009CD08() );
        unk_Vector3 flat;
        flat.x = offset.x;
        flat.y = 0.0f;
        flat.z = offset.z;
        unk_Vector3 playerPos = *player->fn_8009CD08();
        float distance = V3dLength( &playerPos ) - field_0x150;
        if ( distance < 0.0f ) {
            distance = 0.0f;
        }

        if ( !field_0x1B0 && distance > field_0x1A4 && GetMsgName( field_0x1A8 ) != NULL &&
             *GetMsgName( field_0x1A8 ) != '\0' ) {
            PostRegisteredMsg( field_0x1A8, NULL );
            field_0x1B0 = true;
        }

        if ( field_0x158 ) {
            field_0x1A0 = field_0x154;
        } else {
            float growth = pow( distance * field_0x148, field_0x14C );
            field_0x1A0 = field_0x15C + growth;
        }

        unk_RwV3d scale = { field_0x1A0, field_0x1A0, field_0x1A0 };
        unk_RwMatrix matrix;
        matrix = field_0x160;
        RwMatrixScale( &matrix, &scale, rwCOMBINEPRECONCAT );
        fn_8009CE2C( &matrix );
        RwFrameUpdateObjects( field_0x30 );
    }
}
