#ifndef TRK_DISPATCH_H
#define TRK_DISPATCH_H

#include <revolution/types.h>
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL TRKDispatchMessage(TRKBuffer* buffer);

#ifdef __cplusplus
}
#endif

#endif /* TRK_DISPATCH_H */
