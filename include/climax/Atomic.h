#ifndef CLIMAX_ATOMIC_H
#define CLIMAX_ATOMIC_H

// unit name unconfirmed: header name invented. The layout is RenderWare 3.7's RpAtomic (RW SDK
// header rpworld.h), up to the members used so far, and the macros are the SDK's release-build
// macros; they stand in for the SDK header until it is added.

#include <revolution/types.h>
#include <climax/Frame.h>

/* A bounding sphere (RwSphere). Name invented. */
struct unk_RwSphere {
    unk_RwV3d   center;     // 0x0
    float       radius;     // 0xC
};

struct unk_RpAtomic;

/* An atomic's render callback (RpAtomicCallBackRender). */
typedef unk_RpAtomic* ( *unk_RpAtomicCallBackRender )( unk_RpAtomic* atomic );

/* An atomic (RpAtomic), up to its interpolator's flags. Name invented. */
struct unk_RpAtomic {
    u8                          type;               // 0x00, RwObject
    u8                          subType;            // 0x01
    u8                          flags;              // 0x02, rpATOMICCOLLISIONTEST, rpATOMICRENDER
    u8                          privateFlags;       // 0x03
    void*                       parent;             // 0x04, the frame
    void*                       lFrame[2];          // 0x08, RwLLLink in the frame's object list
    void*                       sync;               // 0x10, RwObjectHasFrame sync callback
    void*                       repEntry;           // 0x14
    void*                       geometry;           // 0x18
    unk_RwSphere                boundingSphere;     // 0x1C
    unk_RwSphere                worldBoundingSphere;    // 0x2C
    void*                       clump;              // 0x3C
    void*                       inClumpLink[2];     // 0x40, RwLLLink
    unk_RpAtomicCallBackRender  renderCallBack;     // 0x48
    s32                         interpolatorFlags;  // 0x4C, RpInterpolator flags
};

/* Atomic flags (rpATOMICRENDER) and interpolator flags (rpINTERPOLATORDIRTYSPHERE). */
#define ATOMIC_RENDER                   0x04
#define INTERPOLATOR_DIRTY_SPHERE       0x02

extern "C" {

// defined in 0x802D0464 (RenderWare), unit not yet reconstructed: RpAtomicCreate (C linkage, as
// RenderWare is C)
unk_RpAtomic*   RpAtomicCreate();
// defined in 0x802D1B20 (RenderWare), unit not yet reconstructed: RpAtomicSetFrame
unk_RpAtomic*   RpAtomicSetFrame( unk_RpAtomic* atomic, unk_RwFrame* frame );
// defined in 0x802CFD84 (RenderWare), unit not yet reconstructed: _rpAtomicResyncInterpolatedSphere
void            _rpAtomicResyncInterpolatedSphere( unk_RpAtomic* atomic );
// defined in 0x802CF178 (RenderWare), unit not yet reconstructed: AtomicDefaultRenderCallBack
unk_RpAtomic*   AtomicDefaultRenderCallBack( unk_RpAtomic* atomic );

}

/* The SDK's release macros (RpAtomicSetRenderCallBackMacro, RpAtomicSetFlagsMacro, RpAtomicGetBoundingSphereMacro). */
#define RpAtomicSetRenderCallBack( _atomic, _callback )         \
    do {                                                        \
        ( _atomic )->renderCallBack = ( _callback );            \
        if ( !( _atomic )->renderCallBack ) {                   \
            ( _atomic )->renderCallBack = AtomicDefaultRenderCallBack;          \
        }                                                       \
    } while ( 0 )

#define RpAtomicSetFlags( _atomic, _flags ) \
    ( ( _atomic )->flags = ( _flags ) )

#define RpAtomicGetBoundingSphere( _atomic )                                                \
    ( ( ( ( _atomic )->interpolatorFlags & INTERPOLATOR_DIRTY_SPHERE ) ?                    \
        _rpAtomicResyncInterpolatedSphere( _atomic ), 0 : 0 ), &( ( _atomic )->boundingSphere ) )

#endif
