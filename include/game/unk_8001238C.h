#ifndef GAME_UNK_8001238C_H
#define GAME_UNK_8001238C_H

#include <revolution/types.h>

// unit name unconfirmed: the classes and members below are placeholders (names invented
// from the layout only). Member names are field_0x<offset> until string evidence appears.

namespace RWS {
namespace SHUtils {
u32 GetStringHashForceLower(const char* name, int length);
}
}

// Named object whose first word is its name hash.
struct unk_Named {
    u32 field_0x0;
};

// Table of words the shared object points at: word 0 is unused here, word 4 the slot table.
struct unk_Lookup {
    u32 field_0x0;
    void** field_0x4;      // slot table
};

// Reference-counted shared object reached through unk_8001238C::field_0x28.
struct unk_Shared {
    unk_Lookup* field_0x0; // lookup whose slot table gives the object's current entry
    unk_Named** field_0x4; // item table
    u8 field_0x8[0x4];
    u32* field_0xc;        // index into the lookup's slot table
    u8 field_0x10[0x8];
    s32 field_0x18;        // item count
    u8 field_0x1c[0xc];
    s32 field_0x28;        // name count
    const char** field_0x2c; // name table
    u32 field_0x30;        // reference count
    void* field_0x34;      // resource handle, released through RwFree
    u32 field_0x38;
};

// One 16-byte entry of unk_8001238C::field_0x24: word 0 the name, word 4 its hash, words 8
// and 12 are zeroed by the entry constructor (their meaning is not known yet).
struct unk_NameHashEntry {
    const char* field_0x0;
    u32 field_0x4;
    u32 field_0x8;
    u32 field_0xc;
    unk_NameHashEntry(); // out of line: __ct__13unk_NameEntryFv (0x80012510)
};

// One 8-byte table entry: word 0 key, word 4 value.
struct unk_PairEntry {
    u32 field_0x0;
    u32 field_0x4;
};

// Table of 8-byte pairs with a count and a capacity. Placeholder class unk_PairTable.
struct unk_PairTable {
    unk_PairEntry* field_0x0; // entries
    s32 field_0x4;  // entry count
    u32 field_0x8;  // capacity
};

// A word the original reads as a float and also clears as an integer.
union unk_Word {
    f32 f;
    u32 u;
};

// Object returned by fn_80010FC4: a float at 0x20, a word at 0x24 and the shared
// object at 0x28. Placeholder class unk_Source.
struct unk_Source {
    u8 field_0x0[0x20];
    f32 field_0x20;
    u32 field_0x24;
    unk_Shared* field_0x28;
};

// Placeholder class unk_8001238C (constructor fn_8001238C). Name invented.
struct unk_8001238C {
    void* field_0x0;
    void* field_0x4;
    unk_Word field_0x8;
    unk_Word field_0xc;
    f32 field_0x10;
    f32 field_0x14;
    f32 field_0x18;
    u32 field_0x1c;
    u32 field_0x20;
    unk_NameHashEntry* field_0x24;
    unk_Shared* field_0x28;
};

extern "C" {
extern const f32 lbl_8052B768; // 0.0f, shared zero constant
extern const f32 lbl_8052B76C; // 0.001f, snap threshold
void* fn_801874FC(void* owner);
unk_Shared* fn_80011BB0(void* block, void* arg);
unk_Named* fn_8000F8F0(unk_Named* item);
void fn_80010E28(void* manager);
s32 fn_80010B94(void* manager, unk_8001238C* self, u32 a, u32 b);
s32 fn_80010E2C(void* manager, unk_8001238C* self, u32 a);
void* fn_80010FC4(void* manager, unk_8001238C* self);
void fn_8001238C(unk_8001238C* self, void* arg, void* arg2);
unk_8001238C* fn_80012524(unk_8001238C* self, s32 deleteFlag);
unk_Named* fn_800125C8(unk_Shared* self, const char* name);
unk_Named* fn_80012650(unk_Shared* self, s32 index);
s32 fn_8001267C(unk_8001238C* self, u32 a, u32 b);
s32 fn_80012698(unk_8001238C* self, u32 a);
void fn_800126AC(unk_8001238C* self, f32 delta);
s32 fn_800126C0(unk_8001238C* self);
s32 fn_8001274C(unk_8001238C* self, const char* name, f32 value);
void fn_80012840(unk_8001238C* self, f32 value, f32 target);
void fn_80012888(unk_8001238C* self);
unk_PairTable* fn_80012894(unk_PairTable* self, s32 deleteFlag);
void fn_80012904(unk_PairTable* self, u32 key, u32 value);
u32 fn_8001295C(unk_PairTable* self, u32 key);
}

#endif
