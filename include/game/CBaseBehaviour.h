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

// the class id of the behaviour's own id-0 commands (fn_801A73A8 reads them); the static's owner
// is not yet reconstructed. Its word is in .sbss, shared with the neighbouring units.
extern u32 lbl_805297A8;

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

// The registered id of the class CBaseBehaviour, stored by the static initialiser at 0x8009CEEC
// (unit not yet reconstructed; the .sbss word is defined by the split object of the gap).
extern u32 lbl_805297A8;

// Checks the name and value pairs of a packet's commands addressed to CBaseBehaviour against the
// singleton named entry list. Defined in 0x8009C95C; name invented.
bool fn_8009C95C( const unk_AttributePacket& packet, bool flag );
/*
 * C-linkage accessors of the behaviour's frame whose class is not yet known: the object comes in
 * as an untyped pointer (unk_8009D2E4.h and unk_800174B8.cpp call fn_8009CD14 that way).
 * Names are the retail symbols' (no mangling). Placed here, next to the class that owns them.
 */
extern "C" {
// defined in 0x8009CCA0 and 0x8009CCC4 (retail has two copies): writes the position of the frame's
// matrix and updates the matrix
void            fn_8009CCA0( void* self, const unk_RwV3d* position );
void            fn_8009CCC4( void* self, const unk_RwV3d* position );
// defined in 0x8009CD14: the position, as fn_8009CD08 returns it
const unk_RwV3d* fn_8009CD14( const void* self );
// defined in 0x8009CD58: the up vector of the same matrix, copied to out
void            fn_8009CD58( const void* self, unk_Vec3* out );
}

#endif
