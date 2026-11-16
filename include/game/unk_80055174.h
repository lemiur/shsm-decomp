#ifndef GAME_UNK_80055174_H
#define GAME_UNK_80055174_H

#include <game/unk_8003C454.h>
#include <game/CPointOfInterest.h>

/*
 * The base of the characters and cars (vtable lbl_803E3BD8, size 0x298, constructor 0x80055174,
 * destructor 0x8005524C): unk_8003C454, then CPointOfInterest at 0x700. The base of CEnemy,
 * CGenericCharacter, CHeadTurnNPC, CWalkNPC, CGenericCar and CSUV. It adds no slots of its own:
 * the six after the CPointOfInterest block are its overrides of CPointOfInterest's slots, in
 * declaration order. The object is 0x788 bytes; its own data is padding here.
 */
class unk_80055174 : public unk_8003C454, public CPointOfInterest {
public:
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_80055174();  // override
    virtual void    fn_8003CF5C();  // override
    virtual SClassInfo* fn_800C4E9C();  // override
    virtual CZone*  fn_800C4E8C();  // override
    virtual CZone*  fn_800C4E94();  // override
    virtual bool    fn_8002CDF0();  // override
    virtual void    fn_800C6274( unk_8003C454* who );  // override
    virtual bool    fn_8002CDA4();  // override

    u8              field_0x744[0x44];
};

#endif
