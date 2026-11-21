#ifndef GAME_UNK_80055174_H
#define GAME_UNK_80055174_H

#include <game/unk_8003C454.h>
#include <game/CPointOfInterest.h>

/*
 * The base of the characters and cars (vtable lbl_803E3BD8, size 0x298, constructor 0x80055174,
 * destructor 0x8005524C): unk_8003C454, then CPointOfInterest at 0x700. The base of CEnemy,
 * CGenericCharacter, CHeadTurnNPC, CWalkNPC, CGenericCar and CSUV. It adds no slots of its own:
 * the six after the CPointOfInterest block are its overrides of CPointOfInterest's slots, in
 * declaration order. The object is 0x788 bytes. Field names invented.
 */
class unk_80055174 : public unk_8003C454, public CPointOfInterest {
public:
                    unk_80055174( const unk_AttributePacket& packet );
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_80055174();  // override
    virtual void    fn_8003CF5C( int value );  // override
    virtual SClassInfo* fn_800C4E9C();  // override
    virtual CZone*  fn_800C4E8C();  // override
    virtual CZone*  fn_800C4E94();  // override
    virtual bool    fn_8002CDF0();  // override
    virtual void    fn_800C6274( unk_8003C454* who );  // override
    virtual bool    fn_8002CDA4();  // override

    /* Not virtual: refreshes the vectors below from the base's frame data. */
    void            fn_800552B4();

    unk_Vec3        field_0x744;    // the position, from CBaseBehaviour's frame (fn_8009CCE8)
    unk_Vec3        field_0x750;    // the at vector, from CBaseBehaviour's frame (fn_8009CD84)
    unk_Vec3        field_0x75C;    // zeroed by the constructor
    unk_Vec3        field_0x768;    // copied from the base's field_0x3CC by fn_800552B4
    float           field_0x774;    // copied from the base's field_0x3E4 by fn_800552B4
    u16             field_0x778;    // set to 0xFFFF by the constructor
    u8              field_0x77A[0x2];
    float           field_0x77C;    // the base's angle in radians (field_0x69C in degrees)
    float           field_0x780;    // half of field_0x77C
    u8              field_0x784;
    bool            field_0x785;    // set by the constructor
    u8              field_0x786[0x2];
};

#endif
