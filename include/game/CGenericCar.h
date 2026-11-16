#ifndef GAME_CGENERICCAR_H
#define GAME_CGENERICCAR_H

// unit name unconfirmed: the class name is the registration string "CGenericCar" of the static
// initialiser 0x8007B9A8; the unit (0x80077C9C onwards) is not yet reconstructed.

#include <game/unk_80055174.h>

/*
 * The interface at 0x788 of CGenericCar (vtable lbl_803E7498, destructor 0x80079224; a
 * destructor and four pure slots). Name from its vtable address (invented); the slots are named
 * after CGenericCar's implementations.
 */
class unk_803E7498 {
public:
    virtual         ~unk_803E7498();
    virtual void    fn_8007B3BC() = 0;
    virtual void    fn_8007B4B0() = 0;
    virtual void    fn_8007B8F0() = 0;
    virtual void    fn_8007B3D0() = 0;
};

/*
 * A car (registered as "CGenericCar", vtable lbl_803E71C4, size 0x2D4, constructor 0x80078EFC,
 * destructor 0x80079264): unk_80055174, then the interface unk_803E7498 at 0x788. The object is
 * 0xA030 bytes.
 * Its own data is padding here, and the slots have no signatures yet. Two overrides are inline:
 * their only copies are in the first unit that emits a car vtable (CCopCar).
 */
class CGenericCar : public unk_80055174, public unk_803E7498 {
public:
                    CGenericCar( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CGenericCar();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_80019F88( float dt );  // override
    virtual void    fn_8001A044( float dt );  // override
    virtual void    fn_80019A14();  // override
    virtual void    fn_80019DF4();  // override
    virtual void    fn_80017CBC();  // override
    virtual void    fn_80017DD8();  // override
    virtual void    fn_80017E88();  // override
    virtual void    fn_800180E4();  // override
    virtual void    fn_80018F28();  // override
    virtual int     fn_80072D14() { return 0; }  // override
    virtual bool    fn_800436B8() { return false; }  // override
    virtual void    fn_8003E470();  // override
    virtual void    fn_80042DBC();  // override
    virtual void    fn_8003E658();  // override
    virtual void    fn_80040FD8();  // override
    virtual void    fn_80041D34();  // override
    virtual void    fn_80079E10();
    virtual void    fn_8007B03C();
    virtual void    fn_8007B3BC();  // override
    virtual void    fn_8007B4B0();  // override
    virtual void    fn_8007B8F0();  // override
    virtual void    fn_8007B3D0();  // override
    virtual void    fn_80079AF8();
    virtual void    fn_8007B300();

    u8              field_0x78C[0x98A4];
};

#endif
