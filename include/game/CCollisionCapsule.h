#ifndef GAME_CCOLLISIONCAPSULE_H
#define GAME_CCOLLISIONCAPSULE_H

#include <climax/Physics.h>
#include <game/CBaseBehaviour.h>
#include <game/unk_PhysicsManager.h>

/*
 * A physics body, made from its packet's body definition, that its owner places in a space of
 * the physics manager and takes out again; on lbl_8052A790 it stops the body and stands it
 * upright along the owner's facing. Not a CBaseBehaviour: it derives from the same bases, with its
 * own slots (vtable lbl_803DFBB0, size 0x5C). Member names invented.
 */
class CCollisionCapsule : public unk_BehaviourBase, public CAttributeHandler, public CEventHandler {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetField0x30,       // integer: nonzero sets field_0x30
        CMD_SetField0x34,       // float
        CMD_SetField0x38,       // integer: nonzero sets field_0x38
        CMD_SetField0x3C        // float
    };

                    CCollisionCapsule( const unk_AttributePacket& packet );
    virtual         ~CCollisionCapsule();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_8000CD90( unk_RwpSpace* space );
    virtual void    fn_8000CEB8( unk_RwpSpace* space );

    u32             fn_8000CD40( u32 newGroup );
    bool            fn_8000CF40() const;
    void            fn_8000CF54( bool active );

    /* Applies the shape settings to the body's shape. Name invented. */
    void            UpdateShape() {
                        unk_RwpShape* shape = body->parts->shape;
                        if ( field_0x30 ) {
                            fn_80310880( shape, 0.0f );
                        }
                        if ( field_0x38 ) {
                            fn_80310778( shape, field_0x3C );
                        }
                    }

    static CAttributeHandler* fn_8000D038( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CCollisionCapsule( packet );
    }

    bool                field_0x30;     // 0x30
    float               field_0x34;     // 0x34, set by its command and never read
    bool                field_0x38;     // 0x38
    float               field_0x3C;     // 0x3C
    bool                field_0x40;     // 0x40, set by the owner; clear adds the body with flags 7, not 1
    void*               field_0x44;     // 0x44, set by the owner; passed to fn_8000F0F4
    unk_RwpBody*        body;           // 0x48
    unk_RwpWorld*       world;          // 0x4C, the body's world while it is placed; NULL otherwise
    unk_RwpSpace*       space;          // 0x50, the space it was placed in
    const unk_RwMatrix* ownerMatrix;    // 0x54, set by the owner: its frame's modelling matrix
    u32                 group;          // 0x58
};

extern u32 lbl_80529168;            // class-name hash of "CCollisionCapsule"
extern unk_801A8F6C lbl_8052916C;   // type-registry entry of CCollisionCapsule

#endif
