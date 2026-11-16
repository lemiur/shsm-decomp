// Unit name and boundary: Gormiti link map (RwCore.a dltexdic)
// Names, parameters and locals: Gormiti DWARF (dltexdic.c); layouts: Gormiti DWARF
/*
 * dltexdic.c: a texture in a texture dictionary, in the driver's native format. The chunk holds
 * the texture's structure (the sampler state, name and mask name), then the raster's (its size
 * and format, as GX wants them), then the palette, if there is one, and the pixels.
 *
 * RULE-EXCEPTION: R26, the casts of the void* parameters stay at the point of use. Gormiti's DWARF
 * lists no local for them, and a typed local for the stream or the texture moves registers.
 */
#include <revolution/gx/GXGet.h>
#include <revolution/gx/GXTexture.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batextur.h>
#include <rwsdk/driver/wii/dlraster.h>
#include <rwsdk/driver/wii/dltexdic.h>
#include <rwsdk/driver/wii/dltextur.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bastream.h>
#include <string.h>

/* The platform ID that starts a native texture's structure. */
#define rwID_PLATFORM_GCN 6

/* The texture's structure in the chunk. Names invented, layout and members from Gormiti DWARF. */
typedef struct _rwDlNativeTexture {
    int id;                 /* 0x00: rwID_PLATFORM_GCN */
    int filterAndAddress;   /* 0x04: the low half of the texture's filterAddressing */
    unsigned int maxAniso;  /* 0x08 */
    int biasClamp;          /* 0x0C */
    int edgeLod;            /* 0x10 */
    float lodBias;          /* 0x14 */
    char name[32];          /* 0x18 */
    char mask[32];          /* 0x38 */
} _rwDlNativeTexture;

/* The raster's structure in the chunk. */
typedef struct _rwDlNativeRaster {
    int formatType;                /* 0x00: type in the low byte, format above it */
    unsigned short width;          /* 0x04 */
    unsigned short height;         /* 0x06 */
    unsigned char depth;           /* 0x08 */
    unsigned char numMipLevels;    /* 0x09 */
    unsigned char format;          /* 0x0A: GXTexFmt */
    unsigned char tlutFormat;      /* 0x0B: GXTlutFmt */
    int alpha;                     /* 0x0C */
} _rwDlNativeRaster;

/* The size of a raster's palette: one 16 bit entry for each colour. */
#define PALETTESIZE(_raster) ((1 << (_raster)->depth) * 2)

/* The format bits of a raster that has a palette. */
#define PALETTEFORMATS (rwRASTERFORMATPAL8 | rwRASTERFORMATPAL4)

/*
 * The size of the native chunk in the stream: the chunk header, the two structures, the palette,
 * and the pixels behind their length. A texture with no raster is the header and its structure.
 */
RwBool _rwDlNativeTextureGetSize(void* sizeIn, void* textureIn) {
    unsigned int size;
    RwRaster* raster;

    size = rwCHUNKHEADERSIZE + sizeof(_rwDlNativeTexture);
    raster = RwTextureGetRaster((RwTexture*)textureIn);
    if (raster == NULL) {
        *(unsigned int*)sizeIn = size;
        return 1;
    }

    size += sizeof(_rwDlNativeRaster);
    if (RwRasterGetFormat(raster) & PALETTEFORMATS) {
        size += PALETTESIZE(raster);
    }
    size += sizeof(unsigned int);
    size += _rwDlRasterGetSize(raster);

    *(unsigned int*)sizeIn = size;
    return 1;
} // 0x802B2CB4

/*
 * Writes the native chunk of a texture: its sampler state (defaults when the texture's GX object
 * has none), then the raster's structure, palette and pixels. Returns 0 at the first write that
 * fails.
 */
RwBool _rwDlNativeTextureWrite(void* streamIn, void* textureIn) {
    _rwDlNativeTexture nativeTexture;
    _rwDlNativeRaster nativeRaster;
    unsigned int size;
    int bytesLeftToWrite;
    RwRaster* raster;
    RwGameCubeRasterExtension* rasExt;
    _rwDlTextureExt* texExt;

    _rwDlNativeTextureGetSize(&bytesLeftToWrite, textureIn);
    bytesLeftToWrite -= rwCHUNKHEADERSIZE;
    if (!RwStreamWriteChunkHeader((RwStream*)streamIn, rwID_STRUCT, bytesLeftToWrite)) {
        return 0;
    }

    nativeTexture.id = rwID_PLATFORM_GCN;
    nativeTexture.filterAndAddress =
        ((RwTexture*)textureIn)->filterAddressing & (rwTEXTUREFILTERMODEMASK | rwTEXTUREADDRESSINGMASK);
    texExt = TEXTUREEXTFROMTEXTURE(textureIn);
    if (texExt->flags & rwDLTEXTUREEXTNOTEXOBJ) {
        nativeTexture.maxAniso = 0;
        nativeTexture.biasClamp = 1;
        nativeTexture.edgeLod = 1;
        nativeTexture.lodBias = 0.0f;
    } else {
        nativeTexture.maxAniso = GXGetTexObjMaxAniso(&texExt->texObj);
        nativeTexture.biasClamp = GXGetTexObjBiasClamp(&texExt->texObj);
        nativeTexture.edgeLod = GXGetTexObjEdgeLOD(&texExt->texObj);
        nativeTexture.lodBias = GXGetTexObjLODBias(&texExt->texObj);
    }
    memcpy(nativeTexture.name, RwTextureGetName((RwTexture*)textureIn), sizeof(nativeTexture.name));
    memcpy(nativeTexture.mask, RwTextureGetMaskName((RwTexture*)textureIn), sizeof(nativeTexture.mask));
    if (!RwStreamWrite((RwStream*)streamIn, &nativeTexture, sizeof(nativeTexture))) {
        return 0;
    }
    bytesLeftToWrite -= sizeof(nativeTexture);

    raster = RwTextureGetRaster((RwTexture*)textureIn);
    rasExt = RASTEREXTFROMRASTER(RwRasterGetParent(raster));
    nativeRaster.formatType = raster->cType | RwRasterGetFormat(raster);
    nativeRaster.width = raster->width;
    nativeRaster.height = raster->height;
    nativeRaster.depth = raster->depth;
    nativeRaster.numMipLevels = RwRasterGetNumLevels(raster);
    nativeRaster.format = rasExt->format;
    nativeRaster.tlutFormat = rasExt->tlutFmt;
    nativeRaster.alpha = rasExt->flags & rwGAMECUBERASTERALPHA;
    if (!RwStreamWrite((RwStream*)streamIn, &nativeRaster, sizeof(nativeRaster))) {
        return 0;
    }
    bytesLeftToWrite -= sizeof(nativeRaster);

    if (RwRasterGetFormat(raster) & PALETTEFORMATS) {
        unsigned int paletteSize;

        paletteSize = PALETTESIZE(raster);
        if (!RwStreamWrite((RwStream*)streamIn, rasExt->palette, paletteSize)) {
            return 0;
        }
        bytesLeftToWrite -= paletteSize;
    }

    size = _rwDlRasterGetSize(raster);
    if (!RwStreamWrite((RwStream*)streamIn, &size, sizeof(size))) {
        return 0;
    }
    bytesLeftToWrite -= sizeof(size);
    if (!RwStreamWrite((RwStream*)streamIn, rasExt->pixels, size)) {
        return 0;
    }
    bytesLeftToWrite -= size;
    RWASSERT(bytesLeftToWrite == 0);

    return 1;
} // 0x802B2D3C

