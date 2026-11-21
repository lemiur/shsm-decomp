#ifndef GAME_UNK_800438A8_H
#define GAME_UNK_800438A8_H

#include <revolution/types.h>

// Placeholder classes (names invented; no constructor or string evidence). Layouts are the
// offsets fn_800438A8 touches; gaps are unnamed.

// The sub-object at 0x9FC of unk_LookupCacheOwner: fn_800438A8 sets its flag at 0x20 when a lookup fails.
struct unk_FlagHolder {
    u8  field_0x0[0x20];
    s32 field_0x20;
};

// The record fn_800A2A18 returns; field_0x10 is its result pointer.
struct unk_LookupRecord {
    u8     field_0x0[0x10];
    void*  field_0x10;
};

// The object that caches one lookup: the key and table at 0x14C and 0x99C, the cached record and
// the key it was looked up with at 0x9A4 and 0x9A8.
struct unk_LookupCacheOwner {
    u8               field_0x0[0x14C];
    void*            field_0x14c;
    u8               field_0x150[0x84C];
    void*            field_0x99c;
    s32              field_0x9a0;
    unk_LookupRecord* field_0x9a4;
    void*            field_0x9a8;
    u8               field_0x9ac[0x50];
    unk_FlagHolder*  field_0x9fc;
};

extern "C" {
void fn_800438A8( void* unused, unk_LookupCacheOwner* self );
void fn_8004394C();
unk_LookupRecord* fn_800A2A18( void* key, void* table, s32 value );   // defined in auto_03_800A292C_text
}

#endif
