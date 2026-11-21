#ifndef GAME_UNK_HASHTABLE_H
#define GAME_UNK_HASHTABLE_H

// Names of the table and its entry are invented: no string evidence names the table's class.
// The entry and table layouts are read from fn_800A2A18 (find) and fn_800A2AB8 (append).

/*
 * An entry of a name table (0x14 bytes): a name and its hash, the kind it was appended with and
 * a value. The name is a copy owned by the entry.
 */
struct unk_HashEntry {
    u8              field_0x0[0x4];
    s32             kind;       // 0x4: the third argument of fn_800A2AB8
    char*           name;       // 0x8
    u32             hash;       // 0xC: the hash of the name (GetStringHashForceLower)
    s32             value;      // 0x10
};

/*
 * A name table: an array of entries, the number in use and the number it can hold.
 */
struct unk_HashTable {
    unk_HashEntry*  entries;    // 0x0
    s32             count;      // 0x4
    s32             capacity;   // 0x8
};

extern "C" {
unk_HashEntry*  fn_800A2A18( unk_HashTable* table, const char* name, u32 hash );
unk_HashEntry*  fn_800A2AB8( unk_HashTable* table, const char* name, int kind );
}

#endif
