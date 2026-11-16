// Layout and names: Gormiti DWARF (RtSkinSplit.a rtskinsp.obj, skinsplit.obj)
// Header name invented: Gormiti's toolkit headers are not recorded in its DWARF.
#ifndef RWSDK_TOOL_SKINSPLIT_RTSKINSPLIT_H
#define RWSDK_TOOL_SKINSPLIT_RTSKINSPLIT_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/plugin/hanim/rphanim.h>
#include <rwsdk/plugin/skin2/rpskin.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RtSkinSplit: splits a skinned geometry's triangles into meshes that each use no more bones than
 * the platform holds at once (RpSkin2's split data). The toolkit sorts the triangles by material
 * into lists, splits each list that needs too many bones in two, and rewrites the geometry's
 * triangles' materials and the skin's remap to match.
 */

/* One bit per bone of the skin (up to 256). */
typedef struct rtSkinSplitBoneFlag {
    unsigned int flag[8];                       /* 0x00 */
} rtSkinSplitBoneFlag;

/* A list of the triangles of one material that share a set of bones, kept on a singly linked stack. */
typedef struct rtSkinSplitMatTriList rtSkinSplitMatTriList;
struct rtSkinSplitMatTriList {
    rtSkinSplitMatTriList* next;                /* 0x00 */
    unsigned int numBones;                      /* 0x04: bones the triangles use together */
    unsigned int boneRoot;                      /* 0x08 */
    rtSkinSplitBoneFlag boneFlag;               /* 0x0C: the bones the triangles use */
    rtSkinSplitBoneFlag boneLockFlag;           /* 0x2C */
    rtSkinSplitBoneFlag boneFreeFlag;           /* 0x4C */
    unsigned int* boneRemapIndex;               /* 0x6C */
    unsigned int numRLE;                        /* 0x70: runs in rleMap */
    unsigned int* rleMap;                       /* 0x74: the runs, two entries each */
    RpMaterial* mat;                            /* 0x78 */
    int matIdx;                                 /* 0x7C */
    int origMatIdx;                             /* 0x80: the geometry's material the list came from */
    unsigned int maxTris;                       /* 0x84 */
    unsigned int numTris;                       /* 0x88 */
    unsigned int* triList;                      /* 0x8C: the geometry's triangle indices */
};

/* What a split works on: the atomic's geometry and skin, the counts of the split and its working arrays. */
typedef struct rtSkinSplitGeomStat {
    unsigned int flag;                          /* 0x00 */
    RpAtomic* atomic;                           /* 0x04 */
    RpGeometry* geom;                           /* 0x08 */
    RpMaterialList* matList;                    /* 0x0C */
    RpSkin* skin;                               /* 0x10 */
    RpHAnimHierarchy* hierarchy;                /* 0x14 */
    RwMatrixWeights* skinBoneWeight;            /* 0x18 */
    unsigned int* skinBoneIndex;                /* 0x1C */
    unsigned int* boneRemapIndex;               /* 0x20 */
    unsigned int* rleMap;                       /* 0x24 */
    unsigned int boneLimit;                     /* 0x28 */
    unsigned int maxTriBones;                   /* 0x2C */
    unsigned int numTris;                       /* 0x30 */
    unsigned int numMats;                       /* 0x34 */
    unsigned int numVerts;                      /* 0x38 */
    unsigned int numBones;                      /* 0x3C */
    unsigned int numMatTriLists;                /* 0x40 */
    unsigned int numWeights;                    /* 0x44 */
    unsigned int* meshBoneCount;                /* 0x48: the start of the working arrays' one allocation */
    unsigned int* boneMeshCount;                /* 0x4C */
    unsigned int* boneVertCount;                /* 0x50 */
    unsigned int* meshTriCount;                 /* 0x54 */
    unsigned int* boneBoneCount;                /* 0x58 */
    rtSkinSplitBoneFlag* boneBoneFlag;          /* 0x5C */
    rtSkinSplitMatTriList* matTriListStack;     /* 0x60: lists still too big */
    rtSkinSplitMatTriList* matTriListDone;      /* 0x64: lists that fit the limit */
    unsigned char* skinBoneRemapIndices;        /* 0x68: the skin's split data */
    unsigned char* skinMeshBoneRLECount;        /* 0x6C */
    unsigned char* skinMeshBoneRLE;             /* 0x70 */
} rtSkinSplitGeomStat;

/* skinsplit.c: the lists. */
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListPush(rtSkinSplitMatTriList* triListHead,
                                                          rtSkinSplitMatTriList* triList);
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListPop(rtSkinSplitMatTriList* triListHead,
                                                         rtSkinSplitMatTriList** triListPtr);
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListCreate(unsigned int count);
extern int _rtSkinSplitMatTriListDestroy(rtSkinSplitMatTriList* matTriList);
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListAddTriByMat(rtSkinSplitGeomStat* geomStat,
                                                                 rtSkinSplitMatTriList* matTriList,
                                                                 RpMaterial* mat, unsigned int matIdx);
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListGetBoneFlag(rtSkinSplitGeomStat* geomStat,
                                                                 rtSkinSplitMatTriList* matTriList);
extern rtSkinSplitMatTriList* _rtSkinSplitMatTriListSplit(rtSkinSplitGeomStat* geomStat,
                                                           rtSkinSplitMatTriList* matTriList,
                                                           unsigned int boneLimit);
extern rtSkinSplitGeomStat* _rtSkinSplitMatTriListGetBoneLockFlag(rtSkinSplitGeomStat* geomStat,
                                                                   rtSkinSplitMatTriList* matTriList);
extern rtSkinSplitGeomStat* _rtSkinSplitMatTriListRemapBoneIndex(rtSkinSplitGeomStat* geomStat,
                                                                  rtSkinSplitMatTriList* matTriListHead);
extern rtSkinSplitGeomStat* _rtSkinSplitGeomGetStat(rtSkinSplitGeomStat* geomStat);

/* rtskinsp.c: the split of an atomic's geometry. */
extern RpAtomic* RtSkinSplitAtomicSplitGeometry(RpAtomic* atomic, unsigned int boneLimit);

#ifdef __cplusplus
}
#endif

#endif
