// Unit name and boundary: class-name evidence (CEnemy registration string and factory); TU name unconfirmed

#include <game/CEnemy.h>
#include <climax/Heap.h>

/*
 * Static state of this unit. The static initialiser (0x80052E58) is compiled from these
 * definitions, so it has no source of its own.
 * Open: the initialiser differs from retail (its base is the 0x80469BA0 object, with the list at
 * +0x18 and a second destructor-registered member at +0xC, neither reconstructed yet).
 */

u32 lbl_80529348 = fn_801A7A84( "CEnemy" );
unk_801A8F6C lbl_8052934C( "CEnemy", fn_80052F3C, 0xB60 );
unk_NodeList lbl_80469BB8;
unk_Compare lbl_80529350;
u32 lbl_80529358 = fn_801A7A84( "CEnemySpawn" );
unk_801A8F6C lbl_8052935C( "CEnemySpawn", fn_8005305C, 0x50 );

// 0x80052F3C
CAttributeHandler* fn_80052F3C( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) CEnemy( packet );
}

// 0x80052F84
// Open: retail carries a second copy of unk_NodeList's destructor (the owner is
// src/game/unk_8000EC94.cpp at 0x8000EC94); the body is the same, its emitted owner unresolved.
unk_NodeList::~unk_NodeList() {
    while ( head != NULL ) {
        unk_ListNode< unk_ListValue >* node = head;

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
