#ifndef GAME_UNK_80088F54_H
#define GAME_UNK_80088F54_H

#include <revolution/types.h>
#include <revolution/mtx.h>
#include <climax/Camera.h>
#include <climax/Frame.h>
#include <game/unk_RespotManager.h>

/*
 * The source of a texture matrix, passed to fn_800891D8. Class name invented. Kind 1 multiplies
 * the frame's LTM into the base matrix; any other kind uses the base matrix alone.
 */
class unk_TexMtxSource {
public:
    u8                  kind;       // 0x0
    unk_RwFrame*        frame;      // 0x4, read when kind is 1
};

extern "C" {
unk_RespotManager* fn_800897A8();
u8                 fn_800897B0( unk_RespotManager* manager );
unk_RwMatrix*      fn_80089140( unk_RwFrame* frame );
unk_RwFrame*       fn_80089144( unk_RwCamera* camera );

void fn_80088F54();
void fn_80089034();
void* fn_8008914C( void* arg );
void* fn_800891D8( void* arg, unk_TexMtxSource* source );
void fn_8008930C();
void fn_800897B8();
void fn_800897C0();
void fn_800897C8();
void*              fn_800897D0( unk_RwCamera* camera );
void*              fn_800897D8( unk_RwCamera* camera );
void               fn_800897E0( unk_RwCamera* camera, void* frameBuffer );
void               fn_800897E8( unk_RwCamera* camera, void* zBuffer );
void*              fn_800897F0();
f32                fn_800897F8( f32 angle );
f32                fn_8008981C( f32 angle );
void               fn_80089840();

// RenderWare's texture-matrix converter and texture setter. rwsdk/world/climaxplatform.h declares
// them with the SDK's RwMatrix and RwTexture; that header is not included here (its bamatrix.h
// conflicts with climax/Frame.h), so the signatures are repeated over this unit's matrix type.
void RwClimaxRVLConvertMatrix( const unk_RwMatrix* rwMatrix, Mtx mtx );
void RwClimaxSetTexture( struct RwTexture* texture, s32 index );
}

// Data owned by this unit (defined in its .cpp).
extern u8            lbl_805296C0;
extern struct RwTexture* lbl_805299C4;  // defined elsewhere (the texture set by fn_8008914C)
extern void*        lbl_805299C0;  // an RwRaster, defined elsewhere (the raster the screen quad draws)
extern unk_RwMatrix  lbl_8046A598;
extern unk_RwMatrix  lbl_8046A5D8;
extern unk_RwCamera* lbl_805296C4;
extern u8            lbl_805296C1;
extern unk_RwCamera* lbl_805296C8;
extern u8            lbl_805296C2;
extern s32           lbl_8052726C;          // percent of the second texture matrix's scale

#endif
