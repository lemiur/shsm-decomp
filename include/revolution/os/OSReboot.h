#ifndef REVOLUTION_OS_OSREBOOT_H
#define REVOLUTION_OS_OSREBOOT_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void __OSReboot(u32 resetCode, u32 bootDol);
void __OSGetSaveRegion(void** start, void** end);

#ifdef __cplusplus
}
#endif

#endif
