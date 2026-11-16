// Reference: tgsm/pbg include/rwsdk/plcore/baerr.h
#ifndef RWSDK_PLCORE_BAERR_H
#define RWSDK_PLCORE_BAERR_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwError: an error, as the plugin that raised it and its code. */
typedef struct RwError {
    RwInt32 pluginID;   /* 0x0: plugin that raised the error, 0 for the core */
    RwInt32 errorCode;  /* 0x4: the code; a code with the top bit set is a common code */
} RwError;

/* Error codes shared by every plugin (the top bit marks them). */
typedef enum RwErrorCodeCommon {
    E_RW_NOERROR = 0x80000000,
    E_RW_BADENGINESTATE = 0x80000001,
    E_RW_BADOPEN = 0x80000002,
    E_RW_BADPARAM = 0x80000003,
    E_RW_BADVERSION = 0x80000004,
    E_RW_INVIMAGEDEPTH = 0x80000008,
    E_RW_INVIMAGEFORMAT = 0x80000009,    /* name: Gormiti Debug error table; value from retail */
    E_RW_INVIMAGEMASK = 0x8000000A,
    E_RW_INVRASTERDEPTH = 0x8000000C,
    E_RW_INVRASTERFORMAT = 0x8000000D,
    E_RW_NOMEM = 0x80000013,
    E_RW_NULLP = 0x80000016,
    E_RW_PLUGININIT = 0x80000017,
    E_RW_PLUGINNOTINIT = 0x80000018,     /* name: Gormiti Debug error table; value from retail */
    E_RW_READ = 0x8000001A,
    E_RW_WRITE = 0x8000001C,
    E_RW_STRING_TRUNCATION = 0x8000001E,
    E_RW_LASTERROR = 0x7FFFFFFF
} RwErrorCodeCommon;

/* A common error code's flag bit. Name invented. */
#define rwERRORCOMMONFLAG   0x80000000

/*
 * Error codes of the core plugin (codes without the common flag); enum name as tgsm/pbg.
 */
typedef enum RwErrorCodePlugin_errcore {
    E_RW_ENDOFSTREAM = 5,
    E_RW_INVSTREAMACCESSTYPE = 13,
    E_RW_INVSTREAMTYPE = 14,
    E_RW_ZEROLENGTH = 25,           /* name invented: RwV2dNormalize's zero-length vector */
    /* The pipeline codes (E_RX_*): names from the error table of Gormiti's Debug build (_rwdb_errcore), in the
       table's order; each is the entry at the position of the value retail passes (the values are retail's). */
    E_RX_CYCLICPIPELINE = 28,
    E_RX_DEP_DEPENDENCIESMISMATCH = 29,
    E_RX_DEP_DUPLICATECLUSTERDEFS = 30,
    E_RX_DEP_NULLCLUSTERDEF = 31,
    E_RX_DEP_OUTOFMEMORY = 32,
    E_RX_FRAGMENTEDPIPELINE = 34,
    E_RX_IM3DNOTACTIVE = 35,
    E_RX_INVALIDENTRYPOINT = 36,
    E_RX_INVALIDPRIMTYPE = 37,
    E_RX_NODETOOMANYCLUSTERSOFINTEREST = 40,
    E_RX_NODETOOMANYOUTPUTS = 41,
    E_RX_PIPELINETOOMANYNODES = 42,
    E_RX_TOOMANYVERTS = 50,
    E_RX_UNLOCKEDPIPE = 52,
    rwLASTERROR_errcore = 0x7FFFFFFF
} RwErrorCodePlugin_errcore;

RwError* RwErrorSet(RwError* code);
RwError* RwErrorGet(RwError* code);
RwInt32 _rwerror(RwInt32 code, ...);

void* _rwErrorOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwErrorClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
