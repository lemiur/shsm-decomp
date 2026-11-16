// Reference: tgsm/pbg include/rwsdk/plcore/bamatrix.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h (API prototypes and types), config/GQPE78/symbols.txt and splits.txt (function, data and unit names)
// NonMatching: RwMatrixOptimize keeps a register/schedule residue (see its comment).
/*
 * bamatrix.c: the matrix module. Matrices come from the module's free list; every operation that
 * changes a matrix clears the flags that RwMatrixOptimize sets, unless it keeps them true.
 */
#include <math.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bavector.h>

/* Absolute value of a real. */
#define RwRealAbs(a) (((a) >= 0.0f) ? (a) : -(a))

/* Radians per degree. Name invented. */
#define rwRADIANSPERDEGREE 0.017453292f

/* The matrix module's globals (RWMATRIXGLOBAL invented). */
typedef struct rwMatrixGlobals {
    RwFreeList* matrixFreeList;                 /* 0x0 */
    RwMatrixOptimizations matrixOptimizations;  /* 0x4 */
    rwMatrixMultFn multMatrix;                  /* 0x8 */
    RwMatrixTolerance tolerance;                /* 0xC */
} rwMatrixGlobals;

/* The free list of matrices; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rwMatrixFreeList;

/* Matrices per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwMatrixFreeListBlockSize = 256;
static RwInt32 _rwMatrixFreeListPreallocBlocks = 1;

/* The matrix module's registration (offset of its globals in the engine instance). */
static RwModuleInfo matrixModule;

#define RWMATRIXGLOBAL(var) \
    (RWPLUGINOFFSET(rwMatrixGlobals, RwEngineInstance, matrixModule.globalsOffset)->var)

/* dstMat = matA * matB, rows times columns; the flags are left alone. */
static void MatrixMultiply(RwMatrix* dstMat, const RwMatrix* matA, const RwMatrix* matB) {
    dstMat->right.x = matA->right.x * matB->right.x + matA->right.y * matB->up.x +
                      matA->right.z * matB->at.x;
    dstMat->right.y = matA->right.x * matB->right.y + matA->right.y * matB->up.y +
                      matA->right.z * matB->at.y;
    dstMat->right.z = matA->right.x * matB->right.z + matA->right.y * matB->up.z +
                      matA->right.z * matB->at.z;

    dstMat->up.x = matA->up.x * matB->right.x + matA->up.y * matB->up.x + matA->up.z * matB->at.x;
    dstMat->up.y = matA->up.x * matB->right.y + matA->up.y * matB->up.y + matA->up.z * matB->at.y;
    dstMat->up.z = matA->up.x * matB->right.z + matA->up.y * matB->up.z + matA->up.z * matB->at.z;

    dstMat->at.x = matA->at.x * matB->right.x + matA->at.y * matB->up.x + matA->at.z * matB->at.x;
    dstMat->at.y = matA->at.x * matB->right.y + matA->at.y * matB->up.y + matA->at.z * matB->at.y;
    dstMat->at.z = matA->at.x * matB->right.z + matA->at.y * matB->up.z + matA->at.z * matB->at.z;

    dstMat->pos.x = matA->pos.x * matB->right.x + matA->pos.y * matB->up.x +
                    matA->pos.z * matB->at.x + matB->pos.x;
    dstMat->pos.y = matA->pos.x * matB->right.y + matA->pos.y * matB->up.y +
                    matA->pos.z * matB->at.y + matB->pos.y;
    dstMat->pos.z = matA->pos.x * matB->right.z + matA->pos.y * matB->up.z +
                    matA->pos.z * matB->at.z + matB->pos.z;
} // 0x802A13A0

/*
 * matrixOut = matrixIn with its rotation made orthonormal: the axes are normalised, the pair
 * closest to perpendicular is kept, and the other two axes are rebuilt from cross products.
 */
