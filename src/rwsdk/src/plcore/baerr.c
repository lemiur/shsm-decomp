// Reference: tgsm/pbg src/rwsdk/src/plcore/baerr.c
/*
 * baerr.c: RenderWare error handling. The core keeps one pending error in its globals; RwErrorSet
 * records the first error raised and RwErrorGet hands it out and clears it.
 */
#include <stdarg.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>

/* The error module's registration (offset of its globals in the engine instance). */
static RwModuleInfo errorModule;

/* The pending error, in the error module's globals. Name invented. */
#define RWERRORGLOBAL(var) \
    (RWPLUGINOFFSET(RwError, RwEngineInstance, errorModule.globalsOffset)->var)

/* Opens the error module: records where its globals live and clears the pending error. */
void* _rwErrorOpen(void* instance, RwInt32 offset, RwInt32 size) {
    errorModule.globalsOffset = offset;
    errorModule.numInstances++;

    RWERRORGLOBAL(pluginID) = 0;
    RWERRORGLOBAL(errorCode) = E_RW_NOERROR;

    return instance;
} // 0x802A0E90

/* Closes the error module. */
void* _rwErrorClose(void* instance, RwInt32 offset, RwInt32 size) {
    errorModule.numInstances--;

    return instance;
} // 0x802A0EC8

/* Records code as the pending error, unless an error is already pending. */
RwError* RwErrorSet(RwError* code) {
    if (RWERRORGLOBAL(pluginID) == 0 && RWERRORGLOBAL(errorCode) == E_RW_NOERROR) {
        /* A common error belongs to no plugin. */
        if (code->errorCode & rwERRORCOMMONFLAG) {
            RWERRORGLOBAL(pluginID) = 0;
        } else {
            RWERRORGLOBAL(pluginID) = code->pluginID;
        }
        RWERRORGLOBAL(errorCode) = code->errorCode;
    }

    return code;
} // 0x802A0EDC

/* Copies the pending error to code and clears it. */
RwError* RwErrorGet(RwError* code) {
    *code = *RWPLUGINOFFSET(RwError, RwEngineInstance, errorModule.globalsOffset);

    RWERRORGLOBAL(pluginID) = 0;
    RWERRORGLOBAL(errorCode) = E_RW_NOERROR;

    return code;
} // 0x802A0F3C

/* Builds an error code; the release build ignores the message arguments. */
RwInt32 _rwerror(RwInt32 code, ...) {
    va_list ap;

    va_start(ap, code);
    va_end(ap);

    return code;
} // 0x802A0F7C
