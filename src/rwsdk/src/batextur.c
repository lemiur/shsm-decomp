// Reference: tgsm/pbg src/rwsdk/src/batextur.c
/*
 * batextur.c: textures and texture dictionaries. A texture names a raster (with a mask name,
 * filtering and addressing) and counts its references; a dictionary holds textures by name, and
 * RwTextureRead looks a texture up in the current dictionary before reading its image from a file
 * and building the raster (and its mipmap levels) from it.
 */
#include <string.h>

#include <rwsdk/badevice.h>
#include <rwsdk/baimras.h>
#include <rwsdk/baresamp.h>
#include <rwsdk/batextur.h>
#include <rwsdk/driver/common/palquant.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/rwstring.h>

/*
 * The texture module's globals.
 * RWTEXTUREGLOBAL is invented.
 */
typedef struct rwTextureGlobals {
    RwLinkList texDictList;                                 /* 0x0: every dictionary (lInInstance) */
    RwFreeList* textureFreeList;                            /* 0x8 */
    RwFreeList* texDictFreeList;                            /* 0xC */
    RwTexDictionary* currentTexDict;                        /* 0x10 */
    RwTextureCallBackRead textureReadFunc;                    /* 0x14 */
    RwTextureCallBackFind textureFindFunc;                    /* 0x18 */
    RwBool haveTexMipmaps;                                      /* 0x1C */
    RwBool generateTexMipmaps;                                  /* 0x20 */
    RwUInt8* scratchMem;                                       /* 0x24: a heap block, freed at close */
    RwInt16 scratchMemSize;                                    /* 0x28: cleared with scratchMem */
    RwInt16 pad;                                            /* 0x2A */ // pad: name as in the RenderWare SDK
    RwTextureCallBackMipmapGeneration mipmapCallback; /* 0x2C */
    RwTextureCallBackMipmapName mipmapNameCallback;               /* 0x30 */
} rwTextureGlobals;

#define RWTEXTUREGLOBAL(var) \
    (RWPLUGINOFFSET(rwTextureGlobals, RwEngineInstance, textureModule.globalsOffset)->var)

/* The space of the two free lists. */
static RwFreeList _rwTexDictionaryFreeList;
static RwFreeList _rwTextureFreeList;

RwPluginRegistry textureTKList = {
    sizeof(RwTexture),
    sizeof(RwTexture),
    0,
    0,
    NULL,
    NULL,
};

RwPluginRegistry texDictTKList = {
    sizeof(RwTexDictionary),
    sizeof(RwTexDictionary),
    0,
    0,
    NULL,
    NULL,
};

static RwModuleInfo textureModule;

/* The dictionary the module opens with, the current one until the application sets another. */
static RwTexDictionary* dummyTexDict;

/* The length of the image and mask file names the readers build. Name invented. */
#define rwTEXTUREIMAGENAMELENGTH 256

/* The most colours in a palette (8 bits). Name invented. */
#define rwTEXTUREPALETTESIZE 256

/* The most mipmap levels a texture read handles. Name invented. */
#define rwTEXTUREMAXMIPLEVELS 16

static RwTexture* TextureDefaultRead(const RwChar* name, const RwChar* maskName);
static RwRaster* TextureRasterDefaultBuildMipmaps(RwRaster* raster, RwImage* image);

/* The default mipmap names: the level's hex digit after an 'm' ("brickm1" for level 1). */
static RwBool TextureDefaultMipmapName(RwChar* name, RwChar* maskName, RwUInt8 mipLevel,
                                       RwInt32 format) {
    static const RwChar character[] = "0123456789abcdef";
    RwChar suffix[3];

    suffix[0] = 'm';
    suffix[1] = (mipLevel != 0 && mipLevel < sizeof(character) - 1) ? character[mipLevel] : '\0';
    suffix[2] = '\0';
    if (suffix[1] != '\0') {
        rwstrcat(name, suffix);
        if (maskName != NULL && *maskName != '\0') {
            rwstrcat(maskName, suffix);
        }
    }

    return TRUE;
} // 0x80298318

/* Requantises *image to a palette of 2^depth colours, replacing (and destroying) it; FALSE on failure. */
static RwBool PalettizeImage(RwImage** image, RwInt32 depth) {
    RwRGBA palette[rwTEXTUREPALETTESIZE];
    RwPalQuant palQuant;
    RwInt32 numColours;
    RwImage* palImage;

    if (!_rwPalQuantInit(&palQuant)) {
        return FALSE;
    }

    _rwPalQuantAddImage(&palQuant, *image, 1.0f);
    numColours = 1 << depth;
    _rwPalQuantResolvePalette(palette, numColours, &palQuant);

    palImage = RwImageCreate((*image)->width, (*image)->height, depth);
    if (palImage != NULL) {
        RwImageAllocatePixels(palImage);
        _rwPalQuantMatchImage(palImage->cpPixels, palImage->stride, palImage->depth, FALSE,
                              &palQuant, *image);
        memcpy(palImage->palette, palette, numColours * sizeof(RwRGBA));
        RwImageDestroy(*image);
        *image = palImage;
    } else {
        return FALSE;
    }

    _rwPalQuantTerm(&palQuant);

    return TRUE;
} // 0x802983DC

/*
 * Gives the mipmap levels images[0 .. numImages - 1] one palette of 2^depth colours, written to
 * palette: the levels' own palette when they all share one, else a quantised one, each level
 * replaced by its requantised copy (the replaced level destroyed unless it is keepImage).
 */
