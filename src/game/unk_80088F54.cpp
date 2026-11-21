// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <math.h>
#include <game/unk_80088F54.h>
#include <rwsdk/badevice.h>
#include <climax/RenderState.h>
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGet.h>
#include <revolution/gx/GXTransform.h>

u8            lbl_805296C0;
unk_RwMatrix  lbl_8046A598;
unk_RwMatrix  lbl_8046A5D8;
unk_RwCamera* lbl_805296C4;
u8            lbl_805296C1;
unk_RwCamera* lbl_805296C8;
u8            lbl_805296C2;
s32           lbl_8052726C = 20;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .bss 0x8046A598 size 0x40 lbl_8046A598
//   .bss 0x8046A5D8 size 0x40 lbl_8046A5D8
//   .sdata 0x80527268 size 0x4 lbl_80527268
//   .sdata 0x8052726C size 0x4 lbl_8052726C
//   .sbss 0x805296BC size 0x4 lbl_805296BC
//   .sbss 0x805296C0 size 0x1 lbl_805296C0
//   .sbss 0x805296C1 size 0x1 lbl_805296C1
//   .sbss 0x805296C2 size 0x1 lbl_805296C2
//   .sbss 0x805296C4 size 0x4 lbl_805296C4
//   .sbss 0x805296C8 size 0x8 lbl_805296C8
//   .sdata2 0x8052C078 size 0x4 lbl_8052C078
//   .sdata2 0x8052C07C size 0x4 lbl_8052C07C
//   .sdata2 0x8052C080 size 0x4 lbl_8052C080
//   .sdata2 0x8052C084 size 0x4 lbl_8052C084
//   .sdata2 0x8052C088 size 0x4 lbl_8052C088
//   .sdata2 0x8052C08C size 0x4 lbl_8052C08C
//   .sdata2 0x8052C090 size 0x8 lbl_8052C090
//   .sdata2 0x8052C098 size 0x4 lbl_8052C098
//   .sdata2 0x8052C09C size 0x4 lbl_8052C09C
//   .sdata2 0x8052C0A0 size 0x4 lbl_8052C0A0
//   .sdata2 0x8052C0A4 size 0x4 lbl_8052C0A4
//   .sdata2 0x8052C0A8 size 0x4 lbl_8052C0A8
//   .sdata2 0x8052C0AC size 0x4 lbl_8052C0AC

/*
====================
fn_80088F54
====================
*/
// 0x80088F54
extern "C" void fn_80088F54() {
    f32 negHalf = -0.5f;
    f32 half = 0.5f;
    f32 proj[7];
    GXGetProjectionv( proj );
    unk_RwCamera* camera = (unk_RwCamera*)RWSRCGLOBAL( curCamera );

    // The view-space matrix from the projection values: right and up scaled by half, at offset by
    // half a unit, looking down -Z.
    unk_RwMatrix view;
    view.right.x = proj[1] * half;
    view.right.y = 0.0f;
    view.right.z = 0.0f;
    view.up.x = 0.0f;
    view.up.y = proj[3] * half;
    view.up.z = 0.0f;
    view.at.x = proj[2] + negHalf;
    view.at.y = proj[4] + negHalf;
    view.at.z = -1.0f;
    view.pos.x = 0.0f;
    view.pos.y = 0.0f;
    view.pos.z = 0.0f;
    view.flags = 0;

    RwMatrixMultiply( &lbl_8046A598, DisplayListInvCamLTM(), &view );
    lbl_805296C4 = camera;
    lbl_805296C1 = 1;
}

/*
====================
fn_80089034
====================
*/
// 0x80089034
extern "C" void fn_80089034() {
    f32 negHalf = -0.5f;
    f32 half = 0.5f;
    f32 quarter = 0.25f;
    f32 proj[7];
    GXGetProjectionv( proj );
    unk_RwCamera* camera = (unk_RwCamera*)RWSRCGLOBAL( curCamera );
    unk_RwMatrix* ltm = RwFrameGetLTM( RwCameraGetFrame( camera ) );
    f32 ltmOffset = negHalf + 0.5f * ltm->at.y;

    // The second view-space matrix: as the first, with up scaled by a quarter and the vertical
    // offset taken from the camera's own frame matrix.
    unk_RwMatrix view;
    view.right.x = proj[1] * half;
    view.right.y = 0.0f;
    view.right.z = 0.0f;
    view.up.x = 0.0f;
    view.up.y = proj[3] * quarter;
    view.up.z = 0.0f;
    view.at.x = proj[2] + negHalf;
    view.at.y = proj[4] + ltmOffset;
    view.at.z = -1.0f;
    view.pos.x = 0.0f;
    view.pos.y = 0.0f;
    view.pos.z = 0.0f;
    view.flags = 0;

    RwMatrixMultiply( &lbl_8046A5D8, DisplayListInvCamLTM(), &view );
    lbl_805296C8 = camera;
    lbl_805296C2 = 1;
}

