#ifndef GAME_UNK_800B6210_H
#define GAME_UNK_800B6210_H

// unit name unconfirmed: the class is named after its constructor fn_800B6210 (invented); no string evidence.

#include <revolution/types.h>
#include <climax/Heap.h>

// Placeholder classes for the objects the container constructor allocates. Only the sizes and the
// constructors are known; the vtable slots of the polymorphic ones are not declared here.
class unk_800B1754 {
public:
    unk_800B1754();
    u8 field_0x0[0x2C];
};

class unk_800B2E58 {
public:
    unk_800B2E58();
    u8 field_0x0[0x48];
};

class unk_800B63C8 {
public:
    unk_800B63C8();
    u8 field_0x0[0x14];
};

class unk_800B6520 {
public:
    unk_800B6520();
    u8 field_0x0[0x10];
};

// Base of unk_803EA6A0: constructor fn_800B555C (its vtable is not declared). The second constructor
// argument is passed but the release body never reads it.
class unk_800B555C {
public:
    unk_800B555C( s32 a, s32 b );
    s32 field_0x4;      // 0x4, zeroed
    s32 field_0x8;      // 0x8, zeroed
    s32 field_0xC;      // 0xC, the first constructor argument
    u8  field_0x10;     // 0x10, zeroed
};

// Derived 0x14-byte object: its constructor is inline (base constructor, then the vtable pointer).
// The class is named after its vtable lbl_803EA6A0 (invented); its slots are the 22 virtual functions
// of that vtable, which all belong to other units.
class unk_803EA6A0 : public unk_800B555C {
public:
    unk_803EA6A0( s32 a, s32 b ) : unk_800B555C( a, b ) {}
    virtual void fn_800B5D34();
    virtual void fn_800B5A68();
    virtual void fn_800B5AF0();
    virtual void fn_800AD700();
    virtual void fn_800B55C8();
    virtual void fn_800B578C();
    virtual void fn_800B5AD0();
    virtual void fn_800B26F8();
    virtual void fn_800B577C();
    virtual void fn_800B5AE8();
    virtual void fn_800B5B50();
    virtual void fn_800B291C();
    virtual void fn_800B2914();
    virtual void fn_800B290C();
    virtual void fn_800B59E4();
    virtual void fn_800AD6F8();
    virtual void fn_800B5A28();
    virtual void fn_800B2904();
    virtual void fn_800AE2B0();
    virtual void fn_800ACC2C();
    virtual void fn_800B57A0();
    virtual void fn_800B5698();
};

// Slot of unk_800B6210: an owned object pointer and its ownership flag.
struct unk_800B6210Slot {
    void*   obj;        // 0x0
    u8      owned;      // 0x4
};

// Container of 32 owned object slots (0x100 bytes) and the number of slots in use. Class named after
// its constructor (invented).
class unk_800B6210 {
public:
    // Allocation flags of every owned object (name invented; retail passes 0x40000).
    enum { OBJECT_ALLOC_FLAGS = 0x40000 };

    unk_800B6210();
    void* fn_800B63BC( s32 index );
    // helper (name invented): appends one owned slot; inline, retail has no out-of-line copy
    void AddOwned( void* obj ) {
        s32 index = m_count;
        m_count = index + 1;
        unk_800B6210Slot& slot = m_slots[index];
        slot.owned = 1;
        slot.obj = obj;
    }

    unk_800B6210Slot    m_slots[32];    // 0x0
    s32                 m_count;        // 0x100
};

#endif
