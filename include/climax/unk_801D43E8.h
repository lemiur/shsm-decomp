#ifndef CLIMAX_UNK_801D43E8_H
#define CLIMAX_UNK_801D43E8_H

// unit name unconfirmed: header named after the function's address (unit 0x801D296C, not yet
// reconstructed).

#include <climax/Frame.h>

/*
 * What fn_801D43E8 finds along its line: the hit point first (its callers read it back); the
 * rest is unknown. The size is the room CPsyPointOfInterest's frame leaves it. Name invented.
 */
struct unk_LineHit {
    unk_RwV3d   point;          // 0x0
    u8          field_0xC[0x1C];
};

/*
 * Defined in 0x801D43E8, unit not yet reconstructed: tests the segment from start to end against
 * the collision of world (the word fn_800276D0 reads from a zone), filling hit; flag and types
 * (a mask: 0xE, 0xA at the call sites) select what is tested. Nonzero when something is hit.
 * fn_800277E8 wraps it for a zone.
 */
int     fn_801D43E8( u32 world, const unk_RwV3d* start, const unk_RwV3d* end, unk_LineHit* hit, bool flag, int types );

#endif
