#ifndef GAME_UNK_80011054_H
#define GAME_UNK_80011054_H

#include <revolution/types.h>

// One 16-byte entry of an expression list. The name is at 0x4 (fn_80011054 reads it); the other
// words are not read by this unit. Member names invented.
struct unk_ExpressionEntry {
    u32 field_0x0;
    const char* name;
    u32 field_0x8;
    u32 field_0xC;
};

// The list the entries live in: the entry count is a short at 0x18 and the entry array is at
// 0x1c. Class and member names invented; the type is not recovered beyond these two fields.
struct unk_ExpressionList {
    u8 field_0x0[0x18];
    s16 count;
    s16 field_0x1A;
    unk_ExpressionEntry* entries;
};

// The value a named entry holds: its text is at 0x0, read as a pointer. Names invented.
struct unk_ExpressionValue {
    const char* text;
};

// defined in a unit not yet reconstructed: the entry at a given index, 16 bytes each
extern "C" unk_ExpressionEntry* fn_80188208(unk_ExpressionList* list, int index);
// defined in a unit not yet reconstructed: the value of a named key in an entry
extern "C" unk_ExpressionValue* fn_80189D10(unk_ExpressionEntry* entry, const char* key);
// defined in a unit not yet reconstructed: writes its text into the output object
extern "C" void fn_801B651C(void* out, const char* text);

// Counts the entries whose name ends in "Expression" and reports the text of their "ExpName"
// key to out. Returns the size those entries take in out: 0x2c plus 16 per matched entry. The
// second argument is not read: the caller passes its own object there.
int fn_80011054(unk_ExpressionList* list, void* unused, void* out);

#endif
