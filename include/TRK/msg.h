// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msg.h

#ifndef TRK_MSG_H
#define TRK_MSG_H

#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

DSError TRKMessageSend(TRKBuffer* msg);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MSG_H */