static RwBool PalettizeMipmaps(RwRGBA* palette, RwImage* keepImage, RwImage** images,
                               RwInt32 numImages, RwInt32 depth) {
    RwPalQuant palQuant;
    RwUInt32* firstPalette;
    RwInt32 i;

    firstPalette = (RwUInt32*)images[0]->palette;
    if (firstPalette != NULL) {
        for (i = 1; i < numImages; i++) {
            RwUInt32* levelPalette = (RwUInt32*)images[i]->palette;
            RwInt32 j;

            /* A mismatch sets i past any level count (64): the levels do not share one palette. */
            if (firstPalette == NULL || levelPalette == NULL) {
                i = 64;
                break;
            }

            for (j = 0; j < (1 << depth); j++) {
                if (firstPalette[j] != levelPalette[j]) {
                    i = 64;
                    break;
                }
            }
        }

        if (i == numImages) {
            memcpy(palette, images[0]->palette, (1 << images[0]->depth) * sizeof(RwRGBA));
            return TRUE;
        }
    }

    if (!_rwPalQuantInit(&palQuant)) {
        return FALSE;
    }

    for (i = 0; i < numImages; i++) {
        _rwPalQuantAddImage(&palQuant, images[i], 1.0f);
    }
    _rwPalQuantResolvePalette(palette, 1 << depth, &palQuant);

    for (i = 0; i < numImages; i++) {
        RwImage* image = images[i];
        RwImage* palImage = RwImageCreate(image->width, image->height, depth);

        if (palImage != NULL) {
            RwImageAllocatePixels(palImage);
            _rwPalQuantMatchImage(palImage->cpPixels, palImage->stride, palImage->depth, FALSE,
                                  &palQuant, image);
            palImage->palette = palette;
            images[i] = palImage;
            if (image != keepImage) {
                RwImageDestroy(image);
            }
        } else {
            return FALSE;
        }
    }

    _rwPalQuantTerm(&palQuant);

    return TRUE;
} // 0x802984CC

/*
 * Reads the image name (with the extension of its file type) and its mask; when *width or
 * *height is 0, the raster format to hold it is found first. An image of another size is
 * resampled to *width x *height (at 32 bits, requantised to its old 4 or 8 bits).
 */
static RwImage* TextureImageReadAndSize(const RwChar* name, const RwChar* maskName,
                                        RwInt32 rasterType, RwInt32* width, RwInt32* height,
                                        RwInt32* depth, RwInt32* format) {
    RwChar imageName[rwTEXTUREIMAGENAMELENGTH];
    RwChar maskImageName[rwTEXTUREIMAGENAMELENGTH];
    const RwChar* extension;
    RwImage* image;

    rwstrncpy(imageName, name, rwTEXTUREIMAGENAMELENGTH);
    if (rwstrlen(name) >= rwTEXTUREIMAGENAMELENGTH) {
        RWERROR((E_RW_STRING_TRUNCATION, name, rwTEXTUREIMAGENAMELENGTH,
                 rwTEXTUREIMAGENAMELENGTH - 1, name[rwTEXTUREIMAGENAMELENGTH - 1]));
        imageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
    }
    extension = RwImageFindFileType(name);
    if (extension != NULL) {
        rwstrcat(imageName, extension);
    }

    maskImageName[0] = '\0';
    if (maskName != NULL && *maskName != '\0') {
        rwstrncpy(maskImageName, maskName, rwTEXTUREIMAGENAMELENGTH);
        if (rwstrlen(maskName) >= rwTEXTUREIMAGENAMELENGTH) {
            RWERROR((E_RW_STRING_TRUNCATION, maskName, rwTEXTUREIMAGENAMELENGTH,
                     rwTEXTUREIMAGENAMELENGTH - 1, maskName[rwTEXTUREIMAGENAMELENGTH - 1]));
            maskImageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
        }
        extension = RwImageFindFileType(maskName);
        if (extension != NULL) {
            rwstrcat(maskImageName, extension);
        }
    }

    image = RwImageReadMaskedImage(imageName, maskImageName);
    if (image == NULL) {
        return NULL;
    }

    if ((*width == 0 || *height == 0) &&
        !RwImageFindRasterFormat(image, rasterType, width, height, depth, format)) {
        RwImageDestroy(image);
        /* Error 0x80000009: no raster format holds the image (baerr.h has no name for it yet). */
        RWERROR((0x80000009));
        return NULL;
    }

    if (image->width != *width || image->height != *height) {
        RwInt32 imageDepth = image->depth;
        RwImage* resampled;

        if (imageDepth != 32) {
            RwImage* original = image;

            image = RwImageCreate(original->width, original->height, 32);
            if (image == NULL) {
                RwImageDestroy(original);
                return NULL;
            }
            if (!RwImageAllocatePixels(image)) {
                RwImageDestroy(image);
                RwImageDestroy(original);
                return NULL;
            }
            RwImageCopy(image, original);
            RwImageDestroy(original);
        }

        resampled = RwImageCreate(*width, *height, 32);
        if (resampled == NULL) {
            RwImageDestroy(image);
            return NULL;
        }
        if (!RwImageAllocatePixels(resampled)) {
            RwImageDestroy(resampled);
            RwImageDestroy(image);
            return NULL;
        }
        RwImageResample(resampled, image);
        RwImageDestroy(image);
        image = resampled;

        if (imageDepth == 4) {
            PalettizeImage(&image, imageDepth);
        } else if (imageDepth == 8) {
            PalettizeImage(&image, imageDepth);
        }
    }

    return image;
} // 0x80298698

