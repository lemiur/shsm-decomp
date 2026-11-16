#ifndef GAME_UNK_CLUMPSWAPLIST_H
#define GAME_UNK_CLUMPSWAPLIST_H

#include <climax/Clump.h>

/*
 * The list of the clump swaps in effect, by name (unit from 0x8009EFA0 not yet
 * reconstructed; 0x8009EFA0 creates the one instance lbl_805297D0, a 0xC-byte object of three
 * zeroed words, and 0x8009F1A0 walks its entries from 0x0 to 0x4 comparing each entry's name).
 * Name invented.
 */
class unk_ClumpSwapList {
public:
    // defined in 0x8009EFE8: puts clump into effect under name
    void    fn_8009EFE8( unk_RpClump* clump, const char* name );
    // defined in 0x8009F1A0: drops the swap filed under name
    void    fn_8009F1A0( const char* name );
};

// defined in the list's unit, not yet reconstructed
extern unk_ClumpSwapList* lbl_805297D0;

#endif
