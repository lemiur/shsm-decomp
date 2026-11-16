// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/ppc/Export/targsupp.h

#ifndef TRK_TARGSUPP_H
#define TRK_TARGSUPP_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* File requests forwarded to the host debugger. The retail bodies are trap stubs (twui) that the debugger
 * services; the command byte is the DSMSG_ReadFile / DSMSG_WriteFile / DSMSG_CloseFile request. */
u32 TRKAccessFile(u32 command, u32 handle, u32* count, u8* buffer);
u32 TRKOpenFile(u32 command, u32 path, u32* handle, u8* name);
u32 TRKCloseFile(u32 command, u32 handle);
u32 TRKPositionFile(u32 command, u32 handle, u32* position, u8* seekMode);

#ifdef __cplusplus
}
#endif

#endif /* TRK_TARGSUPP_H */
