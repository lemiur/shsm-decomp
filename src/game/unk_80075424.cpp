// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80075424.h>
#include <climax/Memory.h>

/*
====================
fn_8007552C
====================
*/
// 0x8007552C
unk_ListItem* fn_8007552C(unk_ItemTable* table, u32 key) {
    unk_ListItem* found = NULL;
    for (unk_ItemNode* node = table->head; found == NULL && node != NULL; node = node->next) {
        if (node->data->key == key) {
            found = node->data;
        }
    }
    return found;
}

/*
====================
fn_80075564
====================
*/
// 0x80075564
// Wall (register/schedule, 96%): control flow is isomorphic to retail; the item and the loop's
// temporary swap registers (r3 and r5).
void fn_80075564(unk_ItemTable* table, u32 value) {
    unk_ItemNode* node = table->head;
    unk_ListItem* item = NULL;
    while (item == NULL && node != NULL) {
        if (node->data->key == 0) {
            item = node->data;
        }
        node = node->next;
    }
    item->key = value;
}

/*
====================
fn_800755A0
====================
*/
// 0x800755A0
// Wall (control flow, 91.67%): retail tests the item a second time before the source check
// (three consecutive tests before the allocation); the source form of that test is not known.
void fn_800755A0(unk_ItemList* list, unk_ListSource* source) {
    unk_ListItem* item = source->item;
    if (source != NULL && item != NULL) {
        unk_ItemNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ItemNode( item );
        node->prev = list->tail;
        node->next = NULL;
        if (list->tail != NULL) {
            list->tail->next = node;
        }
        if (list->head == NULL) {
            list->head = node;
        }
        list->tail = node;
        list->count++;
        source->item = NULL;
    }
    source->field_0x0 = 0;
}

/*
====================
fn_80075424
====================
*/
// 0x80075424
// Not reconstructed: the loop strides 0x18 over the object while its fields sit at 0x29..0x38,
// so the nine slots overlap in a way no struct array describes (struct layout wall).
void fn_80075424() {
    // TODO
}