/*
 * Reads a texture without mipmap levels: the image, a raster for it (palettised as the raster is),
 * the texture.
 */
static RwTexture* TextureDefaultNormalRead(const RwChar* name, const RwChar* maskName) {
    RwChar imageName[rwTEXTUREIMAGENAMELENGTH];
    RwChar maskImageName[rwTEXTUREIMAGENAMELENGTH];
    RwRGBA palette[rwTEXTUREPALETTESIZE];
    RwImage* image;
    RwInt32 width;
    RwInt32 height;
    RwInt32 depth;
    RwInt32 format;
    RwRaster* raster;
    RwTexture* texture;

    rwstrncpy(imageName, name, rwTEXTUREIMAGENAMELENGTH);
    if (rwstrlen(name) >= rwTEXTUREIMAGENAMELENGTH) {
        RWERROR((E_RW_STRING_TRUNCATION, name, rwTEXTUREIMAGENAMELENGTH,
                 rwTEXTUREIMAGENAMELENGTH - 1, name[rwTEXTUREIMAGENAMELENGTH - 1]));
        imageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
    }
    maskImageName[0] = '\0';
    if (maskName != NULL && *maskName != '\0') {
        rwstrncpy(maskImageName, maskName, rwTEXTUREIMAGENAMELENGTH);
        if (rwstrlen(maskName) >= rwTEXTUREIMAGENAMELENGTH) {
            RWERROR((E_RW_STRING_TRUNCATION, maskName, rwTEXTUREIMAGENAMELENGTH,
                     rwTEXTUREIMAGENAMELENGTH - 1, maskName[rwTEXTUREIMAGENAMELENGTH - 1]));
            maskImageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
        }
    }

    if (RWTEXTUREGLOBAL(mipmapNameCallback) != NULL) {
        RWTEXTUREGLOBAL(mipmapNameCallback)(imageName, maskImageName, 0, rwRASTERTYPETEXTURE);
    }

    width = 0;
    height = 0;
    image = TextureImageReadAndSize(imageName, maskImageName, rwRASTERTYPETEXTURE, &width, &height,
                                    &depth, &format);
    if (image == NULL) {
        return NULL;
    }

    raster = RwRasterCreate(width, height, depth, format);
    if (raster == NULL) {
        RwImageDestroy(image);
        return NULL;
    }

    if (RwRasterGetFormat(raster) & (rwRASTERFORMATPAL4 | rwRASTERFORMATPAL8)) {
        if (RwRasterGetFormat(raster) & rwRASTERFORMATPAL4) {
            PalettizeMipmaps(palette, NULL, &image, 1, 4);
        } else {
            PalettizeMipmaps(palette, NULL, &image, 1, 8);
        }
        image->palette = palette;
    }

    RwImageGammaCorrect(image);
    if (!RwRasterSetFromImage(raster, image)) {
        RwRasterDestroy(raster);
        RwImageDestroy(image);
        return NULL;
    }
    RwImageDestroy(image);

    texture = RwTextureCreate(raster);
    if (texture == NULL) {
        RwRasterDestroy(raster);
        return NULL;
    }

    RwTextureSetName(texture, name);
    if (maskName != NULL) {
        RwTextureSetMaskName(texture, maskName);
    } else {
        RwTextureSetMaskName(texture, "");
    }

    return texture;
} // 0x802989E8

/*
 * Reads a texture with mipmap levels: each level's image is read under the mipmap name callback's
 * name for it (unless the raster generates its levels), all levels share one palette.
 */
