// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk_glue.h
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/include/PowerPC_EABI_Support/MetroTRK/dolphin_trk_glue.h

#ifndef TRK_DOLPHIN_TRK_GLUE_H
#define TRK_DOLPHIN_TRK_GLUE_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include "TRK/dserror.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware IDs the OS passes to InitMetroTRK. */
#define HARDWARE_NDEV 1
#define HARDWARE_BBA  2

/* Entry points of the active debugger connection, filled by InitMetroTRKCommTable. The receive callback
 * has the EXICallback signature (revolution/exi.h, not included here: the portable nub units declare
 * OSReport without a prototype, see trk_glue.h). */
typedef struct DBCommTable {
    int (*initialize_func)(void* inputPendingPtrRef, void (*monitorCallback)(s32 chan, OSContext* context));
    int (*init_interrupts_func)(void);
    int (*shutdown_func)(void);
    int (*peek_func)(void);
    int (*read_func)(u8* data, int size);
    int (*write_func)(const u8* bytes, int length);
    int (*open_func)(void);
    int (*close_func)(void);
    int (*pre_continue_func)(void);
    int (*post_stop_func)(void);
} DBCommTable;

extern u8 TRK_Use_BBA;
extern DBCommTable gDBCommTable;

void TRKLoadContext(OSContext* context, u32 exceptionVector);
void TRKEXICallBack(s32 chan, OSContext* context);
int InitMetroTRKCommTable(int hwId);
DSError TRKInitializeIntDrivenUART(u32, u32, void** inputPendingPtr);
void EnableEXI2Interrupts(void);
void ReserveEXI2Port(void);
void UnreserveEXI2Port(void);
void TRK_board_display(const char* message);
void InitializeProgramEndTrap(void);

#ifdef __cplusplus
}
#endif

#endif /* TRK_DOLPHIN_TRK_GLUE_H */
