// Unit name and boundary: Gormiti link map (RpHAnim.a rphanim.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpHAnim.a rphanim.obj), Final and Debug builds.
// RpHAnimFrameGetID: Gormiti Debug build (a function there; inlined into HAnimFramesLinkToHierarchy in retail).
/*
 * rphanim.c: RpHAnim, hierarchical animation. The plugin extends frames with an ID and a
 * hierarchy pointer and streams them with the frames; a hierarchy lists the frames that make up a
 * skeleton, owns the interpolator that plays animations over them and the matrices it produces,
 * and is updated by walking its nodes with the parent matrix stack. The unit registers the
 * standard keyframe scheme of stdkey.c when the engine opens.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/babinfrm.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plugin/hanim/rphanim.h>
#include <rwsdk/tool/anim/rtanim.h>

/* The plugin's globals (rphanim.h): where its frame extension lies and the hierarchy free list. */
RpHAnimAtomicGlobalVars RpHAnimAtomicGlobals;

/* The hierarchy free list's block size and preallocated blocks, and its space. */
static RwInt32 _rpHAnimHierarchyFreeListBlockSize = 128;
static RwInt32 _rpHAnimHierarchyFreeListPreallocBlocks = 1;
static RwFreeList _rpHAnimHierarchyFreeList;

/* The plugin's extension of a frame. Macro name invented. */
#define RPHANIMFRAMEGETDATA(frame) \
    ((RpHAnimFrameExtension*)(((RwUInt8*)(frame)) + RpHAnimAtomicGlobals.engineOffset))

/*
 * ==================== HAnimFramesLinkToHierarchy ====================
 * Points the hierarchy's node of each frame at that frame, for a frame and all its children.
 */
static RwFrame* HAnimFramesLinkToHierarchy(RwFrame* rootFrame, void* pData) {
    RpHAnimHierarchy* pHierarchy;
    RwInt32 i;

    pHierarchy = (RpHAnimHierarchy*)pData;

    for (i = 0; i < pHierarchy->numNodes; i++) {
        if (pHierarchy->pNodeInfo[i].nodeID == RpHAnimFrameGetID(rootFrame)) {
            pHierarchy->pNodeInfo[i].pFrame = rootFrame;
        }
    }

    RwFrameUpdateObjects(rootFrame);
    RwFrameForAllChildren(rootFrame, HAnimFramesLinkToHierarchy, pData);

    return rootFrame;
} // 0x802C4114

/*
 * ==================== HAnimOpen ====================
 * Creates the hierarchy free list when an engine instance opens and registers the standard
 * keyframe scheme.
 */
static void* HAnimOpen(void* instance, RwInt32 offset, RwInt32 size) {
    RtAnimInterpolatorInfo interpInfo;

    RpHAnimAtomicGlobals.HAnimFreeList = RwFreeListCreateAndPreallocateSpace(
        sizeof(RpHAnimHierarchy), _rpHAnimHierarchyFreeListBlockSize, sizeof(RwInt32),
        _rpHAnimHierarchyFreeListPreallocBlocks, &_rpHAnimHierarchyFreeList,
        rwMEMHINTDUR_GLOBAL | rwID_HANIMPLUGIN);
    if (RpHAnimAtomicGlobals.HAnimFreeList == NULL) {
        instance = NULL;
    }

    interpInfo.typeID = rpHANIMSTDKEYFRAMETYPEID;
    interpInfo.interpKeyFrameSize = rpHANIMSTDKEYFRAMESIZE;
    interpInfo.animKeyFrameSize = rpHANIMSTDKEYFRAMESIZE;
    interpInfo.keyFrameApplyCB = RpHAnimKeyFrameApply;
    interpInfo.keyFrameBlendCB = RpHAnimKeyFrameBlend;
    interpInfo.keyFrameInterpolateCB = RpHAnimKeyFrameInterpolate;
    interpInfo.keyFrameAddCB = RpHAnimKeyFrameAdd;
    interpInfo.keyFrameMulRecipCB = RpHAnimKeyFrameMulRecip;
    interpInfo.keyFrameStreamReadCB = RpHAnimKeyFrameStreamRead;
    interpInfo.keyFrameStreamWriteCB = RpHAnimKeyFrameStreamWrite;
    interpInfo.keyFrameStreamGetSizeCB = RpHAnimKeyFrameStreamGetSize;
    interpInfo.customDataSize = 0;
    interpInfo.field_0x30 = 0;

    RtAnimRegisterInterpolationScheme(&interpInfo);

    return instance;
} // 0x802C41A8

/*
 * ==================== HAnimClose ====================
 * Destroys the hierarchy free list when an engine instance closes.
 */
