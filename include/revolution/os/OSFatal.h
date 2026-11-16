#ifndef REVOLUTION_OS_OSFATAL_H
#define REVOLUTION_OS_OSFATAL_H

#include <revolution/types.h>
#include <revolution/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shows msg, in colour fg on bg, on the screen and halts. */
void OSFatal(GXColor fg, GXColor bg, const char* msg);

#ifdef __cplusplus
}
#endif

#endif
