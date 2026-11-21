#ifndef GAME_UNK_80075424_H
#define GAME_UNK_80075424_H

#include <revolution/types.h>
#include <climax/unk_List.h>

// The item a list node holds. Its first word is a key: fn_8007552C finds the item whose key is
// the value it is given, and fn_80075564 claims the first item whose key is zero. The other
// words are not read by this unit. Name and member names invented.
struct unk_ListItem {
    u32 key;                        // 0x0
};

// A node of a linked list of items: climax/unk_List.h's node, with the item as its value. The
// next link is read by both list walks; prev is written and read only by fn_800755A0, which
// appends a node.
typedef unk_ListNode< unk_ListItem* > unk_ItemNode;

// The owner of the list fn_8007552C and fn_80075564 walk: its head node is at 0x10. Name and
// the other words invented; the code reads nothing else of it.
struct unk_ItemTable {
    u8 field_0x0[0x10];
    unk_ItemNode* head;             // 0x10
};

// The list fn_800755A0 appends to: a count, a head and a tail. Name and the other words
// invented.
struct unk_ItemList {
    u8 field_0x0[0x1C];
    u32 count;                      // 0x1C
    unk_ItemNode* head;             // 0x20
    unk_ItemNode* tail;             // 0x24
};

// The record fn_800755A0 takes an item from: the item it hands over is at 0xC, and its word at
// 0x0 is cleared on every path. Name and the other words invented.
struct unk_ListSource {
    u32 field_0x0;                  // 0x0, cleared by fn_800755A0
    u8 field_0x4[0x8];
    unk_ListItem* item;             // 0xC, cleared when the item is appended
};

// Returns the item of the table whose key is key, or NULL when no node holds one.
unk_ListItem* fn_8007552C(unk_ItemTable* table, u32 key);

// Finds the first item whose key is zero and stores value in it. When no item is free the
// store goes to address 0, as the original does: the table is sized so that a free item
// exists.
void fn_80075564(unk_ItemTable* table, u32 value);

// Appends the item of source to list and clears the source's item (see unk_ListSource).
void fn_800755A0(unk_ItemList* list, unk_ListSource* source);

// The clump loader of the 9 slots; not yet matched.
void fn_80075424();

#endif
