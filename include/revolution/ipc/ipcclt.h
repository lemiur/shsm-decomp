// Reference: zeldaret/tp libs/revolution/include/revolution/ipcclt.h
#ifndef REVOLUTION_IPC_IPCCLT_H
#define REVOLUTION_IPC_IPCCLT_H

#include <revolution/types.h>
#include <revolution/ios/iostypes.h>
#include <revolution/ios/iosrestypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IOS resource manager client. */

typedef IOSError (*IOSIpcCb)(IOSError result, void* arg);
typedef IOSIpcCb IPCCallback;

/* Defined in ipcclt.c (retail has no MSL strnlen); its callers inline or call this one. */
u32 strnlen(const u8* str, u32 n);

IOSError IPCCltInit(void);
IOSError IPCCltReInit(void);

IOSError IOS_OpenAsync(const char* path, u32 flags, IOSIpcCb cb, void* cbArg);
IOSError IOS_Open(const char* path, u32 flags);
IOSError IOS_CloseAsync(IOSFd fd, IOSIpcCb cb, void* cbArg);
IOSError IOS_Close(IOSFd fd);
IOSError IOS_ReadAsync(IOSFd fd, void* buf, u32 len, IOSIpcCb cb, void* cbArg);
IOSError IOS_Read(IOSFd fd, void* buf, u32 len);
IOSError IOS_WriteAsync(IOSFd fd, void* buf, u32 len, IOSIpcCb cb, void* cbArg);
IOSError IOS_Write(IOSFd fd, void* buf, u32 len);
IOSError IOS_SeekAsync(IOSFd fd, s32 offset, u32 whence, IOSIpcCb cb, void* cbArg);
IOSError IOS_Seek(IOSFd fd, s32 offset, u32 whence);
IOSError IOS_IoctlAsync(IOSFd fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen, IOSIpcCb cb,
                        void* cbArg);
IOSError IOS_Ioctl(IOSFd fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen);
IOSError IOS_IoctlvAsync(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect, IOSIpcCb cb, void* cbArg);
IOSError IOS_Ioctlv(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect);
IOSError IOS_IoctlvReboot(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect);

#ifdef __cplusplus
}
#endif

#endif
