// Unit name and boundary: Gormiti link map (RpUVAnim.a rpuvanim.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpUVAnim.a rpuvanim.obj), Final and Debug builds.
// CreateDummyAnim, _rwStreamReadSingleUVAnim, _rpUVAnimInit, _rpUVAnimDestruct, RpUVAnimCreate and
// RpMaterialUVAnimSetCurrentTime: Gormiti Debug build (functions there; inlined or dropped in retail).
// The UV matrices are tested for truth, not compared with NULL: a comparison of a pointer to the aligned
// RwMatrix typedef turns the IR optimiser off for the whole function (docs/compiler_regalloc.md, "IR optimiser
// gate"), and without it the channel and slot loops of UVAnimDestructor and _rpUVAnimCreateConsistantMatrices
// index with an offset register where retail walks pointer induction variables.
/*
 * rpuvanim.c: RpUVAnim, UV animation. The plugin extends materials with the UV matrices of their
 * two texture channels and the interpolators of up to eight animations, streams the names of the
 * animations (an animation is looked up by name in the current dictionary when a material is
 * read), and turns the interpolators' keyframes into the matrices the MatFX plugin applies. The
 * unit also owns the dictionary schema of UV animations and registers the keyframe schemes of
 * linear.c and param.c when the engine opens.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/plugin/matfx/rpmatfx.h>
#include <rwsdk/plugin/uvanim/rpuvanim.h>
#include <rwsdk/tool/anim/rtanim.h>
#include <rwsdk/tool/dict/rtdict.h>
#include <rwsdk/world/bamateri.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65): the chunk headers it writes carry that number.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/* The version of the dictionary schema's streams and the plugin's bit flags of a material's animations. */
#define rpUVANIMDICTSTREAMCOMPATIBILITYVERSION 0x36000
#define rpUVANIMSTREAMNODES 1

/* Which animations a material's stream has: one bit for each. */
enum MaterialExtStreamFlags {
    MaterialExtHasChannel0Anim = 1,
    MaterialExtHasChannel1Anim = 2,
    MaterialExtHasChannel2Anim = 4,
    MaterialExtHasChannel3Anim = 8,
    MaterialExtHasChannel4Anim = 16,
    MaterialExtHasChannel5Anim = 32,
    MaterialExtHasChannel6Anim = 64,
    MaterialExtHasChannel7Anim = 128,
    MaterialExtStreamFlagsSizeInt = 0x7FFFFFFF
};
typedef enum MaterialExtStreamFlags MaterialExtStreamFlags;

/* The plugin's object callbacks and stream callbacks, registered by RpUVAnimPluginAttach. */
static void* UVAnimConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
static void* UVAnimDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
static void* UVAnimCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject);
static RwStream* UVAnimRead(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                            RwInt32 sizeInObject);
static RwStream* UVAnimWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject,
                             RwInt32 sizeInObject);
static RwInt32 UVAnimSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

/* The module and the plugin's globals (rpuvanim.h). The compiler lays .sbss out in reverse definition order. */
RwInt32 RpUVAnimMaterialGlobals;
RwFreeList* _rpUVAnimCustomDataFreeList;
RwModuleInfo rpUVAnimModule;

/* The custom data free list's block size and preallocated blocks. */
RwInt32 _rpUVAnimCustomDataFreeListBlockSize = 128;
RwInt32 _rpUVAnimCustomDataFreeListPreallocBlocks = 1;

/* The point the parametric scheme rotates and scales about, and its negative. */
const RwV3d rpUVAnimPivot = {-0.5f, -0.5f, 0.0f};
const RwV3d rpUVAnimInvPivot = {0.5f, 0.5f, 0.0f};

/* The dictionary schema of UV animations; its entries are the animations, named by their custom data. */
RtDictSchema _rpUVAnimDictSchema = {
    "UVAnim",
    rwID_UVANIMDICT,
    rwID_HANIMANIMATION,
    rpUVANIMDICTSTREAMCOMPATIBILITYVERSION,
    NULL,
    NULL,
    (RtDictEntryAddRefCallBack*)RpUVAnimAddRef,
    (RtDictEntryDestroyCallBack*)RpUVAnimDestroy,
    (RtDictEntryGetNameCallBack*)RpUVAnimGetName,
    (RtDictEntryStreamGetSizeCallBack*)RtAnimAnimationStreamGetSize,
    (RtDictEntryStreamReadCallBack*)RtAnimAnimationStreamRead,
    NULL,
    (RtDictEntryStreamWriteCallBack*)RtAnimAnimationStreamWrite};

