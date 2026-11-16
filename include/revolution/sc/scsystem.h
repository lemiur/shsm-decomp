#ifndef REVOLUTION_SC_SCSYSTEM_H
#define REVOLUTION_SC_SCSYSTEM_H

#include <revolution/sc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-internal declarations owned by revolution/sc/scsystem.c. */

void DeleteItemByID(SCItemID id);
BOOL CreateItemByID(SCItemID id, SCType type, const u8* data, u32 size);

#ifdef __cplusplus
}
#endif

#endif
