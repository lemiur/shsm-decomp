// Layout and names: Gormiti DWARF (RpPvs.a rppvs.obj)
#ifndef RWSDK_PLUGIN_PVS_RPPVS_H
#define RWSDK_PLUGIN_PVS_RPPVS_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/babbox.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/world/baworld.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Partitioning classification for polygons and sectors against a plane. */
typedef enum _rpPVSPartitionId {
    rpNAPVSPARTITIONID = 0,
    rpPVSFRONT = 1,
    rpPVSBACK = 2,
    rpPVSSPLIT = 3,
    rpPVSCOPLANAR = 4,
    rpPVSPARTITIONIDFORCEENUMSIZEINT = 0x7FFFFFFF
} _rpPVSPartitionId;

/* Plane equation used by PVS polygon clipping and shadow volumes. */
typedef struct _rpPVSPlaneEq {
    RwReal x;                       /* 0x00 */
    RwReal y;                       /* 0x04 */
    RwReal z;                       /* 0x08 */
    RwReal w;                       /* 0x0C */
    RwReal l;                       /* 0x10 */
    _rpPVSPartitionId lastresult;   /* 0x14 */
} _rpPVSPlaneEq; /* size 0x18 */

typedef struct _rpPVSPoly _rpPVSPoly;
struct _rpPVSPoly {
    RwV3d v;                    /* 0x00 */
    _rpPVSPoly* next;           /* 0x0C */
    RwInt32 pscalar;            /* 0x10 */
    RwReal scalar;              /* 0x14 */
    _rpPVSPlaneEq shadowPlane;  /* 0x18 */
}; /* size 0x30 */

typedef struct _rpPVSPolyList _rpPVSPolyList;

typedef struct _rpPVSPolyRecord {
    RwInt32 original;           /* 0x00 */
    RwReal priority;            /* 0x04 */
    _rpPVSPolyList* parent;     /* 0x08 */
    _rpPVSPoly* geom;           /* 0x0C */
    _rpPVSPlaneEq plane;        /* 0x10 */
    RwInt32 home;               /* 0x28 */
    RpWorldSector* homeaddr;    /* 0x2C */
    RwInt32 translucent;        /* 0x30 */
    RwInt32 hasbeenclipper;     /* 0x34 */
    RwV3d centroid;             /* 0x38 */
    RwReal radius;              /* 0x44 */
    RwV3d extreme;              /* 0x48 */
    RwReal coneRadius;          /* 0x54 */
} _rpPVSPolyRecord; /* size 0x58 */

struct _rpPVSPolyList {
    _rpPVSPolyRecord data;      /* 0x00 */
    _rpPVSPolyList* next;       /* 0x58 */
}; /* size 0x5C */

/* Per-sector PVS plugin extension. */
typedef struct RpPVS {
    RwInt32 sectorID;               /* 0x00 */
    RwInt32 vismaplength;           /* 0x04 */
    RwInt32 sampleKey;              /* 0x08 */
    RwUInt8* vismap;                /* 0x0C */
    _rpPVSPolyList* sectailpoly;    /* 0x10 */
    _rpPVSPartitionId potential;    /* 0x14 */
    RwUInt32 numpols;               /* 0x18 */
    RwBBox sbox;                    /* 0x1C */
    RwBBox gbox;                    /* 0x34 */
    RwReal diagonal;                /* 0x4C */
    RwV3d centre;                   /* 0x50 */
    RwInt32 axessig[3];             /* 0x5C */
} RpPVS; /* size 0x68 */

/* Per-world PVS plugin extension. */
typedef struct RpPVSCache {
    RwInt32 processed;                              /* 0x00 */
    RwInt32 formatted;                              /* 0x04 */
    RwInt32 NumWorldSectors;                        /* 0x08 */
    RwInt32 ptotal;                                 /* 0x0C */
    RwInt32 paccept;                                /* 0x10 */
    RwInt32 hooked;                                 /* 0x14 */
    RwUInt32 nextID;                                /* 0x18 */
    RwInt32 viscount;                               /* 0x1C */
    RwInt32 (*progressCallBack)(RwInt32, RwReal);   /* 0x20 */
    _rpPVSPolyList* polygons;                       /* 0x24 */
    RpWorldSectorCallBackRender renderCallBack;     /* 0x28 */
} RpPVSCache; /* size 0x2C */

/* PVS plugin global state. */
typedef struct RpPVSGlobalVars {
    RpWorld* World;             /* 0x00 */
    RwInt32 worldOffset;        /* 0x04 */
    RwInt32 sectorOffset;       /* 0x08 */
    RwInt32 collis;             /* 0x0C */
    RwInt32 bfc;                /* 0x10 */
    RwInt32 progress_count;     /* 0x14 */
    RwReal diagonal;            /* 0x18 */
    RwReal gran;                /* 0x1C */
    RwInt32 InSector;           /* 0x20 */
    RwV3d ViewPos;              /* 0x24 */
    RpPVS* CurrPVS;             /* 0x30 */
} RpPVSGlobalVars; /* size 0x34 */

extern RpPVSGlobalVars lbl_80517780;

RpWorldSector* RpPVSSetViewPosition(RpWorld* wpWorld, RwV3d* pos);
RwBool RpPVSWorldSectorVisible(RpWorldSector* spSect);
RpWorld* RpPVSHook(RpWorld* wpWorld);
RpWorld* RpPVSUnhook(RpWorld* wpWorld);
RwBool RpPVSPluginAttach(void);

void _rpPVSDestroyPolyList(_rpPVSPolyList** P);
void _rpPVSDestroyPoly(_rpPVSPoly** P);
RwInt32 _rpPVSWorldGetNumWorldSectors(RpWorld* world);

#ifdef __cplusplus
}
#endif

#endif
