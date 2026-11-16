// Reference: tgsm/pbg src/rwsdk/src/babinfrm.c
/*
 * babinfrm.c: frames in streams: a frame hierarchy is streamed as a frame list, each frame's
 * modelling matrix and the index of its parent, then each frame's plugin data.
 * RwFrameSetStreamAlwaysCallBack and the frame list builders and writers
 * (_rwFrameListInitialize, _rwFrameListFindFrame, _rwFrameListStreamGetSize,
 * _rwFrameListStreamWrite) have no copy in retail (dead-stripped).
 */
#include <rwsdk/babinfrm.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/batkreg.h>

/*
 * A frame as a frame list stores it: its modelling matrix, the index of its parent in the list
 * (-1 for a root) and its matrix flags. Type and member names invented except the matrix rows
 * (pbg's rwStreamMatrixAlmost; aap/librw's FrameStreamData).
 */
typedef struct rwStreamFrame {
    RwV3d right;            /* 0x0 */
    RwV3d up;               /* 0xC */
    RwV3d at;               /* 0x18 */
    RwV3d pos;              /* 0x24 */
    RwInt32 parentIndex;    /* 0x30 */
    RwUInt32 data;          /* 0x34: the matrix flags */
} rwStreamFrame;

/*
 * Whether a frame list read from a stream marks its frames as moved (RwFrameAddChild, then
 * RwFrameUpdateObjects on the roots). Name invented.
 */
static RwBool _rwFrameListAutoUpdate = TRUE;

/* A matrix within these error bounds is taken as orthonormal. Names invented. */
#define rwFRAMELISTMATRIXERROR 0.01f
#define rwFRAMELISTMATRIXMINDETERMINANT 0.99f

/* Registers a plugin's stream callbacks with frames. */
RwInt32 RwFrameRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                    RwPluginDataChunkWriteCallBack writeCB,
                                    RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&frameTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x80291140

/* Frees the frame array of a frame list (not the frames). */
rwFrameList* _rwFrameListDeinitialize(rwFrameList* frameList) {
    if (frameList->numFrames != 0) {
        RwFree(frameList->frames);
    }

    return frameList;
} // 0x80291168

/* Reads a frame list's structure chunk and its frames' plugin data, building the hierarchy. */
rwFrameList* _rwFrameListStreamRead(RwStream* stream, rwFrameList* frameList) {
    RwInt32 numFrames;
    RwUInt32 length;
    RwUInt32 version;
    RwInt32 i;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (rwLIBRARYBASEVERSION <= version && version <= rwLIBRARYCURRENTVERSION) {
        if (RwStreamRead(stream, &numFrames, sizeof(numFrames)) != sizeof(numFrames)) {
            return NULL;
        }
        RwMemNative32(&numFrames, sizeof(numFrames));

        frameList->numFrames = numFrames;
        frameList->frames = (RwFrame**)RwMalloc(numFrames * sizeof(RwFrame*), rwMEMHINTDUR_EVENT | rwID_FRAMELIST);
        if (frameList->frames == NULL) {
            RWERROR((E_RW_NOMEM, numFrames * sizeof(RwFrame*)));
            return NULL;
        }

        for (i = 0; i < numFrames; i++) {
            rwStreamFrame frameData;
            RwFrame* frame;
            RwMatrix* matrix;

            if (RwStreamRead(stream, &frameData, sizeof(frameData)) != sizeof(frameData)) {
                RwFree(frameList->frames);
                return NULL;
            }
            RwMemNative32(&frameData, sizeof(frameData));

            frame = RwFrameCreate();
            if (frame == NULL) {
                RwFree(frameList->frames);
                return NULL;
            }

            matrix = RwFrameGetMatrix(frame);
            matrix->right = frameData.right;
            matrix->up = frameData.up;
            matrix->at = frameData.at;
            matrix->pos = frameData.pos;

            /* Keep the orthonormal type only if the stored axes still are. */
            if (rwFRAMELISTMATRIXERROR >= _rwMatrixNormalError(matrix) &&
                rwFRAMELISTMATRIXERROR >= _rwMatrixOrthogonalError(matrix) &&
                rwFRAMELISTMATRIXMINDETERMINANT <= _rwMatrixDeterminant(matrix)) {
                rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) & ~rwMATRIXINTERNALIDENTITY);
            } else {
                rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) &
                                             ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK));
            }

            frameList->frames[i] = frame;
            if (frameData.parentIndex >= 0) {
                if (_rwFrameListAutoUpdate) {
                    RwFrameAddChild(frameList->frames[frameData.parentIndex], frame);
                } else {
                    RwFrameAddChildNoUpdate(frameList->frames[frameData.parentIndex], frame);
                }
            }
        }
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    for (i = 0; i < numFrames; i++) {
        RwFrame* frame = frameList->frames[i];

        if (!_rwPluginRegistryReadDataChunks(&frameTKList, stream, frame)) {
            RwFrameDestroyHierarchy(frameList->frames[0]);
            RwFree(frameList->frames);
            return NULL;
        }

        if (frame == RwFrameGetRoot(frame) && _rwFrameListAutoUpdate == TRUE) {
            RwFrameUpdateObjects(frame);
        }
    }

    return frameList;
} // 0x802911B4
