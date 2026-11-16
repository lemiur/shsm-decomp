// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/Os/dolphin/GDEV_Stubs.h

#ifndef TRK_GDEV_STUBS_H
#define TRK_GDEV_STUBS_H

#include <revolution/types.h>

#include "amc/AmcExi2Comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entry points of the gdev connection. The dolphin glue installs them in gDBCommTable. */
int gdev_cc_initialize(void* inputPendingPtrRef, EXICallback monitorCallback);
int gdev_cc_shutdown(void);
int gdev_cc_open(void);
int gdev_cc_close(void);
int gdev_cc_read(u8* data, int size);
int gdev_cc_write(const u8* bytes, int length);
int gdev_cc_pre_continue(void);
int gdev_cc_post_stop(void);
int gdev_cc_peek(void);
int gdev_cc_initinterrupts(void);

#ifdef __cplusplus
}
#endif

#endif /* TRK_GDEV_STUBS_H */
