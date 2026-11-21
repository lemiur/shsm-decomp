// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80031F54.h>

/*
====================
fn_80031F54
====================
*/
// Not exact (99.47%): retail passes a third argument (0) to RwClimaxWorldRender, which the declared
// prototype in rwsdk/world/climaxplatform.h does not take.
// 0x80031F54
extern "C" void fn_80031F54( unk_RenderPass* self, void* arg ) {
    SetRefractionDirty__6CBloomFv();
    SetDepthMode( DEPTHMODE_ON );
    fn_800285BC( self, 0 );
    fn_800C07B4( self, 0 );
    RwClimaxWorldRender( self->field_0xA4, 0x28 );
    fn_80009628( self );
    fn_800C07B4( self, 2 );
    fn_8008E120( self, 1 );
    if ( self->field_0x150 != 0 ) {
        ClimaxT1ResetAlphaPass();
        RwClimaxAtomicRenderPass( 0x80, 0, 0 );
        fn_801D9DD0( 1 );
        for ( unk_RenderNode* node = lbl_80529638; node != NULL; node = node->field_0x34 ) {
            if ( node->field_0x54 != 0 && node->field_0x69 == 1 ) {
                fn_80097564( node );
            }
        }
        SetDepthMode( DEPTHMODE_ON );
        SetBlendMode( BLENDMODE_ALPHA );
    }
    ClimaxT1ResetAlphaPass();
    RwClimaxAtomicRenderPass( 0xD, 0, 0 );
    fn_800C07B4( self, 0xC );
    fn_8007F20C( arg, 1 );
    fn_800C07B4( self, 3 );
    RwClimaxAtomicRenderPass( 0x22, 0, 0 );
    fn_800C07B4( self, 0xC );
    fn_80083BD4( self );
    fn_8008E120( self, 0 );
    SetFogOff__11RenderStateFv( self );
}

/*
====================
fn_80032240
====================
*/
// 0x80032240
extern "C" void fn_80032240( unk_RenderPass* self, void* arg ) {
    fn_80031F54( self, arg );
}

/*
====================
fn_80032244
====================
*/
// 0x80032244
extern "C" void fn_80032244( unk_RenderPass* self, void* arg ) {
    fn_800285BC( self, 0 );
    fn_801D9DD0( 1 );
    SetFogOff__11RenderStateFv( self );
    fn_800927BC( arg );
}
