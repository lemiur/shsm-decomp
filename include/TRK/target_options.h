// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/Os/dolphin/target_options.h

#ifndef TRK_TARGET_OPTIONS_H
#define TRK_TARGET_OPTIONS_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the MSL console streams (stdin, stdout) are routed through the debugger link. */
u8 GetUseSerialIO(void);
void SetUseSerialIO(u8 serialIO);

#ifdef __cplusplus
}
#endif

#endif /* TRK_TARGET_OPTIONS_H */
