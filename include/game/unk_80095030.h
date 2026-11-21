#ifndef GAME_UNK_80095030_H
#define GAME_UNK_80095030_H

#include <revolution/types.h>
#include <rwsdk/bacamera.h>
#include <rwsdk/baraster.h>
#include <rwsdk/world/baclump.h>
#include <climax/TevCBMgr.h>

/*
====================
unk_RenderPass

One render pass of a clump: the camera it is drawn with, the clump and the raster the camera image is
flushed to. Passes form a list through their next pointer, ordered by the priority at 0x10. Class name
invented; the layout is read from fn_80095030 and its caller fn_80094E6C only.
====================
*/
struct unk_RenderPass {
    char                field_0x0[0x10];
    float               priority;           // 0x10, compared against 0.07f by fn_80094E6C
    char                field_0x14[0x14];
    RwCamera*           camera;             // 0x28
    char                field_0x2C[0x1C];
    RpClump*            clump;              // 0x48
    char                field_0x4C[0xC];
    RwRaster*           raster;             // 0x58
    char                field_0x5C[0x10];
    unk_RenderPass*     next;               // 0x6C
};

// The TEV handle fn_80095030 registers its callbacks on. Retail's object is 0x18 bytes; only the
// handle's leading 0x10 bytes are read here. Owner unit not yet identified.
extern RenderState::cTevCBHandle lbl_8046A6E8;

extern "C" {

// Defined in the unit that holds fn_80094E6C (not reconstructed): the post-TEV callback.
unsigned int fn_80094F40( StTevSetupData* tevSetupData );

// Defined in the same unit (not reconstructed): the atomic callback of fn_80095030.
RpAtomic* fn_80094FFC( RpAtomic* atomic, void* data );

// Defined in the auto unit of fn_802038A8 (not reconstructed): called between camera begin and the TEV set-up.
void fn_802038A8( void );

// RenderWare's GameCube camera texture flush (not reconstructed): copies the camera image to a raster.
void RwGameCubeCameraTextureFlushWithCopyFormat( RwRaster* raster, RwInt32 copyFormat, RwBool flush );

}

#endif
