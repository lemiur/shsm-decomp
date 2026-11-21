// Unit name: Gormiti link map (DWARF game/Objects/Effects/DynamicTrails.cpp) for AddTime and FindFirstAtomic; boundary and fn_8007F20C from the unit plan

#include <game/DynamicTrails.h>
#include <climax/RenderState.h>

unk_TrailNode* lbl_80529608;

/*
====================
fn_8007F20C

Draws the trail list: sets the depth, cull and blend states the trails share, draws each trail
through fn_8007F044, then leaves the cull mode set. Names invented; the caller passes param2 as 1.
====================
*/
// 0x8007F20C
void fn_8007F20C( void* param1, int param2 ) {
    if ( lbl_80529608 != NULL ) {
        SetDepthMode( DEPTHMODE_TESTONLY );
        SetCullMode( 0 );
        RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )rwFILTERLINEAR );
        SetBlendMode( BLENDMODE_SUBTRACT );
        for ( unk_TrailNode* node = lbl_80529608; node != NULL; node = node->next ) {
            fn_8007F044( node, param1, param2 );
        }
        SetCullMode( 1 );
    }
}

/*
====================
Effects::TrailAnimInterpolator::AddTime
====================
*/
// 0x8007F478
void Effects::TrailAnimInterpolator::AddTime( float deltaTime ) {
    // Clamp the step so the animation does not run past its end
    if ( deltaTime + m_CurrentTime > m_Anim->duration ) {
        deltaTime = m_Anim->duration - m_CurrentTime;
    }
    RtAnimInterpolatorAddAnimTime( m_Interpolator, deltaTime );
    m_CurrentTime += deltaTime;
    m_Interpolator->keyFrameApplyCB( m_Data, rtANIMGETINTERPFRAME( m_Interpolator, 0 ) );
}

/*
====================
Effects::FindFirstAtomic
====================
*/
// 0x8007F500
RpAtomic* Effects::FindFirstAtomic( RpAtomic* atomic, void* data ) {
    RpAtomic** first = (RpAtomic**)data;
    if ( *first == NULL ) {
        *first = atomic;
    }
    return atomic;
}