static void* HAnimClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RpHAnimAtomicGlobals.HAnimFreeList != NULL) {
        RwFreeListDestroy(RpHAnimAtomicGlobals.HAnimFreeList);
        RpHAnimAtomicGlobals.HAnimFreeList = NULL;
    }

    return instance;
} // 0x802C4294

/*
 * ==================== HAnimConstructor ====================
 * A new frame is no node and roots no hierarchy.
 */
static void* HAnimConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpHAnimFrameExtension* frameExt;

    frameExt = RPHANIMFRAMEGETDATA(object);
    frameExt->hierarchy = NULL;
    frameExt->id = -1;

    return object;
} // 0x802C42E4

/*
 * ==================== HAnimDestructor ====================
 * Unlinks the nodes of the hierarchy a frame roots and destroys the hierarchy.
 */
static void* HAnimDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpHAnimFrameExtension* frameExt;

    frameExt = RPHANIMFRAMEGETDATA(object);

    if (frameExt->hierarchy != NULL) {
        RpHAnimHierarchy* pHierarchy = frameExt->hierarchy;
        RwInt32 frameNum;

        for (frameNum = 0; frameNum < pHierarchy->numNodes; frameNum++) {
            pHierarchy->pNodeInfo[frameNum].pFrame = NULL;
        }

        if (pHierarchy->parentFrame == object) {
            RpHAnimHierarchyDestroy(pHierarchy);
        }

        frameExt->hierarchy = NULL;
    }

    frameExt->id = -1;

    return object;
} // 0x802C4300

/*
 * ==================== HAnimCopy ====================
 * Gives the copy of a frame the ID of its source and, when the source roots a hierarchy of its
 * own, a copy of that hierarchy.
 */
static void* HAnimCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpHAnimFrameExtension* srcFrameExt;
    RpHAnimFrameExtension* dstFrameExt;

    srcFrameExt = RPHANIMFRAMEGETDATA(srcObject);
    dstFrameExt = RPHANIMFRAMEGETDATA(dstObject);

    dstFrameExt->id = srcFrameExt->id;

    if (srcFrameExt->hierarchy != NULL) {
        RpHAnimHierarchy* srcHierarchy = srcFrameExt->hierarchy;

        if (!(srcHierarchy->flags & rpHANIMHIERARCHYSUBHIERARCHY)) {
            RpHAnimHierarchy* dstHierarchy;
            RwInt32 i;

            dstHierarchy = RpHAnimHierarchyCreate(srcHierarchy->numNodes, NULL, NULL,
                                                  (RpHAnimHierarchyFlag)srcHierarchy->flags,
                                                  srcHierarchy->currentAnim->maxInterpKeyFrameSize);

            for (i = 0; i < dstHierarchy->numNodes; i++) {
                dstHierarchy->pNodeInfo[i].pFrame = NULL;
                dstHierarchy->pNodeInfo[i].flags = srcHierarchy->pNodeInfo[i].flags;
                dstHierarchy->pNodeInfo[i].nodeIndex = srcHierarchy->pNodeInfo[i].nodeIndex;
                dstHierarchy->pNodeInfo[i].nodeID = srcHierarchy->pNodeInfo[i].nodeID;
            }

            dstFrameExt->hierarchy = dstHierarchy;
            dstHierarchy->parentFrame = (RwFrame*)dstObject;
        }
    }

    return dstObject;
} // 0x802C4410

/*
 * ==================== HAnimWrite ====================
 * Writes a frame's plugin data: the stream version, the node ID and, for a hierarchy that is not
 * a sub-hierarchy, its node count, flags, keyframe size and nodes; otherwise a node count of 0.
 */
