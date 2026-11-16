// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/tpl/TPL.c
// Reference: ThePlayerRolo/sdk_2009-12-11 source/tpl/TPL.c
//
// Texture palette (.tpl) files: binding a palette loaded into memory turns its file offsets into pointers.
//
// Functions whose closing comment says "no copy in retail" have no out-of-line copy in the binary (removed by the
// linker). They keep the references' bodies, as SDK C units do (AGENTS.md, "Dead-stripped functions").
#include <revolution/gx.h>
#include <revolution/os.h>
#include <revolution/tpl.h>

/* Version stamp of the TPL file format. */
#define TPL_VERSION 2142000

/*
 * TPLBind
 *
 * Converts the palette's descriptor, header and image offsets (relative to the start of the file) into pointers.
 * Headers already converted are marked unpacked and keep their data pointer.
 */
void TPLBind(TPLPalette* pal)
{
    u16 i;

    if (pal->versionNumber != TPL_VERSION) {
        OSPanic(__FILE__, 25, "invalid version number for texture palette");
    }

    pal->descriptorArray = (TPLDescriptor*)((u32)pal->descriptorArray + (u32)pal);

    for (i = 0; i < pal->numDescriptors; i++) {
        if (pal->descriptorArray[i].textureHeader) {
            pal->descriptorArray[i].textureHeader =
                (TPLHeader*)((u32)pal + (u32)pal->descriptorArray[i].textureHeader);

            if (!pal->descriptorArray[i].textureHeader->unpacked) {
                pal->descriptorArray[i].textureHeader->data =
                    (char*)((u32)pal + (u32)pal->descriptorArray[i].textureHeader->data);
                pal->descriptorArray[i].textureHeader->unpacked = TRUE;
            }
        }

        if (pal->descriptorArray[i].CLUTHeader) {
            pal->descriptorArray[i].CLUTHeader =
                (TPLClutHeader*)((u32)pal + (u32)pal->descriptorArray[i].CLUTHeader);

            if (!pal->descriptorArray[i].CLUTHeader->unpacked) {
                pal->descriptorArray[i].CLUTHeader->data =
                    (char*)((u32)pal + (u32)pal->descriptorArray[i].CLUTHeader->data);
                pal->descriptorArray[i].CLUTHeader->unpacked = TRUE;
            }
        }
    }
} // 0x80277F60

/*
 * TPLGet
 *
 * Returns descriptor id of a bound palette (wrapped to the palette's size).
 */
TPLDescriptor* TPLGet(TPLPalette* pal, u32 id)
{
    id %= pal->numDescriptors;
    return &pal->descriptorArray[id];
} // 0x80278080

/*
 * TPLGetGXTexObjFromPalette
 *
 * Initialises a texture object from descriptor id of a bound palette.
 */
void TPLGetGXTexObjFromPalette(TPLPalette* pal, GXTexObj* texObj, u32 id)
{
    TPLDescriptor* desc = TPLGet(pal, id);
    GXBool mipMap;

    if (desc->textureHeader->minLOD == desc->textureHeader->maxLOD) {
        mipMap = GX_FALSE;
    } else {
        mipMap = GX_TRUE;
    }

    GXInitTexObj(texObj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                 (GXTexFmt)desc->textureHeader->format, desc->textureHeader->wrapS, desc->textureHeader->wrapT, mipMap);
    GXInitTexObjLOD(texObj, desc->textureHeader->minFilter, desc->textureHeader->magFilter, desc->textureHeader->minLOD,
                    desc->textureHeader->maxLOD, desc->textureHeader->LODBias, GX_FALSE,
                    desc->textureHeader->edgeLODEnable, GX_ANISO_1);
} // no copy in retail (dead-stripped)

/*
 * TPLGetGXTexObjFromPaletteCI
 *
 * Initialises a colour-indexed texture object and its lookup table from descriptor id of a bound palette.
 */
void TPLGetGXTexObjFromPaletteCI(TPLPalette* pal, GXTexObj* texObj, GXTlutObj* tlutObj, u32 tlutName, u32 id)
{
    TPLDescriptor* desc = TPLGet(pal, id);
    GXBool mipMap;

    if (desc->textureHeader->minLOD == desc->textureHeader->maxLOD) {
        mipMap = GX_FALSE;
    } else {
        mipMap = GX_TRUE;
    }

    GXInitTlutObj(tlutObj, desc->CLUTHeader->data, desc->CLUTHeader->format, desc->CLUTHeader->numEntries);
    GXInitTexObjCI(texObj, desc->textureHeader->data, desc->textureHeader->width, desc->textureHeader->height,
                   (GXCITexFmt)desc->textureHeader->format, desc->textureHeader->wrapS, desc->textureHeader->wrapT,
                   mipMap, tlutName);
    GXInitTexObjLOD(texObj, desc->textureHeader->minFilter, desc->textureHeader->magFilter, desc->textureHeader->minLOD,
                    desc->textureHeader->maxLOD, desc->textureHeader->LODBias, GX_FALSE,
                    desc->textureHeader->edgeLODEnable, GX_ANISO_1);
} // no copy in retail (dead-stripped)
