#ifndef GAME_UNK_8000EC94_H
#define GAME_UNK_8000EC94_H

#include <revolution/types.h>
#include <climax/unk_List.h>

/*
 * The element of unk_NodeList: an 8-byte value that the list code never reads (the node is
 * 0x10 bytes: the value, then next and prev). Name invented.
 */
struct unk_ListValue {
    u32             field_0x0;
    u32             field_0x4;
};

/*
 * An intrusive list that owns its nodes: the destructor unlinks and deletes every node.
 * Name and member names invented; the list has the layout of climax/unk_List.h's template
 * for an 8-byte element, and its vtable is __vt__12unk_NodeList.
 */
class unk_NodeList {
public:
                    virtual ~unk_NodeList();

    int             count;          // 0x4
    unk_ListNode< unk_ListValue >*  head;   // 0x8
    unk_ListNode< unk_ListValue >*  tail;   // 0xC
};

#endif
