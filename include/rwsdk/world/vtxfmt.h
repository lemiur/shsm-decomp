// Name: Gormiti link map (RPworld.a vtxfmt, gcn/vtxfmt.c)
// Layout and member names: Gormiti DWARF (RpGameCubeVtxFmt); the plugin offset names are the link map's
#ifndef RWSDK_WORLD_VTXFMT_H
#define RWSDK_WORLD_VTXFMT_H

#include <rwsdk/badevice.h>
#include <rwsdk/rwtypes.h>
#include <rwsdk/world/gcpipe.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpGameCubeVtxFmt: how a geometry's vertex data is packed in its instanced vertex buffer: the GX
 * format of each attribute and the number of fraction bits of the fixed point ones. A geometry
 * without its own format uses the default one. Size 0x18.
 */
typedef struct RpGameCubeVtxFmt {
    unsigned char pos;              /* 0x00 */
    unsigned char norm;             /* 0x01 */
    unsigned char texCoord[8];      /* 0x02 */
    unsigned char preLight;         /* 0x0A */
    unsigned char format;           /* 0x0B */
    unsigned char posFrac;          /* 0x0C */
    unsigned char nbt;              /* 0x0D */
    unsigned char texCoordFrac[8];  /* 0x0E */
    unsigned short refCnt;          /* 0x16 */
} RpGameCubeVtxFmt;

/* Where the vertex format plugin's data sits in a geometry (a pointer to its format, or NULL). */
extern RwInt32 _rpDlGeomVtxFmtOffset;

/* Where the vertex format plugin's data sits in a world (a pointer to its format, or NULL). */
extern RwInt32 _rpDlWorldVtxFmtOffset;

/* The vertex format a geometry has: the plugin's pointer in the geometry, NULL if it has none. */
#define RpGeometryGetGameCubeVtxFmt(_geometry) (*RWPLUGINOFFSET(RpGameCubeVtxFmt*, (_geometry), _rpDlGeomVtxFmtOffset))

/* The default vertex format, used by a geometry that has none (_rpGameCubeVtxFmtGetDefault in Gormiti). */
extern RpGameCubeVtxFmt* fn_802DF0DC(void);

/* Configures the GX vertex attribute format tables for the given format and pipe data (_rwDlVtxFmtSetup in Gormiti). */
extern void fn_802DEE68(RpGameCubeVtxFmt* fmt, RxGameCubePipeData* pipeData);

#ifdef __cplusplus
}
#endif

#endif
