#ifndef GAME_UNK_8001B200_H
#define GAME_UNK_8001B200_H

#include <revolution/types.h>
#include <climax/Frame.h>

/*
 * Animation controller (name invented). Its vtable is lbl_803E0B40, 22 virtual slots from +0x8,
 * not yet recovered as a class: the functions of this unit take the object as a free-function
 * argument. Partial layout: only the fields the functions here touch; the vtable pointer and the
 * vtable header words stand as padding at 0x0.
 */
/* One named entry of the list at field_0x30 (name invented). Only the name and the hash are used here. */
struct unk_8001B0Entry {
    const char*     name;               // 0x0
    u32             hash;               // 0x4, GetStringHashForceLower of name
};

/* A node of the entry list at field_0x30 (name invented). */
struct unk_8001B0Node {
    unk_8001B0Entry* data;              // 0x0
    unk_8001B0Node*  next;              // 0x4
};

struct unk_8001B000 {
    u8              field_0x0[0x8];     // 0x0, vtable pointer
    unk_8001B000*   field_0x8;          // 0x8, a second controller read by fn_8001B200
    u8              field_0xC[0x8];     // 0xC
    u32             field_0x14;         // 0x14
    float           field_0x18;         // 0x18
    void*           field_0x1C;         // 0x1C
    void*           field_0x20;         // 0x20
    unk_RwV3d       field_0x24;         // 0x24, the vector fn_8001B27C writes out
    unk_8001B0Node* field_0x30;         // 0x30, head of the entry list
    u8              field_0x34[0x8];    // 0x34
    float           field_0x3C;         // 0x3C, returned by fn_8001B298
    u8              field_0x40[0x8];    // 0x40
    void*           field_0x48;         // 0x48, set by fn_8001B778 for id 8
    void*           field_0x4C;         // 0x4C, set by fn_8001B778 for id 0xA
    void*           field_0x50;         // 0x50, set by fn_8001B778 for id 0xB
};

unk_RwV3d fn_8001B200( unk_8001B000* obj );

namespace RWS {
namespace SHUtils {
u32 GetStringHashForceLower( const char* str, int length );
}
}

extern "C" {
unk_RwV3d fn_8001B27C( const unk_8001B000* obj );
float fn_8001B298( const unk_8001B000* obj );
unk_8001B0Entry* fn_8001B2A0( const unk_8001B000* obj, const char* name );
void fn_8001B330();
void fn_8001B5DC();
void fn_8001B650();
void fn_8001B664();
void fn_8001B6F8();
void fn_8001B764();
void fn_8001B768( unk_8001B000* obj );
void fn_8001B778( unk_8001B000* obj, s32 id, void* value );
void fn_8001B7AC();
void fn_8001B884();
void fn_8001BA3C();
void fn_8001C42C();
void fn_8001C5F8();
void fn_8001C7B8();
void fn_8001C9B8();
void fn_8001CB2C();
void fn_8001CC1C();
void fn_8001CD1C();
void fn_8001CD24();
void fn_8001CD30();
void fn_8001CF74();
void fn_8001CF78();
void fn_8001D114();
void fn_8001D158();
void fn_8001D254();
void fn_8001D27C();
void fn_8001D454();
void fn_8001D580();
void fn_8001D6C0();
void fn_8001D934();
void fn_8001DB7C();
void fn_8001DF20();
void fn_8001DF70();
void fn_8001E390();
void fn_8001E5A8();
void fn_8001E5FC();
void fn_8001E820();
void fn_8001ECC0();
void fn_8001EEB0();
void fn_8001EF3C();
void fn_8001EF44();
void fn_8001EF70();
void fn_8001EFC4();
void fn_8001F12C();
void fn_8001F170();
void fn_8001F260();
void fn_8001F48C();
void fn_8001F4E4();
void fn_8001F63C();
void fn_8001F708();
void fn_8001F9C8();
void fn_8001FAF4();
void fn_8001FAF8();
void fn_8001FB00();
void fn_8001FB08();
void fn_8001FB0C();
void fn_8001FB30();
void fn_8001FBAC();
void fn_8001FBCC();
void fn_8001FBEC();
void fn_8001FC10();
void fn_8001FC18();
void fn_8001FC20();
void fn_8001FC94();
void fn_8001FD18();
void fn_8001FDA0();
void fn_8001FDA8();
void fn_8001FE34();
void fn_8001FE3C();
void fn_80020000();
void fn_80020024();
void fn_800200AC();
void fn_80020120();
void fn_800201A8();
void fn_800201B0();
void fn_800201B8();
}

#endif