/* The standard keyframe schemes of UV animations. */
RtAnimInterpolatorInfo _rpUVAnimLinearInterpolatorInfo = {
    rpUVANIMLINEARKEYFRAMETYPEID,
    sizeof(RpUVAnimInterpFrame),
    sizeof(RpUVAnimKeyFrame),
    RpUVAnimLinearKeyFrameApply,
    RpUVAnimLinearKeyFrameBlend,
    RpUVAnimLinearKeyFrameInterpolate,
    RpUVAnimLinearKeyFrameAdd,
    RpUVAnimLinearKeyFrameMulRecip,
    RpUVAnimKeyFrameStreamRead,
    RpUVAnimKeyFrameStreamWrite,
    RpUVAnimKeyFrameStreamGetSize,
    sizeof(_rpUVAnimCustomData),
    0};

RtAnimInterpolatorInfo _rpUVAnimParamInterpolatorInfo = {
    rpUVANIMPARAMKEYFRAMETYPEID,
    sizeof(RpUVAnimInterpFrame),
    sizeof(RpUVAnimKeyFrame),
    RpUVAnimParamKeyFrameApply,
    RpUVAnimParamKeyFrameBlend,
    RpUVAnimParamKeyFrameInterpolate,
    RpUVAnimParamKeyFrameAdd,
    RpUVAnimParamKeyFrameMulRecip,
    RpUVAnimKeyFrameStreamRead,
    RpUVAnimKeyFrameStreamWrite,
    RpUVAnimKeyFrameStreamGetSize,
    sizeof(_rpUVAnimCustomData),
    0};

/*
 * ==================== UVAnimOpen ====================
 * Creates the custom data free list when an engine instance opens, initialises the dictionary
 * schema and registers the two keyframe schemes.
 */
static void* UVAnimOpen(void* instance, RwInt32 offset, RwInt32 size) {
    // Original bug kept: the free list's in-place space is the pointer's old value, not its address.
    _rpUVAnimCustomDataFreeList = RwFreeListCreateAndPreallocateSpace(
        sizeof(_rpUVAnimCustomData), _rpUVAnimCustomDataFreeListBlockSize, sizeof(RwUInt32),
        _rpUVAnimCustomDataFreeListPreallocBlocks, _rpUVAnimCustomDataFreeList,
        rwMEMHINTDUR_GLOBAL | rwID_UVANIMPLUGIN);
    if (_rpUVAnimCustomDataFreeList == NULL) {
        instance = NULL;
    }

    RtDictSchemaInit(&_rpUVAnimDictSchema);
    RtAnimRegisterInterpolationScheme(&_rpUVAnimLinearInterpolatorInfo);
    RtAnimRegisterInterpolationScheme(&_rpUVAnimParamInterpolatorInfo);

    rpUVAnimModule.numInstances++;

    return instance;
} // 0x802CC818

/*
 * ==================== UVAnimClose ====================
 * Destroys the dictionary schema and the custom data free list when an engine instance closes.
 */
static void* UVAnimClose(void* instance, RwInt32 offset, RwInt32 size) {
    rpUVAnimModule.numInstances--;

    RtDictSchemaDestruct(&_rpUVAnimDictSchema);

    if (_rpUVAnimCustomDataFreeList != NULL) {
        RwFreeListDestroy(_rpUVAnimCustomDataFreeList);
        _rpUVAnimCustomDataFreeList = NULL;
    }

    return instance;
} // 0x802CC8AC

/*
 * ==================== RpUVAnimPluginAttach ====================
 * Registers the plugin with the engine and with materials and their streams.
 */
