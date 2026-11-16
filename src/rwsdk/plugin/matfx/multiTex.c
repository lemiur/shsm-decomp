// Unit name and boundary: Gormiti link map (RpMatFx.a multiTex.obj)
// Names and layout: Gormiti link map and DWARF (RpMatFx.a multiTex.obj), Final and Debug builds.
/*
 * multiTex.c: the multitexture of a material in RpMatFX. A material that carries a multitexture
 * keeps a pointer to it in its plugin extension (one registration per platform); the record holds
 * up to eight textures with the texture coordinate set each reads, an effect, and the platform's
 * extension behind it in the same allocation. This file creates, copies, streams and destroys
 * multitextures and registers the plugin.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/rwstring.h>
#include <rwsdk/plugin/matfx/multitex.h>

/* The error ID of the plugin (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_MATERIALEFFECTSPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/* The IDs the plugin registers: the engine's module and the material's plugin. */
#define MULTITEXTURE_ENGINE_PLUGIN_ID 0x12C
#define MULTITEXTURE_MODULE_SIZE 0x18

/* The size of the chunk header in front of a stream's string. */
#define STREAM_CHUNKHEADER_SIZE 12

/* Bit 0 of the header's flags: an effect name follows the textures. */
#define STREAM_FLAG_EFFECT 0x01

/* The multitexture's pointer in the plugin extension of a material. */
typedef struct _MultiTextureExt {
    RpMultiTexture* multiTexture;
} _MultiTextureExt;

/* The stream header of a multitexture (one byte per field). */
typedef struct _MultiTextureStreamHdr {
    unsigned char platformID;
    unsigned char numTextures;
    unsigned char flags;
    unsigned char pad; // pad: Gormiti DWARF member name (_MultiTextureStreamHdr)
} _MultiTextureStreamHdr;

/* The extension of an object of the given plugin offset (RpMatFX's own accessor spelling, name invented). */
#define MULTITEXTUREEXT(_object, _offset) ((_MultiTextureExt*)(((RwUInt8*)(_object)) + (_offset)))

/* The plugin's registration of each platform, indexed by platform ID. */
static rpMultiTextureRegEntry RegEntries[10];

/* The plugin's module: where its globals lie and how often it is open. */
RwModuleInfo _rpMultiTextureModule;

/*
 * ==================== MultiTextureOpen ====================
 * Opens the multitexture plugin's module with the engine and starts the effects.
 */
static void* MultiTextureOpen(void* object, int offset, int size) {
    _rpMultiTextureModule.numInstances++;

    _rpMTEffectOpen();
    return object;
} // 0x802BAC90

/*
 * ==================== MultiTextureClose ====================
 * Closes the effects and the plugin's module.
 */
static void* MultiTextureClose(void* object, int offset, int size) {
    _rpMTEffectClose();

    _rpMultiTextureModule.numInstances--;
    return object;
} // 0x802BACD0

/*
 * ==================== MultiTextureCreate ====================
 * Allocates a multitexture of the given platform with room for its extension, empty. It returns
 * NULL when the allocation fails.
 */
