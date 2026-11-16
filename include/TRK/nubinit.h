#ifndef TRK_NUBINIT_H
#define TRK_NUBINIT_H

#include <revolution/types.h>
#include "TRK/dserror.h"

#ifdef __cplusplus
extern "C" {
#endif

DSError TRKInitializeNub(void);
DSError TRKTerminateNub(void);
void TRKNubWelcome(void);
BOOL TRKInitializeEndian(void);

extern BOOL gTRKBigEndian;

#ifdef __cplusplus
}
#endif

#endif /* TRK_NUBINIT_H */
