#ifndef GAME_UNK_8002C8D0_H
#define GAME_UNK_8002C8D0_H

// Class name invented: unk_8002BC7C is named after its constructor fn_8002BC7C, which
// stores the vtable lbl_803E1788 at offset 0. Only the members this unit reads are declared;
// the other slots of the vtable are not yet declared. Member names invented.

#include <revolution/types.h>
#include <game/unk_80491B38.h>
#include <game/unk_RespotManager.h>

class unk_8002BC7C;

/* The object fn_80030D58 returns; its byte at 0x96 is set by the function. Name invented. */
class unk_80030D58 {
public:
    u8                  field_0x0[0x96];
    u8                  field_0x96;
};

/* One of the four entries of the array at offset 0x344; stride 0x14. */
struct unk_8002BC7C_Slot {
    void*   field_0x0;      // 0x0
    void*   field_0x4;      // 0x4
    s32     field_0x8;      // 0x8
    s32     field_0xC;      // 0xC
    u8      field_0x10;     // 0x10
    u8      field_0x11;     // 0x11
    u8      flagA;          // 0x12
    u8      flagB;          // 0x13
};

class unk_8002BC7C {
public:
    /* Slot 5 (offset 0x1C) of the vtable. */
    virtual void        fn_8002C8D0( void* arg );

    u8                  field_0x4[0x4];
    void*               field_0x8;
    u8                  field_0xC[0x344 - 0xC];
    unk_8002BC7C_Slot   slots[4];           // 0x344
    s32                 index;              // 0x394, the current slot, -1 when none
    u8                  field_0x398[0x3C0 - 0x398];
    u8                  field_0x3C0;
};

extern "C" {
void*                       fn_80030ED4( void* );
unk_80030D58*               fn_80030D58( void*, void*, void*, void*, void*, s32, u8, u8 );
void                        fn_801831E4( unk_8002BC7C* self, void* arg );
}

#endif
