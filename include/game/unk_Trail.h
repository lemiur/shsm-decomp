#ifndef GAME_UNK_TRAIL_H
#define GAME_UNK_TRAIL_H

// unit name unconfirmed: the unit's class name is invented (see unk_Trail below); rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <revolution/types.h>
#include <rwsdk/baraster.h>
#include <rwsdk/world/baclump.h>

/*
 * The trail object fn_80081858 fills in. The name is invented: symbols.txt names Effects::Trail
 * beside this code, but these two functions do not prove that they are its members. Only the
 * raster member these functions write is typed; the rest of the object is not yet known.
 */
struct unk_Trail {
    u8          field_0x00[0x3C];   // 0x00, not yet known
    RwRaster*   raster;             // 0x3C, the raster of the trail placeholder's texture
};

extern "C" {

// defined in 0x801D9074, unit not yet reconstructed: sets up the new trail atomic (by its use in
// fn_80081858)
void fn_801D9074( RpAtomic* atomic );

// defined in 0x800817C0 (this unit): the RpClumpForAllAtomics callback that finds the trail placeholder
RpAtomic* fn_800817C0( RpAtomic* atomic, void* data );

// defined in 0x80081858 (this unit): builds the trail clump from the source clump. The caller
// passes a flag and 0x200 that this body does not read.
RpClump* fn_80081858( unk_Trail* trail, u8 flag, RpClump* source, s32 unused );

}

#endif