RwBool RpUVAnimPluginAttach(void) {
    RwInt32 success;

    if (RwEngineRegisterPlugin(0, rwID_UVANIMPLUGIN, UVAnimOpen, UVAnimClose) < 0) {
        return FALSE;
    }

    RpUVAnimMaterialGlobals = RpMaterialRegisterPlugin(sizeof(RpUVAnimMaterialExtension), rwID_UVANIMPLUGIN,
                                                       UVAnimConstructor, UVAnimDestructor, UVAnimCopy);
    success = RpMaterialRegisterPluginStream(rwID_UVANIMPLUGIN, UVAnimRead, UVAnimWrite, UVAnimSize);

    return success >= 0 && RpUVAnimMaterialGlobals >= 0;
} // 0x802CC90C

/*
 * ==================== UVAnimConstructor ====================
 * A new material animates nothing.
 */
static void* UVAnimConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    memset(RPUVANIMMATERIALGETDATA(object), 0, sizeof(RpUVAnimMaterialExtension));

    return object;
} // 0x802CC9B8

/*
 * ==================== UVAnimDestructor ====================
 * Destroys the UV matrices of a material and releases its animations with their interpolators.
 */
static void* UVAnimDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpUVAnimMaterialExtension* materialExt;
    RwUInt32 i;

    materialExt = RPUVANIMMATERIALGETDATA(object);

    for (i = 0; i < rpUVANIMMAXCHANNELS; i++) {
        RwMatrix** matrix = &materialExt->uv[i];

        if (*matrix) {
            RwMatrixDestroy(*matrix);
        }
        *matrix = NULL;
    }

    for (i = 0; i < rpUVANIMMAXNODES; i++) {
        RtAnimInterpolator* interp = materialExt->interp[i];

        if (interp != NULL) {
            RtAnimAnimation* anim = interp->pCurrentAnim;

            RtAnimInterpolatorDestroy(interp);
            RpUVAnimDestroy(anim);
        }
    }

    return object;
} // 0x802CC9F8

/*
 * ==================== UVAnimCopy ====================
 * Gives the copy of a material interpolators of its own over the animations of its source, which
 * it holds a reference to. The copy gets no UV matrices. A failure releases what was made.
 */
static void* UVAnimCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpUVAnimMaterialExtension* dstMaterialExt;
    RpUVAnimMaterialExtension* srcMaterialExt;

    srcMaterialExt = RPUVANIMMATERIALGETDATA(srcObject);
    dstMaterialExt = RPUVANIMMATERIALGETDATA(dstObject);

    {
        RwUInt32 allocateIndex;
        RwBool allocSuccess = TRUE;

        for (allocateIndex = 0; allocateIndex < rpUVANIMMAXNODES; allocateIndex++) {
            RtAnimInterpolator* interp = srcMaterialExt->interp[allocateIndex];
            RtAnimInterpolator** dest = &dstMaterialExt->interp[allocateIndex];

            if (interp != NULL) {
                RtAnimAnimation* anim = interp->pCurrentAnim;

                *dest = RtAnimInterpolatorCreate(RtAnimAnimationGetNumNodes(anim), anim->interpInfo->interpKeyFrameSize);
                if (*dest != NULL) {
                    RtAnimInterpolatorCopy(*dest, interp);
                    RpUVAnimAddRef(anim);
                } else {
                    allocSuccess = FALSE;
                    break;
                }
            } else {
                *dest = NULL;
            }
        }

        if (!allocSuccess) {
            RwUInt32 cleanupIndex;

            for (cleanupIndex = 0; cleanupIndex < allocateIndex; cleanupIndex++) {
                RtAnimInterpolator** dest = &dstMaterialExt->interp[cleanupIndex];

                if (*dest != NULL) {
                    RtAnimAnimation* anim = (*dest)->pCurrentAnim;

                    RpUVAnimDestroy(anim);
                    RtAnimInterpolatorDestroy(*dest);
                }
            }

            return NULL;
        }
    }

    return dstObject;
} // 0x802CCACC

/*
 * ==================== _rpUVAnimCreateConsistantMatrices ====================
 * Gives a material a UV matrix for each channel some animation node maps to, and takes it away
 * from each channel none does.
 */
