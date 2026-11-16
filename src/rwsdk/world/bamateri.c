// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function names, unit layout)
// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h (material macros)
// Reference: aap/librw src/geometry.cpp (the material chunk's fields)
/*
 * bamateri.c: RenderWare materials. A material is shared by the geometries that use it (counted
 * in refCount) and holds a texture reference; the material module's free list hands them out.
 * RpMaterialCreate has no copy in retail: it is inlined into RpMaterialClone and
 * RpMaterialStreamRead, and its own copy is dead-stripped.
 */
#include <rwsdk/world/rpplugin.h>
#include <rwsdk/babintex.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/bamateri.h>
#include <string.h>

/* The free list of materials; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rpMaterialFreeList;

/* The plugins registered with materials: no plugin data yet, so a material is just an RpMaterial. */
RwPluginRegistry materialTKList = {
    sizeof(RpMaterial),
    sizeof(RpMaterial),
    0,
    0,
    NULL,
    NULL,
};

/* How a new material reflects light: fully, in every term. */
static RwSurfaceProperties defaultSurfaceProperties = {
    1.0f,
    1.0f,
    1.0f,
};

/* Materials per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rpMaterialFreeListBlockSize = 256;
static RwInt32 _rpMaterialFreeListPreallocBlocks = 1;

/* The material module's registration (offset of its globals in the engine instance). */
static RwModuleInfo materialModule;

/*
 * The rights-to-render plugin ID and its extra data last read from a material's extension chunk,
 * applied once the material's plugin data has been read.
 */
static RwInt32 lastSeenRightsPluginId;
static RwInt32 lastSeenExtraData;

/* The material module's globals (rpMaterialGlobals, bamateri.h). Macro name invented, after bacamera's. */
#define RWMATERIALGLOBAL(var) \
    (RWPLUGINOFFSET(rpMaterialGlobals, RwEngineInstance, materialModule.globalsOffset)->var)

/* The size of a rights chunk holding the extra data after the plugin ID. Name invented. */
#define rpMATERIALRIGHTSEXTRASIZE 8

/*
 * The material's pipeline, read into *_pipeline and set: the macro forms of RpMaterialGetPipeline
 * and RpMaterialSetPipeline.
 */
#define RpMaterialGetPipelineMacro(_material, _pipeline) \
    ((*(_pipeline) = (_material)->pipeline), (_material))
#define RpMaterialSetPipelineMacro(_material, _pipeline) \
    (((_material)->pipeline = (_pipeline)), (_material))

/* Reads the rights-to-render plugin ID (and its extra data, if any) of a material. */
RwStream* _rpReadMaterialRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                RwInt32 sizeInObject) {
    if (!RwStreamReadInt32(stream, &lastSeenRightsPluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (binaryLength == rpMATERIALRIGHTSEXTRASIZE) {
        if (!RwStreamReadInt32(stream, &lastSeenExtraData, sizeof(RwInt32))) {
            return NULL;
        }
    }

    return stream;
} // 0x802D3D48

/* Writes the plugin ID and extra data of the material's pipeline, the rights it needs to render. */
RwStream* _rpWriteMaterialRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                                 RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpMaterial* material = (const RpMaterial*)object;

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&material->pipeline->pluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&material->pipeline->pluginData, sizeof(RwInt32))) {
        return NULL;
    }

    return stream;
} // 0x802D3DC4

/* The size of a material's rights chunk: none unless its pipeline belongs to a plugin. */
RwInt32 _rpSizeMaterialRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpMaterial* material = (const RpMaterial*)object;

    if (material->pipeline != NULL && material->pipeline->pluginId != 0) {
        return rpMATERIALRIGHTSEXTRASIZE;
    }

    return 0;
} // 0x802D3E40

/* Sets the block size and the preallocated blocks of the material free list, for the next open. */
void RpMaterialSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    _rpMaterialFreeListBlockSize = blockSize;
    _rpMaterialFreeListPreallocBlocks = numBlocksToPrealloc;
} // 0x802D3E68

/* Opens the material module: creates the material free list. */
void* _rpMaterialOpen(void* instance, RwInt32 offset, RwInt32 size) {
    materialModule.globalsOffset = offset;

    RWMATERIALGLOBAL(matFreeList) = RwFreeListCreateAndPreallocateSpace(
        materialTKList.sizeOfStruct, _rpMaterialFreeListBlockSize, sizeof(RwUInt32),
        _rpMaterialFreeListPreallocBlocks, &_rpMaterialFreeList, rwMEMHINTDUR_GLOBAL | rwID_MATERIAL);
    if (RWMATERIALGLOBAL(matFreeList) == NULL) {
        return NULL;
    }

    materialModule.numInstances++;

    return instance;
} // 0x802D3E74

/* Closes the material module: destroys the material free list. */
void* _rpMaterialClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWMATERIALGLOBAL(matFreeList) != NULL) {
        RwFreeListDestroy(RWMATERIALGLOBAL(matFreeList));
        RWMATERIALGLOBAL(matFreeList) = NULL;
    }

    materialModule.numInstances--;

    return instance;
} // 0x802D3F04

