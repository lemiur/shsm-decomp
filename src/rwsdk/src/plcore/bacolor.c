/*
 * bacolor.c: the colour module (rwID_COLORMODULE). It registers no globals; only its open and close
 * survive in this build. The file name is the one tgsm/pbg lists at this place in the plcore link order.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bacolor.h>

/*
 * The colour module's registration (it has no globals, so only the open count is kept). Name
 * invented, after pbg's vectorModule, streamModule and errorModule.
 */
static RwModuleInfo colorModule;

/* Opens the colour module. */
void* _rwColorOpen(void* instance, RwInt32 offset, RwInt32 size) {
    colorModule.numInstances++;

    return instance;
} // 0x802A0E68

/* Closes the colour module. */
void* _rwColorClose(void* instance, RwInt32 offset, RwInt32 size) {
    colorModule.numInstances--;

    return instance;
} // 0x802A0E7C
