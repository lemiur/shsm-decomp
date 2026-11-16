// Name: Gormiti link map (RPworld.a bapipew)
#ifndef RWSDK_WORLD_BAPIPEW_H
#define RWSDK_WORLD_BAPIPEW_H

#include <rwsdk/pipe/p2/bapipe.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the pipeline objects of the world render through when they have none of their own; NULL restores the platform one. */
extern RxPipeline* RpWorldSetDefaultSectorPipeline(RxPipeline* pipeline);
extern RxPipeline* RpAtomicSetDefaultPipeline(RxPipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif
