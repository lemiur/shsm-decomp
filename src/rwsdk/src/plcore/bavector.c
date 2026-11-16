// Reference: tgsm/pbg src/rwsdk/src/plcore/bavector.c
// Names and locals of the table builders: Gormiti DWARF (RwCore bavector.c)
/*
 * bavector.c: the vector module. Square roots and reciprocal square roots come from two tables
 * of 4096 entries, indexed by the low exponent bit and the top 11 mantissa bits of the input;
 * the matrix transforms of points and vectors are called through the module's globals so that
 * a driver can replace them.
 */
#include <math.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bavector.h>

/* The vector module's globals (RWVECTORGLOBAL invented). */
typedef struct rwVectorGlobals {
    RwSplitBits* SqrtTab;           /* 0x0 */
    RwSplitBits* InvSqrtTab;        /* 0x4 */
    rwVectorMultFn multPoint;       /* 0x8 */
    rwVectorsMultFn multPoints;     /* 0xC */
    rwVectorMultFn multVector;      /* 0x10 */
    rwVectorsMultFn multVectors;    /* 0x14 */
} rwVectorGlobals;

/* The vector module's registration (offset of its globals in the engine instance). */
static RwModuleInfo vectorModule;

#define RWVECTORGLOBAL(var) \
    (RWPLUGINOFFSET(rwVectorGlobals, RwEngineInstance, vectorModule.globalsOffset)->var)

/*
 * Entries in each square-root table: one octave of 2048 mantissas for each exponent parity. The
 * table macro names are invented; rwSqrtMacro and rwInvSqrtMacro are pbg's.
 */
#define rwSQRTTABLESIZE 4096
#define rwSQRTTABLEHALFSIZE (rwSQRTTABLESIZE / 2)

/* The step between table inputs: one unit of the 12th mantissa bit. */
#define rwSQRTTABLESTEP 0x1000

/*
 * The table lookups. The input's exponent, halved, goes into the result's exponent; the table
 * entry supplies the mantissa and the exponent bias. Zero (and only zero) maps to itself.
 */
#define rwSqrtMacro(_root, _input)                                                          \
    do {                                                                                    \
        RwSplitBits _rwSqrtBits;                                                            \
                                                                                            \
        _rwSqrtBits.nReal = (_input);                                                       \
        if (_rwSqrtBits.nUInt != 0) {                                                       \
            _rwSqrtBits.nUInt += 0x800;                                                     \
            _rwSqrtBits.nUInt = ((_rwSqrtBits.nUInt >> 1) & 0x3FC00000) +                   \
                RWVECTORGLOBAL(SqrtTab)[(_rwSqrtBits.nUInt >> 12) & 0xFFF].nUInt;           \
        }                                                                                   \
        *(_root) = _rwSqrtBits.nReal;                                                       \
    } while (0)

#define rwInvSqrtMacro(_recip, _input)                                                      \
    do {                                                                                    \
        RwSplitBits _rwInvSqrtBits;                                                         \
                                                                                            \
        _rwInvSqrtBits.nReal = (_input);                                                    \
        if (_rwInvSqrtBits.nUInt != 0) {                                                    \
            _rwInvSqrtBits.nUInt += 0x800;                                                  \
            _rwInvSqrtBits.nUInt = ((~_rwInvSqrtBits.nUInt >> 1) & 0x3FC00000) +            \
                RWVECTORGLOBAL(InvSqrtTab)[(_rwInvSqrtBits.nUInt >> 12) & 0xFFF].nUInt;     \
        }                                                                                   \
        *(_recip) = _rwInvSqrtBits.nReal;                                                   \
    } while (0)

/*
 * Builds the square-root table: inputs from 1.0 up to 4.0, the odd exponent first (entries
 * 2048..4095), then the even one (entries 0..2047). Each entry is the root's bit pattern less
 * the exponent bias that the lookup adds back.
 */
