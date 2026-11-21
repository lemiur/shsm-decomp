#ifndef GAME_UNK_CLUMPBOUNDS_H
#define GAME_UNK_CLUMPBOUNDS_H

// unit name unconfirmed: the unit is the unnamed range 0x80025460-0x8002598C (docs/game_units.md),
// not yet reconstructed; class and header names invented. symbols.txt names the class of
// CheckAtomic__14CBoundsUpdaterFP8RpAtomic CBoundsUpdater: the rename is pending.

#include <revolution/types.h>
#include <climax/Clump.h>

struct unk_RpHAnimHierarchy;

/*
 * One bone of an atomic's skin: the bone's matrices and the weight of its sphere. Size 0x10.
 * Member names invented.
 */
struct unk_ClumpBoundsSphere {
    unk_RwMatrix*           boneMatrix;     // 0x0, the bone's matrix in the hierarchy
    unk_RwMatrix*           skinMatrix;     // 0x4, the skin-to-bone matrix of the bone
    u32                     field_0x8;      // 0x8, not written by CheckAtomic
    float                   weight;         // 0xC
};

/*
 * One atomic of the clump with its bones' spheres, made by CheckAtomic. Size 0x14. Member names
 * invented.
 */
struct unk_ClumpBoundsNode {
    unk_ClumpBoundsNode*    next;           // 0x0
    unk_RpAtomic*           atomic;         // 0x4
    unk_RpHAnimHierarchy*   hierarchy;      // 0x8
    unk_ClumpBoundsSphere*  spheres;        // 0xC, count entries of 0x10 bytes
    u32                     count;          // 0x10, the bones of the skin
};

/*
 * The bounding spheres of the atomics of a clump, scaled by one factor: fn_80025674 makes one for a
 * clump (NULL when the clump has no atomics), fn_80025750 refits each atomic's sphere to its
 * geometry and the scale. Size 0xC. Member names invented.
 */
class unk_ClumpBounds {
public:
                            unk_ClumpBounds() { clump = NULL; nodes = NULL; scale = 1.0f; }
                            ~unk_ClumpBounds();

    // defined in 0x80025750, unit not yet reconstructed
    void                    fn_80025750();

    unk_RpClump*            clump;      // 0x0
    float                   scale;      // 0x4, 1.0 when made
    unk_ClumpBoundsNode*    nodes;      // 0x8, the atomics
};

// defined in 0x8002563C, unit not yet reconstructed: the RpClumpForAllAtomics callback of
// fn_80025674; its data is the bounds, which it passes to CheckAtomic (C linkage, as the symbol)
extern "C" unk_RpAtomic* fn_8002563C( unk_RpAtomic* atomic, void* data );

// defined in 0x80025674, unit not yet reconstructed
unk_ClumpBounds*    fn_80025674( unk_RpClump* clump );

#endif
