#ifndef GAME_CSEQUENCEOBJECT_H
#define GAME_CSEQUENCEOBJECT_H

#include <revolution/types.h>
#include <climax/unk_801A92A8.h>
#include <game/unk_8009D2E4.h>
#include <game/unk_ClumpBounds.h>

// Element of the array at 0x1B4 of CSequenceObject: 15 entries of 0x18 bytes, each holding two
// event ids. Constructed and destroyed by the array helpers, which take the element's constructor
// and destructor by address. Class name invented (named by its constructor address 0x800A5A3C).
class unk_800A5A3C {
public:
                    unk_800A5A3C();
                    ~unk_800A5A3C();

    unk_801A92A8    first;          // 0x0
    unk_801A92A8    second;         // 0x8
    u8              field_0x10[0x8];
};

/*
 * The scene object with a sequence of event ids (registration string "CSequenceObject" at
 * 0x803E9838, constructor 0x800A5928, size 0x31C). Derived from unk_8009D2E4, which occupies
 * 0x0-0x144. Its own members are named from the constructor only; the vtable (lbl_803E9748,
 * 0xF0 bytes, with secondary vtables at 0x24, 0x2C, 0x34 and 0x3C) is not reconstructed yet.
 */
class CSequenceObject : public unk_8009D2E4 {
public:
                    CSequenceObject( const unk_AttributePacket& packet, bool flag );
    virtual         ~CSequenceObject();  // override
                    void fn_800A6020();  // 0x800A6020, a vtable slot (vtable not reconstructed yet)

    u8              field_0x144[0x8];
    u8              field_0x14C;
    u8              field_0x14D;
    u32             field_0x150;
    u32             field_0x154;
    unk_ClumpBounds* field_0x158;
    unk_801A92A8    field_0x15C;
    unk_801A92A8    field_0x164;
    unk_801A92A8    field_0x16C;
    unk_801A92A8    field_0x174;
    unk_801A92A8    field_0x17C;
    unk_801A92A8    field_0x184;
    unk_801A92A8    field_0x18C;
    u8              field_0x194;
    float           field_0x198;
    unk_801A92A8    field_0x19C;
    u8              field_0x1A4;
    s32             field_0x1A8;
    unk_801A92A8    field_0x1AC;
    unk_800A5A3C    field_0x1B4[0xF];
};

void fn_800A65B0( const char*, ... );

// The registry factory (CSequenceObject's entry in lbl_8052981C): allocates the object and
// constructs it with the packet and the flag 1.
CAttributeHandler* fn_800A69E4( const unk_AttributePacket& packet );

extern "C" {
void fn_800A5ADC();
void fn_800A5D38();
void fn_800A605C();
void fn_800A6600();
void fn_800A6A30();
void fn_800A6A38();
void fn_800A6A40();
void fn_800A6A48();
void fn_800A6A50();
void fn_800A6A58();
}

#endif