static RwStream* HAnimWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject,
                            RwInt32 sizeInObject) {
    RwInt32 i;
    RpHAnimFrameExtension* frameExt;
    RpHAnimNodeInfo* pNodeInfo;
    RpHAnimHierarchy* animHierarchy;
    RwInt32 streamVersion;

    streamVersion = rpHANIMSTREAMCURRENTVERSION;
    if (!RwStreamWriteInt32(stream, &streamVersion, sizeof(streamVersion))) {
        return NULL;
    }

    frameExt = RPHANIMFRAMEGETDATA(object);
    if (!RwStreamWriteInt32(stream, &frameExt->id, sizeof(frameExt->id))) {
        return NULL;
    }

    animHierarchy = frameExt->hierarchy;
    if (animHierarchy != NULL && !(animHierarchy->flags & rpHANIMHIERARCHYSUBHIERARCHY)) {
        if (!RwStreamWriteInt32(stream, &animHierarchy->numNodes, sizeof(animHierarchy->numNodes))) {
            return NULL;
        }
        if (!RwStreamWriteInt32(stream, &animHierarchy->flags, sizeof(animHierarchy->flags))) {
            return NULL;
        }
        if (!RwStreamWriteInt32(stream, &animHierarchy->currentAnim->maxInterpKeyFrameSize,
                                sizeof(animHierarchy->currentAnim->maxInterpKeyFrameSize))) {
            return NULL;
        }

        pNodeInfo = animHierarchy->pNodeInfo;
        for (i = 0; i < animHierarchy->numNodes; i++) {
            if (!RwStreamWriteInt32(stream, &pNodeInfo->nodeID, sizeof(pNodeInfo->nodeID))) {
                return NULL;
            }
            if (!RwStreamWriteInt32(stream, &pNodeInfo->nodeIndex, sizeof(pNodeInfo->nodeIndex))) {
                return NULL;
            }
            if (!RwStreamWriteInt32(stream, &pNodeInfo->flags, sizeof(pNodeInfo->flags))) {
                return NULL;
            }
            pNodeInfo++;
        }
    } else {
        RwInt32 numNodes = 0;

        if (!RwStreamWriteInt32(stream, &numNodes, sizeof(numNodes))) {
            return NULL;
        }
    }

    return stream;
} // 0x802C4508

/*
 * ==================== HAnimRead ====================
 * Reads a frame's plugin data written by HAnimWrite and creates the hierarchy it describes. A
 * stream of another version fails.
 */
static RwStream* HAnimRead(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                           RwInt32 sizeInObject) {
    RwInt32 i;
    RwInt32 numNodes;
    RwInt32 streamVersion;
    RpHAnimNodeInfo* pNodeInfo;
    RpHAnimHierarchy* pHierarchy;
    RpHAnimFrameExtension* frameExt;

    frameExt = RPHANIMFRAMEGETDATA(object);

    if (!RwStreamReadInt32(stream, &streamVersion, sizeof(streamVersion))) {
        return NULL;
    }
    if (streamVersion != rpHANIMSTREAMCURRENTVERSION) {
        return NULL;
    }

    if (!RwStreamReadInt32(stream, &frameExt->id, sizeof(frameExt->id))) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &numNodes, sizeof(numNodes))) {
        return NULL;
    }

    if (numNodes > 0) {
        RpHAnimHierarchyFlag flags;
        RwInt32 maxInterpKeyFrameSize;

        if (!RwStreamReadInt32(stream, (RwInt32*)&flags, sizeof(flags))) {
            return NULL;
        }
        if (!RwStreamReadInt32(stream, &maxInterpKeyFrameSize, sizeof(maxInterpKeyFrameSize))) {
            return NULL;
        }

        pHierarchy = (RpHAnimHierarchy*)RwFreeListAlloc(RpHAnimAtomicGlobals.HAnimFreeList,
                                                         rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);
        memset(pHierarchy, 0, sizeof(RpHAnimHierarchy));

        if (pHierarchy != NULL) {
            pHierarchy->currentAnim = RtAnimInterpolatorCreate(numNodes, maxInterpKeyFrameSize);
            pHierarchy->flags = flags;
            pHierarchy->parentFrame = (RwFrame*)object;
            pHierarchy->numNodes = numNodes;
            pHierarchy->parentHierarchy = pHierarchy;

            if (pHierarchy->flags & rpHANIMHIERARCHYNOMATRICES) {
                pHierarchy->pMatrixArray = NULL;
                pHierarchy->pMatrixArrayUnaligned = NULL;
            } else {
                void* ptr;

                ptr = RwMalloc(sizeof(RwMatrix) * numNodes + 15, rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);
                pHierarchy->pMatrixArray = (RwMatrix*)((((RwUInt32)ptr) + 15) & ~15);
                pHierarchy->pMatrixArrayUnaligned = ptr;
            }

            pHierarchy->pNodeInfo = (RpHAnimNodeInfo*)RwMalloc(sizeof(RpHAnimNodeInfo) * numNodes,
                                                               rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);
            pNodeInfo = pHierarchy->pNodeInfo;

            for (i = 0; i < pHierarchy->numNodes; i++) {
                if (!RwStreamReadInt32(stream, &pNodeInfo->nodeID, sizeof(pNodeInfo->nodeID))) {
                    return NULL;
                }
                if (!RwStreamReadInt32(stream, &pNodeInfo->nodeIndex, sizeof(pNodeInfo->nodeIndex))) {
                    return NULL;
                }
                if (!RwStreamReadInt32(stream, &pNodeInfo->flags, sizeof(pNodeInfo->flags))) {
                    return NULL;
                }
                pNodeInfo->pFrame = NULL;
                pNodeInfo++;
            }
        }

        frameExt->hierarchy = pHierarchy;
    }

    return stream;
} // 0x802C46C4

