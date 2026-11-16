// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h (declarations)
#ifndef RWSDK_BABINTEX_H
#define RWSDK_BABINTEX_H

#include <rwsdk/batextur.h>
#include <rwsdk/plcore/bastream.h>

/* Bit flags of a streamed texture. */
typedef enum RwTextureStreamFlags {
    rwNATEXTURESTREAMFLAG = 0x00,
    rwTEXTURESTREAMFLAGSUSERMIPMAPS = 0x01,
    rwTEXTURESTREAMFLAGSFORCEENUMSIZEINT = 0x7FFFFFFF
} RwTextureStreamFlags;

#ifdef __cplusplus
extern "C" {
#endif

RwInt32 RwTextureRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                      RwPluginDataChunkWriteCallBack writeCB,
                                      RwPluginDataChunkGetSizeCallBack getSizeCB);
RwUInt32 RwTextureStreamGetSize(const RwTexture* texture);
/* Reads a texture whose chunk header the caller has already found (bfbbdecomp/bfbb rwcore.h). */
RwTexture* RwTextureStreamRead(RwStream* stream);
const RwTexture* RwTextureStreamWrite(const RwTexture* texture, RwStream* stream);
RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
