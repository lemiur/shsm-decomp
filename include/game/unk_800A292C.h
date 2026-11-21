#ifndef GAME_UNK_800A292C_H
#define GAME_UNK_800A292C_H

#include <revolution/types.h>
#include <game/unk_8005549C.h>

// Callees outside this unit.
class UTILS {
public:
    static BOOL StringsEqual(const char* a, const char* b);
};
const char* fn_801B67FC(const char* str, int maxLength);

extern "C" {
// Cached lookup of a named entry: fills *out with the entry's index and returns true when found.
bool fn_800A292C(unk_NamedTable* table, unk_LookupSlot* slot, s32* out);
// Lookup of a named entry by name and id.
unk_LookupEntry* fn_800A2A18(unk_NamedTable* table, const char* name, u32 id);
// Appends an entry (copy of the name, index); returns it, or NULL when the table is full.
unk_LookupEntry* fn_800A2AB8(unk_NamedTable* table, const char* name, s32 index);
}

#endif