static RwTexture* TextureDefaultMipmapRead(const RwChar* name, const RwChar* maskName) {
    RwChar imageName[rwTEXTUREIMAGENAMELENGTH];
    RwChar maskImageName[rwTEXTUREIMAGENAMELENGTH];
    RwRGBA palette[rwTEXTUREPALETTESIZE];
    RwImage* images[rwTEXTUREMAXMIPLEVELS];
    RwRaster* raster;
    RwInt32 rasterType;
    RwInt32 width;
    RwInt32 height;
    RwInt32 depth;
    RwInt32 format;
    RwInt32 i;
    RwInt32 numLevels;
    RwTexture* texture;

    rwstrncpy(imageName, name, rwTEXTUREIMAGENAMELENGTH);
    if (rwstrlen(name) >= rwTEXTUREIMAGENAMELENGTH) {
        RWERROR((E_RW_STRING_TRUNCATION, name, rwTEXTUREIMAGENAMELENGTH,
                 rwTEXTUREIMAGENAMELENGTH - 1, name[rwTEXTUREIMAGENAMELENGTH - 1]));
        imageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
    }
    maskImageName[0] = '\0';
    if (maskName != NULL && *maskName != '\0') {
        rwstrncpy(maskImageName, maskName, rwTEXTUREIMAGENAMELENGTH);
        if (rwstrlen(maskName) >= rwTEXTUREIMAGENAMELENGTH) {
            RWERROR((E_RW_STRING_TRUNCATION, maskName, rwTEXTUREIMAGENAMELENGTH,
                     rwTEXTUREIMAGENAMELENGTH - 1, maskName[rwTEXTUREIMAGENAMELENGTH - 1]));
            maskImageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
        }
    }

    rasterType = rwRASTERTYPETEXTURE;
    if (RWTEXTUREGLOBAL(haveTexMipmaps)) {
        rasterType |= rwRASTERFORMATMIPMAP;
        if (RWTEXTUREGLOBAL(generateTexMipmaps)) {
            rasterType |= rwRASTERFORMATAUTOMIPMAP;
        }
    }

    if (RWTEXTUREGLOBAL(mipmapNameCallback) != NULL) {
        RWTEXTUREGLOBAL(mipmapNameCallback)(imageName, maskImageName, 0, rasterType);
    }

    width = 0;
    height = 0;
    images[0] = TextureImageReadAndSize(imageName, maskImageName, rasterType, &width, &height,
                                        &depth, &format);
    if (images[0] == NULL) {
        return NULL;
    }

    raster = RwRasterCreate(width, height, depth, format);
    if (raster == NULL) {
        RwImageDestroy(images[0]);
        return NULL;
    }

    if (format & rwRASTERFORMATMIPMAP) {
        if (format & rwRASTERFORMATAUTOMIPMAP) {
            /* The raster generates its own levels from the top one. */
            if (!RwRasterSetFromImage(raster, images[0])) {
                RwRasterDestroy(raster);
                RwImageDestroy(images[0]);
                return NULL;
            }
            RwImageDestroy(images[0]);
        } else {
            numLevels = RwRasterGetNumLevels(raster);

            for (i = 1; i < numLevels; i++) {
                rwstrncpy(imageName, name, rwTEXTUREIMAGENAMELENGTH);
                if (rwstrlen(name) >= rwTEXTUREIMAGENAMELENGTH) {
                    RWERROR((E_RW_STRING_TRUNCATION, name, rwTEXTUREIMAGENAMELENGTH,
                             rwTEXTUREIMAGENAMELENGTH - 1, name[rwTEXTUREIMAGENAMELENGTH - 1]));
                    imageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
                }
                maskImageName[0] = '\0';
                if (maskName != NULL && *maskName != '\0') {
                    rwstrncpy(maskImageName, maskName, rwTEXTUREIMAGENAMELENGTH);
                    if (rwstrlen(maskName) >= rwTEXTUREIMAGENAMELENGTH) {
                        RWERROR((E_RW_STRING_TRUNCATION, maskName, rwTEXTUREIMAGENAMELENGTH,
                                 rwTEXTUREIMAGENAMELENGTH - 1,
                                 maskName[rwTEXTUREIMAGENAMELENGTH - 1]));
                        maskImageName[rwTEXTUREIMAGENAMELENGTH - 1] = '\0';
                    }
                }

                if (RWTEXTUREGLOBAL(mipmapNameCallback) != NULL) {
                    RWTEXTUREGLOBAL(mipmapNameCallback)(imageName, maskImageName, (RwUInt8)i, rasterType);
                }

                RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
                width = RwRasterGetWidth(raster);
                height = RwRasterGetHeight(raster);
                depth = RwRasterGetDepth(raster);
                format = (raster->cFormat << 8) | raster->cType;
                RwRasterUnlock(raster);

                images[i] = TextureImageReadAndSize(imageName, maskImageName, rasterType, &width,
                                                    &height, &depth, &format);
                if (images[i] == NULL) {
                    while (--i >= 0) {
                        RwImageDestroy(images[i]);
                    }
                    RwRasterDestroy(raster);
                    return NULL;
                }
            }

            if (RwRasterGetFormat(raster) & (rwRASTERFORMATPAL4 | rwRASTERFORMATPAL8)) {
                if (RwRasterGetFormat(raster) & rwRASTERFORMATPAL4) {
                    PalettizeMipmaps(palette, NULL, images, numLevels, 4);
                } else {
                    PalettizeMipmaps(palette, NULL, images, numLevels, 8);
                }
                /* The levels share the palette: correcting it once corrects them all. */
                RwImageGammaCorrect(images[0]);
            } else {
                for (i = 0; i < numLevels; i++) {
                    RwImageGammaCorrect(images[i]);
                }
            }

            for (i = 0; i < numLevels; i++) {
                if (RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH)) {
                    if (!RwRasterSetFromImage(raster, images[i])) {
                        for (; i < numLevels; i++) {
                            RwImageDestroy(images[i]);
                        }
                        RwRasterDestroy(raster);
                        return NULL;
                    }
                    RwRasterUnlock(raster);
                }
                RwImageDestroy(images[i]);
            }
        }
    } else {
        RwImageGammaCorrect(images[0]);
        if (!RwRasterSetFromImage(raster, images[0])) {
            RwRasterDestroy(raster);
            RwImageDestroy(images[0]);
            return NULL;
        }
        RwImageDestroy(images[0]);
    }

    texture = RwTextureCreate(raster);
    if (texture == NULL) {
        RwRasterDestroy(raster);
        return NULL;
    }

    RwTextureSetName(texture, name);
    if (maskName != NULL) {
        RwTextureSetMaskName(texture, maskName);
    } else {
        RwTextureSetMaskName(texture, "");
    }

    return texture;
} // 0x80298E60

/* The default read callback: with or without mipmap levels, as RwTextureSetMipmapping says. */
static RwTexture* TextureDefaultRead(const RwChar* name, const RwChar* maskName) {
    if (RWTEXTUREGLOBAL(haveTexMipmaps)) {
        return TextureDefaultMipmapRead(name, maskName);
    }

    return TextureDefaultNormalRead(name, maskName);
} // 0x802995D4

