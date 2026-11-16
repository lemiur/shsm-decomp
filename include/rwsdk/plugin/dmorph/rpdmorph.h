// Layout and names: Gormiti DWARF and link map (RpDMorph.a rpdmorph.obj, dmphanim.obj, dmphgeom.obj)
#ifndef RWSDK_PLUGIN_DMORPH_RPDMORPH_H
#define RWSDK_PLUGIN_DMORPH_RPDMORPH_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/baclump.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The delta-morph plugin's ID with the engine (Criterion's vendor, chunk 0x22). */
#define rwID_DMORPHPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x22)

/* One key of a delta-morph animation: the target's weight runs from startValue to endValue over duration
   (recipDuration is its reciprocal), and nextFrame is the index of the key that follows (-1 for the last). */
typedef struct rpDMorphFrame {
    RwReal startValue;      /* 0x00 */
    RwReal endValue;        /* 0x04 */
    RwReal duration;        /* 0x08 */
    RwReal recipDuration;   /* 0x0C */
    RwUInt32 nextFrame;     /* 0x10 */
} rpDMorphFrame;

/* The keys of one delta-morph target. */
typedef struct rpDMorphFrameSequence {
    RwUInt32 numFrames;     /* 0x00 */
    rpDMorphFrame* frames;  /* 0x04 */
} rpDMorphFrameSequence;

/* A delta-morph animation: one sequence of keys per delta-morph target. The sequences follow the header
   in the same block. */
typedef struct RpDMorphAnimation {
    RwUInt32 numDMorphTargets;              /* 0x00 */
    rpDMorphFrameSequence* frameSequences;  /* 0x04 */
} RpDMorphAnimation;

/* The run-length encoded deltas of a delta-morph target: the code bytes say which vertices change, the data
   elements are the changes. */
typedef struct rpDMorphRLE {
    RwUInt8* code;              /* 0x00 */
    RwUInt32 numCodeElements;   /* 0x04 */
    RwUInt32 numDataElements;   /* 0x08 */
    void* data;                 /* 0x0C */
} rpDMorphRLE;

/* One delta-morph target of a geometry. flags (an RpGeometryFlag set) names what the target changes (positions,
   normals, pre-light colours, texture coordinates), lockFlags the geometry locks it needs; each pointer below
   is set when its flag is, and they all point into the block that rle.data owns. */
typedef struct RpDMorphTarget {
    RpGeometry* geometry;           /* 0x00 */
    char* name;                     /* 0x04 */
    RwUInt32 flags;                 /* 0x08 */
    RwUInt32 lockFlags;             /* 0x0C */
    rpDMorphRLE rle;                /* 0x10 */
    RwV3d* vertices;                /* 0x20 */
    RwV3d* normals;                 /* 0x24 */
    RwRGBA* preLightColors;         /* 0x28 */
    RwTexCoords* texCoords;         /* 0x2C */
    RwSphere boundingSphere;        /* 0x30 */
} RpDMorphTarget;

/* The delta-morph plugin data of a geometry: its targets and the current weight of each, which follow the
   targets in the same block. */
typedef struct rpDMorphGeometryData {
    RwUInt32 numDMorphTargets;      /* 0x00 */
    RpDMorphTarget* dMorphTargets;  /* 0x04 */
    RwReal* currValues;             /* 0x08 */
} rpDMorphGeometryData;

/* The offset of the delta-morph data pointer in a geometry (Gormiti link map: RpDMorph.a dmphgeom.obj). */
extern RwInt32 rpDMorphGeometryDataOffset;

/* The delta-morph data of a geometry: the pointer the plugin keeps in the geometry. */
#define RPDMORPHGEOMETRYGETDATA(_geometry) \
    (*RWPLUGINOFFSET(rpDMorphGeometryData*, (_geometry), rpDMorphGeometryDataOffset))

/* The interpolator of an atomic: the animation it plays, the time into it, and for each delta-morph target the
   key it is on and the time spent in that key. The two arrays follow the header in the same block (currTimes
   first). loopCallBack is called when an animation wraps. */
typedef struct rpDMorphInterpolator {
    RpDMorphAnimation* animation;                       /* 0x00 */
    RwReal animTime;                                    /* 0x04 */
    rpDMorphFrame** currFrames;                         /* 0x08 */
    RwReal* currTimes;                                  /* 0x0C */
    RpAtomic* (*loopCallBack)(RpAtomic*, void*);        /* 0x10 */
    void* loopCallBackData;                             /* 0x14 */
} rpDMorphInterpolator;

/* The delta-morph plugin data of an atomic: the weight of each target, the interpolator that animates them,
   and the render callback that the plugin replaced with its own. */
typedef struct rpDMorphAtomicData {
    RwReal* values;                         /* 0x00 */
    rpDMorphInterpolator* interpolator;     /* 0x04 */
    RpAtomicCallBackRender defRenderCallBack;   /* 0x08 */
} rpDMorphAtomicData;

/* The offset of the delta-morph data in an atomic (Gormiti link map: RpDMorph.a dmphintp.obj). */
extern RwInt32 rpDMorphAtomicDataOffset;

/* The delta-morph data of an atomic: kept in the atomic itself, unlike the geometry's, which is a pointer. */
#define RPDMORPHATOMICGETDATA(_atomic) \
    RWPLUGINOFFSET(rpDMorphAtomicData, (_atomic), rpDMorphAtomicDataOffset)

/* The module registration of the delta-morph plugin: numInstances is the engine open count, incremented by the
   open callback and decremented by the close callback. Name and type: Gormiti link map and DWARF (RpDMorph.a rpdmorph.obj). */
extern RwModuleInfo rpDMorphModule;

/* Registers the plugin with the engine, then its geometry and atomic extensions. */
RwBool RpDMorphPluginAttach(void);

/* Registers the delta-morph geometry extension (defined in the geometry object). */
RwBool _rpDMorphGeometryPluginAttach(void);

/* Registers the delta-morph atomic extension (defined in the atomic object). */
RwBool _rpDMorphAtomicPluginAttach(void);

RpDMorphAnimation* RpDMorphAnimationCreate(RwUInt32 numDMorphTargets);
RpDMorphAnimation* RpDMorphAnimationDestroy(RpDMorphAnimation* anim);
RpDMorphAnimation* RpDMorphAnimationCreateFrames(RpDMorphAnimation* anim, RwUInt32 dMorphTargetIndex,
                                                 RwUInt32 numFrames);
RpDMorphAnimation* RpDMorphAnimationStreamRead(RwStream* stream);

float* RpDMorphAtomicGetDMorphValues(RpAtomic* atomic);
RpAtomic* RpDMorphAtomicSetAnimation(RpAtomic* atomic, RpDMorphAnimation* animation);
RpAtomic* RpDMorphAtomicAddTime(RpAtomic* atomic, RwReal deltaTime);
RpAtomic* RpDMorphAtomicSetAnimLoopCallBack(RpAtomic* atomic, RpAtomic* (*callBack)(RpAtomic*, void*), void* data);

RpGeometry* RpDMorphGeometryCreateDMorphTargets(RpGeometry* geometry, RwUInt32 number);
RpGeometry* RpDMorphGeometryDestroyDMorphTargets(RpGeometry* geometry);
RwUInt32 RpDMorphGeometryGetNumDMorphTargets(const RpGeometry* geometry);

#ifdef __cplusplus
}
#endif

#endif
