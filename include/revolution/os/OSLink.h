#ifndef REVOLUTION_OS_OSLINK_H
#define REVOLUTION_OS_OSLINK_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSModuleInfo OSModuleInfo;

typedef struct OSModuleQueue {
    OSModuleInfo* head;
    OSModuleInfo* tail;
} OSModuleQueue;

/* Loaded relocatable modules and the string table of their names, in low memory. */
OSModuleQueue __OSModuleInfoList : 0x800030C8;
const void* __OSStringTable : 0x800030D0;

void __OSModuleInit(void);

#ifdef __cplusplus
}
#endif

#endif
