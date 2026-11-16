// Reference: tgsm/pbg include/rwsdk/babinfrm.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BABINFRM_H
#define RWSDK_BABINFRM_H

#include <rwsdk/baframe.h>
#include <rwsdk/plcore/bastream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* rwFrameList: the frames of a hierarchy as an array, parents before their children. */
typedef struct rwFrameList {
    RwFrame** frames;   /* 0x0 */
    RwInt32 numFrames;  /* 0x4 */
} rwFrameList;

RwInt32 RwFrameRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB);
rwFrameList* _rwFrameListDeinitialize(rwFrameList* frameList);
rwFrameList* _rwFrameListStreamRead(RwStream* stream, rwFrameList* frameList);

#ifdef __cplusplus
}
#endif

#endif
