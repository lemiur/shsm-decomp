#ifndef GAME_CPLAYER_H
#define GAME_CPLAYER_H

#include <game/unk_8003C454.h>

/*
 * The two small interfaces CPlayer inherits after unk_8003C454, at 0x700 and 0x704 (their vtable
 * groups sit in lbl_803E5F38 at +0x22C and +0x23C; no standalone vtable or constructor is
 * known). Names invented; the slot names are the addresses of CPlayer's overrides.
 */
class unk_CPlayerBase700 {
public:
    virtual         ~unk_CPlayerBase700();
    virtual void    fn_80067DCC();
};

class unk_CPlayerBase704 {
public:
    virtual void    fn_80071824();
};

/*
 * The player (registered as "CPlayer", vtable lbl_803E5F38, size 0x25C, constructor 0x800558B4,
 * destructor 0x80056ADC, factory 0x80073BDC, size 0x2368): unk_8003C454, then the small interfaces
 * unk_CPlayerBase700 and unk_CPlayerBase704 at 0x700 and 0x704. Its own data is padding here, and
 * only the slots that recovered code calls have signatures. fn_8003AC14 and fn_8003AE98 override
 * slots of unk_80039E40, fn_80017504 one of unk_803E8E64, fn_80067DCC and fn_80071824 those of the
 * two interfaces; the rest override unk_8003C454 and its bases.
 */
class CPlayer : public unk_8003C454, public unk_CPlayerBase700, public unk_CPlayerBase704 {
public:
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CPlayer();  // override
    virtual void    fn_8009CE2C( const void* matrix );  // override
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    virtual void    fn_8009E6D4();  // override
    virtual void    fn_8000CFD4();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_80019F88( float dt );  // override
    virtual void    fn_8001A044( float dt );  // override
    virtual void    fn_80017CBC();  // override
    virtual void    fn_80017E88();  // override
    virtual void    fn_800180E4();  // override
    virtual void    fn_800428B0();  // override
    virtual void    fn_800430A0();  // override
    virtual void    fn_80043118();  // override
    virtual void    fn_80043120();  // override
    virtual void    fn_8004315C();  // override
    virtual void    fn_8003D98C();  // override
    virtual int     fn_80072D14();  // override
    virtual void    fn_8003CCC4();  // override
    virtual void    fn_8003F940();  // override
    virtual bool    fn_800436C8();  // override
    virtual void    fn_8001750C();  // override
    virtual void    fn_80041D08();  // override
    virtual void    fn_800436C0();  // override
    virtual void    fn_8003FE38();  // override
    virtual void    fn_8003FE48();  // override
    virtual void    fn_8003FED8();  // override
    virtual void    fn_8003FFC8();  // override
    virtual void    fn_800435E8();  // override
    virtual void    fn_8003CF5C();  // override
    virtual void    fn_8003EC88();  // override
    virtual void    fn_8003D2B4();  // override
    virtual void    fn_8003F05C();  // override
    virtual void    fn_800436A8();  // override
    virtual void    fn_8003E470();  // override
    virtual void    fn_8003D604();  // override
    virtual void    fn_8003E658();  // override
    virtual void    fn_80040FD8();  // override
    virtual void    fn_80041D34();  // override
    virtual void    fn_8003F218();  // override
    virtual void    fn_80040FC8();  // override
    virtual void    fn_80043650();  // override
    virtual void    fn_80043654();  // override
    virtual bool    fn_80043658();  // override
    virtual void    fn_800433F0();  // override
    virtual void    fn_80040140();  // override
    virtual void    fn_8004073C();  // override
    virtual void    fn_80041D2C();  // override
    virtual void    fn_8003AC14();  // override
    virtual void    fn_80067DCC();  // override
    virtual bool    fn_80017504();  // override
    virtual void    fn_8003AE98();  // override
    virtual void    fn_80071824();  // override

    // the byte at 0x14FD (out of line, at 0x800631E0; role unknown)
    bool            fn_800631E0();
    // 0x8007046C: true when the word at 0x1288 and the object at 0x1588 are both clear (role unknown)
    bool            fn_8007046C();
    // 0x80070490: takes two strings (the second may be NULL or empty) and a flag (role unknown)
    void            fn_80070490( const char* a, const char* b, bool flag );

    u8              field_0x708[0xB84];
    bool            field_0x128C;   // set: the point manager also looks along the camera's query (fn_800C5800)
    u8              field_0x128D[0x273];
    float           field_0x1500;   // scales a point of interest's look time before its prompt may show (CPsyPointOfInterest)
    float           field_0x1504;   // scales a point of interest's look time before it is looked at (CPsyPointOfInterest)
    float           field_0x1508;   // how centred a point of interest must be on the screen to count (CPsyPointOfInterest)
    u8              field_0x150C[0xE5A];
    u8              field_0x2366;   // nonzero: modal tutorial messages are not shown
    u8              field_0x2367;
};

#endif
