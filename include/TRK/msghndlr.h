// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msghndlr.h

#ifndef TRK_MSGHNDLR_H
#define TRK_MSGHNDLR_H

#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL GetTRKConnected(void);
DSError TRKDoConnect(TRKBuffer* buffer);
DSError TRKDoDisconnect(TRKBuffer* buffer);
DSError TRKDoReset(TRKBuffer* buffer);
DSError TRKDoOverride(TRKBuffer* buffer);
DSError TRKDoReadMemory(TRKBuffer* buffer);
DSError TRKDoWriteMemory(TRKBuffer* buffer);
DSError TRKDoReadRegisters(TRKBuffer* buffer);
DSError TRKDoWriteRegisters(TRKBuffer* buffer);
DSError TRKDoContinue(TRKBuffer* buffer);
DSError TRKDoStep(TRKBuffer* buffer);
DSError TRKDoStop(TRKBuffer* buffer);
DSError TRKDoSetOption(TRKBuffer* buffer);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MSGHNDLR_H */
