// Names: Gormiti DWARF and link map (RpMatFX.a multiTex.obj, multiTexGcn.obj and the objects they attach)
// Layout of RwPlatformID: Gormiti DWARF
// Header name invented: no evidence names Gormiti's header for the multitexture plugin.
#ifndef RWSDK_PLUGIN_MATFX_MULTITEX_H
#define RWSDK_PLUGIN_MATFX_MULTITEX_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/batextur.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/world/bamateri.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The platforms a plugin can register its extension for. */
enum RwPlatformID {
    rwID_PCD3D7 = 1,
    rwID_PCOGL = 2,
    rwID_MAC = 3,
    rwID_PS2 = 4,
    rwID_XBOX = 5,
    rwID_GAMECUBE = 6,
    rwID_SOFTRAS = 7,
    rwID_PCD3D8 = 8,
    rwID_PCD3D9 = 9,
    rwID_PSP = 10,
    rwPLATFROMIDFORCEENUMSIZEINT = 2147483647
};
typedef enum RwPlatformID RwPlatformID;

/*
 * The multitexture plugin of a material, attached by the platform's matfx code when the engine
 * starts. Each attach function returns FALSE when the plugin could not be registered.
 */
extern RwBool _rpMultiTexturePluginAttach(void);
extern RwBool _rpMaterialRegisterMultiTexturePlugin(RwPlatformID platformID, RwUInt32 pluginID,
                                                    RwUInt32 extensionSize);

/* The ID of RpMatFX's plugin: the error ID of its sources. */
#define rwID_MATERIALEFFECTSPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x20)

/*
 * A multitexture effect: the platform's data follows this header in the same allocation
 * (RpGameCubeMTConfig on the GameCube).
 */
typedef struct RpMTEffect {
    RwPlatformID platformID;
    RwUInt32 refCount;
    RwChar name[32];
    RwLLLink dictLink;
} RpMTEffect;

/* The most textures a material's multitexture holds. */
#define rpMULTITEXTUREMAXTEXTURES 8

/*
 * What the multitexture plugin keeps of a platform: its ID, the ID of the plugin it registered for the
 * platform's materials, where that plugin's data lies in a material and how large the platform's extension
 * is (names and layout: Gormiti DWARF).
 */
typedef struct rpMultiTextureRegEntry {
    RwPlatformID platformID;    /* 0x0 */
    RwUInt32 pluginID;          /* 0x4 */
    RwUInt32 materialOffset;    /* 0x8 */
    RwUInt32 extensionSize;     /* 0xC */
} rpMultiTextureRegEntry;

/*
 * The multitexture of a material: up to eight textures with the texture coordinate set each one reads, and
 * an effect. The platform's extension follows this record in the same allocation.
 */
typedef struct RpMultiTexture {
    rpMultiTextureRegEntry* regEntry;                   /* 0x00 */
    RwUInt32 numTextures;                               /* 0x04 */
    RwTexture* textures[rpMULTITEXTUREMAXTEXTURES];     /* 0x08 */
    RwUInt8 coordMap[rpMULTITEXTUREMAXTEXTURES];        /* 0x28 */
    RpMTEffect* effect;                                 /* 0x30 */
    void* extension;                                    /* 0x34 */
} RpMultiTexture;

/*
 * What a platform registers for its effects (multiTexEffect.c). The writer and the size query take the effect
 * as const (Gormiti's DWARF records no const; the GameCube writer's schedule, which moves loads through the
 * effect above its stores, needs it).
 */
typedef RpMTEffect* (*RpMTEffectStreamReadFn)(RwStream* stream, RwPlatformID platformID, unsigned int size,
                                              unsigned int version);
typedef const RpMTEffect* (*RpMTEffectStreamWriteFn)(const RpMTEffect* effect, RwStream* stream);
typedef int (*RpMTEffectStreamGetSizeFn)(const RpMTEffect* effect);
typedef void (*RpMTEffectDestroyFn)(RpMTEffect* effect);

extern void _rpMTEffectInit(RpMTEffect* effect, RwPlatformID platformID);
extern RwBool _rpMTEffectRegisterPlatform(RwPlatformID platformID, RpMTEffectStreamReadFn streamRead,
                                          RpMTEffectStreamWriteFn streamWrite,
                                          RpMTEffectStreamGetSizeFn streamGetSize,
                                          RpMTEffectDestroyFn destroy);

/* The plugin's module: where its globals lie and how often it is open (multiTex.c). */
extern RwModuleInfo _rpMultiTextureModule;

/* The multitexture of a material (multiTex.c). */
extern RwUInt32 RpMultiTextureGetNumTextures(RpMultiTexture* multiTexture);
extern RpMultiTexture* RpMultiTextureSetEffect(RpMultiTexture* multiTexture, RpMTEffect* effect);
extern RpMTEffect* RpMultiTextureGetEffect(const RpMultiTexture* multiTexture);
extern RpMultiTexture* RpMultiTextureSetTexture(RpMultiTexture* multiTexture, RwUInt32 index,
                                                RwTexture* texture);
extern RwTexture* RpMultiTextureGetTexture(RpMultiTexture* multiTexture, RwUInt32 index);
extern RpMultiTexture* RpMultiTextureSetCoords(RpMultiTexture* multiTexture, RwUInt32 index, RwUInt8 coords);
extern RwUInt8 RpMultiTextureGetCoords(RpMultiTexture* multiTexture, RwUInt32 index);
extern RpMultiTexture* RpMaterialGetMultiTexture(RpMaterial* material, RwPlatformID platformID);

/* multiTexEffect.c: the effects the multitextures refer to. */
extern RwBool _rpMTEffectSystemInit(void);
extern RwBool _rpMTEffectOpen(void);
extern RwBool _rpMTEffectClose(void);
extern RpMTEffect* RpMTEffectCreateDummy(void);
extern void RpMTEffectDestroy(RpMTEffect* effect);
extern RpMTEffect* RpMTEffectFind(RwChar* name);
extern RpMTEffect* RpMTEffectSetName(RpMTEffect* effect, const RwChar* name);
extern RpMTEffect* RpMTEffectAddRef(RpMTEffect* effect);

/* RpMatFX's texture streaming (rpmatfx.c). */
extern RwUInt32 _rpMatFXStreamSizeTexture(RwTexture* texture);
extern RwStream* _rpMatFXStreamWriteTexture(RwStream* stream, RwTexture* texture);
extern RwStream* _rpMatFXStreamReadTexture(RwStream* stream, RwTexture** texture);

/* GameCube platform plugins (gcn/multiTexGcnData.c, gcn/multiTexGcnPipe.c). */
extern RwBool _rpGameCubeMTDataPluginAttach(void);
extern RwBool _rpGameCubeMTPipePluginAttach(void);

/* gcn/multiTexGcn.c: attaches the multitexture plugin and its GameCube data and pipeline plugins. */
extern RwBool _rpMultiTexturePlatformPluginsAttach(void);

#ifdef __cplusplus
}
#endif

#endif
