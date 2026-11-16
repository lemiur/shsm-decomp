// Reference: tgsm/pbg include/rwsdk/badevice.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h (string macros)
#ifndef RWSDK_PLCORE_RWSTRING_H
#define RWSDK_PLCORE_RWSTRING_H

#include <stdarg.h>
#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The string functions the core calls through RwGlobals (the C library's, or the core's own). */
typedef int (*vecSprintfFunc)(RwChar* buffer, const RwChar* format, ...);
typedef int (*vecVsprintfFunc)(RwChar* buffer, const RwChar* format, va_list argptr);
typedef RwChar* (*vecStrcpyFunc)(RwChar* dest, const RwChar* srce);
typedef RwChar* (*vecStrncpyFunc)(RwChar* dest, const RwChar* srce, size_t size);
typedef RwChar* (*vecStrcatFunc)(RwChar* dest, const RwChar* srce);
typedef RwChar* (*vecStrncatFunc)(RwChar* dest, const RwChar* srce, size_t size);
typedef RwChar* (*vecStrrchrFunc)(const RwChar* string, RwInt32 findThis);
typedef RwChar* (*vecStrchrFunc)(const RwChar* string, RwInt32 findThis);
typedef RwChar* (*vecStrstrFunc)(const RwChar* string, const RwChar* findThis);
typedef int (*vecStrcmpFunc)(const RwChar* string1, const RwChar* string2);
typedef int (*vecStrncmpFunc)(const RwChar* string1, const RwChar* string2, size_t maxLen);
typedef int (*vecStricmpFunc)(const RwChar* string1, const RwChar* string2);
typedef size_t (*vecStrlenFunc)(const RwChar* string);
typedef RwChar* (*vecStruprFunc)(RwChar* string);
typedef RwChar* (*vecStrlwrFunc)(RwChar* string);
typedef RwChar* (*vecStrtokFunc)(RwChar* string, const RwChar* delimit);
typedef int (*vecSscanfFunc)(const RwChar* buffer, const RwChar* format, ...);

/* RwStringFunctions: the string function table of RwGlobals, filled by _rwStringOpen. */
typedef struct RwStringFunctions {
    vecSprintfFunc vecSprintf;      /* 0x0 */
    vecVsprintfFunc vecVsprintf;    /* 0x4 */
    vecStrcpyFunc vecStrcpy;        /* 0x8 */
    vecStrncpyFunc vecStrncpy;      /* 0xC */
    vecStrcatFunc vecStrcat;        /* 0x10 */
    vecStrncatFunc vecStrncat;      /* 0x14 */
    vecStrrchrFunc vecStrrchr;      /* 0x18 */
    vecStrchrFunc vecStrchr;        /* 0x1C */
    vecStrstrFunc vecStrstr;        /* 0x20 */
    vecStrcmpFunc vecStrcmp;        /* 0x24 */
    vecStrncmpFunc vecStrncmp;      /* 0x28 */
    vecStricmpFunc vecStricmp;      /* 0x2C */
    vecStrlenFunc vecStrlen;        /* 0x30 */
    vecStruprFunc vecStrupr;        /* 0x34 */
    vecStrlwrFunc vecStrlwr;        /* 0x38 */
    vecStrtokFunc vecStrtok;        /* 0x3C */
    vecSscanfFunc vecSscanf;        /* 0x40 */
} RwStringFunctions;

/* The core's string functions, called through RwGlobals (bfbbdecomp/bfbb rwplcore.h). */
#define rwsprintf RWSRCGLOBAL(stringFuncs).vecSprintf
#define rwvsprintf RWSRCGLOBAL(stringFuncs).vecVsprintf
#define rwstrcpy RWSRCGLOBAL(stringFuncs).vecStrcpy
#define rwstrncpy RWSRCGLOBAL(stringFuncs).vecStrncpy
#define rwstrcat RWSRCGLOBAL(stringFuncs).vecStrcat
#define rwstrncat RWSRCGLOBAL(stringFuncs).vecStrncat
#define rwstrrchr RWSRCGLOBAL(stringFuncs).vecStrrchr
#define rwstrchr RWSRCGLOBAL(stringFuncs).vecStrchr
#define rwstrstr RWSRCGLOBAL(stringFuncs).vecStrstr
#define rwstrcmp RWSRCGLOBAL(stringFuncs).vecStrcmp
#define rwstrncmp RWSRCGLOBAL(stringFuncs).vecStrncmp
#define rwstricmp RWSRCGLOBAL(stringFuncs).vecStricmp
#define rwstrlen RWSRCGLOBAL(stringFuncs).vecStrlen
#define rwstrupr RWSRCGLOBAL(stringFuncs).vecStrupr
#define rwstrlwr RWSRCGLOBAL(stringFuncs).vecStrlwr
#define rwstrtok RWSRCGLOBAL(stringFuncs).vecStrtok
#define rwsscanf RWSRCGLOBAL(stringFuncs).vecSscanf

struct RwStream;

RwBool _rwStringOpen(void);
void _rwStringClose(void);
RwInt32 _rwStringStreamGetSize(const RwChar* string);
const RwChar* _rwStringStreamWrite(const RwChar* string, struct RwStream* stream);
RwChar* _rwStringStreamFindAndRead(RwChar* string, struct RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
