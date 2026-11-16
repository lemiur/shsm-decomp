// Reference: tgsm/pbg config/GPLE9G/symbols.txt (babintex.c: RwTextureStreamGetSize, RwTextureStreamWrite, destroyTexture, RwTexDictionaryStreamRead)
/*
 * babintex.c: textures and texture dictionaries in streams. A texture is streamed as a structure
 * chunk holding its filtering and addressing, its name and mask name, then its plugin data; the
 * texture itself is found or read by name (RwTextureRead). A dictionary holds its textures in
 * the driver's native format.
 * RwTextureSetStreamAlwaysCallBack, the texture dictionary writers and size functions,
 * RwTexDictionaryRegisterPluginStream, RwTexDictionarySetStreamAlwaysCallBack and
 * _rwTextureChunkInfoRead have no copy in retail (dead-stripped); the string stream functions
 * pbg keeps in this unit are in rwstring.c here.
 */
#include <rwsdk/babintex.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/rwstring.h>
#include <string.h>

/*
 * A texture's structure chunk: its filtering and addressing (as filterAddressing) in the low
 * half, its RwTextureStreamFlags in the third byte. Type and member names invented.
 */
typedef struct rwStreamTexture {
    RwUInt32 filterAndAddress;  /* 0x0 */
} rwStreamTexture;

/* The fields of filterAndAddress beyond filterAddressing. Names invented. */
#define rwSTREAMTEXTUREFILTERADDRESSMASK 0x0000FFFF
#define rwSTREAMTEXTUREFLAGSSHIFT 16
#define rwSTREAMTEXTUREFLAGSMASK 0xFF  /* after the shift */

/*
 * A texture dictionary's structure chunk: the device the native textures were written for and
 * their number, in big-endian order. Type and member names invented.
 */
typedef struct rwStreamTexDictionary {
    RwUInt16 deviceId;     /* 0x0 */
    RwUInt16 numTextures;  /* 0x2 */
} rwStreamTexDictionary;

/* Registers a plugin's stream callbacks with textures. */
RwInt32 RwTextureRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                      RwPluginDataChunkWriteCallBack writeCB,
                                      RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&textureTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x80291540

/* The size of a texture's chunk, without its header. */
RwUInt32 RwTextureStreamGetSize(const RwTexture* texture) {
    RwUInt32 size;

    size = sizeof(rwStreamTexture) + rwCHUNKHEADERSIZE;
    size += _rwStringStreamGetSize(texture->name) + rwCHUNKHEADERSIZE;
    size += _rwStringStreamGetSize(texture->mask) + rwCHUNKHEADERSIZE;
    size += _rwPluginRegistryGetSize(&textureTKList, texture) + rwCHUNKHEADERSIZE;

    return size;
} // 0x80291568

/* Writes a texture's chunk: its structure, name, mask name and plugin data. */
const RwTexture* RwTextureStreamWrite(const RwTexture* texture, RwStream* stream) {
    rwStreamTexture tex;
    RwUInt32 flags;

    if (!RwStreamWriteChunkHeader(stream, rwID_TEXTURE, RwTextureStreamGetSize(texture))) {
        return NULL;
    }

    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, sizeof(tex))) {
        return NULL;
    }

    /* A raster without automatic mipmaps carries the mipmaps the user gave it. */
    if (texture->raster && !(texture->raster->cFormat & (rwRASTERFORMATAUTOMIPMAP >> 8))) {
        flags = rwTEXTURESTREAMFLAGSUSERMIPMAPS;
    } else {
        flags = rwNATEXTURESTREAMFLAG;
    }

    tex.filterAndAddress = (texture->filterAddressing & rwSTREAMTEXTUREFILTERADDRESSMASK) |
                           ((flags & rwSTREAMTEXTUREFLAGSMASK) << rwSTREAMTEXTUREFLAGSSHIFT);
    RwMemLittleEndian32(&tex, sizeof(tex));
    if (!RwStreamWrite(stream, &tex, sizeof(tex))) {
        return NULL;
    }

    if (!_rwStringStreamWrite(texture->name, stream)) {
        return NULL;
    }

    if (!_rwStringStreamWrite(texture->mask, stream)) {
        return NULL;
    }

    if (!_rwPluginRegistryWriteDataChunks(&textureTKList, stream, texture)) {
        return NULL;
    }

    return texture;
} // 0x802915CC

/*
 * Reads a texture whose chunk header the caller has already found: its structure, name and
 * mask name, then finds or reads the texture by name with mipmapping set as the stream asks.
 * Only a newly read texture (one reference) takes the stream's filtering, addressing and plugin
 * data; a texture found already loaded skips them.
 */