/*
====================
fn_80089140
====================
*/
// 0x80089140
extern "C" unk_RwMatrix* fn_80089140( unk_RwFrame* frame ) {
    return RwFrameGetLTM( frame );
}

/*
====================
fn_80089144
====================
*/
// 0x80089144
extern "C" unk_RwFrame* fn_80089144( unk_RwCamera* camera ) {
    return RwCameraGetFrame( camera );
}

/*
====================
fn_8008914C
====================
*/
// 0x8008914C
extern "C" void* fn_8008914C( void* arg ) {
    if ( lbl_805296C0 != 0 ) {
        return arg;
    }
    if ( lbl_805296C1 == 0 || lbl_805296C4 != RWSRCGLOBAL( curCamera ) ) {
        fn_80088F54();
    }
    Mtx texMtx;
    RwClimaxRVLConvertMatrix( &lbl_8046A598, texMtx );
    GXLoadTexMtxImm( texMtx, GX_TEXMTX6, GX_MTX3x4 );
    RwClimaxSetTexture( lbl_805299C4, 1 );
    return arg;
}

/*
====================
fn_800891D8
====================
*/
// 0x800891D8
extern "C" void* fn_800891D8( void* arg, unk_TexMtxSource* source ) {
    if ( lbl_805296C0 != 0 ) {
        return arg;
    }
    fn_80089034();

    Mtx texMtx;
    if ( source->kind == 1 ) {
        unk_RwMatrix product;
        unk_RwMatrix* ltm = RwFrameGetLTM( source->frame );
        RwMatrixMultiply( &product, ltm, &lbl_8046A5D8 );
        RwClimaxRVLConvertMatrix( &product, texMtx );
    } else {
        RwClimaxRVLConvertMatrix( &lbl_8046A5D8, texMtx );
    }
    GXLoadTexMtxImm( texMtx, GX_TEXMTX6, GX_MTX3x4 );

    // The second texture matrix, a 2x4 one: the right, up and at columns of the display-list
    // inverse camera matrix, scaled by lbl_8052726C percent.
    f32 scale = (f32)lbl_8052726C / 100.0f;
    const unk_RwMatrix* inv = DisplayListInvCamLTM();
    Mtx tex2;
    tex2[0][0] = scale * inv->right.x;
    tex2[0][1] = scale * inv->up.x;
    tex2[0][2] = scale * inv->at.x;
    tex2[0][3] = 0.0f;
    tex2[1][0] = scale * inv->right.y;
    tex2[1][1] = scale * inv->up.y;
    tex2[1][2] = scale * inv->at.y;
    tex2[1][3] = 0.0f;
    GXLoadTexMtxImm( tex2, GX_TEXMTX5, GX_MTX2x4 );
    return arg;
}

/*
====================
fn_8008930C
====================
*/
// 0x8008930C
extern "C" void fn_8008930C() {
    // Parked: needs RwEngineInstance (RwGlobals layout), the zone object at CZone 0xA4 and 0x178 and the render-pass helpers (not reconstructed).
}

/*
====================
fn_800897A8
====================
*/
// 0x800897A8
extern "C" unk_RespotManager* fn_800897A8() {
    return lbl_80529260;
}

/*
====================
fn_800897B0
====================
*/
// 0x800897B0
extern "C" u8 fn_800897B0( unk_RespotManager* manager ) {
    return manager->field_0x40;
}

/*
====================
fn_800897B8
====================
*/
// 0x800897B8
extern "C" void fn_800897B8() {
    // Parked: reads field 0xA4 of a CZone; the CZone layout is not recovered (its unit is not reconstructed).
}

/*
====================
fn_800897C0
====================
*/
// 0x800897C0
extern "C" void fn_800897C0() {
    // Parked: reads field 0x154 of the player object, whose type is not recovered (RespotManager::fn_80030254 returns u32).
}

/*
====================
fn_800897C8
====================
*/
// 0x800897C8
extern "C" void fn_800897C8() {
    // Parked: reads field 0x178 of a CZone; the CZone layout is not recovered (its unit is not reconstructed).
}

