// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80092A48.h>
#include <climax/RenderState.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .rodata 0x803D88D0 size 0x10 lbl_803D88D0
//   .data 0x803E8968 size 0x34 jumptable_803E8968
//   .data 0x803E899C size 0x34 jumptable_803E899C
//   .data 0x803E89D0 size 0x34 jumptable_803E89D0
//   .sdata2 0x8052C1B0 .. 0x8052C1E4: float constants, emitted by the compiler from the literals

unk_80092A48* lbl_80529748;     // .sbss, head of the list of live objects (linked through next)
RwRaster* lbl_8052974C;         // .sbss, the shared 256 x 256 raster, made by the first object
unk_80092A48* lbl_8046A6B8[ 8 ]; // .bss, the eight object slots (slot is the object's index here)

/*
====================
fn_80092A48
====================
*/
// 0x80092A48
// Constructor body. The second argument is passed by the caller (fn_8003C454) and never read.
// Wall (register/schedule, 99.68%): retail keeps the zero constant in r4 and the one in r0; the
// compiled order is swapped and no reordering of the stores tried changed the choice.
extern "C" unk_80092A48* fn_80092A48( unk_80092A48* self, int unused, unk_80092A48Source* source ) {
    RwFrame* frame;
    RwV2d viewWindow;

    self->up.x = 0.0f;
    self->up.y = 1.0f;
    self->up.z = 0.0f;
    self->field_0x0C = 0.0f;
    self->field_0x10 = 10.0f;
    self->field_0x15 = 1;
    self->field_0x54 = 1.0f;
    self->field_0x24 = NULL;
    self->field_0x20 = NULL;
    self->sourceFrame = NULL;
    self->sourceMatrix = NULL;
    self->raster = NULL;
    self->texture = NULL;
    self->field_0x60 = NULL;
    self->world = NULL;
    self->field_0x44 = 0;
    self->field_0x4C = 0.5f;
    self->field_0x50 = 0.5f;
    self->source = source;
    frame = source->frame;
    self->sourceFrame = frame;
    self->sourceMatrix = RwFrameGetMatrix( frame );

    self->atomic = RpAtomicCreate();
    self->atomic->boundingSphere.center.x = 0.0f;
    self->atomic->boundingSphere.center.y = 0.0f;
    self->atomic->boundingSphere.center.z = 0.0f;
    self->atomic->boundingSphere.radius = 1.0f;
    RwClimaxAtomicSetPass( self->atomic, 0x20 );

    self->atomicFrame = RwFrameCreate();
    RpAtomicSetFrame( self->atomic, self->atomicFrame );
    self->atomicMatrix = RwFrameGetMatrix( self->atomicFrame );
    RpAtomicSetRenderCallBack( self->atomic, fn_80092DFC );

    self->next = lbl_80529748;
    lbl_80529748 = self;
    self->field_0x14 = 0;
    self->field_0x16 = 0;

    self->camera = RwCameraCreate();
    self->cameraFrame = RwFrameCreate();
    _rwObjectHasFrameSetFrame( self->camera, self->cameraFrame );
    self->cameraMatrix = RwFrameGetMatrix( self->cameraFrame );
    RwCameraSetNearClipPlane( self->camera, 0.1f );
    RwCameraSetFarClipPlane( self->camera, 250.0f );
    RwCameraSetProjection( self->camera, rwPARALLEL );

    self->slot = 0;
    while ( self->slot < 8 && lbl_8046A6B8[ self->slot ] != NULL ) {
        self->slot++;
    }
    if ( self->slot != 8 ) {
        lbl_8046A6B8[ self->slot ] = self;
    }

    if ( lbl_8052974C == NULL ) {
        lbl_8052974C = RwRasterCreate( 0x100, 0x100, 8, 0x485 );
        RASTEREXTFROMRASTER( lbl_8052974C )->flags &= ~1;
    }

    self->raster = RwRasterCreate( 0x80, 0x80, 8, 0x405 );
    if ( self->raster == NULL ) {
        return self;
    }

    viewWindow.x = self->field_0x50;
    viewWindow.y = self->field_0x4C;
    RwCameraSetViewWindow( self->camera, &viewWindow );
    self->texture = RwTextureCreate( self->raster );
    RASTEREXTFROMRASTER( self->raster )->flags &= ~1;
    self->camera->frameBuffer = lbl_8052974C;
    return self;
}