static RwBool SqrtTableCreate(void) {
    RwUInt32 i;
    RwSplitBits* SqrtTab1to2;
    RwSplitBits* SqrtTab2to4;
    RwSplitBits spIn;
    RwSplitBits spOut;

    SqrtTab2to4 = (RwSplitBits*)RwMalloc(rwSQRTTABLESIZE * sizeof(RwSplitBits),
                                         rwMEMHINTDUR_GLOBAL | rwID_VECTORMODULE);
    if (SqrtTab2to4 == NULL) {
        RWERROR((E_RW_NOMEM, rwSQRTTABLESIZE * sizeof(RwSplitBits)));
        return FALSE;
    }

    SqrtTab1to2 = SqrtTab2to4 + rwSQRTTABLEHALFSIZE;
    spIn.nReal = 1.0f;

    for (i = 0; i < rwSQRTTABLEHALFSIZE; i++) {
        spOut.nReal = (RwReal)sqrt(spIn.nReal);
        spOut.nUInt = spOut.nUInt - (127 << 22);
        SqrtTab1to2[i].nUInt = spOut.nUInt;
        spIn.nUInt += rwSQRTTABLESTEP;
    }

    for (i = 0; i < rwSQRTTABLEHALFSIZE; i++) {
        spOut.nReal = (RwReal)sqrt(spIn.nReal);
        spOut.nUInt = spOut.nUInt - (128 << 22);
        SqrtTab2to4[i].nUInt = spOut.nUInt;
        spIn.nUInt += rwSQRTTABLESTEP;
    }

    RWVECTORGLOBAL(SqrtTab) = SqrtTab2to4;

    return TRUE;
} // 0x802A5F00

/* Builds the reciprocal square-root table, in the same order as SqrtTableCreate. */
static RwBool InvSqrtTableCreate(void) {
    RwUInt32 i;
    RwSplitBits* InvSqrtTab1to2;
    RwSplitBits* InvSqrtTab2to4;
    RwSplitBits spIn;
    RwSplitBits spOut;

    InvSqrtTab2to4 = (RwSplitBits*)RwMalloc(rwSQRTTABLESIZE * sizeof(RwSplitBits),
                                            rwMEMHINTDUR_GLOBAL | rwID_VECTORMODULE);
    if (InvSqrtTab2to4 == NULL) {
        RWERROR((E_RW_NOMEM, rwSQRTTABLESIZE * sizeof(RwSplitBits)));
        return FALSE;
    }

    InvSqrtTab1to2 = InvSqrtTab2to4 + rwSQRTTABLEHALFSIZE;
    spIn.nReal = 1.0f;

    for (i = 0; i < rwSQRTTABLEHALFSIZE; i++) {
        spOut.nReal = (RwReal)(1.0 / sqrt(spIn.nReal));
        spOut.nUInt = spOut.nUInt - (128 << 22);
        InvSqrtTab1to2[i].nUInt = spOut.nUInt;
        spIn.nUInt += rwSQRTTABLESTEP;
    }

    for (i = 0; i < rwSQRTTABLEHALFSIZE; i++) {
        spOut.nReal = (RwReal)(1.0 / sqrt(spIn.nReal));
        spOut.nUInt = spOut.nUInt - (127 << 22);
        InvSqrtTab2to4[i].nUInt = spOut.nUInt;
        spIn.nUInt += rwSQRTTABLESTEP;
    }

    RWVECTORGLOBAL(InvSqrtTab) = InvSqrtTab2to4;

    return TRUE;
} // 0x802A6034

