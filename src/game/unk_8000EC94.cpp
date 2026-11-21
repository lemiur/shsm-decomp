// unit name unconfirmed: the unit holds the destructors of two intrusive node lists (no string names the
// classes). Both lists' vtables (__vt__12unk_803DFC38 at 0x803DFC38, __vt__12unk_NodeList at 0x803DFC44) lie in this
// unit's .data and the code ranges are adjacent, so the two are one translation unit (split into two units, the
// code order and the data order of the link cannot both hold).

#include <game/unk_8000EC94.h>
#include <game/unk_803DFC38.h>

// The vtable __vt__12unk_NodeList is emitted from the class's virtual destructor.

/*
====================
__dt__12unk_NodeListFv
====================
*/
// 0x8000EC94
// Unlinks and deletes every node. The test on node repeats the loop condition; the retail
// code keeps the test (climax/unk_List.h's Remove has the same test).
unk_NodeList::~unk_NodeList() {
    while ( head != NULL ) {
        unk_ListNode< unk_ListValue >* node = head;
        if ( node != NULL ) {
            if ( node->prev == NULL ) {
                head = node->next;
            } else {
                node->prev->next = node->next;
            }
            if ( node->next == NULL ) {
                tail = node->prev;
            } else {
                node->next->prev = node->prev;
            }
            node->prev = NULL;
            node->next = NULL;
            count--;
            delete node;
        }
    }
}

/*
====================
unk_803DFC38::Remove

Name invented.
====================
*/
inline void unk_803DFC38::Remove( unk_803DFC38Node* node ) {
    if ( node != NULL ) {
        if ( node->next == NULL ) {
            tail = node->prev;
        } else {
            node->next->prev = node->prev;
        }
        if ( node->prev == NULL ) {
            head = node->next;
        } else {
            node->prev->next = node->next;
        }
        node->next = NULL;
        node->prev = NULL;
        count--;
        delete node;
    }
}

/*
====================
unk_803DFC38::~unk_803DFC38

Name invented.
====================
*/
// 0x8000ED6C
unk_803DFC38::~unk_803DFC38() {
    while ( tail != NULL ) {
        Remove( tail );
    }
}
