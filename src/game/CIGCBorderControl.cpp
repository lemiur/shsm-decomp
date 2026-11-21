// CIGCBorderControl: class name from the registration strings lbl_803E9C08 and lbl_803E9C1C and
// from the debug strings "CIGCBorderControl ON/OFF [%s]: Recieve %s". The border state singleton
// (unk_BorderState) and its per-frame handler follow the behaviour's functions in .text.

#include <game/CIGCBorderControl.h>
#include <game/DisplayHeight.h>
#include <game/Log.h>
#include <game/unk_80491B38.h>
#include <game/unk_MessageInbox.h>
#include <climax/RenderState.h>
#include <climax/Camera.h>

float lbl_805272F0 = 1.0f;     // scale of the border's fade step, 1.0 (name invented)

// the height of the border bar: 7.5 percent of the display height (name invented)
static float borderHeight = 7.5f * ( ( float )GetDisplayHeight() / 100.0f );
unk_BorderState* lbl_8052984C;
u32 lbl_80529850 = fn_801A7A84( "CIGCBorderControl" );
unk_801A8F6C lbl_80529854( "CIGCBorderControl", CIGCBorderControl::fn_800A8F88, sizeof( CIGCBorderControl ) );

/*
====================
fn_800A8644
====================
*/
// 0x800A8644
void fn_800A8644() {
    if ( lbl_8052984C == NULL ) {
        lbl_8052984C = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_BorderState;
    }
}

/*
====================
unk_BorderState::~unk_BorderState
====================
*/
// 0x800A8E58
unk_BorderState::~unk_BorderState() {
    lbl_80529D20->fn_800E97C8( ( CBaseBehaviour* )this );
    fn_801A9E00( lbl_8052A828 );
}

/*
====================
fn_800A86A8

Sets the border direction, on or off, and notifies the inbox.
====================
*/
// 0x800A86A8
void fn_800A86A8( bool on ) {
    unk_BorderState* state = lbl_8052984C;
    if ( state->direction == 0.0f ) {
        state->fn_801A9DE8( lbl_8052A828, NULL, 0xf );
    }
    if ( on ) {
        state->direction = 1.0f;
        lbl_80529D20->fn_800E97C0( ( CBaseBehaviour* )state, 0xd, false, false );
    } else {
        state->direction = -1.0f;
        lbl_80529D20->fn_800E97C8( ( CBaseBehaviour* )state );
    }
}

/*
====================
CIGCBorderControl::~CIGCBorderControl
====================
*/
// 0x800A8740
CIGCBorderControl::~CIGCBorderControl() {
    fn_801A9E00( ids[0] );
    fn_801A9940( ids[0] );
    fn_801A9E00( ids[1] );
    fn_801A9940( ids[1] );
}

/*
====================
CIGCBorderControl::fn_801A73A8

Takes the names of the ON and OFF messages from the attribute packet.
====================
*/
// 0x800A87F4
void CIGCBorderControl::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529850 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetOnMsg:
            fn_801AA218( ids[0], it->GetString(), 0 );
            break;
        case CMD_SetOffMsg:
            fn_801AA218( ids[1], it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

/*
====================
CIGCBorderControl::fn_8009CB1C

Turns the border on or off when the ON or OFF message arrives.
====================
*/
// 0x800A88B4
void CIGCBorderControl::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == ids[0].record ) {
        fn_80028D0C( 2, "CIGCBorderControl ON [%s]: Recieve %s", NULL,
                     ids[0].record ? ids[0].record->name : NULL );
        unk_BorderState* state = lbl_8052984C;
        if ( state->direction == 0.0f ) {
            state->fn_801A9DE8( lbl_8052A828, NULL, 0xf );
        }
        state->direction = 1.0f;
        lbl_80529D20->fn_800E97C0( ( CBaseBehaviour* )state, 0xd, false, false );
    } else if ( msg.record == ids[1].record ) {
        fn_80028D0C( 2, "CIGCBorderControl OFF [%s]: Recieve %s", NULL,
                     ids[1].record ? ids[1].record->name : NULL );
        unk_BorderState* state = lbl_8052984C;
        if ( state->direction == 0.0f ) {
            state->fn_801A9DE8( lbl_8052A828, NULL, 0xf );
        }
        state->direction = -1.0f;
        lbl_80529D20->fn_800E97C8( ( CBaseBehaviour* )state );
    }
}

