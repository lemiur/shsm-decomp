// Header name invented: the skin plugin's private macros, shared by its units (Gormiti's skin2 headers
// are not recorded in its DWARF).
#ifndef RWSDK_PLUGIN_SKIN2_SKIN_H
#define RWSDK_PLUGIN_SKIN2_SKIN_H

#include <rwsdk/plugin/skin2/rpskin.h>

/* The skin of a geometry, and the skin data of an atomic. */
#define RPSKINGEOMETRYGETDATA(geometry) \
    (*RWPLUGINOFFSET(RpSkin*, (geometry), _rpSkinGlobals.geometryOffset))
#define RPSKINATOMICGETDATA(atomic) \
    (RWPLUGINOFFSET(SkinAtomicData, (atomic), _rpSkinGlobals.atomicOffset))

/*
 * The vertex arrays of a skin res entry start on a 32 byte boundary a little way after its header
 * (the GX pipe reads them by DMA); the word before them points back to the entry, whose vertex
 * buffer header holds the draw sync token of the last draw.
 */
#define SKINRESENTRYGETARRAYBASE(resEntry) ((void*)(((unsigned int)(resEntry) + 0x3D) & ~0x1F))
#define SKINRESENTRYGETVBHEADER(resEntry) \
    ((RxGameCubeVertexBuffer*)(((RwResEntry**)SKINRESENTRYGETARRAYBASE(resEntry))[-1] + 1))

#endif
