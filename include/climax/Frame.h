#ifndef CLIMAX_FRAME_H
#define CLIMAX_FRAME_H

// unit name unconfirmed: header name invented. The layouts are RenderWare 3.7's RwMatrix and
// RwFrame (RW SDK headers rwcore.h); they stand in for the SDK header until it is added.

#include <revolution/types.h>
#include <game/Vec3.h>

/*
 * RwV3d: a plain vector (no constructors, so it is copy-initialised word by word and can be
 * brace-initialised), the type of RenderWare's vector arguments and results.
 */
struct unk_RwV3d {
    float       x;
    float       y;
    float       z;
};

/* RwV2d: a plain 2D vector. */
struct unk_RwV2d {
    float       x;
    float       y;
};

/* RwMatrix: the right, up and at vectors and the position, each padded to 16 bytes. */
struct unk_RwMatrixTag {
    unk_RwV3d   right;      // 0x0
    u32         flags;      // 0xC
    unk_RwV3d   up;         // 0x10
    u32         pad1;       // 0x1C; pad: RwMatrix pad1 (rwcore.h)
    unk_RwV3d   at;         // 0x20
    u32         pad2;       // 0x2C; pad: RwMatrix pad2 (rwcore.h)
    unk_RwV3d   pos;        // 0x30
    u32         pad3;       // 0x3C; pad: RwMatrix pad3 (rwcore.h)
};
typedef struct unk_RwMatrixTag unk_RwMatrix __attribute__((aligned(4)));

/* RwObject: the header every RenderWare object starts with. */
struct unk_RwObject {
    u8      type;           // 0x0
    u8      subType;        // 0x1
    u8      flags;          // 0x2
    u8      privateFlags;   // 0x3
    void*   parent;         // 0x4, the owning frame of an atomic, camera or clump
};

/* RwFrame, up to its modelling matrix. */
struct unk_RwFrame {
    unk_RwObject    object;             // 0x0
    void*           inDirtyListLink[2]; // 0x8, RwLLLink
    unk_RwMatrix    modelling;          // 0x10
};

/*
 * The frame's modelling matrix (RW 3.7 API name). An inline function, not the SDK's release macro
 * RwFrameGetMatrixMacro: CPointTrigger's constructor needs the frame bound to a parameter (with the
 * macro's plain expression the matrix pointer and the zero take each other's registers).
 */
inline unk_RwMatrix* RwFrameGetMatrix( unk_RwFrame* frame ) {
    return &frame->modelling;
}

/* RwFrameCallBack: returns NULL to stop the iteration. */
typedef unk_RwFrame* ( *unk_RwFrameCallBack )( unk_RwFrame* frame, void* data );

// defined in rwsdk/src/baframe.c (parked): calls callBack on each child of frame until one returns
// NULL (C linkage). Open item: rwsdk/baframe.h declares it too, on RwFrame.
extern "C" unk_RwFrame* RwFrameForAllChildren( unk_RwFrame* frame, unk_RwFrameCallBack callBack, void* data );

/* RwPlane: a plane's unit normal and its distance term (normal . p + distance = 0 on the plane). */
struct unk_RwPlane {
    unk_RwV3d   normal;     // 0x0
    float       distance;   // 0xC
};

// defined in 0x802A6868 (RenderWare), unit not yet reconstructed: transforms count points by
// the matrix (presumed RwV3dTransformPoints, by its argument order and use; C linkage, as
// RenderWare is C)
extern "C" unk_Vec3* RwV3dTransformPoints( unk_Vec3* out, const unk_Vec3* in, s32 count, const unk_RwMatrix* matrix );

// defined in rwsdk/src/plcore/bavector.c (parked): writes the unit vector of in to out and returns
// in's length (C linkage). Open item: rwsdk/plcore/bavector.h declares it too, on RwV3d.
extern "C" float RwV3dNormalize( unk_RwV3d* out, const unk_RwV3d* in );

// defined in rwsdk/src/plcore/bavector.c (parked): the length of a vector (C linkage). Open item:
// rwsdk/plcore/bavector.h declares it too, on RwV3d.
extern "C" float RwV3dLength( const unk_RwV3d* in );

// defined in rwsdk/src/plcore/bavector.c: the square root (C linkage). Open item:
// rwsdk/plcore/bavector.h declares it too.
extern "C" float _rwSqrt( const float num );

// defined in 0x802A2724 (RenderWare), unit not yet reconstructed: clears the matrix's type flags
// after its vectors were written (presumed RwMatrixUpdate, by its body; C linkage)
extern "C" void RwMatrixUpdate( unk_RwMatrix* matrix );

/*
 * out = a x b (RwV3dCrossProduct; the RW 3.7 SDK's release build expands it as the macro
 * RwV3dCrossProductMacro, rwcore.h).
 */
