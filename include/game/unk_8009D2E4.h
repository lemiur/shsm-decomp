#ifndef GAME_UNK_8009D2E4_H
#define GAME_UNK_8009D2E4_H

#include <game/CBaseBehaviour.h>
#include <game/unk_803E8E64.h>
#include <game/unk_80024158.h>
#include <climax/Clump.h>
#include <climax/Frame.h>
#include <game/Vec4.h>

class unk_8009D2E4;

// C-linkage position accessor at 0x8009CD14: the same body as CBaseBehaviour::fn_8009CD08
// (loads the position from the object's field at 0x30 and returns its vector at 0x40), defined
// in another unit under its address name.
extern "C" const unk_RwV3d* fn_8009CD14( const void* self );

// Clump-world helpers in another unit (0x80027738, 0x80027740): add the clump to the zone's
// world, and remove it from the world it is in (the zone argument is not read there).
extern "C" {
    void    fn_80027738( CZone* zone, unk_RpClump* clump );
    void    fn_80027740( CZone* zone, unk_RpClump* clump );
    // 0x8009C838, unit not yet reconstructed (an assembly function in the retail build): the
    // behaviour's setup step for the clump's root frame
    void    fn_8009C838( unk_8009D2E4* self, unk_RwFrame* frame );
}

/*
 * The base of the scene behaviours (vtable lbl_803E8D78, size 0xEC, constructors 0x8009D2E4 and
 * 0x8009D41C, destructor 0x8009D718): CBaseBehaviour, the interface unk_803E8E64 at 0x34 and
 * unk_80024158 at 0x3C. fn_801A7BFC and fn_8009ECB0 override slots of CAttributeHandler and
 * unk_803E8E64. The object is 0x144 bytes; its own data is padding here.
 */
class unk_8009D2E4 : public CBaseBehaviour, public unk_803E8E64, public unk_80024158 {
public:
                    unk_8009D2E4( const unk_AttributePacket& packet );
                    unk_8009D2E4( const unk_AttributePacket& packet, bool flag );   // 0x8009D41C, the constructor of the derived scene behaviours
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

    float           field_0xA0;
    float           field_0xA4;
    float           field_0xA8;
    float           field_0xAC;
    float           field_0xB0;
    float           field_0xB4;
    u8              field_0xB8[0xC];
    float           field_0xC4;
    float           field_0xC8;
    float           field_0xCC;
    float           field_0xD0;
    float           field_0xD4;
    float           field_0xD8;
    u8              field_0xDC;
    u8              field_0xDD[0x43];
    s16             field_0x120;
    u8              field_0x122[0x2];
    float           field_0x124;
    float           field_0x128;
    u32             field_0x12C;
    unk_RpClump*    field_0x130;    // presumably the behaviour's clump: fn_801DD29C walks its atomics
    unk_RpClump*    field_0x134;    // the second clump slot; the destructor moves it into field_0x130
    u32             field_0x138;    // read by fn_80017568; its writer is not yet identified
    u32             field_0x13C;    // cleared by the clump setup and by the destructor
    u8              field_0x140;
    u8              field_0x141;    // set by fn_8009D520 from its flag argument
    u8              field_0x142;
    u8              field_0x143;
};

#endif
