#ifndef GAME_UNK_8005549C_H
#define GAME_UNK_8005549C_H

#include <revolution/types.h>

// One named entry of a lookup table searched by fn_800A2A18: 0x14 bytes.
struct unk_LookupEntry {
    u8              field_0x00[0x04];
    s32             index;              // 0x04: the index the entry was added with; fn_800A292C returns it
    const char*     name;               // 0x08
    u32             id;                 // 0x0C
    s32             value;              // 0x10
};

// The header of a lookup table: the entry array, its count and the number of entries it holds room for.
struct unk_NamedTable {
    unk_LookupEntry* entries;            // 0x00
    s32             count;              // 0x04
    s32             capacity;           // 0x08 (name invented)
};

// One lazily resolved lookup. The name and the id are searched for in the owner's table; the
// result is cached together with the table it was found in, so the search runs once per table.
struct unk_LookupSlot {
    const char*     name;               // 0x00
    u32             id;                 // 0x04
    unk_LookupEntry* entry;              // 0x08: cached result (NULL when not found)
    unk_NamedTable* table;              // 0x0C: table the cached result belongs to
};

// A record the lookups flag: the flag is a word at 0x20.
struct unk_LookupFlag {
    u8              field_0x00[0x20];
    s32             flag;               // 0x20
};

// The object the lookup callbacks serve. Only the fields these callbacks touch are named; the
// class name and the layout of the rest are unknown (name invented).
struct unk_LookupOwner {
    u8              field_0x000[0x14C];
    unk_NamedTable* table;              // 0x14C: the table every lookup searches
    u8              field_0x150[0xBB0 - 0x150];
    unk_LookupSlot  field_0xBB0;
    u8              field_0xBC0[0xD60 - 0xBC0];
    unk_LookupSlot  field_0xD60;
    u8              field_0xD70[0xF10 - 0xD70];
    unk_LookupSlot  field_0xF10;
    u8              field_0xF20[0x10C0 - 0xF20];
    unk_LookupSlot  field_0x10C0;
    u8              field_0x10D0[0x1174 - 0x10D0];
    unk_LookupFlag* field_0x1174;       // 0x1174: flag record of the lookup at 0xBB0
    unk_LookupFlag* field_0x1178;       // 0x1178: flag record of the lookups at 0xD60
    unk_LookupFlag* field_0x117C;       // 0x117C: flag record of the lookups at 0xF10
    unk_LookupFlag* field_0x1180;       // 0x1180: flag record of the lookup at 0x10C0
    u8              field_0x1184[0x15D1 - 0x1184];
    u8              field_0x15D1;       // 0x15D1: set when fn_8005588C finds its key
    u8              field_0x15D2;
    u8              field_0x15D3;       // 0x15D3: cleared on every call of fn_8005588C
    u32             field_0x15D4;       // 0x15D4: the key fn_8005588C compares with
};

extern "C" {
// Lookup of a named entry in a table (name invented; defined in game/unk_800A292C.cpp).
unk_LookupEntry* fn_800A2A18(unk_NamedTable* table, const char* name, u32 id);

// Callbacks registered by the object factories. The first argument of each is not read.
void fn_8005549C(void* ctx, unk_LookupOwner* owner);
void fn_80055540(void* ctx, unk_LookupOwner* owner);
void fn_80055544(void* ctx, unk_LookupOwner* owner);
void fn_800555E8(void* ctx, unk_LookupOwner* owner);
void fn_800555EC(void* ctx, unk_LookupOwner* owner);
void fn_80055690(void* ctx, unk_LookupOwner* owner);
void fn_80055694(void* ctx, unk_LookupOwner* owner);
void fn_80055738(void* ctx, unk_LookupOwner* owner);
void fn_8005573C(void* ctx, unk_LookupOwner* owner);
void fn_800557E0(void* ctx, unk_LookupOwner* owner);
void fn_800557E4(void* ctx, unk_LookupOwner* owner);
void fn_80055888(void* ctx, unk_LookupOwner* owner);
void fn_8005588C(void* ctx, u32 key, unk_LookupOwner* owner);
}

#endif
