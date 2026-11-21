// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Not matched yet: fn_8001A100 (7%), fn_8001A428 (76%) and fn_8001A520 (99%, register numbering
// only); the walls are in each function's header comment.
// Its vtable is __vt__12unk_803E0A48 (lbl_803E0A48, defined in unit auto_03_8001759C).

#include <game/unk_AnimController.h>
#include <climax/Heap.h>
#include <climax/SHUtils.h>
#include <climax/Utils.h>

/*
 * The search shared by fn_8001A100 and fn_8001A520: the first live slot whose hash and name
 * match, or -1 for an empty name and for a name that starts with '*'.
 */
static inline int FindSlot( unk_AnimController* table, const char* name, u32 hash ) {
    if ( name != NULL && *name != '\0' && *name != '*' ) {
        unk_803E0A48* entry;
        for ( int i = 0; i < table->slotCount; i++ ) {
            entry = table->entries[i];
            if ( entry != NULL && hash == entry->nameHash && fn_801B6138( name, entry->name ) ) {
                return i;
            }
        }
    }
    return -1;
}

/*
 * True when index names a live slot of table.
 */
static inline bool IsLiveSlot( unk_AnimController* table, int index ) {
    return (u32)index != 0xFFFFFFFFu && (u32)index < (u32)table->slotCount && table->entries[index] != NULL;
}

/*
 * A new entry for name and payload: the name is copied and hashed from the copy.
 */
static inline unk_803E0A48* NewEntry( const char* name, int payload ) {
    unk_803E0A48* entry = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E0A48;
    if ( entry != NULL ) {
        if ( name != NULL ) {
            char* copy = fn_801B67FC( name, 0 );
            entry->name = copy;
            entry->nameHash = RWS::SHUtils::GetStringHashForceLower( copy, -1 );
        }
        entry->field_0xC = payload;
    }
    return entry;
}

/*
====================
unk_AnimController::fn_8001A100
====================
*/
// Wall (wall.exe): referent and call set (retail has a __dl__ cleanup in each new-expression
// landing pad, not reproduced by -Cpp_exceptions on), control flow (return-block placement), register/schedule.
// 0x8001A100
int unk_AnimController::fn_8001A100( const char* name, int payload, bool reuseHole ) {
    if ( name == NULL || *name == '\0' || payload == 0 ) {
        return -1;
    }
    u32 hash = RWS::SHUtils::GetStringHashForceLower( name, -1 );
    int index = FindSlot( this, name, hash );
    if ( index != -1 ) {
        return index;
    }

    if ( reuseHole && slotCount != liveCount ) {
        int hole = 0;
        while ( entries[hole] != NULL ) {
            hole++;
        }
        entries[hole] = NewEntry( name, payload );
        liveCount++;
        return hole;
    }

    unk_803E0A48* entry = NewEntry( name, payload );

    int count = entriesEnd - entries;
    int capCount = entriesCapEnd - entries;
    if ( capCount < count + 1 ) {
        unk_803E0A48** grown = NULL;
        int bytes = ( count + 1 ) * 4;
        if ( bytes != 0 ) {
            grown = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E0A48*[ count + 1 ];
        }
        unk_803E0A48** dst = grown;
        for ( unk_803E0A48** src = entries; src != entriesEnd; src++ ) {
            *dst++ = *src;
        }
        unk_803E0A48** oldBegin = entries;
        entriesCapEnd = grown + ( count + 1 );
        entriesEnd = grown + count;
        SafeDeleteArray( oldBegin );
        entries = grown;
    }

    int newIndex = slotCount;
    if ( entriesEnd != NULL ) {
        *entriesEnd = entry;
    }
    entriesEnd++;
    slotCount++;
    liveCount++;
    return newIndex;
}

/*
====================
unk_AnimController::fn_8001A428
====================
*/
// Wall: control flow. Retail materialises the `valid` chain as a value into r0 with one merge
// (the last term as neg/or/srwi); the compiled body threads branches instead. An inline
// IsLiveSlot helper and a bool local give the same threading.
// 0x8001A428
bool unk_AnimController::fn_8001A428( int index ) {
    bool valid = IsLiveSlot( this, index );
    if ( valid ) {
        unk_803E0A48* entry = entries[index];
        entries[index] = NULL;
        delete entry;
        liveCount--;
        while ( slotCount > 0 && entries[slotCount - 1] == NULL ) {
            entriesEnd--;
            slotCount--;
        }
        return true;
    }
    return false;
}

/*
====================
unk_AnimController::fn_8001A520
====================
*/
// Wall: register/schedule only. Equivalent by renaming: retail keeps the slot index in r29, the hash
// in r30 and the slot offset in r31; the compiled body allocates the three the other way round.
// 0x8001A520
int unk_AnimController::fn_8001A520( const char* name, bool ) {
    u32 hash = RWS::SHUtils::GetStringHashForceLower( name, -1 );
    return FindSlot( this, name, hash );
}
