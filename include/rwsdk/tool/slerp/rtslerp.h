// Layout: Gormiti DWARF (RtSlerp.a rtslerp.obj)
#ifndef RWSDK_TOOL_SLERP_RTSLERP_H
#define RWSDK_TOOL_SLERP_RTSLERP_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bavector.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The flags of RtSlerpCreate's mask: a matrix the slerp does not own, because the caller hands it
 * to RtSlerpInitialize and keeps it. Names invented.
 */
#define rtSLERPREFSTARTMAT 0x1
#define rtSLERPREFENDMAT 0x2

/*
 * RtSlerp: the interpolation between two matrices. The rotation from start to end is kept as an
 * axis and an angle; a pair of matrices that turn by a small angle is interpolated linearly
 * instead (useLerp).
 */
typedef struct RtSlerp RtSlerp;
struct RtSlerp {
    RwInt32 matRefMask;     /* 0x00: the rtSLERPREF* flags of the matrices the slerp does not own */
    RwMatrix* startMat;     /* 0x04 */
    RwMatrix* endMat;       /* 0x08 */
    RwV3d axis;             /* 0x0C: the axis of the rotation from start to end */
    RwReal angle;           /* 0x18: its angle in degrees */
    RwInt32 useLerp;        /* 0x1C: nonzero when the angle is small */
};

/* Creates a slerp; its mask names the matrices that will be referenced rather than copied. */
RtSlerp* RtSlerpCreate(RwInt32 nMatRefMask);

/* Sets the two matrices of a slerp (referenced or copied, by its mask) and derives its axis and angle. */
RtSlerp* RtSlerpInitialize(RtSlerp* spSlerp, RwMatrix* startMat, RwMatrix* endMat);

/* Destroys a slerp and the matrices it owns. */
void RtSlerpDestroy(RtSlerp* spSlerp);

/* Sets mpResultMat to the matrix nDelta (0 = start, 1 = end) of the way from start to end. */
RwMatrix* RtSlerpGetMatrix(RtSlerp* spSlerp, RwMatrix* mpResultMat, RwReal nDelta);

#ifdef __cplusplus
}
#endif

#endif