/*
 * The default mipmap builder: each level of the raster resampled from the level above, starting
 * from the image (at 32 bits; the raster's own pixels without one), palettised as the raster is.
 * The raster's automatic mipmapping is off while its levels are written.
 */
static RwRaster* TextureRasterDefaultBuildMipmaps(RwRaster* raster, RwImage* image) {
    RwImage* images[rwTEXTUREMAXMIPLEVELS];
    RwRGBA palette[rwTEXTUREPALETTESIZE];
    RwUInt8 autoMipmap;
    RwInt32 i;
    RwInt32 numLevels;
    RwInt32 width = RwRasterGetWidth(raster);
    RwInt32 height = RwRasterGetHeight(raster);

    if (image == NULL) {
        images[0] = RwImageCreate(width, height, 32);
        if (images[0] != NULL) {
            if (!RwImageAllocatePixels(images[0])) {
                return NULL;
            }
            RwImageSetFromRaster(images[0], raster);
        }
    } else if (image->depth != 32) {
        images[0] = RwImageCreate(width, height, 32);
        if (images[0] != NULL) {
            if (!RwImageAllocatePixels(images[0])) {
                return NULL;
            }
            RwImageCopy(images[0], image);
        }
    } else {
        images[0] = image;
    }

    if (images[0] == NULL) {
        return NULL;
    }

    autoMipmap = raster->cFormat & (rwRASTERFORMATAUTOMIPMAP >> 8);
    raster->cFormat &= ~autoMipmap;

    numLevels = RwRasterGetNumLevels(raster);
    for (i = 1; i < numLevels; i++) {
        images[i] = NULL;
        if (RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKREAD)) {
            images[i] = RwImageCreateResample(images[i - 1], raster->width, raster->height);
            RwRasterUnlock(raster);
        }

        if (images[i] == NULL) {
            while (--i >= 0) {
                if (images[i] != image) {
                    RwImageDestroy(images[i]);
                }
            }
            raster->cFormat |= autoMipmap;
            return NULL;
        }
    }

    if (RwRasterGetFormat(raster) & (rwRASTERFORMATPAL4 | rwRASTERFORMATPAL8)) {
        if (RwRasterGetFormat(raster) & rwRASTERFORMATPAL4) {
            if (!PalettizeMipmaps(palette, image, images, numLevels, 4)) {
                /* The original returns inside its clean-up loop: only the top level is destroyed. */
                for (i = 0; i < numLevels; i++) {
                    if (images[i] != image) {
                        RwImageDestroy(images[i]);
                    }
                    raster->cFormat |= autoMipmap;
                    return NULL;
                }
            }
        } else {
            if (!PalettizeMipmaps(palette, image, images, numLevels, 8)) {
                for (i = 0; i < numLevels; i++) {
                    if (images[i] != image) {
                        RwImageDestroy(images[i]);
                    }
                    raster->cFormat |= autoMipmap;
                    return NULL;
                }
            }
        }
        /* The levels share the palette: correcting it once corrects them all. */
        RwImageGammaCorrect(images[0]);
    } else {
        for (i = 0; i < numLevels; i++) {
            RwImageGammaCorrect(images[i]);
        }
    }

    for (i = 0; i < numLevels; i++) {
        if (RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH)) {
            RwRasterSetFromImage(raster, images[i]);
            RwRasterUnlock(raster);
        }
        if (images[i] != image) {
            RwImageDestroy(images[i]);
        }
    }

    raster->cFormat |= autoMipmap;

    return raster;
} // 0x802995F4

/* Compares two names, ignoring the case of ASCII letters; TRUE when they are equal. */
static RwBool StringCompare(const RwChar* a, const RwChar* b) {
    while (*a != '\0' && *b != '\0') {
        RwChar charA = *a;
        RwChar charB = *b;

        if (charA >= 'a' && charA <= 'z') {
            charA -= 'a' - 'A';
        }
        if (charB >= 'a' && charB <= 'z') {
            charB -= 'a' - 'A';
        }

        if (charA != charB) {
            return FALSE;
        }

        a++;
        b++;
    }

    if (*a == *b) {
        return TRUE;
    }

    return FALSE;
} // 0x802998E8

/*
 * The default find callback: the texture of that name in the current dictionary, or in any
 * dictionary when there is no current one. Name invented (after TextureDefaultRead).
 */
static RwTexture* TextureDefaultFind(const RwChar* name) {
    RwLLLink* cur;
    RwLLLink* end;
    RwTexture* texture;

    if (RWTEXTUREGLOBAL(currentTexDict) != NULL) {
        return RwTexDictionaryFindNamedTexture(RWTEXTUREGLOBAL(currentTexDict), name);
    }

    cur = rwLinkListGetFirstLLLink(&RWTEXTUREGLOBAL(texDictList));
    end = rwLinkListGetTerminator(&RWTEXTUREGLOBAL(texDictList));
    while (cur != end) {
        RwTexDictionary* dict = rwLLLinkGetData(cur, RwTexDictionary, lInInstance);

        texture = RwTexDictionaryFindNamedTexture(dict, name);
        if (texture != NULL) {
            return texture;
        }

        cur = rwLLLinkGetNext(cur);
    }

    return NULL;
} // 0x80299978