static RpUVAnimMaterialExtension* _rpUVAnimCreateConsistantMatrices(RpUVAnimMaterialExtension* matExt) {
    RwUInt32 channel;

    for (channel = 0; channel < rpUVANIMMAXCHANNELS; channel++) {
        RwUInt32 slot = 0;
        RwUInt32 node = 0;
        RwMatrix** matrix = &matExt->uv[channel];
        RwBool needAMatrix = FALSE;

        while (slot < rpUVANIMMAXNODES) {
            RtAnimInterpolator* interp = matExt->interp[slot];

            if (interp != NULL) {
                _rpUVAnimCustomData* customData = (_rpUVAnimCustomData*)interp->pCurrentAnim->customData;

                if (customData->nodeToUVChannelMap[node] == channel) {
                    needAMatrix = TRUE;
                    break;
                }

                node++;
                if (node >= interp->numNodes) {
                    node = 0;
                    slot++;
                }
            } else {
                slot++;
            }
        }

        if (needAMatrix && !*matrix) {
            *matrix = RwMatrixCreate();
            if (!*matrix) {
                return NULL;
            }
        } else if (!needAMatrix && *matrix) {
            RwMatrixDestroy(*matrix);
            *matrix = NULL;
        }
    }

    return matExt;
} // 0x802CCC0C

/*
 * ==================== UVAnimWrite ====================
 * Writes a material's plugin data: a flag for each channel that has an animation, then the name
 * of each animation.
 */
static RwStream* UVAnimWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject,
                             RwInt32 sizeInObject) {
    RpUVAnimMaterialExtension* materialExt;
    RwUInt32 size;
    MaterialExtStreamFlags flags;

    materialExt = RPUVANIMMATERIALGETDATA(object);

    flags = 0;
    size = sizeof(flags);
    {
        RwUInt32 i;

        for (i = 0; i < rpUVANIMMAXNODES; i++) {
            if (materialExt->interp[i] != NULL) {
                flags |= 1 << i;
                size += rpUVANIMNAMESIZE;
            }
        }
    }

    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, size)) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&flags, sizeof(flags))) {
        return NULL;
    }

    {
        RwUInt32 i;

        for (i = 0; i < rpUVANIMMAXNODES; i++) {
            RtAnimInterpolator* interp = materialExt->interp[i];

            if (interp != NULL) {
                RwStreamWrite(stream, ((_rpUVAnimCustomData*)interp->pCurrentAnim->customData)->name,
                              rpUVANIMNAMESIZE);
            }
        }
    }

    return stream;
} // 0x802CCD2C

/*
 * ==================== CreateDummyAnim ====================
 * Makes the stand-in for an animation the dictionary does not have: a linear animation of one
 * node and two keyframes, both the identity.
 */
