#ifndef TRK_TRK_GLUE_H
#define TRK_TRK_GLUE_H

#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/dolphin_trk_glue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OS glue used by the portable nub: message output, and through dolphin_trk_glue.h the UART setup, the
 * end-of-program trap and the board display.
 *
 * OSReport is the RVL SDK function, declared here without a prototype (old-style). The retail nub calls it
 * with one argument (TRKPostEvent) and with two (TRKMessageSend), and neither call sets up CR6 (no crclr):
 * a variadic prototype adds the crclr, a fixed parameter list cannot take both arities, so these units saw
 * no prototype. zeldaret/tp's MetroTRK usr_put.c notes the same crclr with its own non-variadic declaration.
 * Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/usr_put.c */
// RULE-EXCEPTION: R6/R25, the calls compile without crclr at two arities, which only an unprototyped declaration gives
void OSReport();

#ifdef __cplusplus
}
#endif

#endif /* TRK_TRK_GLUE_H */
