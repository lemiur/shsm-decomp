// Reference: tgsm/pbg include/rwsdk/plcore/bamatrix.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h (matrix types, flags and macros)
#ifndef RWSDK_PLCORE_BAMATRIX_H
#define RWSDK_PLCORE_BAMATRIX_H

#include <rwsdk/plcore/batypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How a matrix operation combines with the matrix's current transform. */
typedef enum RwOpCombineType {
    rwCOMBINEREPLACE = 0,
    rwCOMBINEPRECONCAT,
    rwCOMBINEPOSTCONCAT,
    rwOPCOMBINETYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwOpCombineType;

/* What RwMatrixOptimize found the matrix to be (the low bits of its flags). */
typedef enum RwMatrixType {
    rwMATRIXTYPENORMAL = 0x00000001,
    rwMATRIXTYPEORTHOGONAL = 0x00000002,
    rwMATRIXTYPEORTHONORMAL = 0x00000003,
    rwMATRIXTYPEMASK = 0x00000003,
    rwMATRIXTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwMatrixType;

/* The matrix is known to be the identity. */
typedef enum RwMatrixFlag {
    rwMATRIXINTERNALIDENTITY = 0x00020000,
    rwMATRIXFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwMatrixFlag;

/* The optimisations the engine applies to matrices flagged by RwMatrixOptimize. */
typedef enum RwMatrixOptimizations {
    rwMATRIXOPTIMIZE_IDENTITY = 0x00020000,
    rwMATRIXOPTIMIZATIONSFORCEENUMSIZEINT = 0x7FFFFFFF
} RwMatrixOptimizations;

/* RwMatrix: an affine frame: three axes and a position, each padded to 16 bytes. */
struct RwMatrixTag {
    RwV3d right;    /* 0x0 */
    RwUInt32 flags; /* 0xC */
    RwV3d up;       /* 0x10 */
    RwUInt32 pad1;  /* 0x1C */ // pad: RenderWare 3.7 rwplcore.h RwMatrixTag member name
    RwV3d at;       /* 0x20 */
    RwUInt32 pad2;  /* 0x2C */ // pad: RenderWare 3.7 rwplcore.h RwMatrixTag member name
    RwV3d pos;      /* 0x30 */
    RwUInt32 pad3;  /* 0x3C */ // pad: RenderWare 3.7 rwplcore.h RwMatrixTag member name
};

/*
 * The SDK declares the typedef through its alignment macro, so the type carries the platform's matrix alignment.
 */
typedef struct RwMatrixTag RWALIGN(RwMatrix, rwMATRIXALIGNMENT);

/* RwMatrixTolerance: the errors within which RwMatrixOptimize deems a matrix normal, orthogonal, identity. */
typedef struct RwMatrixTolerance {
    RwReal Normal;      /* 0x0 */
    RwReal Orthogonal;  /* 0x4 */
    RwReal Identity;    /* 0x8 */
} RwMatrixTolerance;

/* A matrix product: dstMat = matA * matB. */
typedef void (*rwMatrixMultFn)(RwMatrix* dstMat, const RwMatrix* matA, const RwMatrix* matB);

#define rwMatrixSetFlags(m, flagsbit) ((m)->flags = (flagsbit))
#define rwMatrixGetFlags(m) ((m)->flags)
#define rwMatrixTestFlags(m, flagsbit) ((m)->flags & (RwInt32)(flagsbit))

#define RwMatrixCopy(dst, src) (*(dst) = *(src))

/* Makes m the identity; the chained assignments give the store order. */
#define RwMatrixSetIdentity(m)                                                                  \
    do {                                                                                        \
        (m)->right.x = (m)->up.y = (m)->at.z = 1.0f;                                            \
        (m)->right.y = (m)->right.z = (m)->up.x = 0.0f;                                         \
        (m)->up.z = (m)->at.x = (m)->at.y = 0.0f;                                               \
        (m)->pos.x = (m)->pos.y = (m)->pos.z = 0.0f;                                            \
        rwMatrixSetFlags((m), rwMatrixGetFlags(m) |                                             \
                                  (rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL));        \
    } while (0)

#define RwMatrixGetRight(m) (&(m)->right)
#define RwMatrixGetUp(m) (&(m)->up)
#define RwMatrixGetAt(m) (&(m)->at)
#define RwMatrixGetPos(m) (&(m)->pos)

RwReal _rwMatrixDeterminant(const RwMatrix* matrix);
RwReal _rwMatrixOrthogonalError(const RwMatrix* matrix);
RwReal _rwMatrixNormalError(const RwMatrix* matrix);
RwBool RwEngineGetMatrixTolerances(RwMatrixTolerance* const tolerance);
RwBool RwEngineSetMatrixTolerances(const RwMatrixTolerance* const tolerance);
RwBool _rwMatrixSetMultFn(rwMatrixMultFn multMat);
RwBool _rwMatrixSetOptimizations(RwInt32 optimizeFlags);
RwMatrix* RwMatrixOptimize(RwMatrix* matrix, const RwMatrixTolerance* tolerance);
RwMatrix* RwMatrixUpdate(RwMatrix* matrix);
RwMatrix* RwMatrixMultiply(RwMatrix* matrixOut, const RwMatrix* matrixIn1, const RwMatrix* matrixIn2);
RwMatrix* RwMatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn);
RwMatrix* RwMatrixRotateOneMinusCosineSine(RwMatrix* matrix, const RwV3d* unitAxis,
                                           RwReal oneMinusCosine, RwReal sine,
                                           RwOpCombineType combineOp);
RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp);
RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn);
RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp);
RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp);
RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp);
const RwMatrix* RwMatrixQueryRotate(const RwMatrix* matrix, RwV3d* unitAxis, RwReal* angle,
                                    RwV3d* center);
RwBool RwMatrixDestroy(RwMatrix* matrix);
RwMatrix* RwMatrixCreate(void);

void* _rwMatrixOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwMatrixClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