/*
====================
fn_80092CD8
====================
*/
// 0x80092CD8
// Destructor body: unlinks the object from the slot table and the list, frees its resources, and
// deletes it when the caller asks for the deleting form (flag > 0).
extern "C" unk_80092A48* fn_80092CD8( unk_80092A48* self, int flag ) {
    unk_80092A48* node;
    unk_80092A48* kept;
    unk_80092A48* cur;

    if ( self != NULL ) {
        if ( self->world != NULL ) {
            RpWorldRemoveAtomic( self->world, self->atomic );
            RpWorldRemoveCamera( self->world, self->camera );
            self->world = NULL;
        }

        RpAtomicSetFrame( self->atomic, NULL );
        RpAtomicDestroy( self->atomic );
        RwFrameDestroy( self->atomicFrame );

        if ( self->slot != 8 ) {
            lbl_8046A6B8[ self->slot ] = NULL;
        }

        _rwObjectHasFrameSetFrame( self->camera, NULL );
        RwCameraDestroy( self->camera );
        RwFrameDestroy( self->cameraFrame );
        self->camera = NULL;

        if ( self->texture != NULL ) {
            fn_801D797C( self->texture, 0, 3 );
        }

        node = lbl_80529748;
        kept = NULL;
        lbl_80529748 = NULL;
        while ( node != NULL ) {
            cur = node;
            node = cur->next;
            if ( cur != self ) {
                cur->next = kept;
                kept = cur;
                lbl_80529748 = cur;
            }
        }

        if ( flag > 0 ) {
            delete self;
        }
    }
    return self;
}

/*
====================
fn_80092DFC
====================
*/
// 0x80092DFC
// Atomic render callback: finds the object that owns the atomic and runs its draw (fn_80092E48).
// Retail calls fn_80092E48 out of line (bl). The dont_inline pragma around this callback keeps
// this build from inlining fn_80092E48's partial body into it.
#pragma dont_inline on
extern "C" RpAtomic* fn_80092DFC( RpAtomic* atomic ) {
    unk_80092A48* instance = lbl_80529748;

    while ( instance->atomic != atomic ) {
        instance = instance->next;
    }
    fn_80092E48( instance );
    return atomic;
}
#pragma dont_inline reset

/*
====================
fn_80092E48
====================
*/
// 0x80092E48
// Partial: the state setup and the texture-filter restore are reconstructed. Open, and the reason
// it does not match: the blend-mode state machine is inlined three times over lbl_80528868. Its
// arms are the header's SetBlendMode arms, but the header writes out only three of the thirteen
// (1, 2 and 11); the rest must be added to climax/RenderState.h first, after checking its users.
// The three copies: the exit of the entry mode through jump table 0x803E89D0 (0x80092F98), the
// fixed draw state (0x8009366C: source blend 1, destination blend 4, black fog, vertex alpha on,
// alpha test function 5), and the restore of the saved mode through 0x803E899C and 0x803E8968
// (0x80093954 and 0x800939B8). Then comes the fog alpha byte, min(255, (int)(255 * 1.4 * 0.125 *
// field_0x54)), the four-vertex quad (half sizes field_0x50 and field_0x4C) drawn with RwIm3D
// (0x80093730-0x80093870), and the restore of the saved cull mode (r29). Unit stays NonMatching.
extern "C" void fn_80092E48( unk_80092A48* self ) {
    RwUInt32 savedFilter;

    if ( !self->field_0x15 || !self->field_0x16 ) {
        return;
    }
    RwRenderStateGet( rwRENDERSTATETEXTUREFILTER, &savedFilter );
    SetDepthMode( DEPTHMODE_TESTONLY );
    SetCullMode( 0 );
    RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )rwFILTERLINEAR );
    RwRenderStateSet( rwRENDERSTATETEXTURERASTER, self->raster );

    // TODO: blend-mode exit and enter switches, quad build and draw (see the header comment)

    RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )savedFilter );
}

/*
====================
fn_8009473C
====================
*/
// 0x8009473C
extern "C" void fn_8009473C() {
    // TODO
}

/*
====================
fn_80094754
====================
*/
// 0x80094754
extern "C" void fn_80094754() {
    // TODO
}

/*
====================
fn_80094D38
====================
*/
// 0x80094D38
extern "C" void fn_80094D38() {
    // TODO
}

/*
====================
fn_80094DB0
====================
*/
// 0x80094DB0
extern "C" void fn_80094DB0() {
    // TODO
}

/*
====================
fn_80094E04
====================
*/
// 0x80094E04
extern "C" void fn_80094E04() {
    // TODO
}

/*
====================
fn_80094E50
====================
*/
// 0x80094E50
extern "C" void fn_80094E50() {
    // TODO
}

/*
====================
fn_80094E68
====================
*/
// 0x80094E68
extern "C" void fn_80094E68() {
    // TODO
}

/*
====================
fn_80094E6C
====================
*/
// 0x80094E6C
extern "C" void fn_80094E6C() {
    // TODO
}

/*
====================
fn_80094F40
====================
*/
// 0x80094F40
extern "C" void fn_80094F40() {
    // TODO
}

/*
====================
fn_80094FFC
====================
*/
// 0x80094FFC
extern "C" void fn_80094FFC() {
    // TODO
}