/* Transforms numPoints points by matrix (rotation and translation). */
static RwV3d* VectorMultPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints,
                               const RwMatrix* matrix) {
    RwV3d* result = pointsOut;

    while (--numPoints >= 0) {
        RwReal coord;
        RwReal x, y, z;

        coord = pointsIn->x;
        x = coord * matrix->right.x;
        y = coord * matrix->right.y;
        z = coord * matrix->right.z;

        coord = pointsIn->y;
        x += coord * matrix->up.x;
        y += coord * matrix->up.y;
        z += coord * matrix->up.z;

        coord = pointsIn->z;
        x += coord * matrix->at.x;
        y += coord * matrix->at.y;
        z += coord * matrix->at.z;

        pointsOut->x = x + matrix->pos.x;
        pointsOut->y = y + matrix->pos.y;
        pointsOut->z = z + matrix->pos.z;
        pointsIn++;
        pointsOut++;
    }

    return result;
} // 0x802A618C

/* Transforms one point by matrix (rotation and translation). */
static RwV3d* VectorMultPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix) {
    RwReal coord;
    RwReal x, y, z;

    coord = pointIn->x;
    x = coord * matrix->right.x;
    y = coord * matrix->right.y;
    z = coord * matrix->right.z;

    coord = pointIn->y;
    x += coord * matrix->up.x;
    y += coord * matrix->up.y;
    z += coord * matrix->up.z;

    coord = pointIn->z;
    x += coord * matrix->at.x;
    y += coord * matrix->at.y;
    z += coord * matrix->at.z;

    pointOut->x = x + matrix->pos.x;
    pointOut->y = y + matrix->pos.y;
    pointOut->z = z + matrix->pos.z;

    return pointOut;
} // 0x802A623C

/* Transforms numVectors vectors by matrix (rotation only). */
static RwV3d* VectorMultVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, RwInt32 numVectors,
                               const RwMatrix* matrix) {
    RwV3d* result = vectorsOut;

    while (--numVectors >= 0) {
        RwReal coord;
        RwReal x, y, z;

        coord = vectorsIn->x;
        x = coord * matrix->right.x;
        y = coord * matrix->right.y;
        z = coord * matrix->right.z;

        coord = vectorsIn->y;
        x += coord * matrix->up.x;
        y += coord * matrix->up.y;
        z += coord * matrix->up.z;

        coord = vectorsIn->z;
        vectorsOut->x = x + coord * matrix->at.x;
        vectorsOut->y = y + coord * matrix->at.y;
        vectorsOut->z = z + coord * matrix->at.z;
        vectorsIn++;
        vectorsOut++;
    }

    return result;
} // 0x802A62D0

/* Transforms one vector by matrix (rotation only). */
static RwV3d* VectorMultVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix) {
    RwReal coord;
    RwReal x, y, z;

    coord = vectorIn->x;
    x = coord * matrix->right.x;
    y = coord * matrix->right.y;
    z = coord * matrix->right.z;

    coord = vectorIn->y;
    x += coord * matrix->up.x;
    y += coord * matrix->up.y;
    z += coord * matrix->up.z;

    coord = vectorIn->z;
    vectorOut->x = x + coord * matrix->at.x;
    vectorOut->y = y + coord * matrix->at.y;
    vectorOut->z = z + coord * matrix->at.z;
    return vectorOut;
} // 0x802A6368

/*
 * Installs the point and vector transforms; NULL selects the module's own C transform. The
 * four-function signature is reconstructed (tgsm/pbg's GameCube version takes two).
 */
RwBool _rwVectorSetMultFn(rwVectorMultFn multPoint, rwVectorsMultFn multPoints,
                          rwVectorMultFn multVector, rwVectorsMultFn multVectors) {
    if (multPoint == NULL) {
        multPoint = VectorMultPoint;
    }
    RWVECTORGLOBAL(multPoint) = multPoint;

    if (multPoints == NULL) {
        multPoints = VectorMultPoints;
    }
    RWVECTORGLOBAL(multPoints) = multPoints;

    if (multVector == NULL) {
        multVector = VectorMultVector;
    }
    RWVECTORGLOBAL(multVector) = multVector;

    if (multVectors == NULL) {
        multVectors = VectorMultVectors;
    }
    RWVECTORGLOBAL(multVectors) = multVectors;

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/* Scales in to unit length into out, without computing the length; returns the reciprocal length. */
RwReal _rwV3dNormalize(RwV3d* out, const RwV3d* in) {
    RwReal result;

    _rwV3dNormalizeMacro(result, out, in);

    return result;
} // 0x802A63E4

/* Scales in to unit length into out; returns in's length. */
RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in) {
    RwReal lengthSq = in->x * in->x + in->y * in->y + in->z * in->z;
    RwReal length;
    RwReal recip;

    rwSqrtMacro(&length, lengthSq);
    rwInvSqrtMacro(&recip, lengthSq);

    out->x = in->x * recip;
    out->y = in->y * recip;
    out->z = in->z * recip;

    return length;
} // 0x802A6484

