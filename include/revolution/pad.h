#ifndef REVOLUTION_PAD_H
#define REVOLUTION_PAD_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Controller library: only the entry points the OS library uses. */

/* Controller spec word the boot code copies from bi2 (defined in Pad.c, set in OS.c). */
extern u32 __PADSpec;

BOOL __PADDisableRecalibration(BOOL disable);

#ifdef __cplusplus
}
#endif

#endif