/*
 * ==================== HAnimSize ====================
 * The bytes HAnimWrite writes for a frame, or 0 for a frame with no plugin data.
 */
static RwInt32 HAnimSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RwInt32 needToStream;
    RpHAnimFrameExtension* frameExt;

    RWASSERT(object);

    frameExt = RPHANIMFRAMEGETDATA(object);

    needToStream = (frameExt->id != -1 || frameExt->hierarchy != NULL);

    if (needToStream) {
        RwInt32 size;

        size = sizeof(RwInt32);
        size += sizeof(RwInt32);
        size += sizeof(RwInt32);

        if (frameExt->hierarchy != NULL && !(frameExt->hierarchy->flags & rpHANIMHIERARCHYSUBHIERARCHY)) {
            size += sizeof(RwInt32);
            size += sizeof(RwInt32);
            size += frameExt->hierarchy->numNodes * (sizeof(RwInt32) * 3);
        }

        return size;
    }

    return 0;
} // 0x802C4910

/*
 * ==================== RpHAnimPluginAttach ====================
 * Registers the plugin with the engine and with frames and their streams.
 */
RwBool RpHAnimPluginAttach(void) {
    RwInt32 success;

    if (RwEngineRegisterPlugin(0, rwID_HANIMPLUGIN, HAnimOpen, HAnimClose) < 0) {
        return FALSE;
    }

    RpHAnimAtomicGlobals.engineOffset = RwFrameRegisterPlugin(sizeof(RpHAnimFrameExtension), rwID_HANIMPLUGIN,
                                                              HAnimConstructor, HAnimDestructor, HAnimCopy);
    success = RwFrameRegisterPluginStream(rwID_HANIMPLUGIN, HAnimRead, HAnimWrite, HAnimSize);

    return success >= 0 && RpHAnimAtomicGlobals.engineOffset >= 0;
} // 0x802C4970

/*
 * ==================== RpHAnimHierarchyCreate ====================
 * Creates a hierarchy of numNodes nodes with the interpolator and the matrix array it needs,
 * taking the nodes' flags and IDs from the arrays given, if any.
 */
RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs,
                                         RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    void* ptr;
    RpHAnimHierarchy* pHierarchy;
    RwInt32 node;

    pHierarchy = (RpHAnimHierarchy*)RwFreeListAlloc(RpHAnimAtomicGlobals.HAnimFreeList,
                                                     rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);

    pHierarchy->currentAnim = RtAnimInterpolatorCreate(numNodes, maxInterpKeyFrameSize);
    pHierarchy->flags = flags;
    pHierarchy->numNodes = numNodes;
    pHierarchy->parentFrame = NULL;

    if (!(flags & rpHANIMHIERARCHYNOMATRICES)) {
        ptr = RwMalloc(sizeof(RwMatrix) * numNodes + 15, rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);
        pHierarchy->pMatrixArray = (RwMatrix*)((((RwUInt32)ptr) + 15) & ~15);
        pHierarchy->pMatrixArrayUnaligned = ptr;
    } else {
        pHierarchy->pMatrixArray = NULL;
        pHierarchy->pMatrixArrayUnaligned = NULL;
    }

    pHierarchy->pNodeInfo = (RpHAnimNodeInfo*)RwMalloc(sizeof(RpHAnimNodeInfo) * numNodes,
                                                       rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);

    for (node = 0; node < numNodes; node++) {
        pHierarchy->pNodeInfo[node].pFrame = NULL;
        if (nodeIDs != NULL) {
            pHierarchy->pNodeInfo[node].nodeID = nodeIDs[node];
        }
        pHierarchy->pNodeInfo[node].nodeIndex = node;
        if (nodeFlags != NULL) {
            pHierarchy->pNodeInfo[node].flags = nodeFlags[node];
        }
    }

    pHierarchy->parentHierarchy = pHierarchy;

    return pHierarchy;
} // 0x802C4A1C

/*
 * ==================== RpHAnimHierarchyDestroy ====================
 * Frees a hierarchy and what it owns (a sub-hierarchy owns neither matrices nor nodes) and
 * clears the pointer its frame keeps to it.
 */
RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy) {
    RwFrame* parentFrame;

    parentFrame = hierarchy->parentFrame;

    if (!(hierarchy->flags & rpHANIMHIERARCHYSUBHIERARCHY)) {
        if (!(hierarchy->flags & rpHANIMHIERARCHYNOMATRICES)) {
            RwFree(hierarchy->pMatrixArrayUnaligned);
        }
        RwFree(hierarchy->pNodeInfo);
    }

    hierarchy->pMatrixArrayUnaligned = NULL;
    hierarchy->pMatrixArray = NULL;
    hierarchy->pNodeInfo = NULL;

    RtAnimInterpolatorDestroy(hierarchy->currentAnim);
    RwFreeListFree(RpHAnimAtomicGlobals.HAnimFreeList, hierarchy);

    if (parentFrame != NULL) {
        RPHANIMFRAMEGETDATA(parentFrame)->hierarchy = NULL;
    }

    return NULL;
} // 0x802C4B78

/*
 * ==================== RpHAnimHierarchyCreateSubHierarchy ====================
 * Creates a hierarchy of the nodes of a parent hierarchy from startNode to the end of that
 * node's subtree, sharing the parent's matrices and nodes. Its root's parent is found by
 * walking the nodes before startNode with the parent matrix stack.
 */
RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchy(RpHAnimHierarchy* parentHierarchy, RwInt32 startNode,
                                                     RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    RpHAnimHierarchy* pHierarchy;
    RwInt32 numNodes;
    RwInt32 framesOnStack;
    RpHAnimNodeInfo* pNodeInfo;

    pNodeInfo = &parentHierarchy->pNodeInfo[startNode];
    numNodes = 1;
    framesOnStack = 0;

    if (!(pNodeInfo->flags & rpHANIMPOPPARENTMATRIX)) {
        framesOnStack = 1;
        pNodeInfo++;
    }

    while (framesOnStack != 0) {
        if (pNodeInfo->flags & rpHANIMPUSHPARENTMATRIX) {
            framesOnStack++;
        }
        if (pNodeInfo->flags & rpHANIMPOPPARENTMATRIX) {
            framesOnStack--;
        }
        numNodes++;
        pNodeInfo++;
    }

    pHierarchy = (RpHAnimHierarchy*)RwFreeListAlloc(RpHAnimAtomicGlobals.HAnimFreeList,
                                                     rwMEMHINTDUR_EVENT | rwID_HANIMPLUGIN);

    pHierarchy->flags = flags | rpHANIMHIERARCHYSUBHIERARCHY;
    pHierarchy->numNodes = numNodes;
    pHierarchy->currentAnim = RtAnimInterpolatorCreateSubInterpolator(parentHierarchy->currentAnim, startNode,
                                                                      numNodes, maxInterpKeyFrameSize);

    if (startNode == 0) {
        pHierarchy->rootParentOffset = -1;
    } else {
        RwInt32 i;
        RwInt32 pNodeStack[32];
        RwInt32* pNodeStackPtr;
        RwInt32 parentNode;

        pNodeStackPtr = pNodeStack;
        parentNode = -1;

        for (i = 0; i < startNode; i++) {
            if (parentHierarchy->pNodeInfo[i].flags & rpHANIMPUSHPARENTMATRIX) {
                *pNodeStackPtr = parentNode;
                pNodeStackPtr++;
            }

            if (parentHierarchy->pNodeInfo[i].flags & rpHANIMPOPPARENTMATRIX) {
                pNodeStackPtr--;
                parentNode = *pNodeStackPtr;
            } else {
                parentNode = i;
            }
        }

        pHierarchy->rootParentOffset = parentNode;
    }

    pHierarchy->parentFrame = parentHierarchy->pNodeInfo[startNode].pFrame;
    pHierarchy->pMatrixArray = parentHierarchy->pMatrixArray + startNode;
    pHierarchy->pMatrixArrayUnaligned = NULL;
    pHierarchy->pNodeInfo = parentHierarchy->pNodeInfo + startNode;
    pHierarchy->parentHierarchy = parentHierarchy;

    return pHierarchy;
} // 0x802C4C3C

/*
 * ==================== RpHAnimHierarchyAttach ====================
 * Links the nodes of a hierarchy to the frames below its parent frame that carry their IDs.
 */
RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy) {
    HAnimFramesLinkToHierarchy(hierarchy->parentFrame, hierarchy);

    return hierarchy;
} // 0x802C4DC8

/*
 * ==================== RpHAnimHierarchyGetMatrixArray ====================
 * The matrices the hierarchy's last update produced, one per node.
 */
RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy) {
    return hierarchy->pMatrixArray;
} // 0x802C4E5C

/*
 * ==================== RpHAnimFrameGetHierarchy ====================
 * The hierarchy rooted at a frame, or NULL.
 */
RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* frame) {
    return RPHANIMFRAMEGETDATA(frame)->hierarchy;
} // 0x802C4E64

/*
 * ==================== RpHAnimFrameGetID ====================
 * The ID of the node a frame is, or -1. Inlined into HAnimFramesLinkToHierarchy in retail.
 */
RwInt32 RpHAnimFrameGetID(RwFrame* frame) {
    RwInt32 result = -1;
    RpHAnimFrameExtension* frameExt;

    RWASSERT(frame);

    frameExt = RPHANIMFRAMEGETDATA(frame);
    result = frameExt->id;

    return result;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== RpHAnimIDGetIndex ====================
 * The index of the node with an ID in a hierarchy, or -1.
 */
RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID) {
    RwInt32 result = -1;
    RwInt32 i;
    RpHAnimNodeInfo* pNodeInfo;

    RWASSERT(hierarchy);

    pNodeInfo = hierarchy->pNodeInfo;
    for (i = 0; i < hierarchy->numNodes; i++) {
        if (ID == pNodeInfo[i].nodeID) {
            result = i;
            break;
        }
    }

    return result;
} // 0x802C4E74

/*
 * ==================== HAnimHierarchyUpdateFramesOnly ====================
 * Sets the frames of a hierarchy that has no matrix array from its current animation: finds the
 * matrix of the hierarchy root's parent, then walks the nodes in order, concatenating each node's
 * keyframe matrix with the parent matrix stack and writing the result to the node's frame, as the
 * hierarchy's flags say (the modelling matrix, the LTM).
 */
static RwBool HAnimHierarchyUpdateFramesOnly(RpHAnimHierarchy* hierarchy) {
    RwUInt32 flags = hierarchy->flags;
    RwFrame* parentFrame = RwFrameGetParent(hierarchy->parentFrame);
    RwInt32 keyFrameSize;
    RtAnimKeyFrameApplyCallBack keyFrameApplyCB;
    RwMatrix parentStack[32];
    RwMatrix* stackTop;
    RwMatrix parentMatrix;
    RwInt32 iNode;
    RpHAnimNodeInfo* nodeInfo;
    void* nodeInterpFrame;

    if (parentFrame != NULL) {
        if (!RwFrameDirty(parentFrame)) {
            RwMatrixCopy(&parentMatrix, RwFrameGetLTM(parentFrame));
        } else {
            RwMatrixCopy(&parentMatrix, RwFrameGetMatrix(parentFrame));
            parentFrame = RwFrameGetParent(parentFrame);

            while (parentFrame != NULL) {
                RwMatrix tempMatrix;

                RwMatrixCopy(&tempMatrix, &parentMatrix);
                RwMatrixMultiply(&parentMatrix, &tempMatrix, RwFrameGetMatrix(parentFrame));
                parentFrame = RwFrameGetParent(parentFrame);
            }
        }
    } else {
        RwMatrixSetIdentity(&parentMatrix);
    }

    if (flags & rpHANIMHIERARCHYUPDATELTMS) {
        if (!rwObjectTestPrivateFlags(hierarchy->parentFrame->root, rwFRAMEPRIVATEHIERARCHYSYNC)) {
            rwLinkListAddLLLink(&RWSRCGLOBAL(dirtyFrameList), &hierarchy->parentFrame->root->inDirtyListLink);
            rwObjectSetPrivateFlags(hierarchy->parentFrame->root,
                                    rwObjectGetPrivateFlags(hierarchy->parentFrame->root) |
                                        rwFRAMEPRIVATEHIERARCHYSYNCOBJ);
        }
    }

    nodeInfo = hierarchy->pNodeInfo;
    stackTop = &parentStack[1]; /* the bytes start the stack one entry in; parentStack[0] is never used */
    keyFrameApplyCB = hierarchy->currentAnim->keyFrameApplyCB;
    nodeInterpFrame = rtANIMGETINTERPFRAME(hierarchy->currentAnim, 0);
    keyFrameSize = hierarchy->currentAnim->currentInterpKeyFrameSize;

    for (iNode = 0; iNode < hierarchy->numNodes; iNode++) {
        /* Declared in this order the compiler gives nodeMatrix the lower address, as retail has it. The DWARF
           lists nodeMatrix as a RwMatrix* at function scope (another revision of the loop); here it is the
           matrix itself, a value of its own, not a carrier. */
        RwFrame* nodeFrame;
        RwMatrix nodeModelMatrix;
        RwMatrix nodeMatrix;

        if (RpHAnimKeyFrameApply == keyFrameApplyCB) {
            RpHAnimKeyFrameToMatrixMacro(&nodeModelMatrix, nodeInterpFrame);
        } else {
            keyFrameApplyCB(&nodeModelMatrix, nodeInterpFrame);
        }

        RwMatrixMultiply(&nodeMatrix, &nodeModelMatrix, &parentMatrix);

        nodeFrame = nodeInfo->pFrame;
        if (nodeFrame != NULL) {
            if (flags & rpHANIMHIERARCHYUPDATEMODELLINGMATRICES) {
                RwMatrixCopy(RwFrameGetMatrix(nodeFrame), &nodeModelMatrix);

                if (!(flags & rpHANIMHIERARCHYUPDATELTMS)) {
                    RwFrameUpdateObjects(nodeFrame);
                }
            }

            if (flags & rpHANIMHIERARCHYUPDATELTMS) {
                RwMatrixCopy(&nodeFrame->ltm, &nodeMatrix);
                rwObjectSetPrivateFlags(nodeFrame, (rwObjectGetPrivateFlags(nodeFrame) &
                                                    ~rwFRAMEPRIVATESUBTREESYNCLTM) |
                                                       rwFRAMEPRIVATESUBTREESYNCOBJ);
            }
        }

        switch (nodeInfo->flags & (rpHANIMPUSHPARENTMATRIX | rpHANIMPOPPARENTMATRIX)) {
        case 0:
            RwMatrixCopy(&parentMatrix, &nodeMatrix);
            break;
        case rpHANIMPOPPARENTMATRIX:
            stackTop--;
            RwMatrixCopy(&parentMatrix, stackTop);
            break;
        case rpHANIMPUSHPARENTMATRIX:
            RwMatrixCopy(stackTop, &parentMatrix);
            stackTop++;
            RwMatrixCopy(&parentMatrix, &nodeMatrix);
            break;
        case rpHANIMPOPPARENTMATRIX | rpHANIMPUSHPARENTMATRIX:
            break;
        }

        nodeInterpFrame = (RwUInt8*)nodeInterpFrame + keyFrameSize;
        nodeInfo++;
    }

    return TRUE;
} // 0x802C4EB8