/*
====================
unk_BorderState::fn_8009CB1C

The per-frame handler: moves the border's fade towards its direction and draws the two bars
over the top and bottom of the screen.
====================
*/
// Parked (register/schedule): the state reads and the draw calls match; the fade values take other
// FPRs and the zero another GPR than retail, so the unit stays NonMatching.
// 0x800A89E8
void unk_BorderState::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record != lbl_8052A828.record ) {
        return;
    }
    if ( direction != 0.0f ) {
        alpha += lbl_805272F0 * ( lbl_80491B38.field_0x4 * direction );
        if ( alpha > 1.0f ) {
            alpha = 1.0f;
        }
        if ( alpha < 0.0f ) {
            alpha = 0.0f;
            direction = 0.0f;
        }

        u32 textureAddress;
        u32 textureFilter;
        RwRenderStateGet( rwRENDERSTATETEXTUREFILTER, &textureFilter );
        RwRenderStateGet( rwRENDERSTATETEXTUREADDRESS, &textureAddress );
        RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )2 );
        RwRenderStateSet( rwRENDERSTATETEXTUREADDRESS, ( void* )3 );
        lbl_8052A6E4 = 0;
        RwRenderStateSet( ( RwRenderState )14, ( void* )0 );
        SetDepthMode( DEPTHMODE_OFF );
        SetBlendMode( BLENDMODE_ALPHA );
        RwRenderStateSet( rwRENDERSTATETEXTURERASTER, ( void* )0 );

        float z = ( ( unk_RwCamera* )RWSRCGLOBAL( curCamera ) )->nearPlane;
        u8 a = ( u8 )( s32 )( 255.0f * alpha );
        float t = ( 1.0f - alpha ) * borderHeight;
        RwIm2DVertex verts[4];

        // the top bar: its lower edge slides down with the fade
        verts[0].x = 0.0f;   verts[0].y = -t;                    verts[0].z = z;
        verts[1].x = 0.0f;   verts[1].y = borderHeight - t;      verts[1].z = z;
        verts[2].x = 640.0f; verts[2].y = -t;                    verts[2].z = z;
        verts[3].x = 640.0f; verts[3].y = borderHeight - t;      verts[3].z = z;
        for ( int i = 0; i < 4; i++ ) {
            verts[i].r = 0;
            verts[i].g = 0;
            verts[i].b = 0;
            verts[i].a = a;
        }
        RwIm2DRenderPrimitive( rwPRIMTYPETRISTRIP, verts, 4 );

        // the bottom bar: its upper edge slides up with the fade
z = ( ( unk_RwCamera* )RWSRCGLOBAL( curCamera ) )->nearPlane;
        float top = t + ( ( float )GetDisplayHeight() - borderHeight );
        float bottom = top + borderHeight;
        verts[0].x = 0.0f;   verts[0].y = top;      verts[0].z = z;
        verts[1].x = 0.0f;   verts[1].y = bottom;   verts[1].z = z;
        verts[2].x = 640.0f; verts[2].y = top;      verts[2].z = z;
        verts[3].x = 640.0f; verts[3].y = bottom;   verts[3].z = z;
        for ( int i = 0; i < 4; i++ ) {
            verts[i].r = 0;
            verts[i].g = 0;
            verts[i].b = 0;
            verts[i].a = a;
        }
        RwIm2DRenderPrimitive( rwPRIMTYPETRISTRIP, verts, 4 );

        RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )textureFilter );
        RwRenderStateSet( rwRENDERSTATETEXTUREADDRESS, ( void* )textureAddress );
    } else {
        fn_801A9E00( lbl_8052A828 );
    }
}
