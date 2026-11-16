#ifndef CLIMAX_CLUMP_H
#define CLIMAX_CLUMP_H

#include <revolution/types.h>
#include <climax/Frame.h>

/* RpAtomic (RenderWare 3.7): only handled through pointers here. */
struct unk_RpAtomic;

/* RpClump (RenderWare 3.7), up to its object header; only handled through pointers here. */
struct unk_RpClump {
    unk_RwObject    object;     // 0x0, its parent is the clump's root frame
};

/* The clump's root frame (RpClumpGetFrame; a macro in the RenderWare 3.7 SDK). */
inline unk_RwFrame* RpClumpGetFrame( const unk_RpClump* clump ) {
    return ( unk_RwFrame* )clump->object.parent;
}

/* RpAtomicCallBack: returns NULL to stop the iteration. */
typedef unk_RpAtomic* ( *unk_RpAtomicCallBack )( unk_RpAtomic* atomic, void* data );

extern "C" {

// defined in 0x802D0304 (RenderWare), unit not yet reconstructed: calls callBack on each atomic
// of clump until one returns NULL (presumed RpClumpForAllAtomics, by its use in climax/Utils)
unk_RpClump* RpClumpForAllAtomics( unk_RpClump* clump, unk_RpAtomicCallBack callBack, void* data );

// defined in 0x802D07EC (RenderWare), unit not yet reconstructed: a new clump made from clump
// (by its use in CClumpSwap; C linkage, as RenderWare is C)
unk_RpClump* RpClumpClone( unk_RpClump* clump );

// defined in 0x802D0B5C (RenderWare), unit not yet reconstructed: destroys a clump (by its use in
// CClumpSwap; C linkage)
void RpClumpDestroy( unk_RpClump* clump );

}

// defined in 0x801DD29C (Climax engine), unit not yet reconstructed: sets the alpha (clamped to
// 0-255) of the clump's atomics through RenderWare's atomic iterator (by its body); the meaning of
// index is unknown
void fn_801DD29C( unk_RpClump* clump, int alpha, int index );

#endif
