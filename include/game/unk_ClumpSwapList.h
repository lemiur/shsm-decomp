#ifndef GAME_UNK_CLUMPSWAPLIST_H
#define GAME_UNK_CLUMPSWAPLIST_H

#include <climax/Clump.h>
#include <climax/Utils.h>

/*
 * One clump swap in effect: the name it is filed under (a copy the list makes) and the clump it
 * puts into effect (owned by the CClumpSwap that filed it). Names invented.
 */
struct unk_ClumpSwapEntry {
    char*           name;       // 0x0
    unk_RpClump*    clump;      // 0x4
};

/*
 * The list of the clump swaps in effect, by name. The one instance is lbl_805297D0. The entries
 * are kept in an array of pointers that grows at the back, between begin and capacity, with end
 * after the last entry. Name invented.
 */
class unk_ClumpSwapList {
public:
                    unk_ClumpSwapList() :
                        begin( NULL ),
                        end( NULL ),
                        capacity( NULL ) {}

    /* The number of entries and the number the array holds. Names invented. */
    u32             Count() const {
                        return end - begin;
                    }

    u32             Capacity() const {
                        return capacity - begin;
                    }

    /* The index of the entry filed under name, -1 when none is. Name invented. */
    int             IndexOf( const char* name ) const {
                        int i;
                        for ( i = 0; i < Count(); i++ ) {
                            if ( fn_801B6138( name, begin[i]->name ) ) {
                                return i;
                            }
                        }
                        return -1;
                    }

    // defined in 0x8009EFE8: puts clump into effect under name; false when name is already in effect
    bool            fn_8009EFE8( unk_RpClump* clump, const char* name );
    // defined in 0x8009F1A0: drops the swap filed under name; false when none is filed
    bool            fn_8009F1A0( const char* name );
    // defined in 0x8009F298: the clump filed under name, or NULL
    unk_RpClump*    fn_8009F298( const char* name );

    unk_ClumpSwapEntry**    begin;      // 0x0
    unk_ClumpSwapEntry**    end;        // 0x4
    unk_ClumpSwapEntry**    capacity;   // 0x8
};

extern unk_ClumpSwapList* lbl_805297D0;

#endif
