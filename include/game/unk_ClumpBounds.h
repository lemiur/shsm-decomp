#ifndef GAME_UNK_CLUMPBOUNDS_H
#define GAME_UNK_CLUMPBOUNDS_H

// unit name unconfirmed: the unit is the unnamed range 0x80025460-0x8002598C (docs/game_units.md),
// not yet reconstructed; class and header names invented.

#include <climax/Clump.h>

struct unk_ClumpBoundsNode;     // one atomic of the clump with its bounding sphere

/*
 * The bounding spheres of the atomics of a clump, scaled by one factor: fn_80025674 makes one for a
 * clump (NULL when the clump has no atomics), fn_80025750 refits each atomic's sphere to its
 * geometry and the scale. Size 0xC. Member names invented.
 */
class unk_ClumpBounds {
public:
                            ~unk_ClumpBounds();

    // defined in 0x80025750, unit not yet reconstructed
    void                    fn_80025750();

    unk_RpClump*            clump;      // 0x0
    float                   scale;      // 0x4, 1.0 when made
    unk_ClumpBoundsNode*    nodes;      // 0x8, the atomics
};

// defined in 0x80025674, unit not yet reconstructed
unk_ClumpBounds*    fn_80025674( unk_RpClump* clump );

#endif
