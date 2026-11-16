// Unit name and boundary: Gormiti link map (RwCore.a baimage.obj)
// Names, locals and struct layouts: Gormiti DWARF (baimage.c); the inlined statics are named from the Debug build.
// Reference: tgsm/pbg src/rwsdk/src/baframe.c (style and header layout)
/*
 * baimage.c: RenderWare device-independent images. Manages the RwImage free list, pixel/palette
 * allocation, mask extraction and application, image search path, registered image format lookup
 * and loading, depth conversion, and gamma correction.
 */
#include <math.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baimage.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bafsys.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/rwstring.h>

/* A registered image file format entry (Gormiti DWARF: struct rwImageFormat, size 0x34). */
typedef struct rwImageFormat rwImageFormat;
struct rwImageFormat {
    RwChar lcExtension[20];          /* 0x00 */
    RwChar ucExtension[20];          /* 0x14 */
    RwImageCallBackRead readImage;   /* 0x28 */
    RwImageCallBackWrite writeImage; /* 0x2C */
    rwImageFormat* nextFormat;       /* 0x30 */
};

/* Callback state passed through ImagePathForAllFullNames (Gormiti DWARF: struct _imageReadData). */
typedef struct _imageReadData {
    RwImageCallBackRead readImage;   /* 0x00 */
    RwImage* image;                  /* 0x04 */
} _imageReadData;

/*
 * Globals for the image module inside the engine instance block (size 0x220 = 544 bytes,
 * verified against _rwImageOpen / _rwImageClose).
 */
typedef struct rwImageGlobals {
    RwFreeList* images;              /* 0x000 */
    RwChar* imagePath;               /* 0x004 */
    RwInt32 imagePathSize;           /* 0x008 */
    RwUInt8 gammaTable[256];         /* 0x00C */
    RwUInt8 invGammaTable[256];      /* 0x10C */
    RwReal gamma;                    /* 0x20C */
    RwChar* scratchMem;              /* 0x210 */
    RwInt32 scratchMemSize;          /* 0x214 */
    RwFreeList* imageFormatFreeList; /* 0x218 */
    rwImageFormat* imageFormatList;  /* 0x21C */
} rwImageGlobals;

#define RWIMAGEGLOBAL(var) \
    (RWPLUGINOFFSET(rwImageGlobals, RwEngineInstance, lbl_8052B178.globalsOffset)->var)

/* Plugin registry for RwImage objects (imageTKList). */
RwPluginRegistry lbl_8044A7A8 = {
    sizeof(RwImage),
    sizeof(RwImage),
    0,
    0,
    NULL,
    NULL
};

/* Image module registration (imageModule). */
struct RwModuleInfo lbl_8052B178;

/* Free lists for RwImage and rwImageFormat entries. */
static RwFreeList lbl_804F6EC8;
static RwFreeList lbl_804F6EEC;

/* Free-list block size and preallocation counts for _rwImageOpen. */
static RwInt32 lbl_80528E98 = 128;
static RwInt32 lbl_80528E9C = 1;
static RwInt32 lbl_80528EA0 = 1;

