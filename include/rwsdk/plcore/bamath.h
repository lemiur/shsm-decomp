#ifndef RWSDK_PLCORE_BAMATH_H
#define RWSDK_PLCORE_BAMATH_H

#include <rwsdk/rwtypes.h>

/* A real truncated to an integer; the guard is the SDK header's (a platform may supply a faster one). */
#ifndef RwInt32FromRealMacro
#define RwInt32FromRealMacro(x) ((RwInt32)(x))
#endif

#endif