static RwMatrix* MatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    RwV3d right;
    RwV3d up;
    RwV3d at;
    RwV3d pos;
    RwV3d invLengths;
    RwV3d dots;
    RwV3d* vpU;
    RwV3d* vpV;
    RwV3d* vpW;
    RwReal invLength;
    RwReal lengthSq;

    right = matrixIn->right;
    up = matrixIn->up;
    at = matrixIn->at;
    pos = matrixIn->pos;

    lengthSq = RwV3dDotProductMacro(&right, &right);
    invLengths.x = _rwInvSqrt(lengthSq);
    RwV3dScaleMacro(&right, &right, invLengths.x);
    lengthSq = RwV3dDotProductMacro(&up, &up);
    invLengths.y = _rwInvSqrt(lengthSq);
    RwV3dScaleMacro(&up, &up, invLengths.y);
    lengthSq = RwV3dDotProductMacro(&at, &at);
    invLengths.z = _rwInvSqrt(lengthSq);
    RwV3dScaleMacro(&at, &at, invLengths.z);

    if (invLengths.x > 0.0f) {
        if (invLengths.y > 0.0f) {
            if (invLengths.z > 0.0f) {
                dots.x = RwV3dDotProductMacro(&up, &at);
                dots.x = RwRealAbs(dots.x);
                dots.y = RwV3dDotProductMacro(&at, &right);
                dots.y = RwRealAbs(dots.y);
                dots.z = RwV3dDotProductMacro(&right, &up);
                dots.z = RwRealAbs(dots.z);

                if (dots.x < dots.y) {
                    if (dots.x < dots.z) {
                        vpU = &up;
                        vpV = &at;
                        vpW = &right;
                    } else {
                        vpU = &right;
                        vpV = &up;
                        vpW = &at;
                    }
                } else {
                    if (dots.y < dots.z) {
                        vpU = &at;
                        vpV = &right;
                        vpW = &up;
                    } else {
                        vpU = &right;
                        vpV = &up;
                        vpW = &at;
                    }
                }
            } else {
                vpU = &right;
                vpV = &up;
                vpW = &at;
            }
        } else {
            vpU = &at;
            vpV = &right;
            vpW = &up;
        }
    } else {
        vpU = &up;
        vpV = &at;
        vpW = &right;
    }

    RwV3dCrossProductMacro(vpW, vpU, vpV);
    lengthSq = RwV3dDotProductMacro(vpW, vpW);
    invLength = _rwInvSqrt(lengthSq);
    RwV3dScaleMacro(vpW, vpW, invLength);
    RwV3dCrossProductMacro(vpV, vpW, vpU);
    lengthSq = RwV3dDotProductMacro(vpV, vpV);
    invLength = _rwInvSqrt(lengthSq);
    RwV3dScaleMacro(vpV, vpV, invLength);

    matrixOut->right = right;
    matrixOut->up = up;
    matrixOut->at = at;
    matrixOut->pos = pos;
    rwMatrixSetFlags(matrixOut, (rwMatrixGetFlags(matrixOut) | rwMATRIXTYPEORTHONORMAL) &
                                    ~rwMATRIXINTERNALIDENTITY);

    return matrixOut;
} // 0x802A1680

/*
 * dst = the inverse of src, by the adjugate of its rotation part; a singular matrix gives the
 * adjugate itself. Name: Gormiti link map (MatrixInvertGeneric); locals: Gormiti DWARF.
 */
static void MatrixInvertGeneric(RwMatrix* dst, const RwMatrix* src) {
    RwSplitBits determinant;
    RwReal normalize;

    dst->right.x = src->up.y * src->at.z - src->up.z * src->at.y;
    dst->right.y = -(src->right.y * src->at.z - src->right.z * src->at.y);
    dst->right.z = src->right.y * src->up.z - src->right.z * src->up.y;

    determinant.nReal = dst->right.x * src->right.x + dst->right.y * src->up.x + dst->right.z * src->at.x;
    normalize = (determinant.nInt != 0) ? 1.0f / determinant.nReal : 1.0f;

    RwV3dScaleMacro(&dst->right, &dst->right, normalize);

    dst->up.x = normalize * -(src->up.x * src->at.z - src->up.z * src->at.x);
    dst->up.y = normalize * (src->right.x * src->at.z - src->right.z * src->at.x);
    dst->up.z = normalize * -(src->right.x * src->up.z - src->right.z * src->up.x);

    dst->at.x = normalize * (src->up.x * src->at.y - src->up.y * src->at.x);
    dst->at.y = normalize * -(src->right.x * src->at.y - src->right.y * src->at.x);
    dst->at.z = normalize * (src->right.x * src->up.y - src->right.y * src->up.x);

    dst->pos.x = -(src->pos.x * dst->right.x + src->pos.y * dst->up.x + src->pos.z * dst->at.x);
    dst->pos.y = -(src->pos.x * dst->right.y + src->pos.y * dst->up.y + src->pos.z * dst->at.y);
    dst->pos.z = -(src->pos.x * dst->right.z + src->pos.y * dst->up.z + src->pos.z * dst->at.z);

    rwMatrixSetFlags(dst, 0);
} // 0x802A1B40

