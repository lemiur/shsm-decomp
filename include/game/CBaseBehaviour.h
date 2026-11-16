#ifndef GAME_CBASEBEHAVIOUR_H
#define GAME_CBASEBEHAVIOUR_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Frame.h>

/*
 * The empty, non-polymorphic first base of CBaseBehaviour. Its only evidence is the layout:
 * with it, MWCC places CBaseBehaviour's own vptr at 0x2C instead of sharing CAttributeHandler's.
 * Name invented.
 */
class unk_BehaviourBase {
};

/*
 * The base of the game behaviours (vtable lbl_803E8B78, constructor 0x8009C700, destructor
 * 0x8009C784, size 0x34; unit 0x8009C700-0x8009CF20 not yet reconstructed). Its vtable holds
 * its own vptr's header, the CAttributeHandler block, the CEventHandler block, then the slots
 * below in declaration order.
 */
class CBaseBehaviour : public unk_BehaviourBase, public CAttributeHandler, public CEventHandler {
public:
                    CBaseBehaviour();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~CBaseBehaviour();
    virtual void    fn_8009CE2C( const void* matrix );

    // defined in 0x8009CD08 and 0x8009CD78: the position and the facing ("at") vector of the
    // modelling matrix of the frame at field_0x30 (0x40 and 0x30 into it)
    const unk_RwV3d* fn_8009CD08() const;
    const unk_RwV3d* fn_8009CD78() const;
    // defined in 0x8009CD20 and 0x8009CD4C: the right and up vectors of the same matrix (0x10 and
    // 0x20 into the frame)
    const unk_RwV3d* fn_8009CD20() const;
    const unk_RwV3d* fn_8009CD4C() const;
    // defined in 0x8009CCE8, 0x8009CD2C and 0x8009CD84: copy the position, the right and the at
    // vector of the same matrix to out
    void            fn_8009CCE8( unk_Vec3* out ) const;
    void            fn_8009CD2C( unk_Vec3* out ) const;
    void            fn_8009CD84( unk_Vec3* out ) const;
    // defined in 0x8009CDA4: copies the modelling matrix of the frame at field_0x30 to matrix
    void            fn_8009CDA4( unk_RwMatrix* matrix ) const;

    unk_RwFrame*    field_0x30;     // the behaviour's frame
};

#endif