/*
====================
fn_800897D0
====================
*/
// 0x800897D0
extern "C" void* fn_800897D0( unk_RwCamera* camera ) {
    return camera->frameBuffer;
}

/*
====================
fn_800897D8
====================
*/
// 0x800897D8
extern "C" void* fn_800897D8( unk_RwCamera* camera ) {
    return camera->zBuffer;
}

/*
====================
fn_800897E0
====================
*/
// 0x800897E0
extern "C" void fn_800897E0( unk_RwCamera* camera, void* frameBuffer ) {
    camera->frameBuffer = frameBuffer;
}

/*
====================
fn_800897E8
====================
*/
// 0x800897E8
extern "C" void fn_800897E8( unk_RwCamera* camera, void* zBuffer ) {
    camera->zBuffer = zBuffer;
}

/*
====================
fn_800897F0
====================
*/
// 0x800897F0
extern "C" void* fn_800897F0() {
    return lbl_805299C0;
}

/*
====================
fn_800897F8
====================
*/
// 0x800897F8
extern "C" f32 fn_800897F8( f32 angle ) {
    return sin( angle );
}

/*
====================
fn_8008981C
====================
*/
// 0x8008981C
extern "C" f32 fn_8008981C( f32 angle ) {
    return cos( angle );
}

/*
====================
fn_80089840
====================
*/
// Not matched (74%): the compiler folds the fog-colour switch that follows SetBlendMode; retail keeps it (wall: call set, control flow).
// 0x80089840
extern "C" void fn_80089840() {
    const f32 zero = 0.0f;
    const f32 one = 1.0f;
    const f32 width = 160.0f;
    const f32 height = 120.0f;
    RwIm2DVertex quad[4];
    unk_RwCamera* camera = (unk_RwCamera*)RWSRCGLOBAL( curCamera );
    const f32 nearZ = camera->nearPlane;

    // The 160 x 120 screen quad, white, at the near plane.
    RwIm2DVertexSetScreenX( &quad[0], zero );
    RwIm2DVertexSetScreenY( &quad[0], zero );
    RwIm2DVertexSetScreenZ( &quad[0], nearZ );
    RwIm2DVertexSetU( &quad[0], zero, one );
    RwIm2DVertexSetV( &quad[0], zero, one );
    RwIm2DVertexSetIntRGBA( &quad[0], 255, 255, 255, 255 );

    RwIm2DVertexSetScreenX( &quad[1], width );
    RwIm2DVertexSetScreenY( &quad[1], zero );
    RwIm2DVertexSetScreenZ( &quad[1], nearZ );
    RwIm2DVertexSetU( &quad[1], one, one );
    RwIm2DVertexSetV( &quad[1], zero, one );
    RwIm2DVertexSetIntRGBA( &quad[1], 255, 255, 255, 255 );

    RwIm2DVertexSetScreenX( &quad[2], width );
    RwIm2DVertexSetScreenY( &quad[2], height );
    RwIm2DVertexSetScreenZ( &quad[2], nearZ );
    RwIm2DVertexSetU( &quad[2], one, one );
    RwIm2DVertexSetV( &quad[2], one, one );
    RwIm2DVertexSetIntRGBA( &quad[2], 255, 255, 255, 255 );

    RwIm2DVertexSetScreenX( &quad[3], zero );
    RwIm2DVertexSetScreenY( &quad[3], height );
    RwIm2DVertexSetScreenZ( &quad[3], nearZ );
    RwIm2DVertexSetU( &quad[3], zero, one );
    RwIm2DVertexSetV( &quad[3], one, one );
    RwIm2DVertexSetIntRGBA( &quad[3], 255, 255, 255, 255 );

    SetDepthMode( DEPTHMODE_OFF );
    RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )rwFILTERLINEAR );
    SetCullMode( 0 );
    RwRenderStateSet( rwRENDERSTATETEXTURERASTER, lbl_805299C0 );
    RwRenderStateSet( rwRENDERSTATETEXTUREADDRESS, ( void* )rwTEXTUREADDRESSCLAMP );
    SetBlendMode( BLENDMODE_OPAQUE );
    RwIm2DRenderPrimitive( rwPRIMTYPETRIFAN, quad, 4 );
    SetBlendMode( BLENDMODE_ALPHA );
    RwRenderStateSet( rwRENDERSTATETEXTUREADDRESS, ( void* )rwTEXTUREADDRESSWRAP );
}
