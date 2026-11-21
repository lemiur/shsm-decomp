#ifndef GAME_UNK_8009D2E4_H
#define GAME_UNK_8009D2E4_H

#include <game/CBaseBehaviour.h>
#include <game/unk_803E8E64.h>
#include <game/unk_80024158.h>
#include <climax/Clump.h>
#include <climax/Frame.h>
#include <game/Vec4.h>

// C-linkage position accessor at 0x8009CD14: the same body as CBaseBehaviour::fn_8009CD08
// (loads the position from the object's field at 0x30 and returns its vector at 0x40), defined
// in another unit under its address name.
extern "C" const unk_RwV3d* fn_8009CD14( const void* self );

/*
 * The base of the scene behaviours (vtable lbl_803E8D78, size 0xEC, constructors 0x8009D2E4 and
 * 0x8009D41C, destructor 0x8009D718): CBaseBehaviour, the interface unk_803E8E64 at 0x34 and
 * unk_80024158 at 0x3C. fn_801A7BFC and fn_8009ECB0 override slots of CAttributeHandler and
 * unk_803E8E64. The object is 0x144 bytes; its own data is padding here.
 */
class unk_8009D2E4 : public CBaseBehaviour, public unk_803E8E64, public unk_80024158 {
public:
                    unk_8009D2E4( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_8009D2E4();  // override
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    virtual void    fn_8009E6D4();
    virtual void    fn_8009ECB0( unk_Vec4* out );  // override
    virtual void    fn_8009EB8C();
    virtual u32     fn_80017568();
    virtual void    fn_80017564();
    virtual void    fn_80017560();
    virtual void    fn_8009EC0C();

    u8              field_0xA0[0x90];
    unk_RpClump*    field_0x130;    // presumably the behaviour's clump: fn_801DD29C walks its atomics
    u8              field_0x134[0x4];
    u32             field_0x138;    // read by fn_80017568; its writer is not yet identified
    u8              field_0x13C[0x8];
};

#endif