/*
 * The axis of a half-turn matrix, from its largest diagonal element: there the matrix is
 * 2 * axis * axis' - I. Name invented.
 */
static const RwMatrix* MatrixQueryHalfTurnAxis(const RwMatrix* matrix, RwV3d* unitAxis) {
    RwV3d vTmp;
    RwReal recip;

    if (matrix->right.x > matrix->up.y) {
        if (matrix->right.x > matrix->at.z) {
            vTmp.x = (1.0f + matrix->right.x) + (1.0f + matrix->right.x);
            vTmp.y = matrix->right.y + matrix->up.x;
            vTmp.z = matrix->right.z + matrix->at.x;
        } else {
            vTmp.z = (1.0f + matrix->at.z) + (1.0f + matrix->at.z);
            vTmp.x = matrix->at.x + matrix->right.z;
            vTmp.y = matrix->at.y + matrix->up.z;
        }
    } else {
        if (matrix->up.y > matrix->at.z) {
            vTmp.y = (1.0f + matrix->up.y) + (1.0f + matrix->up.y);
            vTmp.z = matrix->up.z + matrix->at.y;
            vTmp.x = matrix->up.x + matrix->right.y;
        } else {
            vTmp.z = (1.0f + matrix->at.z) + (1.0f + matrix->at.z);
            vTmp.x = matrix->at.x + matrix->right.z;
            vTmp.y = matrix->at.y + matrix->up.z;
        }
    }

    recip = _rwInvSqrt(RwV3dDotProductMacro(&vTmp, &vTmp));
    RwV3dScaleMacro(unitAxis, &vTmp, recip);

    return matrix;
} // 0x802A1D70

/*
 * The rotation the matrix performs: its axis, its angle in degrees and a point on the axis. Name
 * invented (RwMatrixQueryRotate's body).
 */
static const RwMatrix* MatrixQueryRotate(const RwMatrix* matrix, RwV3d* unitAxis, RwReal* angle,
                                         RwV3d* center) {
    RwMatrix mOneMinus;
    RwMatrix mInverse;
    RwV3d vTmp;
    RwReal cosine;
    RwReal sine;

    rwMatrixSetFlags(&mOneMinus, rwMATRIXTYPENORMAL);
    rwMatrixSetFlags(&mInverse, rwMATRIXTYPENORMAL);

    cosine = matrix->right.x + matrix->up.y + matrix->at.z - 1.0f;

    vTmp.x = matrix->up.z - matrix->at.y;
    vTmp.y = matrix->at.x - matrix->right.z;
    vTmp.z = matrix->right.y - matrix->up.x;
    sine = RwV3dLength(&vTmp);

    if (sine > 0.0f) {
        RwReal recip = 1.0f / sine;

        RwV3dScaleMacro(unitAxis, &vTmp, recip);
    } else {
        unitAxis->x = 0.0f;
        unitAxis->y = 0.0f;
        unitAxis->z = 0.0f;
    }

    *angle = (((RwReal)180) / rwPI) * (RwReal)atan2(sine, cosine);

    if (sine <= 0.01f && cosine <= 0.0f) {
        MatrixQueryHalfTurnAxis(matrix, unitAxis);
    }

    /* mOneMinus = I - matrix */
    RwMatrixSetIdentity(&mOneMinus);
    RwV3dSubMacro(&mOneMinus.right, &mOneMinus.right, &matrix->right);
    RwV3dSubMacro(&mOneMinus.up, &mOneMinus.up, &matrix->up);
    RwV3dSubMacro(&mOneMinus.at, &mOneMinus.at, &matrix->at);

    /* Neither I - matrix nor its inverse is the identity; mOneMinus takes mInverse's flags. */
    rwMatrixSetFlags(&mOneMinus, rwMatrixGetFlags(&mInverse) & ~rwMATRIXINTERNALIDENTITY);
    rwMatrixSetFlags(&mInverse, rwMatrixGetFlags(&mInverse) & ~rwMATRIXINTERNALIDENTITY);

    MatrixInvertGeneric(&mInverse, &mOneMinus);

    *center = mInverse.pos;
    RwV3dIncrementScaledMacro(center, &mInverse.right, matrix->pos.x);
    RwV3dIncrementScaledMacro(center, &mInverse.up, matrix->pos.y);
    RwV3dIncrementScaledMacro(center, &mInverse.at, matrix->pos.z);

    return matrix;
} // 0x802A1EE4


