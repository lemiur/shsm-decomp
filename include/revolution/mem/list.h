#ifndef REVOLUTION_MEM_LIST_H
#define REVOLUTION_MEM_LIST_H

#include <revolution/types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMLink {
    void* prev; // 0x00
    void* next; // 0x04
} MEMLink;

typedef struct MEMList {
    void* head; // 0x00
    void* tail; // 0x04
    u16 num;    // 0x08: objects in the list
    u16 offs;   // 0x0A: offset of the MEMLink inside an object
} MEMList;

void MEMInitList(MEMList* list, u16 offs);
void MEMAppendListObject(MEMList* list, void* obj);
void MEMRemoveListObject(MEMList* list, void* obj);
void* MEMGetNextListObject(MEMList* list, void* obj);

#define MEM_INIT_LIST(list, structName, linkName)                              \
    MEMInitList(list, offsetof(structName, linkName))

#ifdef __cplusplus
}
#endif

#endif
