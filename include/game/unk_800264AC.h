#ifndef GAME_UNK_800264AC_H
#define GAME_UNK_800264AC_H

// unit name unconfirmed: header named after the function's address.

#include <revolution/types.h>

class CZone;    // climax/AttributeHandler.h

/*
 * Defined at 0x800264AC, in the CZone unit (not yet reconstructed): returns the word at 0x94 of
 * zone, the name its save block is filed under (callers pass it to fn_802050AC).
 */
const char* fn_800264AC( CZone* zone );

#endif