/* Creates a material: white, untextured, fully reflective, with one reference. */
RpMaterial* RpMaterialCreate(void) {
    RpMaterial* material;
    RwRGBA white;

    material = (RpMaterial*)RwFreeListAlloc(RWMATERIALGLOBAL(matFreeList), rwMEMHINTDUR_EVENT | rwID_MATERIAL);
    if (material == NULL) {
        return NULL;
    }

    white.red = 255;
    white.green = 255;
    white.blue = 255;
    white.alpha = 255;

    material->refCount = 1;
    RpMaterialSetColor(material, &white);
    material->texture = NULL;
    material->pipeline = NULL;
    RpMaterialSetSurfaceProperties(material, &defaultSurfaceProperties);

    _rwPluginRegistryInitObject(&materialTKList, material);

    /* Retail sets the pad to one once the plugins are constructed (its reader is not identified). */
    material->pad = 1;

    return material;
} // no copy in retail (dead-stripped or inlined)

/* Drops a reference to the material, destroying it (and its texture reference) with the last. */
RwBool RpMaterialDestroy(RpMaterial* material) {
    if (material->refCount == 1) {
        _rwPluginRegistryDeInitObject(&materialTKList, material);
        RpMaterialSetTexture(material, NULL);
        RwFreeListFree(RWMATERIALGLOBAL(matFreeList), material);
    } else {
        material->refCount--;
    }

    return TRUE;
} // 0x802D3F68

/* Creates a copy of the material: its texture, colour, pipeline, surface and plugin data. */
RpMaterial* RpMaterialClone(RpMaterial* material) {
    RpMaterial* newMaterial;
    RxPipeline* pipeline;
    const RwSurfaceProperties* surfaceProps;

    newMaterial = RpMaterialCreate();
    if (newMaterial == NULL) {
        return NULL;
    }

    if (RpMaterialGetTexture(material) != NULL) {
        if (!RpMaterialSetTexture(newMaterial, RpMaterialGetTexture(material))) {
            RpMaterialDestroy(newMaterial);
            return NULL;
        }
    }

    RpMaterialSetColor(newMaterial, RpMaterialGetColor(material));

    if (!RpMaterialGetPipelineMacro(material, &pipeline) || !RpMaterialSetPipelineMacro(newMaterial, pipeline)) {
        RpMaterialDestroy(newMaterial);
        return NULL;
    }

    surfaceProps = RpMaterialGetSurfaceProperties(material);
    if (surfaceProps == NULL || !RpMaterialSetSurfaceProperties(newMaterial, surfaceProps)) {
        RpMaterialDestroy(newMaterial);
        return NULL;
    }

    _rwPluginRegistryCopyObject(&materialTKList, newMaterial, material);

    return newMaterial;
} // 0x802D3FF0

/* Sets the material's texture, taking a reference to it and dropping the old one. */
RpMaterial* RpMaterialSetTexture(RpMaterial* material, RwTexture* texture) {
    if (texture != NULL) {
        RwTextureAddRef(texture);
    }

    if (material->texture != NULL) {
        RwTextureDestroy(material->texture);
    }

    material->texture = texture;

    return material;
} // 0x802D4290

/* Registers a plugin's data with every material. */
RwInt32 RpMaterialRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                 RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&materialTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D42F0

/* Registers the stream callbacks of a plugin's material data. */
RwInt32 RpMaterialRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                       RwPluginDataChunkWriteCallBack writeCB,
                                       RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&materialTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D4320

/* The offset of a plugin's data in a material (-1 if it is not registered). */
RwInt32 RpMaterialGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&materialTKList, pluginID);
} // 0x802D4348

/* Reads a material (its structure chunk, its texture, then its plugin data) from the stream. */
RpMaterial* RpMaterialStreamRead(RwStream* stream) {
    RwUInt32 length;
    RwUInt32 version;
    RpMaterialChunkInfo materialInfo;
    RwRGBA color;
    RpMaterial* material;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        memset(&materialInfo, 0, sizeof(materialInfo));
        if (RwStreamRead(stream, &materialInfo, length) != length) {
            return NULL;
        }

        /* The colour is four bytes: keep it out of the 32-bit conversion. */
        color = materialInfo.color;
        RwMemNative32(&materialInfo, sizeof(materialInfo));
        materialInfo.color = color;

        material = RpMaterialCreate();
        if (material == NULL) {
            return NULL;
        }

        RpMaterialSetColor(material, &materialInfo.color);
        RpMaterialSetSurfaceProperties(material, &materialInfo.surfaceProps);
        material->texture = NULL;

        if (materialInfo.textured) {
            if (!RwStreamFindChunk(stream, rwID_TEXTURE, NULL, &version)) {
                RpMaterialDestroy(material);
                return NULL;
            }

            if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
                material->texture = RwTextureStreamRead(stream);
            } else {
                RpMaterialDestroy(material);
                RWERROR((E_RW_BADVERSION));
                return NULL;
            }
        }

        lastSeenRightsPluginId = 0;
        lastSeenExtraData = 0;

        if (!_rwPluginRegistryReadDataChunks(&materialTKList, stream, material)) {
            RpMaterialDestroy(material);
            return NULL;
        }

        if (lastSeenRightsPluginId != 0) {
            _rwPluginRegistryInvokeRights(&materialTKList, lastSeenRightsPluginId, material, lastSeenExtraData);
        }

        return material;
    }

    RWERROR((E_RW_BADVERSION));
    return NULL;
} // 0x802D4358
