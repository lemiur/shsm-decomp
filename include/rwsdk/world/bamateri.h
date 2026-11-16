// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: tgsm/pbg include/rwsdk/world/bamateri.h
#ifndef RWSDK_WORLD_BAMATERI_H
#define RWSDK_WORLD_BAMATERI_H

#include <rwsdk/batextur.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RwCriterionWorldID: IDs of the world plugin's own modules, the values retail registers them with
 * (RpWorldPluginAttach, _rpWorldObjRegisterExtensions). The SDK declares them with RpWorld
 * (baworld.h); here they sit with the first of those modules, which every world header includes.
 */
enum RwCriterionWorldID {
    rwID_NAWORLDID = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x00),
    rwID_MATERIALMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x01),
    rwID_MESHMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x02),
    rwID_GEOMETRYMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x03),
    rwID_CLUMPMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x04),
    rwID_LIGHTMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x05),
    rwID_COLLISIONMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x06),
    rwID_WORLDMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x07),
    rwID_RANDOMMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x08),
    rwID_WORLDOBJMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x09),
    rwID_SECTORMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x0A),
    rwID_BINWORLDMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x0B),
    rwID_WORLDPIPEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x0D),
    rwID_BINMESHPLUGIN = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x0E),    /* geometry and sector meshes in a stream */
    rwID_RXWORLDDEVICEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x0F),
    rwID_NATIVEDATAPLUGIN = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x10), /* instanced geometry and sector data in a stream */
    rwID_VERTEXFMTPLUGIN = MAKECHUNKID(rwVENDORID_CRITERIONWORLD, 0x11),
    rwCRITERIONWORLDIDFORCEENUMSIZEINT = 0x7FFFFFFF
};
typedef enum RwCriterionWorldID RwCriterionWorldID;

/*
 * RpMaterial: the surface of a geometry's or world sector's triangles: its texture, colour, the
 * pipeline that renders it, how it reflects light, and how many geometries share it.
 * Size 0x1C, from retail's material plugin registry.
 */
typedef struct RpMaterial {
    RwTexture* texture;                 /* 0x0 */
    RwRGBA color;                       /* 0x4 */
    RxPipeline* pipeline;               /* 0x8 */
    RwSurfaceProperties surfaceProps;   /* 0xC */
    RwInt16 refCount;                   /* 0x18 */
    RwInt16 pad;                        /* 0x1A */ // pad: bfbbdecomp/bfbb rpworld.h RpMaterial member name
} RpMaterial;

/*
 * RpMaterialChunkInfo: a material's structure chunk: its colour (bytes, not converted), whether
 * a texture chunk follows, and its surface properties.
 */
typedef struct RpMaterialChunkInfo {
    RwInt32 flags;                    /* 0x0: unused */
    RwRGBA color;                     /* 0x4 */
    RwInt32 unused;                   /* 0x8 */
    RwBool textured;                  /* 0xC */
    RwSurfaceProperties surfaceProps; /* 0x10 */
} RpMaterialChunkInfo;

/* rpMaterialGlobals: the material module's globals, the free list of materials. */
typedef struct rpMaterialGlobals {
    RwFreeList* matFreeList; /* 0x0 */
} rpMaterialGlobals;

/* Called for each material of an iteration; NULL stops it. */
typedef RpMaterial* (*RpMaterialCallBack)(RpMaterial* material, void* data);

/* Material accessors (bfbbdecomp/bfbb rpworld.h, the SDK's release macros). */
#define RpMaterialAddRef(_material) (((_material)->refCount++), (_material))
#define RpMaterialSetColor(_material, _color) (((_material)->color = *(_color)), (_material))
#define RpMaterialGetColor(_material) (&((_material)->color))
#define RpMaterialSetSurfaceProperties(_material, _surfProps) \
    (((_material)->surfaceProps = *(_surfProps)), (_material))
#define RpMaterialGetSurfaceProperties(_material) (&((_material)->surfaceProps))
#define RpMaterialGetTexture(_material) ((_material)->texture)

void* _rpMaterialOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rpMaterialClose(void* instance, RwInt32 offset, RwInt32 size);

/* The material rights stream callbacks (rwID_RIGHTTORENDER), registered by _rpWorldObjRegisterExtensions. */
RwStream* _rpReadMaterialRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                                RwInt32 sizeInObject);
RwStream* _rpWriteMaterialRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                                 RwInt32 offsetInObject, RwInt32 sizeInObject);
RwInt32 _rpSizeMaterialRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

void RpMaterialSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);
RpMaterial* RpMaterialCreate(void);
RwBool RpMaterialDestroy(RpMaterial* material);
RpMaterial* RpMaterialClone(RpMaterial* material);
RpMaterial* RpMaterialSetTexture(RpMaterial* material, RwTexture* texture);
RwInt32 RpMaterialRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                 RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpMaterialRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                       RwPluginDataChunkWriteCallBack writeCB,
                                       RwPluginDataChunkGetSizeCallBack getSizeCB);
RwInt32 RpMaterialGetPluginOffset(RwUInt32 pluginID);
RpMaterial* RpMaterialStreamRead(RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
