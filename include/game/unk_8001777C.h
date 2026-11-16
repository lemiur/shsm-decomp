#ifndef GAME_UNK_8001777C_H
#define GAME_UNK_8001777C_H

#include <game/unk_8009D2E4.h>

/* Animation controller owned by unk_8001777C at field_0x148 (partial layout). Name invented. */
struct unk_AnimController {
    u8      field_0x0[0xC];
    s32     field_0xC;
};

struct unk_8001B000;

/*
 * A behaviour class (vtable lbl_803E08F4, size 0x124, constructor 0x8001777C, destructor
 * 0x80017850), the base of CPlayer's base unk_8003C454, CFlare, cPhone and CWheelchair. fn_8000CFD4 and
 * fn_801A7A88 override CAttributeHandler slots. The object is 0x1BC bytes; most fields remain padding.
 */
class unk_8001777C : public unk_8009D2E4 {
public:
                    unk_8001777C( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_8001777C();  // override
    virtual void    fn_8009E6D4();  // override
    virtual void    fn_8000CFD4();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_80019F88( float dt );
    virtual void    fn_8001A044( float dt );
    virtual void    fn_80019A3C();
    virtual void    fn_80019A14();
    virtual void    fn_80019DF4();
    virtual void    fn_800191B4();
    virtual void    fn_80017CBC();
    virtual void    fn_80017DD8();
    virtual void    fn_80017E88();
    virtual void    fn_800180E4();
    virtual void    fn_80018F28();
    virtual void    fn_80018D8C();

    void            fn_80019734( int animId, u32 flags, float a, float b );

    u8                  field_0x144[0x4];
    unk_AnimController* field_0x148;    // 0x148
    u8                  field_0x14C[0x4];
    unk_8001B000*       field_0x150;    // 0x150
    u8                  field_0x154[0x68];
};

#endif
