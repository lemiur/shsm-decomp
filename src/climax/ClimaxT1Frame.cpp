// Unit name and boundary: Gormiti link map (ClimaxT1Frame.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1Frame.obj).
// ClimaxT1FramePrimeOffset, ClimaxT1FrameSetMode, ClimaxT1FrameGetMode, ClimaxT1FrameSetPos,
// ClimaxT1FrameGetPos, ClimaxT1FrameSetScale, ClimaxT1FrameGetScale, ClimaxT1FrameScaleCB and
// ClimaxT1FrameScale have no copy in retail (dead-stripped).
// ClimaxT1FrameUpdateCB: retail's body differs from Gormiti's (another revision: it uses the parent's matrix as it is, with
// no inverse of it).
/*
 * ClimaxT1Frame.cpp: the ClimaxT1 plugin of frames. A frame can face the camera: its mode says
 * whether and how, and the plugin keeps its position and scale. The update rebuilds the matrices
 * of the frames below a frame from the camera's orientation.
 */

#include <climax/ClimaxT1Frame.h>
#include <rwsdk/babinfrm.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>

// The plugin ID of the frame plugin.
#define CLIMAXT1_FRAME_PLUGIN_ID 0xA0B

// The modes of a frame: 0 leaves the frame as it is, the others make it face the camera.
#define CLIMAXT1_FRAME_MODE_FIXED 0
#define CLIMAXT1_FRAME_MODE_BILLBOARD 1
#define CLIMAXT1_FRAME_MODE_BILLBOARD_UPRIGHT 2
#define CLIMAXT1_FRAME_MODE_CAMERA 3

static const RwV3d ZERO = { 0.0f, 0.0f, 0.0f };

// The turn of the camera's orientation that the frames face: the camera's right, up and at flipped.
static const RwV3d c2f_right = { -1.0f, 0.0f, 0.0f };
static const RwV3d c2f_up = { 0.0f, 1.0f, 0.0f };
static const RwV3d c2f_at = { 0.0f, 0.0f, -1.0f };
static const RwV3d c2f_pos = { 0.0f, 0.0f, 0.0f };
static RwMatrix c2f;

static int LocalOffset = -1;
static int ClimaxT1FrameNotSetUp = 1;

// The plugin data of a frame.
#define CLIMAXT1FRAME( _frame ) RWPLUGINOFFSET( ClimaxT1Frame, ( _frame ), LocalOffset )

static void* ClimaxT1FrameDataConstructor( void* frame, int offset, int size );
static void* ClimaxT1FrameDataDestructor( void* frame, int offset, int size );
static void* ClimaxT1FrameDataCopier( void* dstFrame, const void* srcFrame, int offset, int size );
static RwStream* ClimaxT1FrameDataReadStream( RwStream* stream, int binaryLength, void* frame, int offset, int size );
static RwStream* ClimaxT1FrameDataWriteStream( RwStream* stream, int binaryLength, const void* frame, int offset,
                                               int size );
static int ClimaxT1FrameDataGetStreamSize( const void* frame, int offset, int size );
static RwFrame* ClimaxT1FrameUpdateCB( RwFrame* frame, void* data );
static RwFrame* ClimaxT1FrameHasFacingFramesCB( RwFrame* pFrame, void* data );

/*
====================
ClimaxT1FrameDataConstructor
====================
*/
// 0x801DDE04
static void* ClimaxT1FrameDataConstructor( void* frame, int offset, int size ) {
    if ( LocalOffset > 0 ) {
        CLIMAXT1FRAME( frame )->mode = CLIMAXT1_FRAME_MODE_FIXED;
        CLIMAXT1FRAME( frame )->pos = ZERO;
        CLIMAXT1FRAME( frame )->scale = 1.0f;
    }
    return frame;
}

/*
====================
ClimaxT1FrameDataDestructor
====================
*/
// 0x801DDE54
static void* ClimaxT1FrameDataDestructor( void* frame, int offset, int size ) {
    return frame;
}

/*
====================
ClimaxT1FrameDataCopier
====================
*/
// 0x801DDE58
static void* ClimaxT1FrameDataCopier( void* dstFrame, const void* srcFrame, int offset, int size ) {
    CLIMAXT1FRAME( dstFrame )->mode = RWPLUGINOFFSET( const ClimaxT1Frame, srcFrame, LocalOffset )->mode;
    CLIMAXT1FRAME( dstFrame )->pos = RWPLUGINOFFSET( const ClimaxT1Frame, srcFrame, LocalOffset )->pos;
    CLIMAXT1FRAME( dstFrame )->scale = RWPLUGINOFFSET( const ClimaxT1Frame, srcFrame, LocalOffset )->scale;
    return dstFrame;
}