void* _rwImageOpen(void* instance, RwInt32 offset, RwInt32 size) {
    lbl_8052B178.globalsOffset = offset;

    RWIMAGEGLOBAL(images) = RwFreeListCreateAndPreallocateSpace(
        lbl_8044A7A8.sizeOfStruct, lbl_80528E98, sizeof(RwUInt32), lbl_80528E9C, &lbl_804F6EC8,
        rwMEMHINTDUR_GLOBAL | rwID_IMAGE);
    if (RWIMAGEGLOBAL(images) == NULL) {
        return NULL;
    }

    RWIMAGEGLOBAL(imageFormatFreeList) = RwFreeListCreateAndPreallocateSpace(
        sizeof(rwImageFormat), lbl_80528E98, sizeof(RwUInt32), lbl_80528EA0, &lbl_804F6EEC,
        rwMEMHINTDUR_GLOBAL | rwID_IMAGEMODULE);
    if (RWIMAGEGLOBAL(imageFormatFreeList) == NULL) {
        RwFreeListDestroy(RWIMAGEGLOBAL(images));
        RWIMAGEGLOBAL(images) = NULL;
        return NULL;
    }

    RWIMAGEGLOBAL(imagePathSize) = 256;
    RWIMAGEGLOBAL(imagePath) = (RwChar*)RwMalloc(
        RWIMAGEGLOBAL(imagePathSize),
        rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_IMAGEMODULE);
    if (RWIMAGEGLOBAL(imagePath) == NULL) {
        RwFreeListDestroy(RWIMAGEGLOBAL(imageFormatFreeList));
        RWIMAGEGLOBAL(imageFormatFreeList) = NULL;
        RwFreeListDestroy(RWIMAGEGLOBAL(images));
        RWIMAGEGLOBAL(images) = NULL;
        return NULL;
    }

    RWIMAGEGLOBAL(imagePath)[0] = '\0';

    lbl_8052B178.numInstances++;
    RwImageSetGamma((RwReal)1.0);

    RWIMAGEGLOBAL(imageFormatList) = NULL;

    RWIMAGEGLOBAL(scratchMemSize) = 256;
    RWIMAGEGLOBAL(scratchMem) = (RwChar*)RwMalloc(
        RWIMAGEGLOBAL(scratchMemSize),
        rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_IMAGE);
    if (RWIMAGEGLOBAL(scratchMem) == NULL) {
        RwFree(RWIMAGEGLOBAL(imagePath));
        RWIMAGEGLOBAL(imagePath) = NULL;
        RWIMAGEGLOBAL(imagePathSize) = 0;

        RwFreeListDestroy(RWIMAGEGLOBAL(imageFormatFreeList));
        RWIMAGEGLOBAL(imageFormatFreeList) = NULL;

        RwFreeListDestroy(RWIMAGEGLOBAL(images));
        RWIMAGEGLOBAL(images) = NULL;

        return NULL;
    }

    return instance;
} // 0x80294E5C

void* _rwImageClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWIMAGEGLOBAL(scratchMem) != NULL) {
        RwFree(RWIMAGEGLOBAL(scratchMem));
        RWIMAGEGLOBAL(scratchMem) = NULL;
        RWIMAGEGLOBAL(scratchMemSize) = 0;
    }

    if (RWIMAGEGLOBAL(imagePath) != NULL) {
        RwFree(RWIMAGEGLOBAL(imagePath));
        RWIMAGEGLOBAL(imagePath) = NULL;
        RWIMAGEGLOBAL(imagePathSize) = 0;
    }

    while (RWIMAGEGLOBAL(imageFormatList) != NULL) {
        rwImageFormat* formatToDestroy = RWIMAGEGLOBAL(imageFormatList);

        RWIMAGEGLOBAL(imageFormatList) = formatToDestroy->nextFormat;
        RwFreeListFree(RWIMAGEGLOBAL(imageFormatFreeList), formatToDestroy);
    }

    if (RWIMAGEGLOBAL(imageFormatFreeList) != NULL) {
        RwFreeListDestroy(RWIMAGEGLOBAL(imageFormatFreeList));
        RWIMAGEGLOBAL(imageFormatFreeList) = NULL;
    }

    if (RWIMAGEGLOBAL(images) != NULL) {
        RwFreeListDestroy(RWIMAGEGLOBAL(images));
        RWIMAGEGLOBAL(images) = NULL;
    }

    lbl_8052B178.numInstances--;

    return instance;
} // 0x802950F0

static RwChar* ImageGetScratchMem(RwInt32 size) {
    if (size > RWIMAGEGLOBAL(scratchMemSize)) {
        RwChar* newBuf;

        if (RWIMAGEGLOBAL(scratchMem)) {
            newBuf = (RwChar*)RwRealloc(
                RWIMAGEGLOBAL(scratchMem), size,
                rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_IMAGE);
        } else {
            newBuf = (RwChar*)RwMalloc(
                size,
                rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_IMAGE);
        }

        if (!newBuf) {
            RWERROR((E_RW_NOMEM, size));
            return NULL;
        }

        RWIMAGEGLOBAL(scratchMem) = newBuf;
        RWIMAGEGLOBAL(scratchMemSize) = size;
    }

    return RWIMAGEGLOBAL(scratchMem);
} // no copy in retail (inlined): a static in the Gormiti Debug map (baimage.obj, 0x210 bytes), absent from the Final map

