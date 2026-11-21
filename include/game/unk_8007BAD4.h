#ifndef GAME_UNK_8007BAD4_H
#define GAME_UNK_8007BAD4_H

// unit name unconfirmed: the class is named for its constructor's address (0x8007BAD4); no string
// names it. Member and type names are invented.

#include <revolution/types.h>
#include <climax/unk_801A92A8.h>

/* The heap record a slot owns through its pointer. Only its message id at 0xB4 is known. Name invented. */
struct unk_SlotRecord {
    u8                  field_0x0[0xB4];
    unk_801A92A8        message;        // 0xB4, the message id the record registers
};

/* One element of a two-element array in the object at 0xA030 of its owner. */
class unk_8007BAD4 {
public:
            unk_8007BAD4();     // 0x8007BAD4
            ~unk_8007BAD4();    // 0x8007BAF4

    unk_SlotRecord*     record;         // 0x0, owned; NULL when the slot is empty
    float               field_0x4;      // 0x4
    float               field_0x8;      // 0x8
    float               field_0xC;      // 0xC
    u8                  field_0x10;     // 0x10
};

#endif
