#ifndef GAME_UNK_8003C454_H
#define GAME_UNK_8003C454_H

#include <game/unk_8001777C.h>
#include <game/unk_80039E40.h>
#include <game/unk_8000F80C.h>
#include <game/unk_8000FC5C.h>

/*
 * The base of CPlayer and of the actors (vtable lbl_803E2978, size 0x22C, constructor
 * 0x8003C454, destructor 0x8003C9EC): unk_8001777C, then unk_80039E40 at 0x1BC (its constructor calls
 * 0x80039E40 there and stores its second vptr at 0x218). Its slots are the
 * actors' operations, in vtable order; fn_80072D14 is pure (its vtable word is null), fn_80040D4C
 * stores a flag, an integer and a float at 0x690..0x698. fn_80072D14 returns a word its callers
 * test for non-zero (they normalise it with neg/or/srwi, which a bool result does not need; int or
 * pointer), fn_800436B8 a truth value (true here; bool provisional: its one caller cannot tell bool
 * from int). CGenericCar's inline overrides return 0 and false. fn_8000CFD0,
 * fn_8000F5DC, fn_8000F5E0 and fn_8001750C override slots of CAttributeHandler and
 * unk_803E8E64. The object is 0x700 bytes; its own data is padding here.
 */
class unk_ActorSlot;
extern "C" {
unk_EventDescriptor* fn_80012650( unk_EventDescriptor* self, u32 index );  // defined in an unreconstructed unit
s32 fn_8001A520( unk_AnimController* self, s32 value, s32 flag );          // defined in an unreconstructed unit
}

struct unk_MotionCtl;    // name invented: the object at 0x574, whose slot 0x54 takes the owner's value
struct unk_ActorEntry;   // name invented: an entry of the list at 0x6A8

class unk_8003C454 : public unk_8001777C, public unk_80039E40 {
public:
                    unk_8003C454( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_8003C454();  // override
    virtual void    fn_8009CE2C( const void* matrix );  // override
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    virtual void    fn_8009E6D4();  // override
    virtual void    fn_8000CFD4();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_80019F88( float dt );  // override
    virtual void    fn_8001A044( float dt );  // override
    virtual void    fn_80017E88();  // override
    virtual void    fn_800180E4();  // override
    virtual void    fn_800428B0();
    virtual void    fn_8000CFD0();  // override
    virtual void    fn_800430A0();
    virtual void    fn_800430A8();
    virtual void    fn_80043118();
    virtual void    fn_80043120();
    virtual void    fn_8004315C();
    virtual void    fn_8003D98C();
    virtual int     fn_80072D14() = 0;
    virtual void    fn_8003FE30();
    virtual void    fn_8003FE28();
    virtual bool    fn_800436D0();
    virtual void    fn_8003CCC4( u32 index );
    virtual void    fn_8003F940();
    virtual bool    fn_800436C8();
    virtual void    fn_8000F5DC();  // override
    virtual void    fn_8000F5E0();  // override
    virtual void    fn_8001750C();  // override
    virtual void    fn_80041D08();
    virtual bool    fn_800436C0();
    virtual bool    fn_800436B8();
    virtual void    fn_8003FD98();
    virtual void    fn_8003FE38();
    virtual void    fn_8003FE48();
    virtual void    fn_8003FE58();
    virtual void    fn_8003FED8();
    virtual void    fn_8003FFC8();
    virtual void    fn_800435E8();
    virtual void    fn_800435F0();
    virtual void    fn_80040D4C( float time, int value );
    virtual bool    fn_800436B0();
    virtual void    fn_8003CF5C( int value );
    virtual void    fn_8003EC88();
    virtual void    fn_8003D2E0();
    virtual void    fn_8003D3D0();
    virtual void    fn_8003D2B4();
    virtual void    fn_8003D2D8();
    virtual void    fn_8003F05C();
    virtual bool    fn_800436A8();
    virtual void    fn_8003E470();
    virtual void    fn_80042DBC();
    virtual void    fn_8003D604();
    virtual void    fn_8003E658();
    virtual void    fn_80040FD8();
    virtual void    fn_80041D34();
    virtual void    fn_8003DDF4();
    virtual void    fn_8003F218();
    virtual void    fn_80040D60();
    virtual void    fn_80040FC8();
    virtual void    fn_80040FD0();
    virtual void    fn_8004364C();
    virtual void    fn_80043650();
    virtual void    fn_80043654();
    virtual bool    fn_80043658();  // whether field_0x220's name starts with "IGC"
    virtual void    fn_800433F0();
    virtual void    fn_80040140();
    virtual void    fn_80040738();
    virtual void    fn_8004073C();
    virtual void    fn_80041D2C();

    u8              field_0x21C[0x4];
    unk_ActorSlot*  field_0x220;  // a record whose source GetName names (fn_80043658)
    u8              field_0x224[0x190];
    unk_RwV3d       field_0x3B4;    // a direction: the point manager (fn_800C5800) looks along it
    u8              field_0x3C0[0xC];
    unk_Vec3        field_0x3CC;    // copied to the derived object's field_0x768 (fn_800552B4)
    u8              field_0x3D8[0xC];
    float           field_0x3E4;    // copied to the derived object's field_0x774 (fn_800552B4)
    u8              field_0x3E8[0x18C];
    unk_MotionCtl*  field_0x574;
    u8              field_0x578[0x100];
    u8              field_0x678;    // flag: set by the owner's update, cleared by fn_8000CFD0 after its call
    u8              field_0x679[0x23];
    float           field_0x69C;    // an angle in degrees: the derived constructor converts it to radians
    u8              field_0x6A0[0x4];
    s32             field_0x6A4;    // written by fn_8003CF5C
    unk_ActorEntry** field_0x6A8;   // the entries: begin and end, as in a std::vector
    unk_ActorEntry** field_0x6AC;
    u8              field_0x6B0[0x50];
};

#endif
