// Unit name: Gormiti link map (RwCore.a dltexdic, driver/wii/dltexdic.c)
#ifndef RWSDK_DRIVER_WII_DLTEXDIC_H
#define RWSDK_DRIVER_WII_DLTEXDIC_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The native texture chunk of a texture dictionary: size in bytes, write and read. */
RwBool _rwDlNativeTextureGetSize(void* sizeIn, void* textureIn);
RwBool _rwDlNativeTextureWrite(void* streamIn, void* textureIn);
RwBool _rwDlNativeTextureRead(void* streamIn, void* textureIn);

#ifdef __cplusplus
}
#endif

#endif
