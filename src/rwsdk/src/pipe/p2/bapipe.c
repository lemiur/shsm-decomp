// Unit name and boundary: Gormiti link map (RwCore.a bapipe)
/*
 * bapipe.c: the pipeline module. It records where its globals sit in the engine instance and
 * opens and closes the pipeline system (p2core); the attach and per-camera hooks do nothing on
 * this platform.
 */
#include <rwsdk/pipe/p2/bapipe.h>

/* The offset of the pipeline module's globals in the engine instance. */
RwInt32 _rxPipelineGlobalsOffset;

/*
 * Registered as the pipeline module's open callback: remembers the globals' offset and opens the
 * pipeline system; a failure reports NULL, otherwise the instance comes back.
 */
void* _rwRenderPipelineOpen(void* instance, RwInt32 offset, RwInt32 size) {
    _rxPipelineGlobalsOffset = offset;

    if (!_rxPipelineOpen()) {
        return NULL;
    }

    return instance;
} // 0x8029B1EC

/* Registered as the pipeline module's close callback. */
void* _rwRenderPipelineClose(void* instance, RwInt32 offset, RwInt32 size) {
    _rxPipelineClose();

    return instance;
} // 0x8029B230

/* The pipeline plugins need no attachment work here. */
RwBool _rwPipeAttach(void) {
    return TRUE;
} // 0x8029B260

/* Nothing to prepare per camera here. */
void _rwPipeInitForCamera(struct RwCamera* camera) {
} // 0x8029B268
