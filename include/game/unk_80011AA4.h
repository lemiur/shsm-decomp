#ifndef GAME_UNK_80011AA4_H
#define GAME_UNK_80011AA4_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bastream.h>

/*
 * unk_Item: the 0x18-byte record fn_80011AA4 attaches. Its layout and unit are not yet reconstructed
 * (constructor fn_80187458, destructor fn_80187470, loader fn_80187504). Name invented.
 */
struct unk_Item;

/*
 * unk_80011AA4: an 8-byte record, a tag word and the item it holds. Constructor inlined in
 * fn_80011AA4. Name and member names invented.
 */
struct unk_80011AA4 {
    RwUInt32 tag;     /* 0x4D494E41, the four bytes "MINA" */
    unk_Item* item;
};

/* Out-of-line functions of the item, defined in the unit that owns fn_80187458 (not yet reconstructed). */
extern "C" {
unk_Item* fn_80187458( unk_Item* item );              /* constructor: zeroes the record, returns it */
void fn_80187470( unk_Item* item, int freeRecord );   /* destructor: frees the record when freeRecord > 0 */
void fn_80187504( unk_Item* item, void* data );       /* loads the item from its chunk data */
}

extern "C" {
unk_80011AA4* fn_80011AA4( RwStream* stream, RwUInt32 chunkSize );
void fn_80011B74( unk_80011AA4* record );
}

#endif