RwTexture* RwTextureStreamRead(RwStream* stream) {
    RwUInt32 size;
    RwUInt32 version;
    rwStreamTexture tex;
    RwChar textureName[rwTEXTUREBASENAMELENGTH * 4];
    RwChar textureMask[rwTEXTUREBASENAMELENGTH * 4];
    RwTexture* texture;
    RwTextureFilterMode filtering;
    RwBool mipmapState;
    RwBool autoMipmapState;
    RwUInt32 flags;
    RwUInt32 addressingU;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        memset(&tex, 0, sizeof(tex));
        if (RwStreamRead(stream, &tex, size) != size) {
            return NULL;
        }
        RwMemNative32(&tex, sizeof(tex));

        filtering = (RwTextureFilterMode)(tex.filterAndAddress & rwTEXTUREFILTERMODEMASK);

        /* Older streams leave the V addressing unset: it follows the U addressing. */
        addressingU = (tex.filterAndAddress & rwTEXTUREADDRESSINGUMASK) >> 8;
        if (!((tex.filterAndAddress & rwTEXTUREADDRESSINGVMASK) >> 12)) {
            tex.filterAndAddress |= (addressingU << 12) & rwTEXTUREADDRESSINGVMASK;
        }

        flags = (tex.filterAndAddress >> rwSTREAMTEXTUREFLAGSSHIFT) & rwSTREAMTEXTUREFLAGSMASK;

        mipmapState = RwTextureGetMipmapping();
        autoMipmapState = RwTextureGetAutoMipmapping();

        /* Read the raster with mipmaps when the filtering uses them. */
        if (filtering >= rwFILTERMIPNEAREST && filtering <= rwFILTERLINEARMIPLINEAR) {
            RwTextureSetMipmapping(TRUE);
            if (flags & rwTEXTURESTREAMFLAGSUSERMIPMAPS) {
                RwTextureSetAutoMipmapping(FALSE);
            } else {
                RwTextureSetAutoMipmapping(TRUE);
            }
        } else {
            RwTextureSetMipmapping(FALSE);
            RwTextureSetAutoMipmapping(FALSE);
        }

        if (!_rwStringStreamFindAndRead(textureName, stream)) {
            RwTextureSetMipmapping(mipmapState);
            RwTextureSetAutoMipmapping(autoMipmapState);
            return NULL;
        }

        if (!_rwStringStreamFindAndRead(textureMask, stream)) {
            RwTextureSetMipmapping(mipmapState);
            RwTextureSetAutoMipmapping(autoMipmapState);
            return NULL;
        }

        texture = RwTextureRead(textureName, textureMask);
        if (!texture) {
            _rwPluginRegistrySkipDataChunks(&textureTKList, stream);
            RwTextureSetMipmapping(mipmapState);
            RwTextureSetAutoMipmapping(autoMipmapState);
            return NULL;
        }

        RwTextureSetMipmapping(mipmapState);
        RwTextureSetAutoMipmapping(autoMipmapState);

        if (texture->refCount == 1) {
            texture->filterAddressing = tex.filterAndAddress & rwSTREAMTEXTUREFILTERADDRESSMASK;

            if (!_rwPluginRegistryReadDataChunks(&textureTKList, stream, texture)) {
                return NULL;
            }
        } else {
            if (!_rwPluginRegistrySkipDataChunks(&textureTKList, stream)) {
                return NULL;
            }
        }

        return texture;
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
} // 0x8029174C

/* Destroys one texture of a dictionary that failed to read. */
static RwTexture* destroyTexture(RwTexture* texture, void* data) {
    RwTextureDestroy(texture);
    return texture;
} // 0x80291994

/*
 * Reads a texture dictionary whose chunk header the caller has already found: its structure,
 * each native texture with its plugin data, then the dictionary's plugin data. A dictionary
 * written for another device is refused.
 */
RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream) {
    RwUInt32 size;
    RwUInt32 version;
    rwStreamTexDictionary binTexDict;
    RwTexture* texture;
    RwUInt16 deviceId;
    RwTexDictionary* texDict;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        RwDevice* device = &RWSRCGLOBAL(dOpenDevice);

        memset(&binTexDict, 0, sizeof(binTexDict));
        if (RwStreamRead(stream, &binTexDict, size) != size) {
            return NULL;
        }
        RwMemNative32(&binTexDict, sizeof(binTexDict));

        /* The low byte of the device ID names the platform. */
        deviceId = 0;
        _rwDeviceSystemRequest(device, rwDEVICESYSTEMGETID, &deviceId, NULL, 0);
        deviceId &= 0xFF;
        if (deviceId && binTexDict.deviceId && deviceId != binTexDict.deviceId) {
            return NULL;
        }

        texDict = RwTexDictionaryCreate();
        if (!texDict) {
            return NULL;
        }

        while (binTexDict.numTextures--) {
            if (!RwStreamFindChunk(stream, rwID_TEXTURENATIVE, &size, &version)) {
                RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
                RwTexDictionaryDestroy(texDict);
                return NULL;
            }

            if (!(version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION)) {
                RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
                RwTexDictionaryDestroy(texDict);
                RWERROR((E_RW_BADVERSION));
                return NULL;
            }

            if (!RWSRCGLOBAL(stdFunc[rwSTANDARDNATIVETEXTUREREAD])(stream, &texture, size)) {
                RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
                RwTexDictionaryDestroy(texDict);
                return NULL;
            }

            if (!texture) {
                RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
                RwTexDictionaryDestroy(texDict);
                return NULL;
            }

            if (!_rwPluginRegistryReadDataChunks(&textureTKList, stream, texture)) {
                RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
                RwTexDictionaryDestroy(texDict);
                return NULL;
            }

            RwTexDictionaryAddTexture(texDict, texture);
        }

        if (!_rwPluginRegistryReadDataChunks(&texDictTKList, stream, texDict)) {
            RwTexDictionaryForAllTextures(texDict, destroyTexture, NULL);
            RwTexDictionaryDestroy(texDict);
            return NULL;
        }

        return texDict;
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
} // 0x802919C4
