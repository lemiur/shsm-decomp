#ifndef RWSDK_PLCORE_BADEBUG_H
#define RWSDK_PLCORE_BADEBUG_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/baerr.h>

/*
 * Raises an error of the core: ecode is the parenthesised argument list of _rwerror, the code and
 * its message arguments. The core's own sources include this header; a plugin's include
 * rpdbgerr.h, whose RWERROR carries the plugin's rwPLUGIN_ID instead.
 */
#define RWERROR(ecode)                              \
    do {                                            \
        RwError _rwErrorCode;                       \
                                                    \
        _rwErrorCode.pluginID = rwID_COREPLUGIN;    \
        _rwErrorCode.errorCode = _rwerror ecode;    \
        RwErrorSet(&_rwErrorCode);                  \
    } while (0)

/*
 * The debug build's assertion (RWASSERT in the SDK's rwdebug.h). The retail build is a release
 * build, where it expands to nothing.
 */
#ifndef RWASSERT
#define RWASSERT(_condition) /* no-op in a release build */
#endif

#endif
