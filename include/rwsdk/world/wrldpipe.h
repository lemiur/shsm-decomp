// Name: Gormiti link map (RPworld.a wrldpipe)
#ifndef RWSDK_WORLD_WRLDPIPE_H
#define RWSDK_WORLD_WRLDPIPE_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The platform half of the world plugin: the creation and destruction of the platform pipelines. */
extern RwBool _rpCreatePlatformMaterialPipelines(void);
extern void _rpDestroyPlatformMaterialPipelines(void);
extern RwBool _rpCreatePlatformWorldSectorPipelines(void);
extern void _rpDestroyPlatformWorldSectorPipelines(void);
extern RwBool _rpCreatePlatformAtomicPipelines(void);
extern void _rpDestroyPlatformAtomicPipelines(void);

#ifdef __cplusplus
}
#endif

#endif