inline void RwV3dCrossProduct( unk_RwV3d* out, const unk_RwV3d* a, const unk_RwV3d* b ) {
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

/* How a matrix operation combines with the matrix's current transform (RwOpCombineType). */
enum unk_RwOpCombineType {
    rwCOMBINEREPLACE = 0,
    rwCOMBINEPRECONCAT,
    rwCOMBINEPOSTCONCAT
};

/* RwMatrix flags (rwMATRIXTYPEORTHONORMAL, rwMATRIXINTERNALIDENTITY in rwcore.h). */
#define rwMATRIXTYPEORTHONORMAL     0x00000003
#define rwMATRIXINTERNALIDENTITY    0x00020000

/*
 * Makes the matrix the identity (RwMatrixSetIdentity: the RW 3.7 SDK's release build defines it as the
 * macro RwMatrixSetIdentityMacro, rwcore.h, as rwsdk/plcore/bamatrix.h has it). The chained assignments
 * give the store order; the macro form, not an inline function, keeps CBasicMount's constructor out of
 * its factory (MWCC's inline budget counts the extra inline expansion).
 */
#define RwMatrixSetIdentity(m)                                               \
    do {                                                                     \
        (m)->right.x = (m)->up.y = (m)->at.z = 1.0f;                         \
        (m)->right.y = (m)->right.z = (m)->up.x = 0.0f;                      \
        (m)->up.z = (m)->at.x = (m)->at.y = 0.0f;                            \
        (m)->pos.x = (m)->pos.y = (m)->pos.z = 0.0f;                         \
        (m)->flags |= rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEORTHONORMAL;    \
    } while (0)

extern "C" {

// defined in rwsdk/src/baframe.c (parked): marks the frame's hierarchy dirty. Open item:
// rwsdk/baframe.h declares it too, on RwFrame.
unk_RwFrame*    RwFrameUpdateObjects( unk_RwFrame* frame );
// defined in rwsdk/src/baframe.c (parked): the frame's world matrix, at 0x50, synchronised first
// when dirty. Open item: rwsdk/baframe.h declares it too, on RwFrame.
unk_RwMatrix*   RwFrameGetLTM( unk_RwFrame* frame );
// defined in 0x802A2B64 (RenderWare), unit not yet reconstructed: rotates the matrix by an angle
// in degrees about an axis (presumed RwMatrixRotate, by its body and arguments)
unk_RwMatrix*   RwMatrixRotate( unk_RwMatrix* matrix, const unk_RwV3d* axis, float angle, unk_RwOpCombineType combineOp );
// defined in 0x802A2C4C (RenderWare), unit not yet reconstructed: writes the inverse of in to
// out (presumed RwMatrixInvert, by its body)
unk_RwMatrix*   RwMatrixInvert( unk_RwMatrix* out, const unk_RwMatrix* in );
// defined in 0x802A273C (RenderWare), unit not yet reconstructed: out = in1 * in2 (RwMatrixMultiply,
// RW 3.7 SDK bamatrix.h)
unk_RwMatrix*   RwMatrixMultiply( unk_RwMatrix* out, const unk_RwMatrix* in1, const unk_RwMatrix* in2 );
// defined in 0x802A2E18 (RenderWare), unit not yet reconstructed: scales the matrix (presumed
// RwMatrixScale, by its body and arguments)
unk_RwMatrix*   RwMatrixScale( unk_RwMatrix* matrix, const unk_RwV3d* scale, unk_RwOpCombineType combineOp );
// defined in 0x802A65E0 (RenderWare), unit not yet reconstructed: a table square root (presumed
// the RenderWare square root behind rwSqrt, by its body)
float           fn_802A65E0( float x );
// defined in 0x802A681C (RenderWare), unit not yet reconstructed: transforms one point by the
// matrix (presumed RwV3dTransformPoint, by its body)
unk_RwV3d*      RwV3dTransformPoint( unk_RwV3d* out, const unk_RwV3d* in, const unk_RwMatrix* matrix );

}

/* The length of a (RwV2dLength, expanded through rwSqrt as in the SDK's release build). */
inline float RwV2dLength( const unk_RwV2d* a ) {
    return fn_802A65E0( a->x * a->x + a->y * a->y );
}

/* out = a * scale (RwV2dScale; the macro RwV2dScaleMacro in the SDK's release build). */
inline void RwV2dScale( unk_RwV2d* out, const unk_RwV2d* a, float scale ) {
    out->x = a->x * scale;
    out->y = a->y * scale;
}

/* out = a + b (RwV2dAdd; the macro RwV2dAddMacro in the SDK's release build). */
inline void RwV2dAdd( unk_RwV2d* out, const unk_RwV2d* a, const unk_RwV2d* b ) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
}

#endif
