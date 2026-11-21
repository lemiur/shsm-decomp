#ifndef GAME_UNK_8002B7D4_H
#define GAME_UNK_8002B7D4_H

// unit name unconfirmed: class and member names invented (no string or class-name evidence).
// Layouts come from the retail bodies of fn_8002B7D4, fn_8002BB18, fn_8002BBE8 and fn_8002BC44.

#include <revolution/types.h>
#include <climax/Memory.h>

class unk_PairBase;
struct unk_PairNode;
struct unk_Entry;

/* A counted pointer to a unk_PairBase. Name invented. Its destructor is 0x801336A0. */
struct unk_Ref {
    unk_PairBase*       ptr;        // 0x0
};

/* A node of an entry's pair list: the pair (counted) and its two neighbours. Name invented. */
struct unk_PairNode : public unk_Ref {
    unk_PairNode*       next;       // 0x4
    unk_PairNode*       prev;       // 0x8
};

/*
 * An element of the sequence fn_8002B7D4 pairs up. Name invented; the words before 0x14 are not
 * reconstructed.
 */
struct unk_Entry {
    u8                  field_0x0[0x14];
    int                 pairCount;  // 0x14
    unk_PairNode*       pairHead;   // 0x18
    unk_PairNode*       pairTail;   // 0x1C
};

/*
 * The base of the pair objects: a vptr, a count at 0x4 and the two entries at 0x8 and 0xC. Its
 * constructor is 0x80133818 in another unit.
 */
class unk_PairBase {
public:
                        unk_PairBase( unk_Entry* a, unk_Entry* b );
    virtual             ~unk_PairBase();

    int                 refCount;   // 0x4
    unk_Entry*          first;      // 0x8
    unk_Entry*          second;     // 0xC
    int                 field_0x10;
};

/* A pair object, with the vptr 0x803F5EF8 holds. Name invented. */
class unk_Pair : public unk_PairBase {
public:
                        unk_Pair( unk_Entry* a, unk_Entry* b ) : unk_PairBase( a, b ) {}
    virtual             ~unk_Pair();
};

/* A header word of an entry table: the count of its items. Name invented. */
struct unk_EntryHeader {
    u8                  field_0x0[2];
    u16                 count;      // 0x2
};

/* A table of entries: a header and the item array. Name invented. */
struct unk_EntryTable {
    u8                  field_0x0[4];
    unk_EntryHeader*    header;     // 0x4
    unk_Entry**         items;      // 0x8
};

/* The range of entries that fn_8002B7D4 walks: [begin, end). Name invented. */
struct unk_EntryRange {
    unk_Entry**         begin;      // 0x0
    unk_Entry**         end;        // 0x4
};

extern "C" {
void fn_801336A0( unk_Ref* self, int deleteFlag );
void fn_8002B7D4( unk_EntryRange* range );
unk_Ref* fn_8002BB18( unk_Ref* self, unk_PairBase* ptr );
unk_PairNode* fn_8002BBE8( unk_PairNode* self, int deleteFlag );
void fn_8002BC44( unk_EntryTable* table );
}

#endif
