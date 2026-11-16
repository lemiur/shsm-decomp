// Unit name and boundary: Gormiti link map (RPworld.a wrldpipe)
// Locals and names: Gormiti DWARF
/*
 * wrldpipe.c: the GameCube half of the world plugin's start-up. It attaches the vertex format
 * and light plugins, and builds and destroys the platform pipelines the world sectors and the
 * atomics render through.
 */
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/world/bapipew.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/wrldpipe.h>

/* Attaches the GameCube vertex format and light plugins; 0 if either one fails. */
RwBool _rxWorldDevicePluginAttach(void) {
    if (!_rpDlVtxFmtPluginAttach()) {
        return FALSE;
    }

    if (!_rpDlLightPluginAttach()) {
        return FALSE;
    }

    return TRUE;
} // 0x802DF3E0

/* Creates the platform material pipelines; the GameCube has none. */
RwBool _rpCreatePlatformMaterialPipelines(void) {
    return TRUE;
} // 0x802DF420

/* Destroys the platform material pipelines; the GameCube has none. */
void _rpDestroyPlatformMaterialPipelines(void) {
} // 0x802DF428

/* Creates the world sector pipeline and makes it the default one. */
RwBool _rpCreatePlatformWorldSectorPipelines(void) {
    RxPipeline* pipe = _rpDlSectorPipelineCreate(2, 0, _rxGCSectorDefaultInstanceCallback, NULL,
                                                 _rxGCSectorDefaultLightingCallback, _rxGCDefaultRenderCallback);

    if (pipe != NULL) {
        RXPIPELINEGLOBAL(platformWorldSectorPipeline) = pipe;
        RpWorldSetDefaultSectorPipeline(pipe);
        return TRUE;
    }

    return FALSE;
} // 0x802DF42C

/* Drops the default world sector pipeline and destroys the platform one. */
void _rpDestroyPlatformWorldSectorPipelines(void) {
    RpWorldSetDefaultSectorPipeline(NULL);

    if (RXPIPELINEGLOBAL(platformWorldSectorPipeline) != NULL) {
        _rxPipelineDestroy(RXPIPELINEGLOBAL(platformWorldSectorPipeline));
        RXPIPELINEGLOBAL(platformWorldSectorPipeline) = NULL;
    }
} // 0x802DF498

/* Creates the atomic pipeline and makes it the default one. */
RwBool _rpCreatePlatformAtomicPipelines(void) {
    RxPipeline* pipe = _rpDlAtomicPipelineCreate(2, 0, _rxGCAtomicDefaultInstanceCallback,
                                                 _rxGCAtomicDefaultReinstanceCallback, _rxGCAtomicDefaultLightingCallback, _rxGCDefaultRenderCallback);

    if (pipe != NULL) {
        RXPIPELINEGLOBAL(platformAtomicPipeline) = pipe;
        RpAtomicSetDefaultPipeline(pipe);
        return TRUE;
    }

    return FALSE;
} // 0x802DF4EC

/* Drops the default atomic pipeline and destroys the platform one. */
void _rpDestroyPlatformAtomicPipelines(void) {
    RpAtomicSetDefaultPipeline(NULL);

    if (RXPIPELINEGLOBAL(platformAtomicPipeline) != NULL) {
        _rxPipelineDestroy(RXPIPELINEGLOBAL(platformAtomicPipeline));
        RXPIPELINEGLOBAL(platformAtomicPipeline) = NULL;
    }
} // 0x802DF55C
