// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_NamedEntryTable.h>
#include <climax/SHUtils.h>
#include <climax/Utils.h>

/*
====================
fn_8001A5D4

The name of entry index of table, or "-" when the index is out of range or the slot is empty.
====================
*/
// 0x8001A5D4
extern "C" const char* fn_8001A5D4( const unk_NamedEntryTable* table, u32 index ) {
    if ( table->HasEntry( index ) ) {
        return table->entries[index]->name;
    }
    return "-";
}

/*
====================
fn_8001A638

The value of the first filled slot of table, or NULL when every slot is empty.
Open: register/schedule only (retail keeps the loop index and the table base in r5/r4 against
the compiled r4/r5, under the same control flow); the retail assembly decides.
====================
*/
// 0x8001A638
extern "C" void* fn_8001A638( const unk_NamedEntryTable* table ) {
    for ( int i = 0; i < table->count; i++ ) {
        unk_NamedEntry* entry = table->entries[i];
        if ( entry != NULL ) {
            return entry->value;
        }
    }
    return NULL;
}

/* The index of the first entry of table whose name is name (case ignored), or -1. Name invented. */
static inline int FindIndex( const unk_NamedEntryTable* table, const char* name ) {
    u32 hash = RWS::SHUtils::GetStringHashForceLower( name, -1 );
    if ( name != NULL && name[0] != '\0' && name[0] != '*' ) {
        for ( int index = 0; index < table->count; index++ ) {
            unk_NamedEntry* entry = table->entries[index];
            if ( entry != NULL && entry->hash == hash && fn_801B6138( name, entry->name ) ) {
                return index;
            }
        }
    }
    return -1;
}

/*
====================
fn_8001A674

The value of the first entry of table whose name is name (case ignored), or NULL. A name that is
empty or starts with '*' finds nothing.
Open: register/schedule only (the hash, index and loop offset sit in r29, r28 and r30 in retail, and
the compiled code takes a different colouring of the same control flow); the retail assembly decides.
====================
*/
// 0x8001A674
extern "C" void* fn_8001A674( const unk_NamedEntryTable* table, const char* name ) {
    int index = FindIndex( table, name );
    if ( table->HasEntry( index ) ) {
        return table->entries[index]->value;
    }
    return NULL;
}

/*
====================
fn_8001A784

The value of entry index of table, or NULL when the index is out of range or the slot is empty.
====================
*/
// 0x8001A784
extern "C" void* fn_8001A784( const unk_NamedEntryTable* table, u32 index ) {
    if ( table->HasEntry( index ) ) {
        return table->entries[index]->value;
    }
    return NULL;
}

/*
====================
fn_8001A7E8

The entry at index of table, or NULL when the index is out of range or the slot is empty.
====================
*/
// 0x8001A7E8
extern "C" unk_NamedEntry* fn_8001A7E8( const unk_NamedEntryTable* table, u32 index ) {
    if ( table->HasEntry( index ) ) {
        return table->entries[index];
    }
    return NULL;
}
