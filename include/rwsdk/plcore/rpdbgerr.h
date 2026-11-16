#ifndef RWSDK_PLCORE_RPDBGERR_H
#define RWSDK_PLCORE_RPDBGERR_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/baerr.h>

/*
 * Raises an error of a plugin: ecode is the parenthesised argument list of _rwerror, the code and
 * its message arguments. rwPLUGIN_ID is the plugin's own error ID, defined by the plugin's
 * rpplugin.h, which its sources include first.
 */
#define RWERROR(ecode)                              \
    do {                                            \
        RwError _rwErrorCode;                       \
                                                    \
        _rwErrorCode.pluginID = rwPLUGIN_ID;        \
        _rwErrorCode.errorCode = _rwerror ecode;    \
        RwErrorSet(&_rwErrorCode);                  \
    } while (0)

/*
 * The debug build's assertion (RWASSERT in the SDK's rwdebug.h), as in badebug.h. The retail build
 * is a release build, where it expands to nothing.
 */
#ifndef RWASSERT
#define RWASSERT(_condition) /* no-op in a release build */
#endif

#endif
