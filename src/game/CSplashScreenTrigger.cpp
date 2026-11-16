// CSplashScreenTrigger: class name from the registration strings lbl_803ED058 and lbl_803ED070.
// The inline factory fn_800D00AC and the two thunks follow the static initialiser in .text.

#include <game/CSplashScreenTrigger.h>
#include <game/unk_80491B38.h>
#include <game/Display.h>
#include <climax/RenderState.h>

u32 lbl_80529BD8 = fn_801A7A84( "CSplashScreenTrigger" );
unk_801A8F6C lbl_80529BDC( "CSplashScreenTrigger", CSplashScreenTrigger::fn_800D00AC, sizeof( CSplashScreenTrigger ) );

/*
====================
CSplashScreenTrigger::CSplashScreenTrigger

Inlined into the factory fn_800D00AC; the out-of-line copy is dead-stripped by the linker (no address). Compiled
first, it creates the unit's first two .sdata2 constants (0.0f, 1.0f), ahead of the attribute handler's.
====================
*/
CSplashScreenTrigger::CSplashScreenTrigger() {
    imageName = NULL;
    raster = NULL;
    fade = 0.0f;
    duration = 1.0f;
}

/*
====================
CSplashScreenTrigger::~CSplashScreenTrigger
====================
*/
// 0x800CF95C
CSplashScreenTrigger::~CSplashScreenTrigger() {
    fn_801A9E00( showMsg );
    fn_801A9940( showMsg );
    if ( fn_801AA17C( lbl_8052A828 ) ) {
        fn_801A9E00( lbl_8052A828 );
    }
}

/*
====================
CSplashScreenTrigger::fn_801A73A8

Takes the image name, the name of the message and the fade duration from the attribute packet.
====================
*/
// 0x800CFA10
void CSplashScreenTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529BD8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetImageName:
            imageName = it->GetString();
            break;
        case CMD_SetShowMsg:
            fn_801AA218( showMsg, it->GetString(), 0 );
            break;
        case CMD_SetDuration:
            duration = it->GetFloat();
            break;
        }
        ++it;
    }
    duration = ( 0.1f > duration ) ? 0.1f : duration;
    duration = ( 20.0f < duration ) ? 20.0f : duration;
}

/*
====================
CSplashScreenTrigger::LoadRaster

Reads the image into a texture raster; false when the file or the image is missing.
====================
*/
inline bool CSplashScreenTrigger::LoadRaster() {
    bool loaded = false;
    unk_File* file = fn_801B32C8( imageName, 0, 0 );
    if ( file ) {
        RwImage* image = fn_801B5EB0( file );
        fn_801B37E4( file );
        if ( image ) {
            raster = RwRasterCreate( image->width, image->height, image->depth, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 );
            RwRasterSetFromImage( raster, image );
            RwImageDestroy( image );
            loaded = true;
        }
    }
    return loaded;
}

/*
====================
CSplashScreenTrigger::fn_8009CB1C

Loads the image as a texture when the message arrives, and fades it in a step each frame after that.
====================
*/
// 0x800CFB10
void CSplashScreenTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == showMsg.record ) {
        if ( LoadRaster() ) {
            fn_801A9DE8( lbl_8052A828 );
        }
    } else if ( msg.record == lbl_8052A828.record ) {
        fade += 1.0f / duration * lbl_80491B38.field_0x8;
        if ( fade > 1.0f ) {
            fade = 1.0f;
        }
        fn_800CFC40( fade );
    }
}

/*
====================
CSplashScreenTrigger::fn_800CFC40

Draws the image over the whole screen with the alpha fade * 255, with depth and culling off.
====================
*/
// 0x800CFC40
void CSplashScreenTrigger::fn_800CFC40( float fade ) {
    float width = 640.0f;
    float height = lbl_8052861C;
    u8 alpha = ( u8 )( 255.0f * fade );
    float left = 0.0f;
    float top = 0.0f;
    float z = RwIm2DGetNearScreenZ();
    if ( alpha ) {
        u32 oldCull = lbl_80528870;
        RwIm2DVertex vertices[4];
        SetVertex( &vertices[0], left, top, z, 0.0f, 0.0f, alpha );
        SetVertex( &vertices[1], left, top + height, z, 0.0f, 1.0f, alpha );
        SetVertex( &vertices[2], left + width, top, z, 1.0f, 0.0f, alpha );
        SetVertex( &vertices[3], left + width, top + height, z, 1.0f, 1.0f, alpha );

        SetCullMode( 0 );
        SetBlendMode( BLENDMODE_ALPHA );
        RwRenderStateSet( rwRENDERSTATESTENCILENABLE, ( void* )0 );
        RwRenderStateSet( rwRENDERSTATETEXTURERASTER, raster );
        SetDepthMode( DEPTHMODE_OFF );
        RwIm2DRenderPrimitive( rwPRIMTYPETRISTRIP, vertices, 4 );
        RwRenderStateSet( rwRENDERSTATESTENCILENABLE, ( void* )1 );
        SetDepthMode( DEPTHMODE_ON );
        SetCullMode( oldCull );
    }
}