/* Sets whether RwTextureRead reads mipmap levels; always TRUE. */
RwBool RwTextureSetMipmapping(RwBool enable) {
    RWTEXTUREGLOBAL(haveTexMipmaps) = enable;

    return TRUE;
} // 0x80299A60

/* Whether RwTextureRead reads mipmap levels. */
RwBool RwTextureGetMipmapping(void) {
    return RWTEXTUREGLOBAL(haveTexMipmaps);
} // 0x80299A78

/* Sets whether RwTextureRead generates the mipmap levels instead of reading them; always TRUE. */
RwBool RwTextureSetAutoMipmapping(RwBool enable) {
    RWTEXTUREGLOBAL(generateTexMipmaps) = enable;

    return TRUE;
} // 0x80299A8C

/* Whether RwTextureRead generates the mipmap levels. */
RwBool RwTextureGetAutoMipmapping(void) {
    return RWTEXTUREGLOBAL(generateTexMipmaps);
} // 0x80299AA4

/* Gives the texture a raster through the driver; NULL when the driver refuses it. */
RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster) {
    if (raster != NULL) {
        if (RWSRCGLOBAL(stdFunc)[rwSTANDARDTEXTURESETRASTER](texture, raster, 0)) {
            return texture;
        }

        return NULL;
    }

    texture->raster = NULL;

    return texture;
} // 0x80299AB8

/* Creates an empty dictionary and puts it in the module's list. */
RwTexDictionary* RwTexDictionaryCreate(void) {
    RwTexDictionary* dict;

    dict = (RwTexDictionary*)RwFreeListAlloc(RWTEXTUREGLOBAL(texDictFreeList),
                                             rwMEMHINTDUR_EVENT | rwID_TEXDICTIONARY);
    if (dict == NULL) {
        return NULL;
    }

    rwObjectInitialize(dict, rwTEXDICTIONARY, 0);
    rwLinkListAddLLLink(&RWTEXTUREGLOBAL(texDictList), &dict->lInInstance);
    rwLinkListInitialize(&dict->texturesInDict);
    _rwPluginRegistryInitObject(&texDictTKList, dict);

    return dict;
} // 0x80299B1C

/* Destroys a dictionary and its textures (each loses the dictionary's reference). */
RwBool RwTexDictionaryDestroy(RwTexDictionary* dict) {
    if (RWTEXTUREGLOBAL(currentTexDict) == dict) {
        RWTEXTUREGLOBAL(currentTexDict) = NULL;
    }

    RwTexDictionaryForAllTextures(dict, (RwTextureCallBack)RwTextureDestroy, NULL);

    _rwPluginRegistryDeInitObject(&texDictTKList, dict);
    rwLinkListRemoveLLLink(&dict->lInInstance);
    RwFreeListFree(RWTEXTUREGLOBAL(texDictFreeList), dict);

    return TRUE;
} // 0x80299BF4

/* Calls fpCallBack for each texture in the dictionary until it returns NULL. */
const RwTexDictionary* RwTexDictionaryForAllTextures(const RwTexDictionary* dict,
                                                     RwTextureCallBack fpCallBack, void* pData) {
    const RwLLLink* next;
    const RwLLLink* cur;
    const RwLLLink* end;

    cur = rwLinkListGetFirstLLLink(&dict->texturesInDict);
    end = rwLinkListGetTerminator(&dict->texturesInDict);
    while (cur != end) {
        RwTexture* texture = (RwTexture*)rwLLLinkGetConstData(cur, RwTexture, lInDictionary);

        next = rwLLLinkGetNext(cur);
        if (!fpCallBack(texture, pData)) {
            break;
        }

        cur = next;
    }


    return dict;
} // 0x80299CC8

/* Calls fpCallBack for each dictionary until it returns NULL; always TRUE. */
RwBool RwTexDictionaryForAllTexDictionaries(RwTexDictionaryCallBack fpCallBack, void* pData) {
    RwLLLink* cur;
    RwLLLink* end;

    cur = rwLinkListGetFirstLLLink(&RWTEXTUREGLOBAL(texDictList));
    end = rwLinkListGetTerminator(&RWTEXTUREGLOBAL(texDictList));
    while (cur != end) {
        RwTexDictionary* dict = rwLLLinkGetData(cur, RwTexDictionary, lInInstance);

        cur = rwLLLinkGetNext(cur);
        if (!fpCallBack(dict, pData)) {
            break;
        }
    }

    return TRUE;
} // 0x80299D30

/* Creates a texture of the raster, unnamed, nearest filtering, wrapping, one reference. */
RwTexture* RwTextureCreate(RwRaster* raster) {
    RwTexture* texture;

    texture = (RwTexture*)RwFreeListAlloc(RWTEXTUREGLOBAL(textureFreeList),
                                          rwMEMHINTDUR_EVENT | rwID_TEXTURE);
    if (texture != NULL) {
        texture->dict = NULL;
        texture->name[0] = '\0';
        texture->mask[0] = '\0';
        texture->raster = raster;
        texture->refCount = 1;
        texture->filterAddressing = 0;
        RwTextureSetAddressing(texture, rwTEXTUREADDRESSWRAP);
        RwTextureSetFilterMode(texture, rwFILTERNEAREST);
        _rwPluginRegistryInitObject(&textureTKList, texture);
    }

    return texture;
} // 0x80299DB4

/*
 * Drops a reference; the last one takes the texture out of its dictionary and destroys it with
 * its raster. TRUE either way.
 */
