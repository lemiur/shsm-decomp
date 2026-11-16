#ifndef REVOLUTION_NDEVEXI2AD_DEBUGGERDRIVER_H
#define REVOLUTION_NDEVEXI2AD_DEBUGGERDRIVER_H

#include <revolution/types.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/NdevExi2AD/exi2.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reference: doldecomp/ogws include/revolution/NdevExi2AD/DebuggerDriver.h */
typedef void (*DBMtrCallback)(s32 unused, OSContext* context);
typedef void (*DBIntrCallback)(__OSInterrupt interrupt, OSContext* context);

void DBInitComm(u8** flagOut, DBMtrCallback handler);
void DBInitInterrupts(void);
u32 DBQueryData(void);
BOOL DBRead(void* dst, u32 size);
BOOL DBWrite(const void* src, u32 size);
void DBOpen(void);
void DBClose(void);

#ifdef __cplusplus
}
#endif

#endif