/* The determinant of the matrix's rotation part: right . (up x at). */
RwReal _rwMatrixDeterminant(const RwMatrix* matrix) {
    RwV3d vTmp;

    vTmp.x = matrix->up.y * matrix->at.z - matrix->up.z * matrix->at.y;
    vTmp.y = matrix->up.z * matrix->at.x - matrix->up.x * matrix->at.z;
    vTmp.z = matrix->up.x * matrix->at.y - matrix->up.y * matrix->at.x;

    return RwV3dDotProductMacro(&vTmp, &matrix->right);
} // 0x802A21A4

/* How far the matrix's axes are from orthogonal: the sum of the squares of their dot products. */
RwReal _rwMatrixOrthogonalError(const RwMatrix* matrix) {
    RwV3d vTmp;

    vTmp.x = RwV3dDotProductMacro(&matrix->up, &matrix->at);
    vTmp.y = RwV3dDotProductMacro(&matrix->at, &matrix->right);
    vTmp.z = RwV3dDotProductMacro(&matrix->right, &matrix->up);

    return RwV3dDotProductMacro(&vTmp, &vTmp);
} // 0x802A2218

/* How far the matrix's axes are from unit length: the sum of the squares of (length^2 - 1). */
RwReal _rwMatrixNormalError(const RwMatrix* matrix) {
    RwV3d vTmp;

    vTmp.x = RwV3dDotProductMacro(&matrix->right, &matrix->right) - 1.0f;
    vTmp.y = RwV3dDotProductMacro(&matrix->up, &matrix->up) - 1.0f;
    vTmp.z = RwV3dDotProductMacro(&matrix->at, &matrix->at) - 1.0f;

    return RwV3dDotProductMacro(&vTmp, &vTmp);
} // 0x802A22A4

/* Closes the matrix module: destroys the matrix free list. */
void* _rwMatrixClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWMATRIXGLOBAL(matrixFreeList) != NULL) {
        RwFreeListDestroy(RWMATRIXGLOBAL(matrixFreeList));
        RWMATRIXGLOBAL(matrixFreeList) = NULL;
    }

    matrixModule.numInstances--;

    return instance;
} // 0x802A2340

/*
 * Installs the matrix multiply; NULL selects the module's own C multiply. Gormiti: _rwMatrixSetMultFn.
 */