static void _rwImageGammaCorrectArrayOfRGBA(RwRGBA* rgbaOut, RwRGBA* rgbaIn, RwInt32 numEls) {
    RwUInt8* gammaTab = RWIMAGEGLOBAL(gammaTable);

    while (numEls--) {
        rgbaOut->red = gammaTab[rgbaIn->red];
        rgbaOut->green = gammaTab[rgbaIn->green];
        rgbaOut->blue = gammaTab[rgbaIn->blue];
        rgbaOut->alpha = rgbaIn->alpha;
        rgbaIn++;
        rgbaOut++;
    }
} // no copy in retail (inlined): an UNUSED static in the Gormiti Final map (baimage.obj, 0x128 bytes), a copy in the Debug map

RwImage* RwImageCreate(RwInt32 width, RwInt32 height, RwInt32 depth) {
    RwImage* image;

    image = (RwImage*)RwFreeListAlloc(RWIMAGEGLOBAL(images), rwMEMHINTDUR_EVENT | rwID_IMAGE);
    if (!image) {
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->depth = depth;
    image->cpPixels = NULL;
    image->palette = NULL;
    image->flags = 0;

    _rwPluginRegistryInitObject(&lbl_8044A7A8, image);

    return image;
} // 0x80295254

RwBool RwImageDestroy(RwImage* image) {
    if (image->flags & rwIMAGEALLOCATED) {
        RwFree(image->cpPixels);
        image->cpPixels = NULL;
        image->palette = NULL;
        image->flags &= ~rwIMAGEALLOCATED;
    }

    _rwPluginRegistryDeInitObject(&lbl_8044A7A8, image);
    RwFreeListFree(RWIMAGEGLOBAL(images), image);

    return TRUE;
} // 0x80295300

RwImage* RwImageAllocatePixels(RwImage* image) {
    RwUInt32 imageDepth;
    RwBool imagePalette;
    RwUInt32 paletteSize;
    RwUInt32 pixelsSize;
    RwUInt32 totalSize;

    imageDepth = (RwUInt32)image->depth;
    imagePalette = (imageDepth == 4 || imageDepth == 8);
    if (imagePalette) {
        paletteSize = (1U << imageDepth) * (RwUInt32)sizeof(RwRGBA);
    } else {
        paletteSize = 0;
    }

    image->stride = ((((RwInt32)imageDepth + 7) >> 3) * image->width + 3) & ~3;
    pixelsSize = (RwUInt32)(image->stride * image->height);
    totalSize = pixelsSize + paletteSize;

    image->cpPixels = (RwUInt8*)RwMalloc(totalSize, rwMEMHINTDUR_EVENT | rwID_IMAGE);
    if (!image->cpPixels) {
        RWERROR((E_RW_NOMEM, totalSize));
        return NULL;
    }

    image->palette = imagePalette ? (RwRGBA*)(image->cpPixels + pixelsSize) : NULL;
    image->flags |= rwIMAGEALLOCATED;

    return image;
} // 0x80295390

RwImage* RwImageFreePixels(RwImage* image) {
    RwFree(image->cpPixels);
    image->cpPixels = NULL;
    image->palette = NULL;
    image->flags &= ~rwIMAGEALLOCATED;

    return image;
} // 0x80295494

RwImage* RwImageMakeMask(RwImage* image) {
    RwInt32 i;

    switch (image->depth) {
        case 4:
        case 8: {
            RwInt32 palSize = 1 << image->depth;
            RwRGBA* rpPal = image->palette;

            for (i = 0; i < palSize; i++) {
                RwInt32 nOpacity;

                nOpacity = rpPal[i].red;
                if (rpPal[i].green > nOpacity) {
                    nOpacity = rpPal[i].green;
                }
                if (rpPal[i].blue > nOpacity) {
                    nOpacity = rpPal[i].blue;
                }
                rpPal[i].alpha = (RwUInt8)nOpacity;
            }
            break;
        }
        case 32: {
            RwUInt8* cpSpan;
            RwInt32 j;

            cpSpan = image->cpPixels;

            for (i = 0; i < image->height; i++) {
                RwRGBA* rpCur = (RwRGBA*)cpSpan;

                for (j = 0; j < image->width; j++) {
                    RwInt32 nOpacity;

                    nOpacity = rpCur[j].red;
                    if (rpCur[j].green > nOpacity) {
                        nOpacity = rpCur[j].green;
                    }
                    if (rpCur[j].blue > nOpacity) {
                        nOpacity = rpCur[j].blue;
                    }
                    rpCur[j].alpha = (RwUInt8)nOpacity;
                }

                cpSpan += image->stride;
            }
            break;
        }
        default:
            break;
    }

    return image;
} // 0x802954EC

RwImage* RwImageApplyMask(RwImage* image, const RwImage* mask) {
    if (image->width != mask->width || image->height != mask->height) {
        RWERROR((E_RW_INVIMAGEMASK));
        return NULL;
    }

    switch (image->depth) {
        case 4:
        case 8: {
            RwImage* tempImage = RwImageCreate(image->width, image->height, image->depth);
            if (!tempImage) {
                return NULL;
            }
            if (!RwImageAllocatePixels(tempImage)) {
                RwImageDestroy(tempImage);
                return NULL;
            }

            RwImageCopy(tempImage, image);
            if (image->flags & rwIMAGEALLOCATED) {
                RwImageFreePixels(image);
            }
            image->depth = 32;
            RwImageAllocatePixels(image);
            RwImageCopy(image, tempImage);
            RwImageFreePixels(tempImage);
            RwImageDestroy(tempImage);
        }
        /* fall through */
        case 32: {
            RwInt32 i, j;
            RwUInt8* cpSrc = mask->cpPixels;
            RwRGBA* rpPal = mask->palette;
            RwUInt8* cpDst = image->cpPixels;

            for (i = 0; i < image->height; i++) {
                RwRGBA* dstRGB = (RwRGBA*)cpDst;

                switch (mask->depth) {
                    case 4:
                    case 8: {
                        RwUInt8* srcInd = cpSrc;
                        for (j = 0; j < image->width; j++) {
                            dstRGB->alpha = rpPal[*srcInd].alpha;
                            dstRGB++;
                            srcInd++;
                        }
                        break;
                    }
                    case 32: {
                        RwRGBA* srcRGB = (RwRGBA*)cpSrc;
                        for (j = 0; j < image->width; j++) {
                            dstRGB->alpha = srcRGB->alpha;
                            dstRGB++;
                            srcRGB++;
                        }
                        break;
                    }
                }

                cpDst += image->stride;
                cpSrc += mask->stride;
            }
            break;
        }
        default:
            RWERROR((E_RW_INVIMAGEFORMAT));
            return NULL;
    }

    return image;
} // 0x802955C4

// Gormiti name: RwImageSetPath (same size 0xF0 and position in baimage.obj; the symbol keeps its placeholder because
// sibling.exe pairs it with no counterpart).
const RwChar* fn_80295AA4(const RwChar* path) {
    RwInt32 newPathLength;

    newPathLength = (RwInt32)rwstrlen(path) + 1;
    if (newPathLength > RWIMAGEGLOBAL(imagePathSize)) {
        RwChar* newPath = (RwChar*)RwRealloc(
            RWIMAGEGLOBAL(imagePath), newPathLength,
            rwMEMHINTDUR_GLOBAL | rwMEMHINTFLAG_RESIZABLE | rwID_IMAGEMODULE);
        if (!newPath) {
            RWERROR((E_RW_NOMEM, newPathLength));
            return NULL;
        }
        RWIMAGEGLOBAL(imagePath) = newPath;
        RWIMAGEGLOBAL(imagePathSize) = newPathLength;
    }

    memcpy(RWIMAGEGLOBAL(imagePath), path, newPathLength);

    return path;
} // 0x80295AA4

// Gormiti name: ImagePathForAllFullNames (a static there; same size 0x330 and position in baimage.obj; the symbol
// keeps its placeholder because sibling.exe pairs it with no counterpart): calls callBack on each full name.
RwChar* fn_80295B94(const RwChar* filename, RwInt32 extraBytes,
                    RwChar* (*callBack)(const RwChar*, void*), void* data) {
    RwInt32 pathsize;
    RwChar* fullname;
    RwChar* pathElement = RWIMAGEGLOBAL(imagePath);

    if (fn_802A75A0(filename) || !pathElement || !(*pathElement)) {
        pathsize = (RwInt32)rwstrlen(filename) + extraBytes;
        fullname = ImageGetScratchMem(pathsize);
        if (!fullname) {
            return NULL;
        }

        rwstrcpy(fullname, filename);
        callBack(fullname, data);
    } else {
        while (pathElement && *pathElement) {
            RwChar* nextPathElement = rwstrchr(pathElement, ';');
            RwInt32 pathElementLength;

            /* Retail evaluates the strchr twice (the bytes do not match with a single evaluation), as written here. */
            if ((nextPathElement = rwstrchr(pathElement, ';')) != NULL) {
                pathElementLength = (RwInt32)(nextPathElement - pathElement);
                nextPathElement++;
            } else {
                pathElementLength = (RwInt32)rwstrlen(pathElement);
            }

            pathsize = (RwInt32)rwstrlen(filename) + (extraBytes + pathElementLength);
            fullname = ImageGetScratchMem(pathsize);
            if (!fullname) {
                return NULL;
            }

            memcpy(fullname, pathElement, pathElementLength);
            rwstrcpy(fullname + pathElementLength, filename);
            if (!callBack(fullname, data)) {
                return (RwChar*)filename;
            }

            pathElement = nextPathElement;
        }
    }

    return (RwChar*)filename;
} // 0x80295B94

RwChar* ImageAttempRead(const RwChar* pathname, void* data) {
    _imageReadData* imageData = (_imageReadData*)data;

    if (RwFexist(pathname)) {
        imageData->image = imageData->readImage(pathname);
        if (imageData->image) {
            return NULL;
        }
    }

    return (RwChar*)pathname;
} // 0x80295EC4

RwImage* RwImageRead(const RwChar* imageName) {
    const RwChar* lastSeparator;
    RwChar* testSeparator;
    RwChar* extender;

    testSeparator = rwstrrchr(imageName, ':');
    if (testSeparator) {
        lastSeparator = testSeparator;
    } else {
        lastSeparator = imageName;
    }

    testSeparator = rwstrrchr(lastSeparator, '/');
    if (testSeparator) {
        lastSeparator = testSeparator;
    }

    testSeparator = rwstrrchr(lastSeparator, '\\');
    extender = rwstrrchr(testSeparator ? testSeparator : lastSeparator, '.');

    if (extender) {
        rwImageFormat* imageFormat = RWIMAGEGLOBAL(imageFormatList);

        while (imageFormat) {
            if (!rwstrcmp(imageFormat->lcExtension, extender) ||
                !rwstrcmp(imageFormat->ucExtension, extender)) {
                if (imageFormat->readImage) {
                    _imageReadData imageData;
                    imageData.readImage = imageFormat->readImage;
                    imageData.image = NULL;
                    fn_80295B94(imageName, 5, ImageAttempRead, &imageData);
                    return imageData.image;
                }
                return NULL;
            }
            imageFormat = imageFormat->nextFormat;
        }
        return NULL;
    }

    return NULL;
} // 0x80295F2C

RwChar* ImageDetermineExtender(const RwChar* pathname, void* data) {
    const RwChar** extender = (const RwChar**)data;
    rwImageFormat* imageFormat;
    RwChar* extPos;

    extPos = (RwChar*)pathname + rwstrlen(pathname);
    imageFormat = RWIMAGEGLOBAL(imageFormatList);

    while (imageFormat) {
        rwstrcpy(extPos, imageFormat->lcExtension);
        if (RwFexist(pathname)) {
            *extender = imageFormat->lcExtension;
            return NULL;
        }

        rwstrcpy(extPos, imageFormat->ucExtension);
        if (RwFexist(pathname)) {
            *extender = imageFormat->ucExtension;
            return NULL;
        }

        imageFormat = imageFormat->nextFormat;
    }

    return (RwChar*)pathname;
} // 0x802960A4

const RwChar* RwImageFindFileType(const RwChar* imageName) {
    const RwChar* extender = NULL;

    fn_80295B94(imageName, 20, ImageDetermineExtender, &extender);

    return extender;
} // 0x8029618C

RwBool RwImageRegisterImageFormat(const RwChar* extension,
                                   RwImageCallBackRead imageRead,
                                   RwImageCallBackWrite imageWrite) {
    rwImageFormat* imageFormat;
    rwImageFormat* newFormat;
    RwChar lcExtension[20];

    lcExtension[0] = '.';
    rwstrncpy(&lcExtension[1], extension, 18);
    lcExtension[19] = '\0';
    rwstrlwr(lcExtension);

    newFormat = NULL;
    imageFormat = RWIMAGEGLOBAL(imageFormatList);
    while (imageFormat && !newFormat) {
        if (!rwstrcmp(imageFormat->lcExtension, lcExtension)) {
            newFormat = imageFormat;
        } else {
            imageFormat = imageFormat->nextFormat;
        }
    }

    if (!newFormat) {
        newFormat = (rwImageFormat*)RwFreeListAlloc(
            RWIMAGEGLOBAL(imageFormatFreeList),
            rwMEMHINTDUR_EVENT | rwID_IMAGEMODULE);
        if (!newFormat) {
            return FALSE;
        }
        memcpy(newFormat->lcExtension, lcExtension, sizeof(newFormat->lcExtension));
        newFormat->ucExtension[0] = '.';
        rwstrncpy(&newFormat->ucExtension[1], extension, 18);
        newFormat->ucExtension[19] = '\0';
        rwstrupr(newFormat->ucExtension);
        newFormat->readImage = NULL;
        newFormat->writeImage = NULL;
        newFormat->nextFormat = RWIMAGEGLOBAL(imageFormatList);
        RWIMAGEGLOBAL(imageFormatList) = newFormat;
    }

    if (imageRead) {
        newFormat->readImage = imageRead;
    }
    if (imageWrite) {
        newFormat->writeImage = imageWrite;
    }

    return TRUE;
} // 0x802961C8

RwImage* RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname) {
    RwImage* image;

    image = RwImageRead(imageName);
    if (image) {
        if (maskname && *maskname) {
            RwImage* mask;

            mask = RwImageRead(maskname);
            if (!mask) {
                RwImageDestroy(image);
                return NULL;
            }

            if (!RwImageMakeMask(mask)) {
                RwImageDestroy(image);
                RwImageDestroy(mask);
                return NULL;
            }

            if (!RwImageApplyMask(image, mask)) {
                RwImageDestroy(image);
                RwImageDestroy(mask);
                return NULL;
            }

            RwImageDestroy(mask);
        }

        return image;
    }

    return NULL;
} // 0x80296364

