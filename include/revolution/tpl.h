#ifndef REVOLUTION_TPL_H
#define REVOLUTION_TPL_H

// Reference: ThePlayerRolo/sdk_2009-12-11 include/revolution/tpl.h

#include <revolution/types.h>
#include <revolution/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TPLHeader
 *
 * One texture of a TPL palette.
 */
typedef struct TPLHeader {
    u16 height;           // 0x00
    u16 width;            // 0x02
    u32 format;           // 0x04
    char* data;           // 0x08
    GXTexWrapMode wrapS;  // 0x0C
    GXTexWrapMode wrapT;  // 0x10
    GXTexFilter minFilter;  // 0x14
    GXTexFilter magFilter;  // 0x18
    f32 LODBias;          // 0x1C
    u8 edgeLODEnable;     // 0x20
    u8 minLOD;            // 0x21
    u8 maxLOD;            // 0x22
    u8 unpacked;          // 0x23
} TPLHeader; // size 0x24

/*
 * TPLClutHeader
 *
 * The colour lookup table of an indexed texture.
 */
typedef struct TPLClutHeader {
    u16 numEntries;   // 0x00
    u8 unpacked;      // 0x02
    u8 pad8;          // 0x03 // pad: SDK TPLClutHeader.pad8
    GXTlutFmt format; // 0x04
    char* data;       // 0x08
} TPLClutHeader; // size 0x0C

/*
 * TPLDescriptor
 *
 * A texture and its optional colour lookup table.
 */
typedef struct TPLDescriptor {
    TPLHeader* textureHeader;  // 0x00
    TPLClutHeader* CLUTHeader; // 0x04
} TPLDescriptor; // size 0x08

/*
 * TPLPalette
 *
 * A texture palette file as loaded into memory.
 */
typedef struct TPLPalette {
    u32 versionNumber;              // 0x00
    u32 numDescriptors;             // 0x04
    TPLDescriptor* descriptorArray; // 0x08
} TPLPalette; // size 0x0C

void TPLBind(TPLPalette* pal);
TPLDescriptor* TPLGet(TPLPalette* pal, u32 id);
void TPLGetGXTexObjFromPalette(TPLPalette* pal, GXTexObj* texObj, u32 id);
void TPLGetGXTexObjFromPaletteCI(TPLPalette* pal, GXTexObj* texObj, GXTlutObj* tlutObj, u32 tlutName, u32 id);

#ifdef __cplusplus
}
#endif

#endif