/* Returns in's length. */
RwReal RwV3dLength(const RwV3d* in) {
    RwReal length;

    rwSqrtMacro(&length, in->x * in->x + in->y * in->y + in->z * in->z);

    return length;
} // 0x802A656C

/* Returns the square root of num, from the table. */
RwReal _rwSqrt(const RwReal num) {
    RwReal root;

    rwSqrtMacro(&root, num);

    return root;
} // 0x802A65E0

/* Returns the reciprocal square root of num, from the table. */
RwReal _rwInvSqrt(const RwReal num) {
    RwReal recip;

    rwInvSqrtMacro(&recip, num);

    return recip;
} // 0x802A6634

/* Returns in's length. */
RwReal RwV2dLength(const RwV2d* in) {
    RwReal length;

    rwSqrtMacro(&length, in->x * in->x + in->y * in->y);

    return length;
} // 0x802A6690

/* Scales in to unit length into out; returns in's length (an error if it is zero). */
RwReal RwV2dNormalize(RwV2d* out, const RwV2d* in) {
    RwReal lengthSq = in->x * in->x + in->y * in->y;
    RwReal length;
    RwReal recip;

    rwSqrtMacro(&length, lengthSq);
    rwInvSqrtMacro(&recip, lengthSq);

    out->x = in->x * recip;
    out->y = in->y * recip;

    if (length <= 0.0f) {
        RWERROR((E_RW_ZEROLENGTH));
    }

    return length;
} // 0x802A66F8

// ORIGINAL-ASM: hand-scheduled paired-single transform (psq_l pairs and ps_madds0/ps_madds1 over the matrix rows), which no C body compiles to
/* Transforms one point by matrix with paired-single arithmetic. Name unknown. */
asm RwV3d* RwV3dTransformPoint(register RwV3d* pointOut, register const RwV3d* pointIn,
                       register const RwMatrix* matrix) {
    nofralloc
    psq_l f8, 0x0(pointIn), 0, 0
    psq_l f0, 0x0(matrix), 0, 0
    psq_l f6, 0x30(matrix), 0, 0
    psq_l f1, 0x8(matrix), 1, 0
    psq_l f7, 0x38(matrix), 1, 0
    ps_madds0 f10, f0, f8, f6
    psq_l f2, 0x10(matrix), 0, 0
    ps_madds0 f11, f1, f8, f7
    psq_l f3, 0x18(matrix), 1, 0
    ps_madds1 f10, f2, f8, f10
    psq_l f9, 0x8(pointIn), 1, 0
    psq_l f4, 0x20(matrix), 0, 0
    ps_madds1 f11, f3, f8, f11
    psq_l f5, 0x28(matrix), 1, 0
    ps_madds0 f10, f4, f9, f10
    ps_madds0 f11, f5, f9, f11
    psq_st f10, 0x0(pointOut), 0, 0
    psq_st f11, 0x8(pointOut), 1, 0
    blr
} // 0x802A681C

