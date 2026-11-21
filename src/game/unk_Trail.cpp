// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_Trail.h>
#include <rwsdk/baframe.h>
#include <rwsdk/batextur.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/bamatlst.h>
#include <rwsdk/plugin/userdata/rpusrdat.h>
#include <string.h>

/*
====================
fn_800817C0

Atomic callback for RpClumpForAllAtomics. Until one is found, looks for the atomic whose geometry
carries a user data array named "trail_placeholder" and stores it in *found. It returns the atomic
every time, so the iteration goes on.
====================
*/
// 0x800817C0
extern "C" RpAtomic* fn_800817C0( RpAtomic* atomic, void* data ) {
    RpAtomic** found = ( RpAtomic** )data;
    if ( *found != NULL ) {
        return atomic;
    }
    RpGeometry* geometry = atomic->geometry;
    RwInt32 count = RpGeometryGetUserDataArrayCount( geometry );
    for ( RwInt32 i = 0; i < count; i++ ) {
        RpUserDataArray* array = RpGeometryGetUserDataArray( geometry, i );
        if ( strcmp( "trail_placeholder", RpUserDataArrayGetName( array ) ) == 0 ) {
            *found = atomic;
            return atomic;
        }
    }
    return atomic;
}

/*
====================
fn_80081858

Builds the trail clump. If the source clump holds the trail placeholder atomic and its first
material has a texture, the trail takes that texture's raster. The clump gets one new atomic in
a frame of its own. The flag and the 0x200 the caller passes are not read by this body.
====================
*/
// 0x80081858
RpClump* fn_80081858( unk_Trail* trail, u8 flag, RpClump* source, s32 unused ) {
    RpAtomic* placeholder = NULL;
    RpClumpForAllAtomics( source, fn_800817C0, &placeholder );
    if ( placeholder != NULL ) {
        RwTexture* texture = placeholder->geometry->matList.materials[ 0 ]->texture;
        if ( texture != NULL ) {
            trail->raster = texture->raster;
        }
    }

    // RULE-EXCEPTION: R30, atomicFrame is declared before the first call: the bytes give it the
    // register retail uses for it when the declaration sits here
    RwFrame* atomicFrame;
    RpClump* clump = RpClumpCreate();
    RpClumpSetFrame( clump, RwFrameCreate() );
    RpAtomic* atomic = RpAtomicCreate();
    atomicFrame = RwFrameCreate();
    RpAtomicSetFrame( atomic, atomicFrame );
    fn_801D9074( atomic );
    RpClumpAddAtomic( clump, atomic );
    RwFrameAddChild( RpClumpGetFrame( clump ), atomicFrame );
    return clump;
}
