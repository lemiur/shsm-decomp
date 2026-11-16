// Reference: zeldaret/tp libs/revolution/src/mem/mem_list.c
// Reference: doldecomp/ogws src/revolution/MEM/mem_list.c (MEMRemoveListObject)
#include <revolution/mem.h>

#define GetLink(list, obj) ((MEMLink*)(((u32)(obj)) + (list)->offs))

/*
 * MEMInitList
 *
 * Empties a list whose objects carry their MEMLink at offset offs.
 */
void MEMInitList(MEMList* list, u16 offs) {
    list->head = NULL;
    list->tail = NULL;
    list->num = 0;
    list->offs = offs;
} // 0x802400B0

/*
 * MEMAppendListObject
 *
 * Adds an object at the tail of the list.
 */
void MEMAppendListObject(MEMList* list, void* obj) {
    MEMLink* link;

    if (list->head == NULL) {
        link = GetLink(list, obj);
        link->next = NULL;
        link->prev = NULL;
        list->head = obj;
        list->tail = obj;
        list->num++;
    } else {
        link = GetLink(list, obj);
        link->prev = list->tail;
        link->next = NULL;

        GetLink(list, list->tail)->next = obj;
        list->tail = obj;
        list->num++;
    }
} // 0x802400D0

/*
 * MEMRemoveListObject
 *
 * Unlinks an object from the list.
 */
void MEMRemoveListObject(MEMList* list, void* obj) {
    MEMLink* link = GetLink(list, obj);

    if (link->prev == NULL) {
        list->head = link->next;
    } else {
        GetLink(list, link->prev)->next = link->next;
    }

    if (link->next == NULL) {
        list->tail = link->prev;
    } else {
        GetLink(list, link->next)->prev = link->prev;
    }

    link->prev = NULL;
    link->next = NULL;
    list->num--;
} // 0x80240140

/*
 * MEMGetNextListObject
 *
 * Returns the object after obj, or the head when obj is NULL.
 */
void* MEMGetNextListObject(MEMList* list, void* obj) {
    if (obj == NULL) {
        return list->head;
    }

    return GetLink(list, obj)->next;
} // 0x802401B0
