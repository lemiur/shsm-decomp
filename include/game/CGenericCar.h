#ifndef GAME_CGENERICCAR_H
#define GAME_CGENERICCAR_H

// unit name unconfirmed: the class name is the registration string "CGenericCar" of the static
// initialiser 0x8007B9A8; the unit (0x80077C9C onwards) is not yet reconstructed.

#include <game/unk_80055174.h>
#include <game/unk_HashTable.h>

/*
 * A slot of a car (0xE0 bytes) at 0x7C0 + index * 0xE0: a name and its hash, the name-table entry
 * last found for it, and the table that entry was found in. Name invented; the rest of the slot
 * is padding here.
 */
struct unk_CarSlot {
    const char*     name;       // 0x0
    u32             hash;       // 0x4
    unk_HashEntry*  entry;      // 0x8
    unk_HashTable*  owner;      // 0xC
    u8              field_0x10[0xD0];
};

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
    virtual void    fn_8007B8F0( int slot ) = 0;
    virtual void    fn_8007B3D0() = 0;
};

/*
 * A car (registered as "CGenericCar", vtable lbl_803E71C4, size 0x2D4, constructor 0x80078EFC,
 * destructor 0x80079264): unk_80055174, then the interface unk_803E7498 at 0x788. The object is
 * 0xA030 bytes.
 * Its own data is padding here except the slot array at 0x7C0; the count is the most slots
 * that fit before the object's end, and the real count is not known. The slot-array slots have no
 * signatures yet. Two overrides are inline:
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
    virtual void    fn_8007B8F0( int slot );  // override
    virtual void    fn_8007B3D0();  // override
    virtual void    fn_80079AF8();
    virtual void    fn_8007B300();

    u8              field_0x78C[0x34];
    unk_CarSlot     slots[174];
    u8              field_0xA000[0x30];
};

extern s32 lbl_8052721C;    // the default value written into a name-table entry (unnamed literal)

#endif