RwRGBA* RwRGBASetFromPixel(RwRGBA* rgbOut, RwUInt32 pixelValue, RwInt32 rasterFormat) {
    RwUInt32 nInPixel = pixelValue;

    RWSRCGLOBAL(stdFunc[rwSTANDARDPIXELTORGB])((void*)rgbOut, (void*)&nInPixel, rasterFormat);

    return rgbOut;
} // 0x80296668

static RwBool ImageStraightCopy(RwImage* ipDestin, const RwImage* ipSource) {
    RwInt32 i;
    RwInt32 nSpanLength;
    RwUInt8* cpSrc;
    RwUInt8* cpDst;

    if (ipDestin->palette && ipSource->palette && ipSource->depth <= 8) {
        memcpy(ipDestin->palette, ipSource->palette,
               (1 << ipSource->depth) * sizeof(RwRGBA));
    }

    nSpanLength = ((ipDestin->depth + 7) >> 3) * ipDestin->width;
    cpSrc = ipSource->cpPixels;
    cpDst = ipDestin->cpPixels;

    for (i = 0; i < ipDestin->height; i++) {
        memcpy(cpDst, cpSrc, nSpanLength);
        cpDst += ipDestin->stride;
        cpSrc += ipSource->stride;
    }

    return TRUE;
} // no copy in retail (inlined): a static in the Gormiti Debug map (baimage.obj, 0x19C bytes), absent from the Final map

