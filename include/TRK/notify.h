// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/notify.c

#ifndef TRK_NOTIFY_H
#define TRK_NOTIFY_H

#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/dsmsg.h"

#ifdef __cplusplus
extern "C" {
#endif

DSError TRKDoNotifyStopped(MessageCommandID cmd);

#ifdef __cplusplus
}
#endif

#endif /* TRK_NOTIFY_H */
