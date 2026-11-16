#ifndef GAME_UNK_800276E0_H
#define GAME_UNK_800276E0_H

// unit name unconfirmed: header named after the function's address.

#include <revolution/types.h>
#include <climax/Atomic.h>

class CZone;    // climax/AttributeHandler.h

/*
 * Defined at 0x800276E0, in the CZone unit (not yet reconstructed): adds atomic to the world at
 * 0xA4 of zone (RpWorldAddAtomic).
 */
void fn_800276E0( CZone* zone, unk_RpAtomic* atomic );

/*
 * Defined at 0x800276D0, in the CZone unit (not yet reconstructed): the word at 0xC4 of zone
 * (CPsyPointOfInterest tests it, then passes it to fn_801D43E8).
 */
u32 fn_800276D0( CZone* zone );

#endif
