#ifndef GAME_UNK_8004B2F0_H
#define GAME_UNK_8004B2F0_H

#include <revolution/types.h>

// Name invented. Identity TODO: the class and vtable of the object at +0x8 are not
// identified. Slots 0-2 and 4 are placeholders; this unit calls only slots 3, 5 and 6.
class unk_AnimTarget {
public:
    virtual void unk_Slot0();
    virtual void unk_Slot1();
    virtual void unk_Slot2();
    virtual void* unk_Slot3();
    virtual void unk_Slot4();
    virtual void unk_Slot5();
    virtual void unk_Slot6();
};

// Class name from the constructor address (fn_8004B2F0). Partial layout: this unit
// reads only the member at +0x8.
class unk_8004B2F0 {
public:
    void fn_8001B19C();
    void fn_8001B1BC();
    void* fn_8001B1DC();

    u32 field_0x0;
    u32 field_0x4;
    unk_AnimTarget* field_0x8;
};

extern "C" {
void fn_8001A8E8();
}

#endif