RwBool RwTextureDestroy(RwTexture* texture) {
    RwBool result = TRUE;

    texture->refCount--;
    if (texture->refCount <= 0) {
        /* The plugins see the texture still referenced while they let go of it. */
        texture->refCount++;
        _rwPluginRegistryDeInitObject(&textureTKList, texture);
        if (texture->dict != NULL) {
            rwLinkListRemoveLLLink(&texture->lInDictionary);
        }
        if (texture->raster != NULL) {
            RwRasterDestroy(texture->raster);
            texture->raster = NULL;
        }
        texture->refCount--;
        RwFreeListFree(RWTEXTUREGLOBAL(textureFreeList), texture);
        result = TRUE;
    }

    return result;
} // 0x80299E4C

/* Names the texture; a name too long for it is cut, with an error. */
RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name) {
    rwstrncpy(texture->name, name, rwTEXTUREBASENAMELENGTH);
    if (rwstrlen(name) >= rwTEXTUREBASENAMELENGTH) {
        RWERROR((E_RW_STRING_TRUNCATION, name, rwTEXTUREBASENAMELENGTH, rwTEXTUREBASENAMELENGTH - 1,
                 name[rwTEXTUREBASENAMELENGTH - 1]));
        texture->name[rwTEXTUREBASENAMELENGTH - 1] = '\0';
    }

    return texture;
} // 0x80299F10

/* Sets the texture's mask name; a name too long for it is cut, with an error. */
RwTexture* RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName) {
    rwstrncpy(texture->mask, maskName, rwTEXTUREBASENAMELENGTH);
    if (rwstrlen(maskName) >= rwTEXTUREBASENAMELENGTH) {
        RWERROR((E_RW_STRING_TRUNCATION, maskName, rwTEXTUREBASENAMELENGTH,
                 rwTEXTUREBASENAMELENGTH - 1, maskName[rwTEXTUREBASENAMELENGTH - 1]));
        texture->mask[rwTEXTUREBASENAMELENGTH - 1] = '\0';
    }

    return texture;
} // 0x80299FBC

/* Moves the texture into the dictionary (out of the one holding it). */
RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture) {
    if (texture->dict != NULL) {
        rwLinkListRemoveLLLink(&texture->lInDictionary);
    }

    texture->dict = dict;
    rwLinkListAddLLLink(&dict->texturesInDict, &texture->lInDictionary);

    return texture;
} // 0x8029A068

/* Takes the texture out of the dictionary holding it. */
RwTexture* RwTexDictionaryRemoveTexture(RwTexture* texture) {
    if (texture->dict != NULL) {
        texture->dict = NULL;
        rwLinkListRemoveLLLink(&texture->lInDictionary);
    }

    return texture;
} // 0x8029A0B8

/* The dictionary's texture of that name (case ignored), or NULL. */
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const RwChar* name) {
    RwTexture* texture;
    RwLLLink* cur;
    RwLLLink* end;

    cur = rwLinkListGetFirstLLLink(&dict->texturesInDict);
    end = rwLinkListGetTerminator(&dict->texturesInDict);
    while (cur != end) {
        texture = rwLLLinkGetData(cur, RwTexture, lInDictionary);

        if (texture->name != NULL && StringCompare(texture->name, name)) {
            return texture;
        }

        cur = rwLLLinkGetNext(cur);
    }

    return NULL;
} // 0x8029A0E8

/* Makes the dictionary the one RwTextureRead looks in and adds to. */
RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* dict) {
    RWTEXTUREGLOBAL(currentTexDict) = dict;

    return dict;
} // 0x8029A168

/* The dictionary RwTextureRead looks in and adds to. */
RwTexDictionary* RwTexDictionaryGetCurrent(void) {
    return RWTEXTUREGLOBAL(currentTexDict);
} // 0x8029A17C

/*
 * The texture of that name: found by the find callback (one more reference), else read by the
 * read callback and added to the current dictionary. NULL, with an error, when it cannot be read.
 */
RwTexture* RwTextureRead(const RwChar* name, const RwChar* maskName) {
    RwTexture* texture;

    texture = RWTEXTUREGLOBAL(textureFindFunc)(name);
    if (texture != NULL) {
        RwTextureAddRef(texture);
        return texture;
    }

    texture = RWTEXTUREGLOBAL(textureReadFunc)(name, maskName);
    if (texture == NULL) {
        /* Core error 22: the texture (and mask) could not be read; its name is in no reachable source. */
        if (maskName != NULL) {
            RWERROR((22, name, maskName));
        } else {
            RWERROR((22, name, "(null)"));
        }
        return NULL;
    }

    if (RWTEXTUREGLOBAL(currentTexDict) != NULL) {
        RwTexDictionaryAddTexture(RWTEXTUREGLOBAL(currentTexDict), texture);
    }

    return texture;
} // 0x8029A190

/* Registers a plugin with textures; its offset in a texture, or -1. */
RwInt32 RwTextureRegisterPlugin(RwInt32 size, RwUInt32 pluginID,
                                RwPluginObjectConstructor constructCB,
                                RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&textureTKList, size, pluginID, constructCB, destructCB,
                                      copyCB);
} // 0x8029A2E0

/* Builds the raster's mipmap levels from the image with the module's callback. */
RwBool RwTextureRasterGenerateMipmaps(RwRaster* raster, RwImage* image) {
    return RWTEXTUREGLOBAL(mipmapCallback)(raster, image) != NULL;
} // 0x8029A310

