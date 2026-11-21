#ifndef GAME_UNK_TABLEENTRY_H
#define GAME_UNK_TABLEENTRY_H

#include <revolution/types.h>

// Types and globals of the table that fn_80015358 appends to and fn_80015598 walks. Names and roles are
// invented from the retail accesses; the layout is the offsets the code uses.

// Entry of the 8-byte table lbl_8045A7E0 (its count is lbl_805291B8): a value and the number
// of records it creates.
struct unk_TableEntry {        // 0x8 bytes
    u32 value;                 // +0x0
    u32 count;                 // +0x4
};

// Globals of this unit's neighbourhood. Their owning unit has not been reconstructed yet; the first
// reconstructed user that takes them defines them.
extern unk_TableEntry lbl_8045A7E0[8];
extern u32 lbl_805291B8;
extern s32 lbl_805291C4;
extern s32 lbl_805291C8;

#endif