// Gormiti name: ImageConvertDepth (a static there; same position in baimage.obj; the symbol keeps its placeholder
// because sibling.exe pairs it with no counterpart).
RwBool fn_802966AC(RwImage* ipDestin, const RwImage* ipSource) {
    RwBool result = FALSE;
    RwInt32 i, j;
    RwInt32 width = ipDestin->width;
    RwInt32 height = ipDestin->height;
    RwUInt8* cpDst = ipDestin->cpPixels;
    RwRGBA* rpSrcPalette = ipSource->palette;
    RwUInt8* cpSrc = ipSource->cpPixels;

    switch ((RwUInt32)((ipSource->depth << 8) | ipDestin->depth)) {
        case (4 << 8) | 4:
        case (8 << 8) | 8:
        case (32 << 8) | 32:
            result = TRUE;
            break;

        case (4 << 8) | 8:
            for (i = 0; i < height; i++) {
                memcpy(cpDst, cpSrc, width);
                cpSrc += ipSource->stride;
                cpDst += ipDestin->stride;
            }
            result = TRUE;
            break;

        case (4 << 8) | 32:
        case (8 << 8) | 32:
            for (i = 0; i < height; i++) {
                RwRGBA* rpDst = (RwRGBA*)cpDst;
                for (j = 0; j < width; j++) {
                    rpDst[j] = rpSrcPalette[cpSrc[j]];
                }
                cpSrc += ipSource->stride;
                cpDst += ipDestin->stride;
            }
            result = TRUE;
            break;

        default:
            RWERROR((E_RW_INVIMAGEFORMAT));
            break;
    }

    return result;
} // 0x802966AC

