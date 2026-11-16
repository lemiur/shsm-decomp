// Reference: tgsm/pbg include/rwsdk/plcore/batkreg.h
#ifndef RWSDK_PLCORE_BATKREG_H
#define RWSDK_PLCORE_BATKREG_H

#include <rwsdk/plcore/bastream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plugin callbacks: stream data chunks of a plugin's data in an object. */
typedef RwStream* (*RwPluginDataChunkWriteCallBack)(RwStream* stream, RwInt32 binaryLength,
                                                    const void* object, RwInt32 offsetInObject,
                                                    RwInt32 sizeInObject);
typedef RwStream* (*RwPluginDataChunkReadCallBack)(RwStream* stream, RwInt32 binaryLength,
                                                   void* object, RwInt32 offsetInObject,
                                                   RwInt32 sizeInObject);
typedef RwInt32 (*RwPluginDataChunkGetSizeCallBack)(const void* object, RwInt32 offsetInObject,
                                                    RwInt32 sizeInObject);
typedef RwBool (*RwPluginDataChunkAlwaysCallBack)(void* object, RwInt32 offsetInObject,
                                                  RwInt32 sizeInObject);
typedef RwBool (*RwPluginDataChunkRightsCallBack)(void* object, RwInt32 offsetInObject,
                                                  RwInt32 sizeInObject, RwUInt32 extraData);

/* Plugin callbacks: the life cycle of a plugin's data in an object. */
typedef void* (*RwPluginObjectConstructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void* (*RwPluginObjectCopy)(void* dstObject, const void* srcObject, RwInt32 offsetInObject,
                                    RwInt32 sizeInObject);
typedef void* (*RwPluginObjectDestructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void* (*RwPluginErrorStrCallBack)(void* object);

typedef struct RwPluginRegEntry RwPluginRegEntry;

/* RwPluginRegistry: the plugins registered with one object type. */
typedef struct RwPluginRegistry {
    RwInt32 sizeOfStruct;               /* 0x0: object size with every plugin's data */
    RwInt32 origSizeOfStruct;           /* 0x4: object size without plugins */
    RwInt32 maxSizeOfStruct;            /* 0x8 */
    RwInt32 staticAlloc;                /* 0xC */
    RwPluginRegEntry* firstRegEntry;    /* 0x10 */
    RwPluginRegEntry* lastRegEntry;     /* 0x14 */
} RwPluginRegistry;

/* RwPluginRegEntry: one plugin of a registry. */
struct RwPluginRegEntry {
    RwInt32 offset;                             /* 0x0: of the plugin's data in the object */
    RwInt32 size;                               /* 0x4: of the plugin's data */
    RwUInt32 pluginID;                          /* 0x8 */
    RwPluginDataChunkReadCallBack readCB;       /* 0xC */
    RwPluginDataChunkWriteCallBack writeCB;     /* 0x10 */
    RwPluginDataChunkGetSizeCallBack getSizeCB; /* 0x14 */
    RwPluginDataChunkAlwaysCallBack alwaysCB;   /* 0x18 */
    RwPluginDataChunkRightsCallBack rightsCB;   /* 0x1C */
    RwPluginObjectConstructor constructCB;      /* 0x20 */
    RwPluginObjectDestructor destructCB;        /* 0x24 */
    RwPluginObjectCopy copyCB;                  /* 0x28 */
    RwPluginErrorStrCallBack errorStrCB;        /* 0x2C */
    RwPluginRegEntry* nextRegEntry;             /* 0x30 */
    RwPluginRegEntry* prevRegEntry;             /* 0x34 */
    RwPluginRegistry* parentRegistry;           /* 0x38 */
};

/* batkreg.c: plugin registries. */
RwBool _rwPluginRegistryOpen(void);
RwBool _rwPluginRegistryClose(void);
RwInt32 _rwPluginRegistryGetPluginOffset(const RwPluginRegistry* reg, RwUInt32 pluginID);
RwInt32 _rwPluginRegistryAddPlugin(RwPluginRegistry* reg, RwInt32 size, RwUInt32 pluginID,
                                   RwPluginObjectConstructor constructCB,
                                   RwPluginObjectDestructor destructCB,
                                   RwPluginObjectCopy copyCB);
const RwPluginRegistry* _rwPluginRegistryInitObject(const RwPluginRegistry* reg, void* object);
const RwPluginRegistry* _rwPluginRegistryDeInitObject(const RwPluginRegistry* reg, void* object);
const RwPluginRegistry* _rwPluginRegistryCopyObject(const RwPluginRegistry* reg, void* dstObject,
                                                    const void* srcObject);

/* batkbin.c: plugin data in streams. */
RwInt32 _rwPluginRegistryAddPluginStream(RwPluginRegistry* reg, RwUInt32 pluginID,
                                         RwPluginDataChunkReadCallBack readCB,
                                         RwPluginDataChunkWriteCallBack writeCB,
                                         RwPluginDataChunkGetSizeCallBack getSizeCB);
RwInt32 _rwPluginRegistryAddPlgnStrmlwysCB(RwPluginRegistry* reg, RwUInt32 pluginID,
                                           RwPluginDataChunkAlwaysCallBack alwaysCB);
RwInt32 _rwPluginRegistryAddPlgnStrmRightsCB(RwPluginRegistry* reg, RwUInt32 pluginID,
                                             RwPluginDataChunkRightsCallBack rightsCB);
const RwPluginRegistry* _rwPluginRegistryReadDataChunks(const RwPluginRegistry* reg, RwStream* stream,
                                                        void* object);
const RwPluginRegistry* _rwPluginRegistryInvokeRights(const RwPluginRegistry* reg, RwUInt32 id,
                                                      void* obj, RwUInt32 extraData);
RwInt32 _rwPluginRegistryGetSize(const RwPluginRegistry* reg, const void* object);
const RwPluginRegistry* _rwPluginRegistryWriteDataChunks(const RwPluginRegistry* reg, RwStream* stream,
                                                         const void* object);
const RwPluginRegistry* _rwPluginRegistrySkipDataChunks(const RwPluginRegistry* reg, RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
