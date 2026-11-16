#ifndef RWSDK_PLUGIN_COLLIS_COLLDATA_H
#define RWSDK_PLUGIN_COLLIS_COLLDATA_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/babbox.h>
#include <rwsdk/plugin/collis/ctdata.h>

/* The library version from which a collision chunk is a version word and a collision tree chunk; older chunks are
   converted (name invented, value from retail's version check). */
#define rwCOLLTREEVERSION 0x36001

/* The collision plugin's data, attached to atomics, geometries and world sectors. */
typedef struct RpCollisionData {
    RpCollTree *tree;   /* 0x00 */
} RpCollisionData;

/* Plugin registration (the collision plugin's public entry point). */
RwBool RpCollisionPluginAttach(void);

/* The open counter and the plugin offsets (engine, atomic, geometry and world sector), defined in colldata.c. */
extern RwInt32 _rpCollisionNumInstances;
extern RwInt32 _rpCollisionGlobalRefOffset;
extern RwInt32 _rpCollisionAtomicRefOffset;
extern RwInt32 _rpCollisionGeometryDataOffset;
extern RwInt32 _rpCollisionWorldSectorDataOffset;

#endif