RwImage* RwImageCopy(RwImage* destImage, const RwImage* sourceImage) {
    if (destImage->depth == sourceImage->depth) {
        ImageStraightCopy(destImage, sourceImage);
    } else if (!fn_802966AC(destImage, sourceImage)) {
        destImage = NULL;
    }

    destImage->flags &= ~rwIMAGEGAMMACORRECTED;
    destImage->flags |= sourceImage->flags & rwIMAGEGAMMACORRECTED;

    return destImage;
} // 0x802968EC

RwImage* RwImageGammaCorrect(RwImage* image) {
    switch (image->depth) {
        case 4:
        case 8: {
            RwRGBA* pal = image->palette;
            RwUInt32 palSize = 1 << image->depth;

            if (!pal) {
                RWERROR((E_RW_NULLP));
                return NULL;
            }

            _rwImageGammaCorrectArrayOfRGBA(pal, pal, palSize);
            break;
        }
        case 32: {
            RwUInt8* curLine = image->cpPixels;
            RwInt32 width = image->width;
            RwInt32 height = image->height;
            RwInt32 y;

            if (!curLine) {
                RWERROR((E_RW_NULLP));
                return NULL;
            }

            for (y = 0; y < height; y++) {
                _rwImageGammaCorrectArrayOfRGBA((RwRGBA*)curLine, (RwRGBA*)curLine, width);
                curLine += image->stride;
            }
            break;
        }
        default:
            RWERROR((E_RW_INVIMAGEDEPTH));
            return NULL;
    }

    image->flags |= rwIMAGEGAMMACORRECTED;

    return image;
} // 0x802969D8

RwBool RwImageSetGamma(RwReal gammaValue) {
    RwReal nGammaExp = gammaValue;
    RwReal nGammaInv = (RwReal)1.0 / gammaValue;
    RwInt32 i;

    RWIMAGEGLOBAL(gamma) = gammaValue;

    RWIMAGEGLOBAL(gammaTable)[0] = 0;
    RWIMAGEGLOBAL(invGammaTable)[0] = 0;

    for (i = 1; i < 256; i++) {
        RwReal nT = (RwReal)i / (RwReal)255.0;
        RwReal scaled;
        RwInt32 quantize;

        scaled = (RwReal)255.0 * (RwReal)pow(nT, nGammaInv) + (RwReal)0.5;
        quantize = RwInt32FromRealMacro(scaled);
        RWIMAGEGLOBAL(gammaTable)[i] = (RwUInt8)quantize;

        scaled = (RwReal)255.0 * (RwReal)pow(nT, nGammaExp) + (RwReal)0.5;
        quantize = RwInt32FromRealMacro(scaled);
        RWIMAGEGLOBAL(invGammaTable)[i] = (RwUInt8)quantize;
    }

    return TRUE;
} // 0x80296D44
