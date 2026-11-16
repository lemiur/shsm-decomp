// Names and layout: Gormiti link map and DWARF (RPCollis.a ctdata.obj); constants and signatures: the SDK header
#ifndef RWSDK_PLUGIN_COLLIS_CTDATA_H
#define RWSDK_PLUGIN_COLLIS_CTDATA_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/babbox.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/babinary.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The chunk ID of a collision tree. */
#ifndef rwID_COLLTREE
#define rwID_COLLTREE MAKECHUNKID(rwVENDORID_CORE, 0x2C)
#endif

#define rpCOLLTREE_MAXDEPTH                 32
#define rpCOLLSPLIT_ALIGNMENT               16

/* Allow 16 special contents values */
#define rpCOLLSECTOR_CONTENTS_MAXCOUNT      0xef
#define rpCOLLSECTOR_CONTENTS_SPLIT         0xff

#define rpCOLLSECTOR_TYPE_NEG               0x01
#define rpCOLLSECTOR_TYPE_AXISMASK          (0x08 | 0x04)
#define rpCOLLSECTOR_TYPE_X                 0x00
#define rpCOLLSECTOR_TYPE_Y                 0x04
#define rpCOLLSECTOR_TYPE_Z                 0x08

/* The tree has a map (the entry numbers the leaves index into). */
enum RpCollTreeFlags {
    rpCOLLTREE_USEMAP = 0x01
};
typedef enum RpCollTreeFlags RpCollTreeFlags;

/* One side of a split: a leaf (contents entries, from index on in the tree's map) or a further split. */
typedef struct RpCollSector {
    RwUInt8 type;       /* 0x0 */
    RwUInt8 contents;   /* 0x1: the number of entries of a leaf, rpCOLLSECTORSPLIT for a split */
    RwUInt16 index;     /* 0x2: the first entry of a leaf, or the split's number */
    RwReal value;       /* 0x4 */
} RpCollSector;

/* A split of the tree: its two sides. */
typedef struct RpCollSplit {
    RpCollSector leftSector;    /* 0x0 */
    RpCollSector rightSector;   /* 0x8 */
} RpCollSplit;

/* A collision tree: its header, then the splits (16-byte aligned) and the map (2-byte aligned) in the same block. */
typedef struct RpCollTree {
    RwUInt32 flags;         /* 0x00 */
    RwBBox bbox;            /* 0x04 */
    RwUInt16 numEntries;    /* 0x1C */
    RwUInt16 numSplits;     /* 0x1E */
    RpCollSplit *splits;    /* 0x20 */
    RwUInt16 *map;          /* 0x24 */
} RpCollTree;

/* Error codes of the collision plugin (the codes without the common flag). Names: the string table of Gormiti's
   Debug build, in its order; only the code 1 is used by retail. */
typedef enum RpCollisionErrorCode {
    E_RP_COLLIS_BUILD_FAILURE = 1,
    E_RP_COLLIS_STREAM_FAILURE = 2
} RpCollisionErrorCode;

RpCollTree *_rpCollTreeCreate(RwUInt16 numEntries, RwUInt16 numSplits, RwBBox *bbox, RwUInt32 flags);
void _rpCollTreeDestroy(RpCollTree *tree);
RwUInt32 _rpCollTreeStreamGetSize(const RpCollTree *tree);
const RpCollTree *_rpCollTreeStreamWrite(const RpCollTree *tree, RwStream *stream);
RpCollTree *_rpCollTreeStreamRead(RwStream *stream);
/* The reader of the old format: the tree's counts from the stream's head and the owner's bounds; failConv is set
   when the conversion is not possible. */
RpCollTree *_rpCollTreeStreamReadPre36001(RwStream *stream, RwUInt32 numEntries, RwUInt32 numSplits,
                                          RwBBox *bbox, RwBool *failConv);

#ifdef __cplusplus
}
#endif

#endif