/*
====================
ClimaxT1FrameDataReadStream

The scale is not streamed.
====================
*/
// 0x801DDEA0
static RwStream* ClimaxT1FrameDataReadStream( RwStream* stream, int binaryLength, void* frame, int offset, int size ) {
    ClimaxT1Frame binaryData;

    if ( !RwStreamReadInt32( stream, (RwInt32*)&binaryData.mode, sizeof( int ) ) ||
         !RwStreamReadReal( stream, &binaryData.pos.x, sizeof( float ) ) ||
         !RwStreamReadReal( stream, &binaryData.pos.y, sizeof( float ) ) ||
         !RwStreamReadReal( stream, &binaryData.pos.z, sizeof( float ) ) ) {
        return NULL;
    }
    if ( binaryData.mode >= CLIMAXT1_FRAME_MODE_FIXED && binaryData.mode <= CLIMAXT1_FRAME_MODE_CAMERA ) {
        CLIMAXT1FRAME( frame )->mode = binaryData.mode;
    }
    CLIMAXT1FRAME( frame )->pos = binaryData.pos;
    return stream;
}

/*
====================
ClimaxT1FrameDataWriteStream
====================
*/
// 0x801DDF70
static RwStream* ClimaxT1FrameDataWriteStream( RwStream* stream, int binaryLength, const void* frame, int offset,
                                               int size ) {
    ClimaxT1Frame binaryData;

    binaryData.mode = RWPLUGINOFFSET( const ClimaxT1Frame, frame, LocalOffset )->mode;
    binaryData.pos = RWPLUGINOFFSET( const ClimaxT1Frame, frame, LocalOffset )->pos;
    if ( !RwStreamWriteInt32( stream, (RwInt32*)&binaryData.mode, sizeof( int ) ) ||
         !RwStreamWriteReal( stream, &binaryData.pos.x, sizeof( float ) ) ||
         !RwStreamWriteReal( stream, &binaryData.pos.y, sizeof( float ) ) ||
         !RwStreamWriteReal( stream, &binaryData.pos.z, sizeof( float ) ) ) {
        return NULL;
    }
    return stream;
}

/*
====================
ClimaxT1FrameDataGetStreamSize
====================
*/
// 0x801DE028
static int ClimaxT1FrameDataGetStreamSize( const void* frame, int offset, int size ) {
    return sizeof( int ) + sizeof( RwV3d );
}

/*
====================
ClimaxT1FramePluginAttach

Registers the plugin with frames and their streams. Returns false when either fails.
====================
*/
// 0x801DE030
extern "C" int ClimaxT1FramePluginAttach() {
    int offset;

    LocalOffset = RwFrameGetPluginOffset( CLIMAXT1_FRAME_PLUGIN_ID );
    if ( LocalOffset >= 0 ) {
        return TRUE;
    }
    LocalOffset = RwFrameRegisterPlugin( sizeof( ClimaxT1Frame ), CLIMAXT1_FRAME_PLUGIN_ID,
                                         ClimaxT1FrameDataConstructor, ClimaxT1FrameDataDestructor,
                                         ClimaxT1FrameDataCopier );
    if ( LocalOffset < 0 ) {
        return FALSE;
    }
    offset = RwFrameRegisterPluginStream( CLIMAXT1_FRAME_PLUGIN_ID, ClimaxT1FrameDataReadStream,
                                          ClimaxT1FrameDataWriteStream, ClimaxT1FrameDataGetStreamSize );
    return offset == LocalOffset;
}