static RpMultiTexture* MultiTextureCreate(rpMultiTextureRegEntry* regEntry, unsigned int numTextures) {
    unsigned int size;
    RpMultiTexture* multiTexture;

    size = sizeof(RpMultiTexture) + regEntry->extensionSize;

    multiTexture = (RpMultiTexture*)RwMalloc(size, rwMEMHINTFLAG_RESIZABLE | rwMEMHINTDUR_EVENT |
                                                       MULTITEXTURE_ENGINE_PLUGIN_ID);
    if (multiTexture == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    memset(multiTexture, 0, size);
    multiTexture->regEntry = regEntry;
    multiTexture->numTextures = numTextures;
    if (regEntry->extensionSize != 0) {
        multiTexture->extension = multiTexture + 1;
    }
    return multiTexture;
}

/*
 * ==================== MultiTextureDestroy ====================
 * Releases the textures and the effect of a multitexture and frees it.
 */
static void MultiTextureDestroy(RpMultiTexture* multiTexture) {
    unsigned int i;

    for (i = 0; i < multiTexture->numTextures; i++) {
        if (multiTexture->textures[i] != NULL) {
            RwTextureDestroy(multiTexture->textures[i]);
            multiTexture->textures[i] = NULL;
        }
    }

    if (multiTexture->effect != NULL) {
        RpMTEffectDestroy(multiTexture->effect);
        multiTexture->effect = NULL;
    }

    RwFree(multiTexture);
}

/*
 * ==================== MultiTextureConstructor ====================
 * A material starts without a multitexture.
 */
static void* MultiTextureConstructor(void* object, int offset, int size) {
    _MultiTextureExt* ext;

    ext = MULTITEXTUREEXT(object, offset);
    ext->multiTexture = NULL;
    return object;
} // 0x802BAD10

/*
 * ==================== MultiTextureDestructor ====================
 * Destroys the multitexture of a material that has one.
 */
static void* MultiTextureDestructor(void* object, int offset, int size) {
    _MultiTextureExt* ext;

    ext = MULTITEXTUREEXT(object, offset);
    if (ext->multiTexture != NULL) {
        MultiTextureDestroy(ext->multiTexture);
        ext->multiTexture = NULL;
    }
    return object;
} // 0x802BAD1C

/*
 * ==================== MultiTextureCopy ====================
 * Gives the destination material a multitexture of its own that holds the source's textures
 * (one more reference each), coordinate sets and effect.
 */
static void* MultiTextureCopy(void* dstObject, const void* srcObject, int offset, int size) {
    RpMultiTexture* srcMT;
    RpMultiTexture* dstMT;
    unsigned int i;

    srcMT = MULTITEXTUREEXT(srcObject, offset)->multiTexture;
    if (srcMT == NULL) {
        return dstObject;
    }

    dstMT = MultiTextureCreate(srcMT->regEntry, srcMT->numTextures);
    if (dstMT == NULL) {
        return NULL;
    }

    MULTITEXTUREEXT(dstObject, offset)->multiTexture = dstMT;

    for (i = 0; i < srcMT->numTextures; i++) {
        RpMultiTextureSetTexture(dstMT, i, RpMultiTextureGetTexture(srcMT, i));
        RpMultiTextureSetCoords(dstMT, i, RpMultiTextureGetCoords(srcMT, i));
    }

    RpMultiTextureSetEffect(dstMT, RpMultiTextureGetEffect(srcMT));
    return dstObject;
} // 0x802BADC4

/*
 * ==================== MultiTextureStreamGetSize ====================
 * The bytes a material's multitexture takes in a stream: the version, the header, the coordinate
 * sets, each texture and the effect's name in a chunk of its own.
 */
static RwInt32 MultiTextureStreamGetSize(const void* object, RwInt32 offset, RwInt32 size) {
    RpMultiTexture* multiTexture;
    unsigned int streamSize;
    unsigned int i;

    multiTexture = MULTITEXTUREEXT(object, offset)->multiTexture;

    streamSize = 0;
    if (multiTexture != NULL) {
        streamSize = sizeof(RwUInt32) + sizeof(_MultiTextureStreamHdr) + multiTexture->numTextures;

        for (i = 0; i < multiTexture->numTextures; i++) {
            streamSize += _rpMatFXStreamSizeTexture(multiTexture->textures[i]);
        }

        if (multiTexture->effect != NULL) {
            streamSize += _rwStringStreamGetSize(multiTexture->effect->name) + STREAM_CHUNKHEADER_SIZE;
        }
    }
    return streamSize;
} // 0x802BAF34

/*
 * ==================== MultiTextureStreamWrite ====================
 * Writes a material's multitexture: the engine's version, the header, the coordinate sets, the
 * textures and the effect's name. A material without one writes nothing.
 */
static RwStream* MultiTextureStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object,
                                         RwInt32 offset, RwInt32 size) {
    RpMultiTexture* multiTexture;

    multiTexture = MULTITEXTUREEXT(object, offset)->multiTexture;
    if (multiTexture != NULL) {
        unsigned int version;
        _MultiTextureStreamHdr header;
        unsigned int i;

        version = RwEngineGetVersion();
        if (!RwStreamWriteInt32(stream, (RwInt32*)&version, sizeof(version))) {
            return NULL;
        }

        header.platformID = multiTexture->regEntry->platformID;
        header.numTextures = multiTexture->numTextures;
        header.flags = multiTexture->effect != NULL;
        header.pad = 0;
        if (!RwStreamWrite(stream, &header, sizeof(header))) {
            return NULL;
        }

        if (multiTexture->numTextures != 0) {
            if (!RwStreamWrite(stream, multiTexture->coordMap, multiTexture->numTextures)) {
                return NULL;
            }

            for (i = 0; i < multiTexture->numTextures; i++) {
                _rpMatFXStreamWriteTexture(stream, multiTexture->textures[i]);
            }
        }

        if (multiTexture->effect != NULL) {
            if (!_rwStringStreamWrite(multiTexture->effect->name, stream)) {
                return NULL;
            }
        }
    }
    return stream;
} // 0x802BAFD4