/*
 * Reads a native chunk: creates the raster without pixels, lets the driver allocate them, reads
 * the palette and pixels into the driver's buffers, and makes a texture of it. The mip map
 * generation flag is held back while the pixels are read. Returns the texture through textureIn.
 */
RwBool _rwDlNativeTextureRead(void* streamIn, void* textureIn) {
    _rwDlNativeTexture nativeTexture;
    _rwDlNativeRaster nativeRaster;
    unsigned int length, version;
    unsigned int autoMipmap;
    unsigned int size;
    RwTexture* texture;
    RwRaster* raster;
    RwGameCubeRasterExtension* rasExt;

    if (!RwStreamFindChunk((RwStream*)streamIn, rwID_STRUCT, &length, &version)) {
        return 0;
    }
    if (version < rwLIBRARYBASEVERSION || version > rwLIBRARYCURRENTVERSION) {
        return 0;
    }

    if (RwStreamRead((RwStream*)streamIn, &nativeTexture, sizeof(nativeTexture)) != sizeof(nativeTexture)) {
        return 0;
    }
    if (nativeTexture.id != rwID_PLATFORM_GCN) {
        return 0;
    }

    if (RwStreamRead((RwStream*)streamIn, &nativeRaster, sizeof(nativeRaster)) != sizeof(nativeRaster)) {
        return 0;
    }
    raster = RwRasterCreate(nativeRaster.width, nativeRaster.height, nativeRaster.depth,
                            nativeRaster.formatType | rwRASTERDONTALLOCATE);
    if (raster == NULL) {
        return 0;
    }

    rasExt = RASTEREXTFROMRASTER(raster);
    rasExt->format = nativeRaster.format;
    rasExt->tlutFmt = nativeRaster.tlutFormat;
    rasExt->flags = (nativeRaster.alpha != 0);
    if (!_rwDlTextureRasterCreate(raster, nativeRaster.numMipLevels)) {
        RwRasterDestroy(raster);
        return 0;
    }
    raster->cFlags &= ~rwRASTERDONTALLOCATE;

    if (RwRasterGetFormat(raster) & PALETTEFORMATS) {
        unsigned int paletteSize;

        paletteSize = PALETTESIZE(raster);
        if (RwStreamRead((RwStream*)streamIn, rasExt->palette, paletteSize) != paletteSize) {
            return 0;
        }
        DCFlushRange(rasExt->palette, PALETTESIZE(raster));
    }

    autoMipmap = raster->cFormat & (rwRASTERFORMATAUTOMIPMAP >> 8);
    raster->cFormat &= ~autoMipmap;
    if (RwStreamRead((RwStream*)streamIn, &size, sizeof(size)) != sizeof(size)) {
        return 0;
    }
    if (RwStreamRead((RwStream*)streamIn, rasExt->pixels, size) != size) {
        return 0;
    }
    DCFlushRange(rasExt->pixels, size);
    GXInvalidateTexAll();
    raster->cFormat |= autoMipmap;

    texture = RwTextureCreate(raster);
    if (texture == NULL) {
        RwRasterDestroy(raster);
        return 0;
    }
    RwTextureSetFilterMode(texture, nativeTexture.filterAndAddress);
    RwTextureSetAddressingU(texture, (nativeTexture.filterAndAddress & rwTEXTUREADDRESSINGUMASK) >> 8);
    RwTextureSetAddressingV(texture, (nativeTexture.filterAndAddress & rwTEXTUREADDRESSINGVMASK) >> 12);
    RwTextureSetName(texture, nativeTexture.name);
    RwTextureSetMaskName(texture, nativeTexture.mask);
    RwGameCubeTextureSetLOD(texture, nativeTexture.lodBias, nativeTexture.biasClamp,
                            nativeTexture.edgeLod, nativeTexture.maxAniso);

    *(RwTexture**)textureIn = texture;
    return 1;
} // 0x802B2FB4
