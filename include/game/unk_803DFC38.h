#ifndef GAME_UNK_803DFC38_H
#define GAME_UNK_803DFC38_H

// unit name unconfirmed: the class and member names are invented (no string names it). Its
// vtable __vt__12unk_803DFC38 holds two empty slots and the destructor, which deletes the list.

#include <revolution/types.h>

/* A node of unk_803DFC38: the word the destructor never reads, and its two neighbours. */
struct unk_803DFC38Node {
    void*               field_0x0;  // 0x00
    unk_803DFC38Node*   prev;       // 0x04
    unk_803DFC38Node*   next;       // 0x08
};

/*
 * A doubly linked list that owns its nodes. It is embedded at a fixed offset in a larger object
 * (no out-of-line constructor here). The destructor frees the nodes from the tail back.
 */
class unk_803DFC38 {
public:
    virtual         ~unk_803DFC38();

    /* Unlinks node and deletes it; does nothing for NULL. */
    void            Remove( unk_803DFC38Node* node );

    int                 count;  // 0x04
    unk_803DFC38Node*   tail;   // 0x08
    unk_803DFC38Node*   head;   // 0x0C
};

#endif