/*
 * ==================== MultiTextureStreamRead ====================
 * Reads a material's multitexture back. Anything that fails after the multitexture is created
 * destroys it again, and the read fails. An effect that is not in the effect dictionary is
 * replaced by a dummy of that name.
 */
static RwStream* MultiTextureStreamRead(RwStream* stream, RwInt32 binaryLength, void* object,
                                        RwInt32 offset, RwInt32 size) {
    unsigned int version;
    _MultiTextureStreamHdr header;
    rpMultiTextureRegEntry* regEntry;
    RpMultiTexture* multiTexture;
    unsigned int i;
    RpMTEffect* effect;
    char name[32];

    if (!RwStreamReadInt32(stream, (RwInt32*)&version, sizeof(version))) {
        return NULL;
    }

    if (!RwStreamRead(stream, &header, sizeof(header))) {
        return NULL;
    }

    regEntry = &RegEntries[header.platformID];
    multiTexture = MultiTextureCreate(regEntry, header.numTextures);
    if (multiTexture == NULL) {
        return NULL;
    }

    if (header.numTextures != 0) {
        if (!RwStreamRead(stream, multiTexture->coordMap, multiTexture->numTextures)) {
            MultiTextureDestroy(multiTexture);
            return NULL;
        }

        for (i = 0; i < multiTexture->numTextures; i++) {
            if (!_rpMatFXStreamReadTexture(stream, &multiTexture->textures[i])) {
                MultiTextureDestroy(multiTexture);
                return NULL;
            }
        }
    }

    if (header.flags & STREAM_FLAG_EFFECT) {
        if (!_rwStringStreamFindAndRead(name, stream)) {
            MultiTextureDestroy(multiTexture);
            return NULL;
        }

        effect = RpMTEffectFind(name);
        if (effect == NULL) {
            effect = RpMTEffectCreateDummy();
            if (effect == NULL) {
                MultiTextureDestroy(multiTexture);
                return NULL;
            }
            RpMTEffectSetName(effect, name);
        }

        RpMultiTextureSetEffect(multiTexture, effect);
        RpMTEffectDestroy(effect);
    }

    MULTITEXTUREEXT(object, offset)->multiTexture = multiTexture;
    return stream;
} // 0x802BB118

/*
 * ==================== _rpMultiTexturePluginAttach ====================
 * Registers the multitexture plugin with the engine, after the effects' system and with every
 * platform unregistered. It returns FALSE when either fails.
 */
RwBool _rpMultiTexturePluginAttach(void) {
    if (!_rpMTEffectSystemInit()) {
        return FALSE;
    }

    memset(RegEntries, 0, sizeof(RegEntries));

    _rpMultiTextureModule.globalsOffset = RwEngineRegisterPlugin(
        MULTITEXTURE_MODULE_SIZE, MULTITEXTURE_ENGINE_PLUGIN_ID, MultiTextureOpen, MultiTextureClose);
    return _rpMultiTextureModule.globalsOffset >= 0;
} // 0x802BB4BC

/*
 * ==================== _rpMaterialRegisterMultiTexturePlugin ====================
 * Registers the multitexture as a plugin of materials for one platform and records where its
 * extension lies in a material. It returns FALSE when either registration fails.
 */
