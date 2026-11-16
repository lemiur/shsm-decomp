// Reference: zeldaret/tp libs/revolution/include/revolution/private/ipcProfile.h
//
// IPC request bookkeeping (pending / unissued request counts and the request table).
#ifndef REVOLUTION_IPC_IPCPROFILE_H
#define REVOLUTION_IPC_IPCPROFILE_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void IPCiProfInit(void);
void IPCiProfQueueReq(void* req, s32 handle);
void IPCiProfAck(void);
void IPCiProfReply(void* req, s32 handle);

#ifdef __cplusplus
}
#endif

#endif
