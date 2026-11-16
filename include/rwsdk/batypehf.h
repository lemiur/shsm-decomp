// Reference: tgsm/pbg include/rwsdk/batypehf.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h, include/rwsdk/rwcore.h
#ifndef RWSDK_BATYPEHF_H
#define RWSDK_BATYPEHF_H

#include <rwsdk/plcore/bamemory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RwObject: the header every RenderWare object starts with: its type, flags the application may
 * set, flags private to the library, and its parent (a frame's parent frame, or the frame an
 * object is attached to).
 */
typedef struct RwObject {
    RwUInt8 type;           /* 0x0 */
    RwUInt8 subType;        /* 0x1 */
    RwUInt8 flags;          /* 0x2 */
    RwUInt8 privateFlags;   /* 0x3 */
    void* parent;           /* 0x4 */
} RwObject;

/* RwObject accessors (bfbbdecomp/bfbb rwplcore.h). */
#define rwObjectCopy(d, s)                                                      \
    do {                                                                        \
        ((RwObject*)(d))->type = ((const RwObject*)(s))->type;                  \
        ((RwObject*)(d))->subType = ((const RwObject*)(s))->subType;            \
        ((RwObject*)(d))->flags = ((const RwObject*)(s))->flags;                \
        ((RwObject*)(d))->privateFlags = ((const RwObject*)(s))->privateFlags;  \
        ((RwObject*)(d))->parent = NULL;                                        \
    } while (0)

#define rwObjectInitialize(o, t, s)                     \
    do {                                                \
        ((RwObject*)(o))->type = (RwUInt8)(t);          \
        ((RwObject*)(o))->subType = (RwUInt8)(s);       \
        ((RwObject*)(o))->flags = 0;                    \
        ((RwObject*)(o))->privateFlags = 0;             \
        ((RwObject*)(o))->parent = NULL;                \
    } while (0)

#define RwObjectGetType(o) (((const RwObject*)(o))->type)
#define rwObjectGetSubType(o) (((const RwObject*)(o))->subType)
#define rwObjectSetSubType(o, t) (((RwObject*)(o))->subType) = (RwUInt8)(t)
#define rwObjectGetFlags(o) (((const RwObject*)(o))->flags)
#define rwObjectSetFlags(o, f) (((RwObject*)(o))->flags) = (RwUInt8)(f)
#define rwObjectGetPrivateFlags(c) (((const RwObject*)(c))->privateFlags)
#define rwObjectSetPrivateFlags(c, f) (((RwObject*)(c))->privateFlags) = (RwUInt8)(f)
#define rwObjectTestPrivateFlags(c, flag) ((((const RwObject*)(c))->privateFlags) & (RwUInt8)(flag))
#define rwObjectGetParent(object) (((const RwObject*)(object))->parent)
#define rwObjectSetParent(c, p) (((RwObject*)(c))->parent) = (void*)(p)

typedef struct RwObjectHasFrame RwObjectHasFrame;

/* Called when the frame an object hangs from has moved. */
typedef RwObjectHasFrame* (*RwObjectHasFrameSyncFunction)(RwObjectHasFrame* object);

/*
 * RwObjectHasFrame: an object attached to a frame (a camera, a light, an atomic): its parent is
 * the frame, and lFrame links it into the frame's objectList.
 */
struct RwObjectHasFrame {
    RwObject object;                    /* 0x0 */
    RwLLLink lFrame;                    /* 0x8 */
    RwObjectHasFrameSyncFunction sync;  /* 0x10 */
};

struct RwFrame;

void _rwObjectHasFrameSetFrame(void* object, struct RwFrame* frame);
void _rwObjectHasFrameReleaseFrame(void* object);

/* Attaching an object to a frame and releasing it (bfbbdecomp/bfbb rwcore.h). */
#define rwObjectHasFrameSetFrame(object, frame) _rwObjectHasFrameSetFrame(object, frame)
#define rwObjectHasFrameReleaseFrame(object) _rwObjectHasFrameReleaseFrame(object)

#ifdef __cplusplus
}
#endif

#endif