static RtAnimAnimation* CreateDummyAnim(const RwChar* name) {
    RtAnimAnimation* anim;
    RwUInt32 dummyChannelMap[1] = {0};
    RpUVAnimKeyFrame* first;
    RpUVAnimKeyFrame* last;
    RwMatrix identity;

    anim = RpUVAnimCreate(name, 1, 2, 1.0f, dummyChannelMap, rpUVANIMLINEARKEYFRAMES);
    if (anim == NULL) {
        return NULL;
    }

    first = (RpUVAnimKeyFrame*)anim->pFrames;
    last = (RpUVAnimKeyFrame*)((RwUInt8*)first + anim->interpInfo->animKeyFrameSize);

    RwMatrixSetIdentity(&identity);
    RpUVAnimKeyFrameInit(anim, first, NULL, 0.0f, &identity);
    RpUVAnimKeyFrameInit(anim, last, first, 1.0f, &identity);

    return anim;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== _rwStreamReadSingleUVAnim ====================
 * Reads the name of an animation and finds the animation by it in the current dictionary,
 * holding a reference to it. An animation the dictionary lacks, or a stream with no current
 * dictionary, gets a stand-in.
 */
static RwStream* _rwStreamReadSingleUVAnim(RwStream* stream, RtAnimAnimation** anim) {
    RwChar name[rpUVANIMNAMESIZE];

    RWASSERT(stream);

    if (!RwStreamRead(stream, name, sizeof(name))) {
        return NULL;
    }

    {
        RtDictSchema* schema = &_rpUVAnimDictSchema;
        RtDict* dict = RtDictSchemaGetCurrentDict(schema);

        if (dict != NULL) {
            *anim = (RtAnimAnimation*)RtDictFindNamedEntry(dict, name);
            if (*anim != NULL) {
                RpUVAnimAddRef(*anim);
            } else {
                *anim = CreateDummyAnim(name);
            }
        } else {
            *anim = CreateDummyAnim(name);
        }
    }

    if (*anim == NULL) {
        return NULL;
    }

    return stream;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== UVAnimRead ====================
 * Reads a material's plugin data written by UVAnimWrite: the animations it names, each with an
 * interpolator of its own, and the UV matrices the animations need.
 */
static RwStream* UVAnimRead(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                            RwInt32 sizeInObject) {
    MaterialExtStreamFlags flags;
    RpUVAnimMaterialExtension* matExt;
    RwUInt32 i;

    matExt = RPUVANIMMATERIALGETDATA(object);

    if (!RwStreamFindChunk(stream, rwID_STRUCT, NULL, NULL) ||
        !RwStreamReadInt32(stream, (RwInt32*)&flags, sizeof(flags))) {
        return NULL;
    }

    for (i = 0; i < rpUVANIMMAXNODES; i++) {
        if (flags & (1 << i)) {
            RtAnimAnimation* anim;

            if (!_rwStreamReadSingleUVAnim(stream, &anim)) {
                return NULL;
            }

            if (anim != NULL) {
                RtAnimInterpolator** interp = &matExt->interp[i];

                *interp = RtAnimInterpolatorCreate(RtAnimAnimationGetNumNodes(anim),
                                                   anim->interpInfo->interpKeyFrameSize);
                if (*interp == NULL || !RtAnimInterpolatorSetCurrentAnim(*interp, anim)) {
                    RpUVAnimDestroy(anim);
                    return NULL;
                }
            }
        }
    }

    if (!_rpUVAnimCreateConsistantMatrices(matExt)) {
        return NULL;
    }

    return stream;
} // 0x802CCE94

/*
 * ==================== UVAnimSize ====================
 * The bytes UVAnimWrite writes for a material, or 0 for a material that animates nothing.
 */
static RwInt32 UVAnimSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    if (RpMaterialUVAnimExists((const RpMaterial*)object)) {
        RpUVAnimMaterialExtension* materialExt = RPUVANIMMATERIALGETDATA(object);
        RwInt32 size = rwCHUNKHEADERSIZE + sizeof(MaterialExtStreamFlags);
        RwUInt32 i;

        for (i = 0; i < rpUVANIMMAXNODES; i++) {
            if (materialExt->interp[i] != NULL) {
                size += rpUVANIMNAMESIZE;
            }
        }

        return size;
    }

    return 0;
} // 0x802CD2C0

/*
 * ==================== _rpUVAnimCustomDataStreamRead ====================
 * Reads an animation's custom data: its name and the channel each node maps to. It holds one
 * reference. A read that fails loses the memory it took.
 */
_rpUVAnimCustomData* _rpUVAnimCustomDataStreamRead(RwStream* stream) {
    _rpUVAnimCustomData* customData;

    customData = (_rpUVAnimCustomData*)RwFreeListAlloc(_rpUVAnimCustomDataFreeList,
                                                       rwMEMHINTDUR_EVENT | rwID_UVANIMPLUGIN);
    if (customData == NULL) {
        return NULL;
    }

    if (!RwStreamRead(stream, customData->name, sizeof(customData->name))) {
        return NULL;
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)customData->nodeToUVChannelMap, sizeof(customData->nodeToUVChannelMap))) {
        return NULL;
    }

    customData->refCount = 1;

    return customData;
} // 0x802CD408

/*
 * ==================== _rpUVAnimCustomDataStreamWrite ====================
 * Writes what _rpUVAnimCustomDataStreamRead reads; returns the custom data (NULL on failure).
 */
const _rpUVAnimCustomData* _rpUVAnimCustomDataStreamWrite(const _rpUVAnimCustomData* customData, RwStream* stream) {
    if (!RwStreamWrite(stream, customData->name, sizeof(customData->name))) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)customData->nodeToUVChannelMap,
                            sizeof(customData->nodeToUVChannelMap))) {
        return NULL;
    }

    return customData;
} // 0x802CD4B4