/*
 * Closes the texture module: frees the scratch block, the dummy dictionary and the free lists.
 * Locals: Gormiti DWARF (texDictsExistOnShutdown is read only by the debug build's assertion).
 */
void* _rwTextureClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWTEXTUREGLOBAL(scratchMem) != NULL) {
        RwFree(RWTEXTUREGLOBAL(scratchMem));
        RWTEXTUREGLOBAL(scratchMem) = NULL;
        RWTEXTUREGLOBAL(scratchMemSize) = 0;
    }

    if (RWTEXTUREGLOBAL(textureFreeList) != NULL && RWTEXTUREGLOBAL(texDictFreeList) != NULL) {
        RwLLLink* cur = rwLinkListGetFirstLLLink(&RWTEXTUREGLOBAL(texDictList));
        RwLLLink* end = rwLinkListGetTerminator(&RWTEXTUREGLOBAL(texDictList));
        RwBool texDictsExistOnShutdown = (cur != end);

        while (cur != end) {
            RwLLLink* next = rwLLLinkGetNext(cur);

            if (rwLLLinkGetData(cur, RwTexDictionary, lInInstance) == dummyTexDict) {
                RwTexDictionaryDestroy(dummyTexDict);
                dummyTexDict = NULL;
                cur = rwLinkListGetFirstLLLink(&RWTEXTUREGLOBAL(texDictList));
                end = rwLinkListGetTerminator(&RWTEXTUREGLOBAL(texDictList));
                texDictsExistOnShutdown = (cur != end);
                break;
            }

            cur = next;
        }

        /* Debug build: only the dummy dictionary may still exist. */
        RWASSERT(!texDictsExistOnShutdown);
    }

    if (RWTEXTUREGLOBAL(textureFreeList) != NULL) {
        RwFreeListDestroy(RWTEXTUREGLOBAL(textureFreeList));
        RWTEXTUREGLOBAL(textureFreeList) = NULL;
    }

    if (RWTEXTUREGLOBAL(texDictFreeList) != NULL) {
        RwFreeListDestroy(RWTEXTUREGLOBAL(texDictFreeList));
        RWTEXTUREGLOBAL(texDictFreeList) = NULL;
    }

    textureModule.numInstances--;

    return instance;
} // 0x8029A350

static RwInt32 _rwTextureFreeListBlockSize = 128;
static RwInt32 _rwTextureFreeListPreallocBlocks = 1;
static RwInt32 _rwTexDictionaryFreeListBlockSize = 5;
static RwInt32 _rwTexDictionaryFreeListPreallocBlocks = 1;

/*
 * Opens the texture module: the free lists, the list of dictionaries, the dummy dictionary
 * (current) and the default callbacks.
 */
void* _rwTextureOpen(void* instance, RwInt32 offset, RwInt32 size) {
    textureModule.globalsOffset = offset;

    RWTEXTUREGLOBAL(textureFreeList) = RwFreeListCreateAndPreallocateSpace(
        textureTKList.sizeOfStruct, _rwTextureFreeListBlockSize, 4,
        _rwTextureFreeListPreallocBlocks, &_rwTextureFreeList, rwMEMHINTDUR_GLOBAL | rwID_TEXTURE);
    if (RWTEXTUREGLOBAL(textureFreeList) == NULL) {
        return NULL;
    }

    RWTEXTUREGLOBAL(texDictFreeList) = RwFreeListCreateAndPreallocateSpace(
        texDictTKList.sizeOfStruct, _rwTexDictionaryFreeListBlockSize, 4,
        _rwTexDictionaryFreeListPreallocBlocks, &_rwTexDictionaryFreeList,
        rwMEMHINTDUR_GLOBAL | rwID_TEXTUREMODULE);
    if (RWTEXTUREGLOBAL(texDictFreeList) == NULL) {
        RwFreeListDestroy(RWTEXTUREGLOBAL(textureFreeList));
        RWTEXTUREGLOBAL(textureFreeList) = NULL;
        return NULL;
    }

    rwLinkListInitialize(&RWTEXTUREGLOBAL(texDictList));
    textureModule.numInstances++;

    dummyTexDict = RwTexDictionaryCreate();
    RWTEXTUREGLOBAL(currentTexDict) = dummyTexDict;
    if (RWTEXTUREGLOBAL(currentTexDict) == NULL) {
        RwFreeListDestroy(RWTEXTUREGLOBAL(texDictFreeList));
        RWTEXTUREGLOBAL(texDictFreeList) = NULL;
        RwFreeListDestroy(RWTEXTUREGLOBAL(textureFreeList));
        RWTEXTUREGLOBAL(textureFreeList) = NULL;
        return NULL;
    }

    RWTEXTUREGLOBAL(haveTexMipmaps) = FALSE;
    RWTEXTUREGLOBAL(generateTexMipmaps) = FALSE;
    RWTEXTUREGLOBAL(textureFindFunc) = TextureDefaultFind;
    RWTEXTUREGLOBAL(textureReadFunc) = TextureDefaultRead;
    RWTEXTUREGLOBAL(mipmapCallback) = TextureRasterDefaultBuildMipmaps;
    RWTEXTUREGLOBAL(mipmapNameCallback) = TextureDefaultMipmapName;
    RWTEXTUREGLOBAL(scratchMem) = NULL;
    RWTEXTUREGLOBAL(scratchMemSize) = 0;

    return instance;
} // 0x8029A530

