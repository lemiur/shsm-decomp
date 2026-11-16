#ifndef NW4HBM_UT_LIST_H
#define NW4HBM_UT_LIST_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/list.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {

/*
 * List
 *
 * Intrusive doubly linked list of arbitrary objects. Each object carries a Link
 * at byte offset `offset`.
 */
struct List {
    void* headObject; // 0x00 first object, NULL when empty
    void* tailObject; // 0x04 last object, NULL when empty
    u16 numObjects;   // 0x08
    u16 offset;       // 0x0A byte offset of the Link inside each object
}; // size 0x0C

/*
 * Link
 *
 * The list node embedded in each object of a List.
 */
struct Link {
    void* prevObject; // 0x00
    void* nextObject; // 0x04
}; // size 0x08

void List_Init(List* list, u16 offset);
void List_Append(List* list, void* object);
void List_Prepend(List* list, void* object);
void List_Insert(List* list, void* target, void* object);
void List_Remove(List* list, void* object);
void* List_GetNext(const List* list, const void* object);
void* List_GetPrev(const List* list, const void* object);
void* List_GetNth(const List* list, u16 index);

inline void* List_GetFirst(const List* list) {
    return List_GetNext(list, NULL);
}

inline u16 List_GetSize(const List* list) {
    return list->numObjects;
}

} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_LIST_H
