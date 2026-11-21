#ifndef GAME_CBASICANIMATEDOBJECT_H
#define GAME_CBASICANIMATEDOBJECT_H

// unit name unconfirmed: the class name is the registration string "CBasicAnimatedObject" of the
// static initialiser 0x800220EC; the unit (0x80020B04 onwards) is not yet reconstructed.

#include <game/unk_8009D2E4.h>
#include <game/unk_ClumpBounds.h>
#include <game/unk_80017B88.h>

/*
 * An animated scene object (registered as "CBasicAnimatedObject", vtable lbl_803E0F14, size 0x1D4,
 * constructor 0x80020B04, destructor 0x80020C4C): the base of CCardObject, CPickupObject,
 * CDrawingTest, CCrayon and CQuestionnaire. fn_801A7A88 and fn_80017554 override slots of the
 * non-primary bases CAttributeHandler and unk_80024158, so MWCC appends them to the primary
 * vtable as well. Member names invented; the field types are read from their uses (loads and
 * stores) only.
 */
class CBasicAnimatedObject : public unk_8009D2E4 {
public:
                    CBasicAnimatedObject( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CBasicAnimatedObject();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_80017554();  // override

    unk_ClumpBounds* field_0x144;
    Animation::s_MountRef* field_0x148;   // new[] array of 12 entries
    unk_801A92A8    field_0x14C;
    unk_801A92A8    field_0x154;
    unk_801A92A8    field_0x15C;
    unk_801A92A8    field_0x164;
    unk_801A92A8    field_0x16C;
    unk_801A92A8    field_0x174;
    const char*     field_0x17C;    // a name string: its first byte is tested
    unk_RwMatrix    field_0x180;    // its flags and pad words are read and written as words
    u8              field_0x1C0;
    u8              field_0x1C1;
    u8              field_0x1C2;
    u8              field_0x1C3;
    u8              field_0x1C4;
    u8              field_0x1C5;
    u8              field_0x1C6;
    u8              field_0x1C7;
    u8              field_0x1C8;
    u8              field_0x1C9;
    u32             field_0x1CC;
    u32             field_0x1D0;
};

extern unk_801A92A8 lbl_805298A0;   // defined elsewhere, unit not yet reconstructed
extern unk_801A92A8 lbl_805298B0;   // defined elsewhere, unit not yet reconstructed

#endif
