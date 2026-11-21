// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800A292C.h>
#include <climax/SHUtils.h>

// The entry of the table whose name and id both match, or NULL.
static inline unk_LookupEntry* FindEntry(unk_NamedTable* table, const char* name, u32 id) {
    if (name != NULL && name[0] != '\0') {
        for (s32 i = 0; i < table->count; i++) {
            unk_LookupEntry* entry = &table->entries[i];
            if (id == entry->id && UTILS::StringsEqual(name, entry->name)) {
                return &table->entries[i];
            }
        }
    }
    return NULL;
}

/*
====================
fn_800A292C
====================
Cached lookup: the slot keeps the entry found for its name and id in this table; a slot that
was searched in another table is searched again.
Open: 99.49%, the register/schedule residue of the inlined FindEntry (the id and the offset
swap registers).
*/
// 0x800A292C
extern "C" bool fn_800A292C(unk_NamedTable* table, unk_LookupSlot* slot, s32* out) {
    unk_LookupEntry* entry = (table == slot->table) ? slot->entry : NULL;
    if (entry == NULL) {
        const char* name = slot->name;
        u32 id = slot->id;
        entry = FindEntry(table, name, id);
        slot->entry = entry;
        slot->table = table;
    }
    if (entry != NULL) {
        *out = entry->index;
        return true;
    }
    return false;
}

/*
====================
fn_800A2A18
====================
Finds the entry of the table whose name and id both match.
*/
// 0x800A2A18
extern "C" unk_LookupEntry* fn_800A2A18(unk_NamedTable* table, const char* name, u32 id) {
    if (name != NULL && name[0] != '\0') {
        for (s32 i = 0; i < table->count; i++) {
            unk_LookupEntry* entry = &table->entries[i];
            if (id == entry->id && UTILS::StringsEqual(name, entry->name)) {
                return &table->entries[i];
            }
        }
    }
    return NULL;
}

/*
====================
fn_800A2AB8
====================
Appends an entry at the end of the table, copying the name and hashing it; the entry slot's
old name is freed first.
*/
// 0x800A2AB8
extern "C" unk_LookupEntry* fn_800A2AB8(unk_NamedTable* table, const char* name, s32 index) {
    if (table->count < table->capacity) {
        unk_LookupEntry* entry = &table->entries[table->count];
        if (entry->name != NULL) {
            delete entry->name;
            entry->name = NULL;
            entry->id = 0;
        }
        if (name != NULL) {
            entry->name = fn_801B67FC(name, 0);
            entry->id = RWS::SHUtils::GetStringHashForceLower(entry->name, -1);
        }
        entry->index = index;
        table->count++;
        return entry;
    }
    return NULL;
}
