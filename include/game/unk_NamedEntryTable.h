#ifndef GAME_UNK_NAMEDENTRYTABLE_H
#define GAME_UNK_NAMEDENTRYTABLE_H

#include <revolution/types.h>

/*
 * One entry of a named table: the name it is looked up by, the case-folded hash of that name and
 * the value it holds. Names invented; the field at 0x0 is not read by this unit.
 */
struct unk_NamedEntry {
    u8                  field_0x0[0x4];     // 0x0
    const char*         name;               // 0x4
    u32                 hash;               // 0x8, RWS::SHUtils::GetStringHashForceLower( name, -1 )
    void*               value;              // 0xC, meaning unknown
};

/*
 * A table of named entries: entries holds count pointers, NULL where a slot is empty. Name invented.
 * The fields at 0x4..0xB are not read by this unit.
 */
struct unk_NamedEntryTable {
    unk_NamedEntry**    entries;            // 0x0
    u8                  field_0x4[0x8];     // 0x4
    int                 count;              // 0xC

    /* True when index is in range and its slot is filled. Name invented. */
    bool                HasEntry( u32 index ) const {
                            if ( index == 0xFFFFFFFF ) {
                                return false;
                            }
                            if ( index >= count ) {
                                return false;
                            }
                            return entries[index] != NULL;
                        }
};

extern "C" const char*     fn_8001A5D4( const unk_NamedEntryTable* table, u32 index );
extern "C" void*           fn_8001A638( const unk_NamedEntryTable* table );
extern "C" void*           fn_8001A674( const unk_NamedEntryTable* table, const char* name );
extern "C" void*           fn_8001A784( const unk_NamedEntryTable* table, u32 index );
extern "C" unk_NamedEntry* fn_8001A7E8( const unk_NamedEntryTable* table, u32 index );

#endif
