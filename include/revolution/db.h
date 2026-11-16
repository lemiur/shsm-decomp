#ifndef REVOLUTION_DB_H
#define REVOLUTION_DB_H

#include <revolution/types.h>
#include <revolution/os/OSException.h>
#include <revolution/db/DBInterface.h>

#ifdef __cplusplus
extern "C" {
#endif

void DBPrintf(char* str, ...);
BOOL __DBIsExceptionMarked(__OSException exception);

#ifdef __cplusplus
}
#endif

#endif
