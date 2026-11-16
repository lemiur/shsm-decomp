// Unit name and boundary: Gormiti link map (RPworld.a bapipew)
// Names and locals: Gormiti DWARF
/*
 * bapipew.c: the world plugin's pipeline module. It keeps the default pipelines atomics and world
 * sectors render through, and opens and closes the platform pipelines that back them.
 */
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/world/bapipew.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/wrldpipe.h>

/* Sets the default world sector pipeline; NULL selects the platform pipeline. */
RxPipeline* RpWorldSetDefaultSectorPipeline(RxPipeline* pipeline) {
    if (pipeline == NULL) {
        if (RXPIPELINEGLOBAL(platformWorldSectorPipeline) != NULL) {
            pipeline = RXPIPELINEGLOBAL(platformWorldSectorPipeline);
        } else {
            pipeline = NULL;
        }
    }

    RXPIPELINEGLOBAL(currentWorldSectorPipeline) = pipeline;

    return pipeline;
} // 0x802DB834

/* Sets the default atomic pipeline; NULL selects the platform pipeline. */
RxPipeline* RpAtomicSetDefaultPipeline(RxPipeline* pipeline) {
    if (pipeline == NULL) {
        if (RXPIPELINEGLOBAL(platformAtomicPipeline) != NULL) {
            pipeline = RXPIPELINEGLOBAL(platformAtomicPipeline);
        } else {
            pipeline = NULL;
        }
    }

    RXPIPELINEGLOBAL(currentAtomicPipeline) = pipeline;

    return pipeline;
} // 0x802DB870

/* Destroys the platform pipelines. */
void _rpWorldPipelineClose(void) {
    _rpDestroyPlatformWorldSectorPipelines();
    _rpDestroyPlatformAtomicPipelines();
    _rpDestroyPlatformMaterialPipelines();
} // 0x802DB8AC

/* Clears the module's pipelines and builds the platform ones; destroys them again if any fails. */
RwBool _rpWorldPipelineOpen(void) {
    RwBool success;

    RXPIPELINEGLOBAL(currentAtomicPipeline) = NULL;
    RXPIPELINEGLOBAL(currentWorldSectorPipeline) = NULL;
    RXPIPELINEGLOBAL(currentMaterialPipeline) = NULL;
    RXPIPELINEGLOBAL(genericAtomicPipeline) = NULL;
    RXPIPELINEGLOBAL(genericWorldSectorPipeline) = NULL;
    RXPIPELINEGLOBAL(genericMaterialPipeline) = NULL;
    RXPIPELINEGLOBAL(platformAtomicPipeline) = NULL;
    RXPIPELINEGLOBAL(platformWorldSectorPipeline) = NULL;
    RXPIPELINEGLOBAL(platformMaterialPipeline) = NULL;

    success = _rpCreatePlatformMaterialPipelines();
    if (success) {
        success = _rpCreatePlatformAtomicPipelines();
    }
    if (success) {
        success = _rpCreatePlatformWorldSectorPipelines();
    }

    if (success) {
        return TRUE;
    }

    _rpDestroyPlatformWorldSectorPipelines();
    _rpDestroyPlatformAtomicPipelines();
    _rpDestroyPlatformMaterialPipelines();

    return FALSE;
} // 0x802DB8D4

/* Attaches the module's plugin; nothing to register here. */
RwBool _rpWorldPipeAttach(void) {
    return TRUE;
} // 0x802DB9C0