RwBool _rwMatrixSetMultFn(rwMatrixMultFn multMat) {
    if (multMat == NULL) {
        multMat = MatrixMultiply;
    }
    RWMATRIXGLOBAL(multMatrix) = multMat;

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/* Selects the optimisations RwMatrixOptimize applies. Gormiti: _rwMatrixSetOptimizations. */
RwBool _rwMatrixSetOptimizations(RwInt32 optimizeFlags) {
    RWMATRIXGLOBAL(matrixOptimizations) = optimizeFlags;

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/* Sets the engine's matrix tolerances. Gormiti: RwEngineSetMatrixTolerances. */
RwBool RwEngineSetMatrixTolerances(const RwMatrixTolerance* const tolerance) {
    RwMatrixTolerance* dest = &RWMATRIXGLOBAL(tolerance);

    *dest = *tolerance;

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/* Opens the matrix module: creates the matrix free list and sets the default tolerances. */
void* _rwMatrixOpen(void* instance, RwInt32 offset, RwInt32 size) {
    matrixModule.globalsOffset = offset;

    RWMATRIXGLOBAL(matrixFreeList) = RwFreeListCreateAndPreallocateSpace(
        sizeof(RwMatrix), _rwMatrixFreeListBlockSize, sizeof(RwUInt32), _rwMatrixFreeListPreallocBlocks,
        &_rwMatrixFreeList, rwMEMHINTDUR_GLOBAL | rwID_MATRIX);
    if (RWMATRIXGLOBAL(matrixFreeList) == NULL) {
        instance = NULL;
    } else {
        RwMatrixTolerance tolerance = { 0.01f, 0.01f, 0.01f };

        _rwMatrixSetOptimizations(rwMATRIXOPTIMIZE_IDENTITY);
        _rwMatrixSetMultFn(NULL);
        RwEngineSetMatrixTolerances(&tolerance);

        matrixModule.numInstances++;
    }

    return instance;
} // 0x802A23A4


/* Copies the engine's matrix tolerances into tolerance. */
RwBool RwEngineGetMatrixTolerances(RwMatrixTolerance* const tolerance) {
    *tolerance = RWMATRIXGLOBAL(tolerance);

    return TRUE;
} // 0x802A2484

/*
 * How far the matrix is from the identity: the sum of the squared distances of its axes from the
 * unit axes and of its position from the origin. Gormiti: _rwMatrixIdentityError (static there;
 * its local names are the DWARF's). Each axis error first holds the axis's diagonal term minus one,
 * then the sum: Gormiti's line table gives the subtraction a statement of its own (lines 841, 844,
 * 847) and the DWARF puts error_x, error_y and error_z in the registers of those differences.
 */
static RwReal _rwMatrixIdentityError(const RwMatrix* matrix) {
    RwReal result;
    const RwV3d* mx;
    const RwV3d* my;
    const RwV3d* mz;
    const RwV3d* mw;
    RwReal error_x;
    RwReal error_y;
    RwReal error_z;
    RwReal error_w;

    mx = &matrix->right;
    my = &matrix->up;
    mz = &matrix->at;
    mw = &matrix->pos;

    error_x = mx->x - 1.0f;
    error_x = error_x * error_x + mx->y * mx->y + mx->z * mx->z;

    error_y = my->y - 1.0f;
    error_y = my->x * my->x + error_y * error_y + my->z * my->z;

    error_z = mz->z - 1.0f;
    error_z = mz->x * mz->x + mz->y * mz->y + error_z * error_z;

    error_w = mw->x * mw->x + mw->y * mw->y + mw->z * mw->z;

    result = error_x + error_y + error_z + error_w;

    return result;
}

/*
 * Tests the matrix against tolerance (the engine's when NULL) and sets its normal, orthogonal and
 * identity flags. Locals: Gormiti DWARF.
 * The null test is retail's (li r0, 0; cmplw r0, r4): it needs a typed null on the left (a bare NULL
 * gives cmpwi, and a void-pointer NULL changes other functions).
 */
RwMatrix* RwMatrixOptimize(RwMatrix* matrix, const RwMatrixTolerance* tolerance) {
    RwUInt32 flags;
    RwBool MatrixIsNormal;
    RwBool MatrixIsOrthogonal;
    RwBool MatrixIsIdentity;

    if ((const RwMatrixTolerance*)NULL == tolerance) {
        tolerance = &RWMATRIXGLOBAL(tolerance);
    }

    MatrixIsNormal = (tolerance->Normal >= _rwMatrixNormalError(matrix));
    MatrixIsOrthogonal = (tolerance->Orthogonal >= _rwMatrixOrthogonalError(matrix));
    MatrixIsIdentity = (MatrixIsNormal && MatrixIsOrthogonal) &&
                       (tolerance->Identity >= _rwMatrixIdentityError(matrix));

    flags = rwMatrixGetFlags(matrix);

    if (MatrixIsNormal) {
        flags |= rwMATRIXTYPENORMAL;
    } else {
        flags &= ~rwMATRIXTYPENORMAL;
    }

    if (MatrixIsOrthogonal) {
        flags |= rwMATRIXTYPEORTHOGONAL;
    } else {
        flags &= ~rwMATRIXTYPEORTHOGONAL;
    }

    if (MatrixIsIdentity) {
        flags |= rwMATRIXINTERNALIDENTITY;
    } else {
        flags &= ~rwMATRIXINTERNALIDENTITY;
    }

    rwMatrixSetFlags(matrix, flags);

    return matrix;
} // 0x802A24B0

/* Marks the matrix changed: it is no longer known to be normal, orthogonal or the identity. */
RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) & ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK));

    return matrix;
} // 0x802A2724