/*
 * ==================== RpHAnimHierarchyUpdateMatrices ====================
 * Updates the matrix array of a hierarchy from its current animation: the keyframe of each node
 * is made a matrix and concatenated with the parent matrix stack, and, as the hierarchy's flags say,
 * written to the node's frame (its modelling matrix, its LTM). A hierarchy with no matrix array
 * only updates its frames. Matrices are relative to the hierarchy's root unless the hierarchy
 * asks for the local space, and a sub-hierarchy starts from the matrix of its root's parent.
 */
RwBool RpHAnimHierarchyUpdateMatrices(RpHAnimHierarchy* hierarchy) {
    RwUInt32 flags = hierarchy->flags;
    RwBool needHierarchyLTM = FALSE;
    RwFrame* hierarchyRootFrame = NULL;
    RwMatrix hierarchyLTM;
    RwMatrix identity;
    RwInt32 keyFrameSize;
    RtAnimKeyFrameApplyCallBack keyFrameApplyCB;
    RwMatrix* parentStack[32];
    RwMatrix** stackTop;
    RwMatrix* parentMatrix;
    RwInt32 iNode;
    RpHAnimNodeInfo* nodeInfo;
    RwMatrix* nodeMatrix;
    void* nodeInterpFrame;

    if (flags & rpHANIMHIERARCHYNOMATRICES) {
        return HAnimHierarchyUpdateFramesOnly(hierarchy);
    }

    if ((flags & rpHANIMHIERARCHYSUBHIERARCHY) && hierarchy->rootParentOffset != -1) {
        parentMatrix = &hierarchy->parentHierarchy->pMatrixArray[hierarchy->rootParentOffset];

        if ((flags & rpHANIMHIERARCHYUPDATELTMS) && (flags & rpHANIMHIERARCHYLOCALSPACEMATRICES)) {
            needHierarchyLTM = TRUE;
            hierarchyRootFrame = hierarchy->parentHierarchy->parentFrame;
        }
    } else if (flags & rpHANIMHIERARCHYLOCALSPACEMATRICES) {
        RwMatrixSetIdentity(&identity);
        parentMatrix = &identity;

        if (flags & rpHANIMHIERARCHYUPDATELTMS) {
            needHierarchyLTM = TRUE;
            hierarchyRootFrame = hierarchy->parentFrame;
        }
    } else {
        parentMatrix = &hierarchyLTM;
        needHierarchyLTM = TRUE;
        hierarchyRootFrame = hierarchy->parentFrame;
    }

    if (needHierarchyLTM) {
        RwFrame* parentFrame;

        if (hierarchyRootFrame != NULL && (parentFrame = RwFrameGetParent(hierarchyRootFrame)) != NULL) {
            if (!RwFrameDirty(parentFrame)) {
                RwMatrixCopy(&hierarchyLTM, RwFrameGetLTM(parentFrame));
            } else {
                RwMatrix tempMatrix;

                RwMatrixCopy(&hierarchyLTM, RwFrameGetMatrix(parentFrame));
                parentFrame = RwFrameGetParent(parentFrame);

                while (parentFrame != NULL) {
                    RwMatrixCopy(&tempMatrix, &hierarchyLTM);
                    RwMatrixMultiply(&hierarchyLTM, &tempMatrix, RwFrameGetMatrix(parentFrame));
                    parentFrame = RwFrameGetParent(parentFrame);
                }
            }
        } else {
            RwMatrixSetIdentity(&hierarchyLTM);
        }
    }

    if (flags & rpHANIMHIERARCHYUPDATELTMS) {
        if (!rwObjectTestPrivateFlags(hierarchy->parentFrame->root, rwFRAMEPRIVATEHIERARCHYSYNC)) {
            rwLinkListAddLLLink(&RWSRCGLOBAL(dirtyFrameList), &hierarchy->parentFrame->root->inDirtyListLink);
            rwObjectSetPrivateFlags(hierarchy->parentFrame->root,
                                    rwObjectGetPrivateFlags(hierarchy->parentFrame->root) |
                                        rwFRAMEPRIVATEHIERARCHYSYNCOBJ);
        }
    }

    nodeInfo = hierarchy->pNodeInfo;
    stackTop = &parentStack[1]; /* the bytes start the stack one entry in; parentStack[0] is never used */
    keyFrameApplyCB = hierarchy->currentAnim->keyFrameApplyCB;
    nodeInterpFrame = rtANIMGETINTERPFRAME(hierarchy->currentAnim, 0);
    keyFrameSize = hierarchy->currentAnim->currentInterpKeyFrameSize;
    nodeMatrix = hierarchy->pMatrixArray;

    for (iNode = 0; iNode < hierarchy->numNodes; iNode++) {
        RwFrame* nodeFrame;
        RwMatrix nodeModelMatrix;

        if (RpHAnimKeyFrameApply == keyFrameApplyCB) {
            RpHAnimKeyFrameToMatrixMacro(&nodeModelMatrix, nodeInterpFrame);
        } else {
            keyFrameApplyCB(&nodeModelMatrix, nodeInterpFrame);
        }

        RwMatrixMultiply(nodeMatrix, &nodeModelMatrix, parentMatrix);

        nodeFrame = nodeInfo->pFrame;
        if (nodeFrame != NULL) {
            if (flags & rpHANIMHIERARCHYUPDATEMODELLINGMATRICES) {
                RwMatrixCopy(RwFrameGetMatrix(nodeFrame), &nodeModelMatrix);

                if (!(flags & rpHANIMHIERARCHYUPDATELTMS)) {
                    RwFrameUpdateObjects(nodeFrame);
                }
            }

            if (flags & rpHANIMHIERARCHYUPDATELTMS) {
                if (flags & rpHANIMHIERARCHYLOCALSPACEMATRICES) {
                    RwMatrixMultiply(&nodeFrame->ltm, nodeMatrix, &hierarchyLTM);
                } else {
                    RwMatrixCopy(&nodeFrame->ltm, nodeMatrix);
                }

                rwObjectSetPrivateFlags(nodeFrame, (rwObjectGetPrivateFlags(nodeFrame) &
                                                    ~rwFRAMEPRIVATESUBTREESYNCLTM) |
                                                       rwFRAMEPRIVATESUBTREESYNCOBJ);
            }
        }

        switch (nodeInfo->flags & (rpHANIMPUSHPARENTMATRIX | rpHANIMPOPPARENTMATRIX)) {
        case 0:
            parentMatrix = nodeMatrix;
            break;
        case rpHANIMPOPPARENTMATRIX:
            stackTop--;
            parentMatrix = *stackTop;
            break;
        case rpHANIMPUSHPARENTMATRIX:
            *stackTop = parentMatrix;
            stackTop++;
            parentMatrix = nodeMatrix;
            break;
        case rpHANIMPOPPARENTMATRIX | rpHANIMPUSHPARENTMATRIX:
            break;
        }

        nodeInterpFrame = (RwUInt8*)nodeInterpFrame + keyFrameSize;
        nodeMatrix++;
        nodeInfo++;
    }

    return TRUE;
} // 0x802C56D4