RwBool _rpMaterialRegisterMultiTexturePlugin(RwPlatformID platformID, RwUInt32 pluginID,
                                             RwUInt32 extensionSize) {
    int offset;
    rpMultiTextureRegEntry* regEntry;

    offset = RpMaterialRegisterPlugin(sizeof(_MultiTextureExt), pluginID, MultiTextureConstructor,
                                      MultiTextureDestructor, MultiTextureCopy);
    if (offset < 0) {
        return FALSE;
    }

    regEntry = &RegEntries[platformID];
    regEntry->materialOffset = offset;

    offset = RpMaterialRegisterPluginStream(pluginID, MultiTextureStreamRead, MultiTextureStreamWrite,
                                            MultiTextureStreamGetSize);
    if (offset < 0) {
        return FALSE;
    }

    regEntry->platformID = platformID;
    regEntry->pluginID = pluginID;
    regEntry->extensionSize = extensionSize;
    return TRUE;
} // 0x802BB528

/*
 * ==================== RpMultiTextureGetNumTextures ====================
 * The number of textures of a multitexture. (The link map names it; Gormiti's DWARF does not list it.)
 */
RwUInt32 RpMultiTextureGetNumTextures(RpMultiTexture* multiTexture) {
    return multiTexture->numTextures;
} // 0x802BB5F4

/*
 * ==================== RpMultiTextureSetEffect ====================
 * Replaces the effect of a multitexture, releasing the old one and taking a reference to the new one.
 */
// no copy in retail (inlined)
RpMultiTexture* RpMultiTextureSetEffect(RpMultiTexture* multiTexture, RpMTEffect* effect) {
    if (multiTexture->effect != NULL) {
        RpMTEffectDestroy(multiTexture->effect);
    }

    multiTexture->effect = effect;
    if (multiTexture->effect != NULL) {
        RpMTEffectAddRef(RpMultiTextureGetEffect(multiTexture));
    }
    return multiTexture;
}

/*
 * ==================== RpMultiTextureGetEffect ====================
 * The effect of a multitexture, or NULL. (Gormiti's DWARF records no const: SetEffect's reload of the effect it
 * has just stored, which retail repeats, needs the read through a pointer to const.)
 */
RpMTEffect* RpMultiTextureGetEffect(const RpMultiTexture* multiTexture) {
    return multiTexture->effect;
} // 0x802BB5FC

/*
 * ==================== RpMultiTextureSetTexture ====================
 * Replaces the texture at an index, releasing the old one and taking a reference to the new one.
 */
// no copy in retail (inlined)
RpMultiTexture* RpMultiTextureSetTexture(RpMultiTexture* multiTexture, RwUInt32 index, RwTexture* texture) {
    if (multiTexture->textures[index] != NULL) {
        RwTextureDestroy(multiTexture->textures[index]);
    }

    multiTexture->textures[index] = texture;
    if (texture != NULL) {
        RwTextureAddRef(texture);
    }
    return multiTexture;
}

/*
 * ==================== RpMultiTextureGetTexture ====================
 * The texture at an index of a multitexture.
 */
RwTexture* RpMultiTextureGetTexture(RpMultiTexture* multiTexture, RwUInt32 index) {
    return multiTexture->textures[index];
} // 0x802BB604

/*
 * ==================== RpMultiTextureSetCoords ====================
 * Sets the texture coordinate set that the texture at an index reads.
 */
// no copy in retail (inlined)
RpMultiTexture* RpMultiTextureSetCoords(RpMultiTexture* multiTexture, RwUInt32 index, RwUInt8 coords) {
    multiTexture->coordMap[index] = coords;
    return multiTexture;
}

/*
 * ==================== RpMultiTextureGetCoords ====================
 * The texture coordinate set that the texture at an index reads.
 */
// no copy in retail (inlined)
RwUInt8 RpMultiTextureGetCoords(RpMultiTexture* multiTexture, RwUInt32 index) {
    return multiTexture->coordMap[index];
}

/*
 * ==================== RpMaterialGetMultiTexture ====================
 * The multitexture of a material for a platform, or NULL when the platform is not registered
 * or the material has none.
 */
RpMultiTexture* RpMaterialGetMultiTexture(RpMaterial* material, RwPlatformID platformID) {
    rpMultiTextureRegEntry* regEntry;
    _MultiTextureExt* ext;

    regEntry = &RegEntries[platformID];
    if (regEntry->pluginID != 0) {
        ext = MULTITEXTUREEXT(material, regEntry->materialOffset);
        return ext->multiTexture;
    }
    return NULL;
} // 0x802BB614
