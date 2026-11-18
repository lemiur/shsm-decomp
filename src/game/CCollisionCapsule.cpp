// CCollisionCapsule: class name from the registration strings lbl_803DFC0C and lbl_803DFC20.
// CAttributeHandler's inline defaults (0x8000CFA8-0x8000CFD8) are compiled here for the vtable and
// kept by the link as the first copies; the inline factory fn_8000D038 and the two thunks follow
// the static initialiser in .text.

#include <game/CCollisionCapsule.h>
#include <game/CSystemCommands.h>
#include <game/Messages.h>

u32 lbl_80529168 = fn_801A7A84( "CCollisionCapsule" );
unk_801A8F6C lbl_8052916C( "CCollisionCapsule", CCollisionCapsule::fn_8000D038, sizeof( CCollisionCapsule ) );

/*
====================
CCollisionCapsule::CCollisionCapsule
====================
*/
// 0x8000C8A8
CCollisionCapsule::CCollisionCapsule( const unk_AttributePacket& packet ) {
    field_0x30 = true;
    field_0x34 = 0.0f;
    field_0x38 = true;
    field_0x3C = 1.0f;
    field_0x40 = false;
    field_0x44 = NULL;
    world = NULL;
    space = NULL;
    ownerMatrix = NULL;
    body = fn_801AB878( packet );
    group = 0;
    fn_801A9DE8( lbl_8052A790 );
}

/*
====================
CCollisionCapsule::~CCollisionCapsule
====================
*/
// 0x8000C97C
CCollisionCapsule::~CCollisionCapsule() {
    if ( body != NULL ) {
        if ( world != NULL ) {
            fn_8000CEB8( space );
        }
        fn_802FA9D4( body );
    }
    fn_801A9E00( lbl_8052A790 );
}

/*
====================
CCollisionCapsule::fn_8009CB1C

On lbl_8052A790 stops the body and stands it upright, facing along the owner's facing.
====================
*/
// 0x8000CA40
void CCollisionCapsule::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == lbl_8052A790 && body != NULL ) {
        unk_RwV3d zero = { 0.0f, 0.0f, 0.0f };
        unk_RwpStateRef* ref = body->parts->state;
        if ( ref != NULL ) {
            ref->simulation->field_0x10->states[ref->index].field_0x10 = zero;
        }

        unk_RwMatrix* matrix = body->parts->matrix;
        matrix->at = ownerMatrix->at;
        matrix->at.y = 0.0f;
        RwV3dNormalize( &matrix->at, &matrix->at );
        matrix->up.x = 0.0f;
        matrix->up.y = 1.0f;
        matrix->up.z = 0.0f;
        RwV3dCrossProduct( &matrix->right, &matrix->up, &matrix->at );
        RwMatrixUpdate( matrix );
        fn_8032110C( body, 0 );
    }
}

/*
====================
CCollisionCapsule::fn_801A73A8

Moves the body to the matrix the packet places it at, then takes the shape settings.
====================
*/
// 0x8000CB88
void CCollisionCapsule::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_AttributeCommandIterator sys( packet, lbl_805297E8 );
    while ( !sys.IsFinished() ) {
        if ( sys->id == SYSCMD_SetMatrix ) {
            RwpMgrDeactivate( body->world, body );
            fn_8032110C( body, 0 );
            fn_802FAB10( body );
            RwpMgrSetMoving( body->world, body, 0 );
            RwpMgrActivate( body->world, body );
            RwpMgrSetMoving( body->world, body, 1 );
        }
        ++sys;
    }

    unk_AttributeCommandIterator it( packet, lbl_80529168 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetField0x30:
            field_0x30 = it->GetU32() != 0;
            break;
        case CMD_SetField0x38:
            field_0x38 = it->GetU32() != 0;
            break;
        case CMD_SetField0x34:
            field_0x34 = it->GetFloat();
            break;
        case CMD_SetField0x3C:
            field_0x3C = it->GetFloat();
            break;
        }
        UpdateShape();
        ++it;
    }
}

/*
====================
CCollisionCapsule::fn_8000CD40

Sets the body's group, in its world too while it is placed; returns the previous group.
====================
*/
// 0x8000CD40
u32 CCollisionCapsule::fn_8000CD40( u32 newGroup ) {
    u32 oldGroup = group;
    group = newGroup;
    if ( body->world != NULL ) {
        RwpMgrSetGroup( body->world, body, newGroup );
    }
    return oldGroup;
}

/*
====================
CCollisionCapsule::fn_8000CD90

Places the body in newSpace when the physics manager has that space registered.
====================
*/
// 0x8000CD90
void CCollisionCapsule::fn_8000CD90( unk_RwpSpace* newSpace ) {
    if ( world == NULL ) {
        if ( newSpace != NULL && lbl_80529170->FindSpace( newSpace ) != NULL ) {
            u32 flags = 1;
            if ( !field_0x40 ) {
                flags |= 6;
            }
            lbl_80529170->fn_8000F5E4( newSpace, body, -1, flags );
            lbl_80529170->fn_8000F0F4( body, field_0x44 );
            RwpMgrSetGroup( body->world, body, group );
            UpdateShape();
            world = body->world;
            space = newSpace;
        } else {
            space = NULL;
        }
    }
}

/*
====================
CCollisionCapsule::fn_8000CEB8

Takes the body out of its world and clears its contacts.
====================
*/
// 0x8000CEB8
void CCollisionCapsule::fn_8000CEB8( unk_RwpSpace* ) {
    if ( world != NULL ) {
        lbl_80529170->fn_8000F278( body );
        RwpMgrSetGroup( body->world, body, 0 );
        lbl_80529170->fn_8000F6AC( body );
        unk_RwpContact* contact;
        unk_RwpBody* placedBody = body;
        for ( contact = placedBody->contacts; contact != NULL; contact = contact->next ) {
            contact->field_0x0 = NULL;
        }
        placedBody->contacts = NULL;
        world = NULL;
    }
}

/*
====================
CCollisionCapsule::fn_8000CF40

Whether the body is placed.
====================
*/
// 0x8000CF40
bool CCollisionCapsule::fn_8000CF40() const {
    return world != NULL;
}

/*
====================
CCollisionCapsule::fn_8000CF54

Places the body in its space or takes it out.
====================
*/
// 0x8000CF54
void CCollisionCapsule::fn_8000CF54( bool active ) {
    if ( active && !fn_8000CF40() ) {
        fn_8000CD90( space );
    } else if ( !active && fn_8000CF40() ) {
        fn_8000CEB8( space );
    }
}