/*
 * ==================== _rpUVAnimCustomDataStreamGetSize ====================
 * The bytes _rpUVAnimCustomDataStreamWrite writes.
 */
RwInt32 _rpUVAnimCustomDataStreamGetSize(const _rpUVAnimCustomData* customData) {
    return sizeof(customData->name) + sizeof(customData->nodeToUVChannelMap);
} // 0x802CD52C

/*
 * ==================== _rpUVAnimInit ====================
 * Gives an animation custom data: its name, the channels its nodes map to and one reference.
 */
static RtAnimAnimation* _rpUVAnimInit(RtAnimAnimation* anim, const RwChar* name, RwUInt32 numNodes,
                                      RwUInt32* nodeToUVChannelMap) {
    RWASSERT(anim);
    RWASSERT(numNodes >= 1);
    RWASSERT(numNodes <= rpUVANIMMAXNODES);
    RWASSERT(nodeToUVChannelMap);

    {
        _rpUVAnimCustomData* customData;

        customData = (_rpUVAnimCustomData*)RwFreeListAlloc(_rpUVAnimCustomDataFreeList,
                                                           rwMEMHINTDUR_EVENT | rwID_UVANIMPLUGIN);
        if (customData == NULL) {
            return NULL;
        }

        strncpy(customData->name, name, rpUVANIMNAMESIZE);
        customData->name[rpUVANIMNAMESIZE - 1] = '\0';
        memcpy(customData->nodeToUVChannelMap, nodeToUVChannelMap, numNodes * sizeof(RwUInt32));
        customData->refCount = 1;
        anim->customData = customData;
    }

    return anim;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== _rpUVAnimDestruct ====================
 * Releases a reference to the custom data of an animation; the last frees it.
 */
static RwBool _rpUVAnimDestruct(RtAnimAnimation* anim) {
    _rpUVAnimCustomData* customData;

    RWASSERT(anim);

    customData = (_rpUVAnimCustomData*)anim->customData;
    customData->refCount--;
    if (customData->refCount != 0) {
        return FALSE;
    }

    RwFreeListFree(_rpUVAnimCustomDataFreeList, anim->customData);

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== RpUVAnimCreate ====================
 * Creates an animation of numNodes nodes and numFrames keyframes of one of the two schemes,
 * named and with the channels its nodes map to.
 */
RpUVAnim* RpUVAnimCreate(const RwChar* name, RwUInt32 numNodes, RwUInt32 numFrames, RwReal duration,
                         RwUInt32* nodeIndexToUVChannelMap, RpUVAnimKeyFrameType keyframeType) {
    RtAnimAnimation* anim = NULL;

    RWASSERT(rpUVAnimModule.numInstances);
    RWASSERT(numNodes >= 1);
    RWASSERT(numNodes <= rpUVANIMMAXNODES);
    RWASSERT(nodeIndexToUVChannelMap);

    switch (keyframeType) {
    case rpUVANIMLINEARKEYFRAMES:
        anim = RtAnimAnimationCreate(_rpUVAnimLinearInterpolatorInfo.typeID, numFrames, 0, duration);
        break;
    case rpUVANIMPARAMKEYFRAMES:
        anim = RtAnimAnimationCreate(_rpUVAnimParamInterpolatorInfo.typeID, numFrames, 0, duration);
        break;
    default:
        break;
    }

    if (anim != NULL) {
        return _rpUVAnimInit(anim, name, numNodes, nodeIndexToUVChannelMap);
    }

    return NULL;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== RpUVAnimDestroy ====================
 * Releases a reference to an animation; the last destroys it. Always succeeds.
 */
RwBool RpUVAnimDestroy(RpUVAnim* anim) {
    RWASSERT(anim);

    if (_rpUVAnimDestruct(anim)) {
        RtAnimAnimationDestroy(anim);
    }

    return TRUE;
} // 0x802CD534

/*
 * ==================== RpUVAnimAddRef ====================
 * Holds a reference to an animation.
 */
RpUVAnim* RpUVAnimAddRef(RpUVAnim* anim) {
    RWASSERT(anim);

    ((_rpUVAnimCustomData*)anim->customData)->refCount++;

    return anim;
} // 0x802CD5A8

/*
 * ==================== RpMaterialUVAnimApplyUpdate ====================
 * Rebuilds a material's UV matrices from its animations' interpolated keyframes: each matrix
 * starts as the identity and each node's keyframe, turned into a matrix by its scheme, is
 * concatenated onto the matrix of the channel the node maps to. The keyframes of the two
 * standard schemes are turned into matrices in place. The result goes to the MatFX plugin.
 * Original quirk kept: the keyframe pointer advances only past nodes that were applied.
 */
RpMaterial* RpMaterialUVAnimApplyUpdate(RpMaterial* material) {
    RpUVAnimMaterialExtension* matExt;
    RwUInt32 channel;
    RwInt32 iSlot;

    RWASSERT(material);

    matExt = RPUVANIMMATERIALGETDATA(material);

    for (channel = 0; channel < rpUVANIMMAXCHANNELS; channel++) {
        RwMatrix* matrix = matExt->uv[channel];

        if (matrix != NULL) {
            RwMatrixSetIdentity(matrix);
        }
    }

    for (iSlot = 0; iSlot < rpUVANIMMAXNODES; iSlot++) {
        RtAnimInterpolator* interp = matExt->interp[iSlot];

        if (interp != NULL) {
            RtAnimAnimation* anim = interp->pCurrentAnim;
            RwInt32 keyFrameSize = interp->currentInterpKeyFrameSize;
            RtAnimKeyFrameApplyCallBack keyFrameApplyCB = interp->keyFrameApplyCB;
            RwUInt32* nodeToUVChannelMap = ((_rpUVAnimCustomData*)anim->customData)->nodeToUVChannelMap;
            RwInt32 iNode;
            void* nodeInterpFrame = rtANIMGETINTERPFRAME(interp, 0);

            for (iNode = 0; iNode < interp->numNodes; iNode++) {
                if (nodeToUVChannelMap[iNode] < rpUVANIMMAXCHANNELS) {
                    RwMatrix* matrix = matExt->uv[nodeToUVChannelMap[iNode]];

                    if (matrix != NULL) {
                        RwMatrix interpMatrix;

                        if (RpUVAnimLinearKeyFrameApply == keyFrameApplyCB) {
                            RpUVAnimInterpFrame* iFrame = (RpUVAnimInterpFrame*)nodeInterpFrame;
                            RpUVAnimLinearKeyFrameData* lkf = &iFrame->data.linear;

                            interpMatrix.right.x = lkf->uv[0];
                            interpMatrix.right.y = lkf->uv[1];
                            interpMatrix.right.z = 0.0f;
                            interpMatrix.up.x = lkf->uv[2];
                            interpMatrix.up.y = lkf->uv[3];
                            interpMatrix.up.z = 0.0f;
                            interpMatrix.at.x = 0.0f;
                            interpMatrix.at.y = 0.0f;
                            interpMatrix.at.z = 0.0f;
                            interpMatrix.pos.x = lkf->uv[4];
                            interpMatrix.pos.y = lkf->uv[5];
                            interpMatrix.pos.z = 0.0f;
                            RwMatrixUpdate(&interpMatrix);
                        } else if (RpUVAnimParamKeyFrameApply == keyFrameApplyCB) {
                            RwV3d zAxis = {0.0f, 0.0f, 1.0f};
                            RpUVAnimParamKeyFrameData* param = &((RpUVAnimInterpFrame*)nodeInterpFrame)->data.param;

                            interpMatrix.right.x = param->s0;
                            interpMatrix.right.y = param->skew;
                            interpMatrix.right.z = 0.0f;
                            interpMatrix.up.x = 0.0f;
                            interpMatrix.up.y = param->s1;
                            interpMatrix.up.z = 0.0f;
                            interpMatrix.at.x = 0.0f;
                            interpMatrix.at.y = 0.0f;
                            interpMatrix.at.z = 0.0f;
                            interpMatrix.pos.x = param->x;
                            interpMatrix.pos.y = param->y;
                            interpMatrix.pos.z = 0.0f;
                            RwMatrixUpdate(&interpMatrix);
                            RwMatrixTranslate(&interpMatrix, &rpUVAnimPivot, rwCOMBINEPOSTCONCAT);
                            RwMatrixRotate(&interpMatrix, &zAxis, 180.0f * (param->theta / rwPI), rwCOMBINEPOSTCONCAT);
                            RwMatrixTranslate(&interpMatrix, &rpUVAnimInvPivot, rwCOMBINEPOSTCONCAT);
                        } else {
                            keyFrameApplyCB(&interpMatrix, nodeInterpFrame);
                        }

                        RwMatrixTransform(matrix, &interpMatrix, rwCOMBINEPRECONCAT);
                        nodeInterpFrame = (RwUInt8*)nodeInterpFrame + keyFrameSize;
                    }
                }
            }
        }
    }

    RpMatFXMaterialSetUVTransformMatrices(material, matExt->uv[0], matExt->uv[1]);

    return material;
} // 0x802CD5BC

/*
 * ==================== RpUVAnimGetName ====================
 * The name an animation is found by.
 */
RwChar* RpUVAnimGetName(RpUVAnim* anim) {
    RWASSERT(anim);

    return ((_rpUVAnimCustomData*)anim->customData)->name;
} // 0x802CD8AC

/*
 * ==================== RpMaterialUVAnimGetInterpolator ====================
 * The interpolator of a material's animation slot, or NULL.
 */
RtAnimInterpolator* RpMaterialUVAnimGetInterpolator(const RpMaterial* material, RwUInt32 uvIndex) {
    RpUVAnimMaterialExtension* materialExt;

    RWASSERT(material);

    materialExt = RPUVANIMMATERIALGETDATA(material);

    return materialExt->interp[uvIndex];
} // 0x802CD8B4

/*
 * ==================== RpMaterialUVAnimSetCurrentTime ====================
 * Sets the time of every animation of a material.
 */
RpMaterial* RpMaterialUVAnimSetCurrentTime(RpMaterial* material, RwReal time) {
    RpUVAnimMaterialExtension* materialExt;

    RWASSERT(rpUVAnimModule.numInstances);
    RWASSERT(material);

    materialExt = RPUVANIMMATERIALGETDATA(material);

    {
        RwUInt32 i;

        for (i = 0; i < rpUVANIMMAXNODES; i++) {
            RtAnimInterpolator* interp = materialExt->interp[i];

            if (interp != NULL) {
                RtAnimInterpolatorSetCurrentTime(interp, time);
            }
        }
    }

    return material;
} // no copy in retail (dead-stripped or inlined)

/*
 * ==================== RpMaterialUVAnimAddAnimTime ====================
 * Advances every animation of a material by a time. The name is the SDK's, taken from the
 * function's body (it adds the time to each interpolator).
 */
RpMaterial* RpMaterialUVAnimAddAnimTime(RpMaterial* material, RwReal deltaTime) {
    RpUVAnimMaterialExtension* materialExt;

    RWASSERT(rpUVAnimModule.numInstances);
    RWASSERT(material);

    materialExt = RPUVANIMMATERIALGETDATA(material);

    {
        RwUInt32 i;

        for (i = 0; i < rpUVANIMMAXNODES; i++) {
            RtAnimInterpolator* interp = materialExt->interp[i];

            if (interp != NULL) {
                RtAnimInterpolatorAddAnimTime(interp, deltaTime);
            }
        }
    }

    return material;
} // 0x802CD8CC

/*
 * ==================== RpMaterialUVAnimExists ====================
 * Whether a material has an animation in any slot.
 */
RwBool RpMaterialUVAnimExists(const RpMaterial* material) {
    RpUVAnimMaterialExtension* materialExt;
    RwUInt32 i;

    RWASSERT(material);

    materialExt = RPUVANIMMATERIALGETDATA(material);

    for (i = 0; i < rpUVANIMMAXNODES; i++) {
        if (materialExt->interp[i] != NULL) {
            return TRUE;
        }
    }

    return FALSE;
} // 0x802CD944