/*
====================
ClimaxT1FrameUpdateCB

Faces a frame to the camera as its mode says, then goes on to its children. The camera's orientation
and position are in data.
====================
*/
// 0x801DE0D0
static RwFrame* ClimaxT1FrameUpdateCB( RwFrame* frame, void* data ) {
    // one and two are locals of Gormiti's ClimaxT1FrameUpdateCB (DWARF); nothing reads them in retail either.
    RwV3d one = { 0.0f, 1.0f, 0.0f };
    RwV3d two = { 1.0f, 0.0f, 0.0f };
    ClimaxT1FrameUpdateCBData* cbData;
    RwFrame* parent;
    RwMatrix* fmdl;
    RwMatrix* fltm;
    RwMatrix* pltm;
    RwMatrix* tmatrix;
    RwMatrix ymatrix;
    RwV3d* pos;
    RwV3d* tpos;
    int mode;
    RwV3d scale;

    if ( frame != NULL && ( parent = RwFrameGetParent( frame ) ) != NULL ) {
        pltm = RwFrameGetLTM( parent );
        fmdl = RwFrameGetMatrix( frame );
        fltm = RwFrameGetLTM( frame );
        cbData = (ClimaxT1FrameUpdateCBData*)data;
        tmatrix = &cbData->tmatrix;
        tpos = &cbData->tpos;

        mode = CLIMAXT1FRAME( frame )->mode;
        switch ( mode ) {
        case CLIMAXT1_FRAME_MODE_FIXED:
            RwMatrixMultiply( fltm, fmdl, pltm );
            break;
        case CLIMAXT1_FRAME_MODE_BILLBOARD:
            pos = &CLIMAXT1FRAME( frame )->pos;
            RwV3dTransformPoint( &tmatrix->pos, pos, pltm );
            ymatrix.pos = tmatrix->pos;
            ymatrix.up = tmatrix->up;
            RwV3dSubMacro( &ymatrix.at, tpos, &tmatrix->pos );
            RwV3dNormalize( &ymatrix.at, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.right, &ymatrix.up, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.up, &ymatrix.at, &ymatrix.right );
            RwV3dNormalize( &ymatrix.right, &ymatrix.right );
            RwV3dNormalize( &ymatrix.up, &ymatrix.up );
            scale.x = scale.y = scale.z = CLIMAXT1FRAME( frame )->scale;
            RwMatrixScale( &ymatrix, &scale, rwCOMBINEPRECONCAT );
            RwMatrixCopy( fmdl, &ymatrix );
            RwMatrixCopy( fltm, &ymatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        case CLIMAXT1_FRAME_MODE_BILLBOARD_UPRIGHT:
            pos = &CLIMAXT1FRAME( frame )->pos;
            RwV3dTransformPoint( &tmatrix->pos, pos, pltm );
            ymatrix.pos = tmatrix->pos;
            ymatrix.up = c2f_up;
            RwV3dSubMacro( &ymatrix.at, tpos, &tmatrix->pos );
            ymatrix.at.y = 0.0f;
            RwV3dNormalize( &ymatrix.at, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.right, &ymatrix.up, &ymatrix.at );
            scale.x = scale.y = scale.z = CLIMAXT1FRAME( frame )->scale;
            RwMatrixScale( &ymatrix, &scale, rwCOMBINEPRECONCAT );
            RwMatrixCopy( fmdl, &ymatrix );
            RwMatrixCopy( fltm, &ymatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        case CLIMAXT1_FRAME_MODE_CAMERA:
            pos = &CLIMAXT1FRAME( frame )->pos;
            RwV3dTransformPoint( &tmatrix->pos, pos, pltm );
            RwMatrixCopy( fmdl, tmatrix );
            RwMatrixCopy( fltm, tmatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        }
        RwFrameForAllChildren( frame, ClimaxT1FrameUpdateCB, data );
    }
    return frame;
}

/*
====================
ClimaxT1FrameUpdate

Faces the frames below pFrame to the camera pCamera.
====================
*/
// 0x801DE760
extern "C" void ClimaxT1FrameUpdate( RwFrame* pFrame, RwFrame* pCamera ) {
    static const RwV3d Zero = { 0.0f, 0.0f, 0.0f };
    RwMatrix* fmdl;
    RwMatrix* fltm;
    RwMatrix* ctm;
    RwMatrix cmatrix;
    RwMatrix ymatrix;
    ClimaxT1FrameUpdateCBData data;
    RwV3d* right;
    RwV3d* up;
    RwV3d* at;
    RwV3d* tpos;
    RwV3d scale;
    int mode;

    if ( ClimaxT1FrameNotSetUp ) {
        right = &c2f.right;
        up = &c2f.up;
        at = &c2f.at;
        *right = c2f_right;
        *up = c2f_up;
        *at = c2f_at;
        c2f.pos = c2f_pos;
        ClimaxT1FrameNotSetUp = 0;
    }
    if ( pFrame != NULL && pCamera != NULL ) {
        fmdl = RwFrameGetMatrix( pFrame );
        fltm = RwFrameGetLTM( pFrame );
        ctm = RwFrameGetLTM( pCamera );

        // The camera's orientation without its position, turned to face the frames.
        cmatrix.pos = Zero;
        cmatrix.at = ctm->at;
        cmatrix.right = ctm->right;
        cmatrix.up = ctm->up;
        RwMatrixMultiply( &data.tmatrix, &cmatrix, &c2f );
        tpos = RwMatrixGetPos( ctm );
        data.tpos = *tpos;
        scale.x = scale.y = scale.z = CLIMAXT1FRAME( pFrame )->scale;
        RwMatrixScale( &data.tmatrix, &scale, rwCOMBINEPRECONCAT );

        mode = CLIMAXT1FRAME( pFrame )->mode;
        switch ( mode ) {
        case CLIMAXT1_FRAME_MODE_FIXED:
            RwMatrixCopy( fltm, fmdl );
            break;
        case CLIMAXT1_FRAME_MODE_BILLBOARD:
            data.tmatrix.pos = CLIMAXT1FRAME( pFrame )->pos;
            ymatrix.pos = data.tmatrix.pos;
            ymatrix.up = data.tmatrix.up;
            RwV3dSubMacro( &ymatrix.at, &data.tpos, &data.tmatrix.pos );
            RwV3dNormalize( &ymatrix.at, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.right, &ymatrix.up, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.up, &ymatrix.at, &ymatrix.right );
            RwV3dNormalize( &ymatrix.right, &ymatrix.right );
            RwV3dNormalize( &ymatrix.up, &ymatrix.up );
            scale.x = scale.y = scale.z = CLIMAXT1FRAME( pFrame )->scale;
            RwMatrixScale( &ymatrix, &scale, rwCOMBINEPRECONCAT );
            RwMatrixCopy( fmdl, &ymatrix );
            RwMatrixCopy( fltm, &ymatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        case CLIMAXT1_FRAME_MODE_BILLBOARD_UPRIGHT:
            data.tmatrix.pos = CLIMAXT1FRAME( pFrame )->pos;
            ymatrix.pos = data.tmatrix.pos;
            ymatrix.up = c2f_up;
            RwV3dSubMacro( &ymatrix.at, &data.tpos, &data.tmatrix.pos );
            ymatrix.at.y = 0.0f;
            RwV3dNormalize( &ymatrix.at, &ymatrix.at );
            RwV3dCrossProductMacro( &ymatrix.right, &ymatrix.up, &ymatrix.at );
            scale.x = scale.y = scale.z = CLIMAXT1FRAME( pFrame )->scale;
            RwMatrixScale( &ymatrix, &scale, rwCOMBINEPRECONCAT );
            RwMatrixCopy( fmdl, &ymatrix );
            RwMatrixCopy( fltm, &ymatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        case CLIMAXT1_FRAME_MODE_CAMERA:
            data.tmatrix.pos = CLIMAXT1FRAME( pFrame )->pos;
            RwMatrixCopy( fmdl, &data.tmatrix );
            RwMatrixCopy( fltm, &data.tmatrix );
            RwMatrixOptimize( fmdl, NULL );
            RwMatrixOptimize( fltm, NULL );
            break;
        }
        RwFrameForAllChildren( pFrame, ClimaxT1FrameUpdateCB, &data );
    }
}

/*
====================
ClimaxT1FrameHasFacingFramesCB

Sets the flag in data when a frame below has a mode other than fixed.
====================
*/
// 0x801DEF48
static RwFrame* ClimaxT1FrameHasFacingFramesCB( RwFrame* pFrame, void* data ) {
    if ( pFrame != NULL ) {
        if ( CLIMAXT1FRAME( pFrame )->mode > 0 ) {
            *(int*)data = TRUE;
        }
        RwFrameForAllChildren( pFrame, ClimaxT1FrameHasFacingFramesCB, data );
    }
    return pFrame;
}

/*
====================
ClimaxT1FrameHasFacingFrames

Whether pFrame, or a frame below it, faces the camera.
====================
*/
// 0x801DEFA8
extern "C" int ClimaxT1FrameHasFacingFrames( RwFrame* pFrame ) {
    int facingFrame = FALSE;

    if ( pFrame != NULL ) {
        if ( CLIMAXT1FRAME( pFrame )->mode > 0 ) {
            return TRUE;
        }
        RwFrameForAllChildren( pFrame, ClimaxT1FrameHasFacingFramesCB, &facingFrame );
    }
    return facingFrame;
}
