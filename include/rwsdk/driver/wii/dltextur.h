// Unit name: Gormiti link map (RwCore.a dltextur); only the declarations dltexdic.c and climaxplatform.c need
// Layout: Gormiti DWARF (_rwDlTextureExt)
// Name and signature of _rwDlTextureSet: Gormiti link map (RwCore.a dltextur, driver/wii/dltextur.c) and DWARF
#ifndef RWSDK_DRIVER_WII_DLTEXTUR_H
#define RWSDK_DRIVER_WII_DLTEXTUR_H

#include <revolution/gx/GXStruct.h>
#include <rwsdk/batextur.h>
#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The fields of _rwDlTextureExt.flags. Name invented. */
#define rwDLTEXTUREEXTNOTEXOBJ 0x01000000 /* the GX texture object has no sampler state of its own */

/* The driver's data in a texture: its GX texture object. */
typedef struct _rwDlTextureExt {
    GXTexObj texObj;     /* 0x00 */
    RwUInt32 flags;      /* 0x20: rwDLTEXTUREEXT* */
} _rwDlTextureExt;

/* Where the extension sits in a texture. */
extern RwInt32 _RwGameCubeTextureExtOffset;

#define TEXTUREEXTFROMTEXTURE(_tex) \
    ((_rwDlTextureExt*)(((RwUInt8*)(_tex)) + _RwGameCubeTextureExtOffset))

/* Sets the texture's LOD bias, bias clamp, edge LOD and maximum anisotropy. */
void RwGameCubeTextureSetLOD(RwTexture* texture, RwReal lodBias, RwUInt32 biasClamp,
                             RwUInt32 edgeLod, RwUInt32 maxAniso);

/* Loads a texture into a GX texture map: the raster's texture object goes to the map the index names. */
extern void _rwDlTextureSet(RwTexture* texture, RwInt32 index);

#ifdef __cplusplus
}
#endif

#endif