// ORIGINAL-ASM: hand-scheduled paired-single loop (psq_lu/psq_stu with update, count in ctr with no zero test), which no C body compiles to
/* Transforms numPoints points by matrix with paired-single arithmetic. */
asm RwV3d* RwV3dTransformPoints(register RwV3d* pointsOut, register const RwV3d* pointsIn,
                       register RwInt32 numPoints, register const RwMatrix* matrix) {
    nofralloc
    mtctr numPoints
    psq_l f8, 0x0(pointsIn), 0, 0
    psq_l f0, 0x0(matrix), 0, 0
    subi r5, pointsOut, 4
    psq_l f1, 0x8(matrix), 1, 0
    psq_l f6, 0x30(matrix), 0, 0
    psq_l f7, 0x38(matrix), 1, 0
    psq_lu f9, 0x8(pointsIn), 1, 0
    psq_l f2, 0x10(matrix), 0, 0
    psq_l f3, 0x18(matrix), 1, 0
    psq_l f4, 0x20(matrix), 0, 0
    psq_l f5, 0x28(matrix), 1, 0
loop:
    ps_madds0 f10, f0, f8, f6
    ps_madds0 f11, f1, f8, f7
    ps_madds1 f10, f2, f8, f10
    ps_madds1 f11, f3, f8, f11
    psq_lu f8, 0x4(pointsIn), 0, 0
    ps_madds0 f10, f4, f9, f10
    ps_madds0 f11, f5, f9, f11
    psq_lu f9, 0x8(pointsIn), 1, 0
    psq_stu f10, 0x4(r5), 0, 0
    psq_stu f11, 0x8(r5), 1, 0
    bdnz loop
    blr
} // 0x802A6868

// ORIGINAL-ASM: hand-scheduled paired-single transform (ps_muls0 and ps_madds0/ps_madds1 over the matrix rows), which no C body compiles to
/* Transforms one vector by matrix with paired-single arithmetic. Name unknown. */
asm RwV3d* RwV3dTransformVector(register RwV3d* vectorOut, register const RwV3d* vectorIn,
                       register const RwMatrix* matrix) {
    nofralloc
    psq_l f6, 0x0(vectorIn), 0, 0
    psq_l f0, 0x0(matrix), 0, 0
    psq_l f1, 0x8(matrix), 1, 0
    ps_muls0 f10, f0, f6
    psq_l f2, 0x10(matrix), 0, 0
    ps_muls0 f11, f1, f6
    psq_l f3, 0x18(matrix), 1, 0
    psq_l f7, 0x8(vectorIn), 1, 0
    ps_madds1 f10, f2, f6, f10
    psq_l f4, 0x20(matrix), 0, 0
    ps_madds1 f11, f3, f6, f11
    psq_l f5, 0x28(matrix), 1, 0
    ps_madds0 f10, f4, f7, f10
    ps_madds0 f11, f5, f7, f11
    psq_st f10, 0x0(vectorOut), 0, 0
    psq_st f11, 0x8(vectorOut), 1, 0
    blr
} // 0x802A68C8

/* Closes the vector module: frees both tables. */
void* _rwVectorClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWVECTORGLOBAL(InvSqrtTab) != NULL) {
        RwFree(RWVECTORGLOBAL(InvSqrtTab));
        RWVECTORGLOBAL(InvSqrtTab) = NULL;
    }

    if (RWVECTORGLOBAL(SqrtTab) != NULL) {
        RwFree(RWVECTORGLOBAL(SqrtTab));
        RWVECTORGLOBAL(SqrtTab) = NULL;
    }

    vectorModule.numInstances--;

    return instance;
} // 0x802A690C

/* Opens the vector module: installs the default transforms and builds both tables. */
void* _rwVectorOpen(void* instance, RwInt32 offset, RwInt32 size) {
    vectorModule.globalsOffset = offset;

    _rwVectorSetMultFn(NULL, NULL, NULL, NULL);

    if (!SqrtTableCreate()) {
        return NULL;
    }

    if (!InvSqrtTableCreate()) {
        return NULL;
    }

    vectorModule.numInstances++;

    return instance;
} // 0x802A69B0
