// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
#ifndef RWSDK_WORLD_BAWOROBJ_H
#define RWSDK_WORLD_BAWOROBJ_H

#include <rwsdk/bacamera.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/world/baworld.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpTie: ties an atomic to a world sector it lies in: linked into the sector's atomic list and
 * into the atomic's llWorldSectorsInAtomic. Size 0x18, from the free list WorldObjectOpen creates
 * and _rpTieDestroy's unlinking. The SDK declares it with RpAtomic (baclump.h).
 */
typedef struct RpTie {
    RwLLLink lAtomicInWorldSector;  /* 0x0 */
    RpAtomic* apAtom;               /* 0x8 */
    RwLLLink lWorldSectorInAtomic;  /* 0xC */
    RpWorldSector* worldSector;     /* 0x14 */
} RpTie;

/*
 * RpLightTie: ties a light to a world sector it lights: linked into the sector's light list and
 * into the light's WorldSectorsInLight. Size 0x18, from the free list WorldObjectOpen creates and
 * _rpLightTieDestroy's unlinking. The SDK declares it with RpLight (balight.h).
 */
typedef struct RpLightTie {
    RwLLLink lightInWorldSector;    /* 0x0 */
    RpLight* light;                 /* 0x8 */
    RwLLLink WorldSectorInLight;    /* 0xC */
    RpWorldSector* sect;            /* 0x14 */
} RpLightTie;

/*
 * The world's error codes (plugin ID 2); only the ones the world units raise are known. Enum and member
 * names invented, in the SDK's E_RP_ form.
 */
typedef enum RpWorldErrorCode {
    E_RP_WORLD_CLUMPNOTINWORLD = 4, /* RpWorldRemoveClump: the clump is in no world */
    E_RP_WORLD_ATOMICNOFRAME = 5,   /* RpClumpClone: an atomic of the clump has no frame */
    E_RP_WORLD_TOOMANYVERTICES = 6, /* RpGeometryCreate: more vertices than a triangle's 16-bit index reaches */
    rpWORLDERRORCODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RpWorldErrorCode;

/*
 * rpWorldObjCameraExt: what the world adds to a camera: the world sectors in its frustum at the
 * last sync (a list that grows as needed), the world it is in, and the camera functions it hooks.
 * Size 0x1C (_rpWorldObjRegisterExtensions). Type and field names invented.
 */
typedef struct rpWorldObjCameraExt {
    RpWorldSector** frustumSectors;             /* 0x0: allocated with RwMalloc, grown by 50 entries */
    RwInt32 space;                              /* 0x4: entries allocated */
    RwInt32 position;                           /* 0x8: entries in use */
    RpWorld* world;                             /* 0xC */
    RwCameraBeginUpdateFunc oldBeginUpdate;     /* 0x10 */
    RwCameraEndUpdateFunc oldEndUpdate;         /* 0x14 */
    RwObjectHasFrameSyncFunction oldSync;       /* 0x18 */
} rpWorldObjCameraExt;

/*
 * rpWorldObjAtomicExt, rpWorldObjLightExt: the world an atomic or a light is in, and the frame
 * sync function the world hooks. Size 8 each (_rpWorldObjRegisterExtensions). Type and field
 * names invented.
 */
typedef struct rpWorldObjAtomicExt {
    RpWorld* world;                         /* 0x0 */
    RwObjectHasFrameSyncFunction oldSync;   /* 0x4 */
} rpWorldObjAtomicExt;

typedef struct rpWorldObjLightExt {
    RpWorld* world;                         /* 0x0 */
    RwObjectHasFrameSyncFunction oldSync;   /* 0x4 */
} rpWorldObjLightExt;

/*
 * rpWorldObjClumpExt: the world a clump is in, and a copy of the world object module's field_0x8
 * taken when the clump is created or added to a world (no reader in retail). Size 8
 * (_rpWorldObjRegisterExtensions). Type and field names invented.
 */
typedef struct rpWorldObjClumpExt {
    RpWorld* world;     /* 0x0 */
    RwInt32 field_0x4;  /* 0x4 */
} rpWorldObjClumpExt;

/*
 * The geometry and world sector native data stream functions (rwID_NATIVEDATAPLUGIN) of the
 * platform driver, registered by _rpWorldObjRegisterExtensions; their unit is not split yet.
 * Names from bfbbdecomp/bfbb config/GQPE78/symbols.txt (same order and sizes).
 */
RwInt32 _rpGeometryNativeSize(const RpGeometry* geometry);
RwStream* _rpGeometryNativeWrite(RwStream* stream, const RpGeometry* geometry);
RpGeometry* _rpGeometryNativeRead(RwStream* stream, RpGeometry* geometry);
RwInt32 _rpWorldSectorNativeSize(const RpWorldSector* sector);
RwStream* _rpWorldSectorNativeWrite(RwStream* stream, const RpWorldSector* sector);
RpWorldSector* _rpWorldSectorNativeRead(RwStream* stream, RpWorldSector* sector);

/*
 * Attaches the world's pipelines (returns TRUE); its unit is not split yet. Name from
 * bfbbdecomp/bfbb config/GQPE78/symbols.txt (after the default pipeline setters and the world
 * pipeline open/close, as here).
 */
RwBool _rpWorldPipeAttach(void);

void RpLightTieSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);
RwBool _rpLightTieDestroy(RpLightTie* tie);
RwBool _rpTieDestroy(RpTie* tie);
RwBool _rpWorldObjRegisterExtensions(void);
RpWorld* RpWorldAddCamera(RpWorld* world, RwCamera* camera);
RpWorld* RpWorldRemoveCamera(RpWorld* world, RwCamera* camera);
RpWorld* RwCameraGetWorld(const RwCamera* camera);
RpWorld* RpWorldAddAtomic(RpWorld* world, RpAtomic* atomic);
RpWorld* RpWorldRemoveAtomic(RpWorld* world, RpAtomic* atomic);
RpWorld* RpAtomicGetWorld(const RpAtomic* atomic);
RpWorld* RpWorldAddClump(RpWorld* world, RpClump* clump);
RpWorld* RpWorldRemoveClump(RpWorld* world, RpClump* clump);
RwCamera* RwCameraForAllSectorsInFrustum(RwCamera* camera, RpWorldSectorCallBack callBack, void* pData);
RpWorld* RpClumpGetWorld(const RpClump* clump);
RpWorld* RpLightGetWorld(const RpLight* light);
RpWorld* RpWorldAddLight(RpWorld* world, RpLight* light);
RpWorld* RpWorldRemoveLight(RpWorld* world, RpLight* light);

#ifdef __cplusplus
}
#endif

#endif
