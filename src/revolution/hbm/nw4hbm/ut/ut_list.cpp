// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_list.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_list.cpp
#include "nw4hbm/ut/list.h"

// The Link of `object` in `list`.
#define ObjectToLink_(list, object) ((Link*)((u32)(object) + (list)->offset))

namespace nw4hbm {
namespace ut {

/*
 * List_Init
 *
 * Empties a list whose objects carry their Link at byte offset `offset`.
 */
void List_Init(List* list, u16 offset) {
    list->headObject = NULL;
    list->tailObject = NULL;
    list->numObjects = 0;
    list->offset = offset;
} // 0x803CDFF0

/*
 * SetFirstObject
 *
 * Makes `object` the only element of the empty `list`.
 */
static void SetFirstObject(List* list, void* object) {
    Link* link = ObjectToLink_(list, object);

    link->nextObject = NULL;
    link->prevObject = NULL;

    list->headObject = object;
    list->tailObject = object;
    list->numObjects++;
}

/*
 * List_Append
 *
 * Adds `object` at the tail of `list`.
 */
void List_Append(List* list, void* object) {
    if (list->headObject == NULL) {
        SetFirstObject(list, object);
        return;
    }

    Link* link = ObjectToLink_(list, object);
    link->prevObject = list->tailObject;
    link->nextObject = NULL;

    ObjectToLink_(list, list->tailObject)->nextObject = object;
    list->tailObject = object;
    list->numObjects++;
} // 0x803CE010

/*
 * List_Remove
 *
 * Unlinks `object` from `list` and clears its Link.
 */
void List_Remove(List* list, void* object) {
    Link* link = ObjectToLink_(list, object);

    if (link->prevObject == NULL) {
        list->headObject = link->nextObject;
    } else {
        ObjectToLink_(list, link->prevObject)->nextObject = link->nextObject;
    }

    if (link->nextObject == NULL) {
        list->tailObject = link->prevObject;
    } else {
        ObjectToLink_(list, link->nextObject)->prevObject = link->prevObject;
    }

    link->prevObject = NULL;
    link->nextObject = NULL;
    list->numObjects--;
} // 0x803CE080

/*
 * List_GetNext
 *
 * The object after `object`, or the head of `list` when `object` is NULL.
 */
void* List_GetNext(const List* list, const void* object) {
    if (object == NULL) {
        return list->headObject;
    }

    return ObjectToLink_(list, object)->nextObject;
} // 0x803CE0F0

/*
 * List_GetNth
 *
 * The object at position `index`, or NULL when the list is shorter.
 */
void* List_GetNth(const List* list, u16 index) {
    int count;
    void* object;

    for (count = 0, object = NULL; (object = List_GetNext(list, object)) != NULL; count++) {
        if (index == count) {
            return object;
        }
    }

    return NULL;
} // 0x803CE110

} // namespace ut
} // namespace nw4hbm
