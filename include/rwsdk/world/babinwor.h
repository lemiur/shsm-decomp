// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
#ifndef RWSDK_WORLD_BABINWOR_H
#define RWSDK_WORLD_BABINWOR_H

#include <rwsdk/plcore/bastream.h>
#include <rwsdk/world/baworld.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpWorldSectorChunkInfo: a world sector's structure chunk: its first material in the world's
 * list, its triangle and vertex counts, its tight bounding box, and two flags kept for the
 * structure's size.
 */
typedef struct RpWorldChunkInfoSector RpWorldSectorChunkInfo;

struct RpWorldChunkInfoSector {
    RwInt32 matListWindowBase;  /* 0x0 */
    RwInt32 numTriangles;       /* 0x4 */
    RwInt32 numVertices;        /* 0x8 */
    RwV3d inf;                  /* 0xC */
    RwV3d sup;                  /* 0x18 */
    RwBool collSectorPresent;   /* 0x24: unused */
    RwBool unused;              /* 0x28 */
};

/*
 * RpPlaneSectorChunkInfo: a plane sector's structure chunk: its plane, the kinds of its subtrees
 * and the plane values at the extents of its two sides.
 */
typedef struct RpPlaneSectorChunkInfo {
    RwInt32 type;               /* 0x0 */
    RwReal value;               /* 0x4 */
    RwBool leftIsWorldSector;   /* 0x8 */
    RwBool rightIsWorldSector;  /* 0xC */
    RwReal leftValue;           /* 0x10 */
    RwReal rightValue;          /* 0x14 */
} RpPlaneSectorChunkInfo;

/*
 * RpWorldChunkInfo: a world's structure chunk: the kind of its root sector, its origin
 * (negated), how many triangles, vertices, plane sectors and world sectors it has, the size of
 * the collision data of old files (unsupported), its flags and its bounding box.
 */
typedef struct RpWorldChunkInfo {
    RwBool rootIsWorldSector;   /* 0x0 */
    RwV3d invWorldOrigin;       /* 0x4 */
    RwInt32 numTriangles;       /* 0x10 */
    RwInt32 numVertices;        /* 0x14 */
    RwInt32 numPlaneSectors;    /* 0x18 */
    RwInt32 numWorldSectors;    /* 0x1C */
    RwInt32 colSectorSize;      /* 0x20 */
    RwInt32 format;             /* 0x24: the world's flags */
    RwBBox boundingBox;         /* 0x28 */
} RpWorldChunkInfo;

/*
 * The world and world sector rights stream callbacks (rwID_RIGHTTORENDER), registered by
 * _rpWorldObjRegisterExtensions.
 */
RwStream* _rpReadWorldRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                             RwInt32 sizeInObject);
RwStream* _rpWriteWorldRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                              RwInt32 offsetInObject, RwInt32 sizeInObject);
RwInt32 _rpSizeWorldRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
RwStream* _rpReadSectRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                            RwInt32 sizeInObject);
RwStream* _rpWriteSectRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                             RwInt32 offsetInObject, RwInt32 sizeInObject);
RwInt32 _rpSizeSectRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

RpWorld* RpWorldStreamRead(RwStream* stream);
void* _rpBinaryWorldClose(void* instance, RwInt32 offset, RwInt32 size);
void* _rpBinaryWorldOpen(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