// ORIGINAL-ASM: hand-scheduled paired-single product (psq_l row pairs with ps_muls0/ps_madds0/ps_madds1, f30/f31 saved by psq_st), which no C body compiles to
/* matrixOut = matrixIn1 * matrixIn2 with paired-single arithmetic; the flags are those both share. */
asm RwMatrix* RwMatrixMultiply(register RwMatrix* matrixOut, register const RwMatrix* matrixIn1,
                               register const RwMatrix* matrixIn2) {
    nofralloc
    stwu r1, -0x30(r1)
    stfd f31, 0x20(r1)
    psq_st f31, 0x28(r1), 0, 0
    stfd f30, 0x10(r1)
    psq_st f30, 0x18(r1), 0, 0
    psq_l f8, 0x0(matrixIn2), 0, 0
    psq_l f0, 0x0(matrixIn1), 0, 0
    psq_l f2, 0x10(matrixIn1), 0, 0
    psq_l f4, 0x20(matrixIn1), 0, 0
    ps_muls0 f12, f8, f0
    psq_l f9, 0x10(matrixIn2), 0, 0
    ps_muls0 f13, f8, f2
    ps_muls0 f30, f8, f4
    psq_l f6, 0x30(matrixIn1), 0, 0
    psq_l f11, 0x30(matrixIn2), 0, 0
    ps_madds1 f12, f9, f0, f12
    psq_l f10, 0x20(matrixIn2), 0, 0
    ps_madds0 f31, f8, f6, f11
    psq_l f1, 0x8(matrixIn1), 1, 0
    ps_madds1 f13, f9, f2, f13
    psq_l f3, 0x18(matrixIn1), 1, 0
    ps_madds0 f12, f10, f1, f12
    psq_l f5, 0x28(matrixIn1), 1, 0
    ps_madds1 f30, f9, f4, f30
    psq_l f8, 0x8(matrixIn2), 1, 0
    ps_madds0 f13, f10, f3, f13
    psq_l f11, 0x38(matrixIn2), 1, 0
    ps_madds0 f30, f10, f5, f30
    psq_st f12, 0x0(matrixOut), 0, 0
    ps_madds1 f31, f9, f6, f31
    psq_l f7, 0x38(matrixIn1), 1, 0
    psq_st f13, 0x10(matrixOut), 0, 0
    ps_muls0 f12, f8, f0
    ps_madds0 f31, f10, f7, f31
    psq_st f30, 0x20(matrixOut), 0, 0
    psq_l f9, 0x18(matrixIn2), 1, 0
    ps_muls0 f13, f8, f2
    ps_madds0 f11, f8, f6, f11
    lwz r0, 0xc(matrixIn1)
    ps_muls0 f30, f8, f4
    lwz r4, 0xc(matrixIn2)
    ps_madds1 f12, f9, f0, f12
    psq_l f10, 0x28(matrixIn2), 1, 0
    ps_madds1 f13, f9, f2, f13
    and r4, r0, r4
    ps_madds1 f8, f9, f4, f30
    psq_st f31, 0x30(matrixOut), 0, 0
    ps_madds1 f11, f9, f6, f11
    ps_madds0 f12, f10, f1, f12
    stw r4, 0xc(matrixOut)
    ps_madds0 f13, f10, f3, f13
    ps_madds0 f8, f10, f5, f8
    psq_st f12, 0x8(matrixOut), 1, 0
    ps_madds0 f11, f10, f7, f11
    psq_st f13, 0x18(matrixOut), 1, 0
    psq_st f8, 0x28(matrixOut), 1, 0
    psq_st f11, 0x38(matrixOut), 1, 0
    psq_l f31, 0x28(r1), 0, 0
    lfd f31, 0x20(r1)
    psq_l f30, 0x18(r1), 0, 0
    lfd f30, 0x10(r1)
    addi r1, r1, 0x30
    blr
} // 0x802A273C

/* matrixOut = matrixIn with its axes made orthogonal and of unit length. */
RwMatrix* RwMatrixOrthoNormalize(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return MatrixOrthoNormalize(matrixOut, matrixIn);
} // 0x802A2838

/*
 * Rotates the matrix about unitAxis by the angle whose cosine and sine are given (as
 * 1 - cosine), combined with the matrix's transform by combineOp.
 */
