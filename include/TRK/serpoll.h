#ifndef TRK_SERPOLL_H
#define TRK_SERPOLL_H

#include "TRK/dserror.h"
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* gTRKInputPendingPtr;

MessageBufferID TRKTestForPacket(void);
void TRKGetInput(void);
void TRKProcessInput(int bufferIdx);
DSError TRKInitializeSerialHandler(void);
DSError TRKTerminateSerialHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* TRK_SERPOLL_H */
