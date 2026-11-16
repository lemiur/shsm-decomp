// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/ppc/Generic/mpc_7xx_603e.h

#ifndef TRK_MPC_7XX_603E_H
#define TRK_MPC_7XX_603E_H

#include <revolution/types.h>
#include "TRK/targimpl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Copy the segment registers, time base, BATs and the other supervisor registers into gTRKCPUState. */
void TRKSaveExtended1Block(void);
/* Write them back from gTRKCPUState. */
void TRKRestoreExtended1Block(void);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MPC_7XX_603E_H */