RwMatrix* RwMatrixRotateOneMinusCosineSine(RwMatrix* matrix, const RwV3d* unitAxis,
                                           RwReal oneMinusCosine, RwReal sine,
                                           RwOpCombineType combineOp) {
    RwMatrix mRotate;
    RwMatrix mTemp;
    RwV3d leftDiagonal;
    RwV3d scaledAxis;
    RwV3d crossTerms;

    leftDiagonal.x = 1.0f - unitAxis->x * unitAxis->x;
    leftDiagonal.y = 1.0f - unitAxis->y * unitAxis->y;
    leftDiagonal.z = 1.0f - unitAxis->z * unitAxis->z;
    RwV3dScaleMacro(&leftDiagonal, &leftDiagonal, oneMinusCosine);

    crossTerms.x = unitAxis->y * unitAxis->z;
    crossTerms.y = unitAxis->z * unitAxis->x;
    crossTerms.z = unitAxis->x * unitAxis->y;
    RwV3dScaleMacro(&crossTerms, &crossTerms, oneMinusCosine);

    scaledAxis.x = unitAxis->x * sine;
    scaledAxis.y = unitAxis->y * sine;
    scaledAxis.z = unitAxis->z * sine;

    mRotate.right.x = 1.0f - leftDiagonal.x;
    mRotate.right.y = crossTerms.z + scaledAxis.z;
    mRotate.right.z = crossTerms.y - scaledAxis.y;

    mRotate.up.x = crossTerms.z - scaledAxis.z;
    mRotate.up.y = 1.0f - leftDiagonal.y;
    mRotate.up.z = crossTerms.x + scaledAxis.x;

    mRotate.at.x = crossTerms.y + scaledAxis.y;
    mRotate.at.y = crossTerms.x - scaledAxis.x;
    mRotate.at.z = 1.0f - leftDiagonal.z;

    mRotate.pos.x = 0.0f;
    mRotate.pos.y = 0.0f;
    mRotate.pos.z = 0.0f;

    rwMatrixSetFlags(&mRotate, rwMATRIXTYPEORTHONORMAL);

    switch (combineOp) {
    case rwCOMBINEREPLACE:
        RwMatrixCopy(matrix, &mRotate);
        break;
    case rwCOMBINEPRECONCAT:
        RwMatrixMultiply(&mTemp, &mRotate, matrix);
        RwMatrixCopy(matrix, &mTemp);
        break;
    case rwCOMBINEPOSTCONCAT:
        RwMatrixMultiply(&mTemp, matrix, &mRotate);
        RwMatrixCopy(matrix, &mTemp);
        break;
    default:
        RWERROR((E_RW_BADPARAM, "Invalid combination type"));
        matrix = NULL;
        break;
    }

    return matrix;
} // 0x802A283C

/* Rotates the matrix about axis by angle degrees, combined with its transform by combineOp. */
RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    RwV3d unitAxis;
    RwReal sine;
    RwReal cosine;
    RwReal radians = rwRADIANSPERDEGREE * angle;
    RwReal recip = _rwInvSqrt(RwV3dDotProductMacro(axis, axis));

    RwV3dScaleMacro(&unitAxis, axis, recip);

    sine = (RwReal)sin(radians);
    cosine = (RwReal)cos(radians);

    RwMatrixRotateOneMinusCosineSine(matrix, &unitAxis, 1.0f - cosine, sine, combineOp);

    return matrix;
} // 0x802A2B64


/* matrixOut = the inverse of matrixIn; an orthonormal matrix inverts by transposing. */
RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    if (rwMatrixTestFlags(matrixIn, RWMATRIXGLOBAL(matrixOptimizations) & rwMATRIXINTERNALIDENTITY)) {
        RwMatrixCopy(matrixOut, matrixIn);
    } else if ((rwMatrixGetFlags(matrixIn) & rwMATRIXTYPEMASK) == rwMATRIXTYPEORTHONORMAL) {
        matrixOut->right.x = matrixIn->right.x;
        matrixOut->right.y = matrixIn->up.x;
        matrixOut->right.z = matrixIn->at.x;

        matrixOut->up.x = matrixIn->right.y;
        matrixOut->up.y = matrixIn->up.y;
        matrixOut->up.z = matrixIn->at.y;

        matrixOut->at.x = matrixIn->right.z;
        matrixOut->at.y = matrixIn->up.z;
        matrixOut->at.z = matrixIn->at.z;

        matrixOut->pos.x = -RwV3dDotProductMacro(&matrixIn->pos, &matrixIn->right);
        matrixOut->pos.y = -RwV3dDotProductMacro(&matrixIn->pos, &matrixIn->up);
        matrixOut->pos.z = -RwV3dDotProductMacro(&matrixIn->pos, &matrixIn->at);

        rwMatrixSetFlags(matrixOut, rwMATRIXTYPEORTHONORMAL);
    } else {
        MatrixInvertGeneric(matrixOut, matrixIn);
    }

    return matrixOut;
} // 0x802A2C4C

/*
 * Scales the matrix by scale, combined with its transform by combineOp.
 */
RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp) {
    switch (combineOp) {
    case rwCOMBINEREPLACE:
        RwMatrixSetIdentity(matrix);
        matrix->right.x = scale->x;
        matrix->up.y = scale->y;
        matrix->at.z = scale->z;
        break;
    case rwCOMBINEPRECONCAT:
        RwV3dScaleMacro(&matrix->right, &matrix->right, scale->x);
        RwV3dScaleMacro(&matrix->up, &matrix->up, scale->y);
        RwV3dScaleMacro(&matrix->at, &matrix->at, scale->z);
        break;
    case rwCOMBINEPOSTCONCAT:
        matrix->right.x *= scale->x;
        matrix->right.y *= scale->y;
        matrix->right.z *= scale->z;
        matrix->up.x *= scale->x;
        matrix->up.y *= scale->y;
        matrix->up.z *= scale->z;
        matrix->at.x *= scale->x;
        matrix->at.y *= scale->y;
        matrix->at.z *= scale->z;
        matrix->pos.x *= scale->x;
        matrix->pos.y *= scale->y;
        matrix->pos.z *= scale->z;
        break;
    default:
        RWERROR((E_RW_BADPARAM, "Invalid combination type"));
        matrix = NULL;
        break;
    }

    /* Original bug: after a bad combineOp, matrix is NULL here. */
    rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) & ~(rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK));

    return matrix;
} // 0x802A2E18

/*
 * Translates the matrix by translation, combined with its transform by combineOp.
 */
RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp) {
    switch (combineOp) {
    case rwCOMBINEREPLACE:
        RwMatrixSetIdentity(matrix);
        matrix->pos.x = translation->x;
        matrix->pos.y = translation->y;
        matrix->pos.z = translation->z;
        break;
    case rwCOMBINEPRECONCAT:
        matrix->pos.x += translation->x * matrix->right.x + translation->y * matrix->up.x +
                         translation->z * matrix->at.x;
        matrix->pos.y += translation->x * matrix->right.y + translation->y * matrix->up.y +
                         translation->z * matrix->at.y;
        matrix->pos.z += translation->x * matrix->right.z + translation->y * matrix->up.z +
                         translation->z * matrix->at.z;
        break;
    case rwCOMBINEPOSTCONCAT:
        matrix->pos.x += translation->x;
        matrix->pos.y += translation->y;
        matrix->pos.z += translation->z;
        break;
    default:
        RWERROR((E_RW_BADPARAM, "Invalid combination type"));
        matrix = NULL;
        break;
    }

    /* Original bug: after a bad combineOp, matrix is NULL here. */
    rwMatrixSetFlags(matrix, rwMatrixGetFlags(matrix) & ~rwMATRIXINTERNALIDENTITY);

    return matrix;
} // 0x802A300C

/*
 * Transforms the matrix by transform, combined with its transform by combineOp.
 */
RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    switch (combineOp) {
    case rwCOMBINEREPLACE:
        RwMatrixCopy(matrix, transform);
        break;
    case rwCOMBINEPRECONCAT: {
        RwMatrix mTemp;

        RwMatrixMultiply(&mTemp, transform, matrix);
        RwMatrixCopy(matrix, &mTemp);
        break;
    }
    case rwCOMBINEPOSTCONCAT: {
        RwMatrix mTemp;

        RwMatrixMultiply(&mTemp, matrix, transform);
        RwMatrixCopy(matrix, &mTemp);
        break;
    }
    default:
        RWERROR((E_RW_BADPARAM, "Invalid combination type"));
        matrix = NULL;
        break;
    }

    return matrix;
} // 0x802A31A8

/* The rotation the matrix performs: its unit axis, its angle in degrees and a point on the axis. */
const RwMatrix* RwMatrixQueryRotate(const RwMatrix* matrix, RwV3d* unitAxis, RwReal* angle,
                                    RwV3d* center) {
    return MatrixQueryRotate(matrix, unitAxis, angle, center);
} // 0x802A33D4

/* Returns the matrix to the free list. */
RwBool RwMatrixDestroy(RwMatrix* matrix) {
    RwFreeListFree(RWMATRIXGLOBAL(matrixFreeList), matrix);

    return TRUE;
} // 0x802A33D8

/* Takes a matrix from the free list and makes it the identity. */
RwMatrix* RwMatrixCreate(void) {
    RwMatrix* matrix = (RwMatrix*)RwFreeListAlloc(RWMATRIXGLOBAL(matrixFreeList),
                                                  rwMEMHINTDUR_EVENT | rwID_MATRIX);

    if (matrix != NULL) {
        rwMatrixSetFlags(matrix, rwMATRIXTYPEORTHONORMAL);
        RwMatrixSetIdentity(matrix);
    }

    return matrix;
} // 0x802A3414
