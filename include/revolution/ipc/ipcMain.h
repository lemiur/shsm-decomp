#ifndef REVOLUTION_IPC_IPCMAIN_H
#define REVOLUTION_IPC_IPCMAIN_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hollywood IPC registers (Starlet mailbox), placed by address. */
vu32 __IPCRegs[] : 0xCD000000; // volatile: hardware registers

void IPCInit(void);
void IPCReInit(void);
u32 IPCReadReg(u32 regIdx);
void IPCWriteReg(u32 regIdx, u32 data);
void* IPCGetBufferHi(void);
void* IPCGetBufferLo(void);
void IPCSetBufferLo(void* newLo);

#ifdef __cplusplus
}
#endif

#endif
