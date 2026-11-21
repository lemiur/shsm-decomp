#ifndef GAME_UNK_80015F8C_H
#define GAME_UNK_80015F8C_H

#include <revolution/types.h>
#include <game/CGenericSpawner.h>

// unit name unconfirmed and type names invented: no string or class-name evidence
// names these structures or their members.

// A named descriptor. Its first bytes are the name; the word at +0x44 is read into
// the table entry on a match.
struct unk_NameDesc {
    char name[0x44];
    u32 field_0x44;
};

// One table entry, 0x3c bytes. The name is copied into `name` when the entry is bound.
struct unk_NameEntry {
    unk_NameDesc* desc;
    char name[0x20];
    u32 field_0x24;
    u32 field_0x28;
    u32 field_0x2c;
    u8 field_0x30;
    void* field_0x34;
    void* field_0x38;
};

struct unk_NameTableHeader {
    u32 field_0x0;
    u32 count;
};

// The table object: the header holds the entry count, the array holds the entries.
struct unk_NameTable {
    u8 field_0x0[0xC];
    unk_NameTableHeader* header;
    unk_NameEntry* entries;
};

extern "C" {
void fn_8001590C(unk_NameTable* table, u32 value, u32 mode, void* ctx);
bool fn_80015F8C(unk_NameTable* table, const char* name, u8 flag, void* ctxA, void* ctxB);
}


#endif
