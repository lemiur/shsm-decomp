// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk.c
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/dolphin_trk.c

#ifndef TRK_DOLPHIN_TRK_H
#define TRK_DOLPHIN_TRK_H

#include <revolution/types.h>
#include "TRK/dserror.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Debugger stack top, from the linker command file. */
extern u8 _db_stack_addr[];

void InitMetroTRK(void);
void InitMetroTRK_BBA(void);
void EnableMetroTRKInterrupts(void);
void __TRK_reset(void);
void __TRK_copy_vectors(void);
u32 TRKTargetTranslate(u32 addr);

#ifdef __cplusplus
}
#endif

#endif /* TRK_DOLPHIN_TRK_H */
