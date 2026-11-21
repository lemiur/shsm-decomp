// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80095030.h>
#include <rwsdk/world/baworobj.h>

/*
====================
fn_80095030

Draws one render pass: clears the pass's camera, sets the TEV callbacks of its clump's atomics and
flushes the camera image. Does nothing for a pass without a clump or whose clump has no world.
====================
*/
// 0x80095030
void fn_80095030( unk_RenderPass* pass ) {
    if ( pass->clump != NULL && RpClumpGetWorld( pass->clump ) != NULL ) {
        RwRGBA clearColour;

        clearColour.red = 0;
        clearColour.green = 0;
        clearColour.blue = 0;
        clearColour.alpha = 0;
        RwCameraClear( pass->camera, &clearColour, rwCAMERACLEARIMAGE | rwCAMERACLEARZ );
        RwCameraBeginUpdate( pass->camera );
        fn_802038A8();
        lbl_8046A6E8.SetCallBacks( NULL, NULL, fn_80094F40 );
        RpClumpForAllAtomics( pass->clump, fn_80094FFC, NULL );
        lbl_8046A6E8.SetCallBacks( NULL, NULL, NULL );
        RwCameraEndUpdate( pass->camera );
        RwGameCubeCameraTextureFlushWithCopyFormat( pass->raster, 0x28, TRUE );
    }
}
