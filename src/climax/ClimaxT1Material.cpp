// Unit name and boundary: Gormiti link map (ClimaxT1Material.obj)
// Names and layout: Gormiti link map and DWARF (ClimaxT1Material.obj)
//
// The Climax "ClimaxT1" effect system's material plugin: per-material data (sidedness, frame
// blending, an animation with its interpolator, and UV transform matrices) attached to
// RenderWare materials, with the plugin's stream callbacks.

#include <climax/ClimaxT1Material.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plugin/matfx/rpmatfx.h>

// The plugin ID the data is registered under (invented name).
#define rwID_CLIMAXT1MATERIAL 0xA01

// The size of the keyframes the material's interpolator holds (invented name).
#define CLIMAXT1_KEYFRAME_SIZE 0x18

// The high 16 bits of a material's frame blend mode, which the stream does not set (invented name).
#define CLIMAXT1_FRAMEBLENDMODE_KEPT 0xFFFF0000

// The UV matrices come from the engine's global memory (RwMalloc hint).
#define CLIMAXT1_UVMATRIX_HINT rwMEMHINTDUR_GLOBAL

extern "C" {
// defined in ClimaxT1Dictionary.cpp; its header (climax/ClimaxT1Dictionary.h, another lane) declares it too
RtAnimAnimation* ClimaxT1DictionarySchemaGetAnimation( int animation );
}

// The offset of the plugin's data in a material, set when the plugin is attached.
static int LocalOffset = -1;

// The plugin data of a material (as its plugin offset, so it is read again after each store).
#define CLIMAXT1MATERIAL( material ) RWPLUGINOFFSET( ClimaxT1Material, material, LocalOffset )

/*
====================
ClimaxT1MaterialDataConstructor

Plugin constructor: a new material has no animation and no matrices.
====================
*/
// 0x801DF7BC
static void* ClimaxT1MaterialDataConstructor( void* material, int offset, int size ) {
    if ( LocalOffset > 0 ) {
        CLIMAXT1MATERIAL( material )->isDoubleSided = 0;
        CLIMAXT1MATERIAL( material )->frameBlendMode = 0;
        CLIMAXT1MATERIAL( material )->animation = -1;
        CLIMAXT1MATERIAL( material )->interpolator = NULL;
        CLIMAXT1MATERIAL( material )->UvMatrix = NULL;
    }
    return material;
}

/*
====================
ClimaxT1MaterialDataDestructor

Plugin destructor: frees the UV matrices and the interpolator.
====================
*/
// 0x801DF808
static void* ClimaxT1MaterialDataDestructor( void* material, int offset, int size ) {
    if ( CLIMAXT1MATERIAL( material )->UvMatrix ) {
        RwFree( CLIMAXT1MATERIAL( material )->UvMatrix );
    }
    if ( CLIMAXT1MATERIAL( material )->interpolator ) {
        RtAnimInterpolatorDestroy( CLIMAXT1MATERIAL( material )->interpolator );
        CLIMAXT1MATERIAL( material )->interpolator = NULL;
    }
    return material;
}

/*
====================
ClimaxT1MaterialDataCopier

Plugin copy: the settings are copied, the UV matrices are made again on demand and the
interpolator is a copy of the source's.
====================
*/
// 0x801DF880
static void* ClimaxT1MaterialDataCopier( void* dstMaterial, const void* srcMaterial, int offset, int size ) {
    CLIMAXT1MATERIAL( dstMaterial )->isDoubleSided = CLIMAXT1MATERIAL( srcMaterial )->isDoubleSided;
    CLIMAXT1MATERIAL( dstMaterial )->frameBlendMode = CLIMAXT1MATERIAL( srcMaterial )->frameBlendMode;
    CLIMAXT1MATERIAL( dstMaterial )->animation = CLIMAXT1MATERIAL( srcMaterial )->animation;
    CLIMAXT1MATERIAL( dstMaterial )->UvMatrix = NULL;
    if ( CLIMAXT1MATERIAL( srcMaterial )->interpolator ) {
        CLIMAXT1MATERIAL( dstMaterial )->interpolator = RtAnimInterpolatorCreate(
            RtAnimAnimationGetNumNodes( CLIMAXT1MATERIAL( srcMaterial )->interpolator->pCurrentAnim ),
            CLIMAXT1_KEYFRAME_SIZE );
        if ( CLIMAXT1MATERIAL( dstMaterial )->interpolator ) {
            RtAnimInterpolatorCopy( CLIMAXT1MATERIAL( dstMaterial )->interpolator,
                                    CLIMAXT1MATERIAL( srcMaterial )->interpolator );
        }
    } else {
        CLIMAXT1MATERIAL( dstMaterial )->interpolator = NULL;
    }
    return dstMaterial;
}

/*
====================
ClimaxT1MaterialGetIsDoubleSided
====================
*/
// 0x801DF958
extern "C" int ClimaxT1MaterialGetIsDoubleSided( RpMaterial* material ) {
    return CLIMAXT1MATERIAL( material )->isDoubleSided;
}

/*
====================
ClimaxT1MaterialGetFrameBlendMode
====================
*/
// 0x801DF964
extern "C" int ClimaxT1MaterialGetFrameBlendMode( RpMaterial* material ) {
    return CLIMAXT1MATERIAL( material )->frameBlendMode;
}

/*
====================
ClimaxT1MaterialGetAnimation
====================
*/
// 0x801DF974
extern "C" int ClimaxT1MaterialGetAnimation( RpMaterial* material ) {
    return CLIMAXT1MATERIAL( material )->animation;
}

/*
====================
ClimaxT1MaterialSetIsDoubleSided
====================
*/
// no copy in retail (dead-stripped or inlined): called by ClimaxT1MaterialDataReadStream
extern "C" void ClimaxT1MaterialSetIsDoubleSided( RpMaterial* material, int isDoubleSided ) {
    CLIMAXT1MATERIAL( material )->isDoubleSided = isDoubleSided;
}

/*
====================
ClimaxT1MaterialSetFrameBlendMode

Sets the low 16 bits of the frame blend mode and keeps the high 16.
====================
*/
// no copy in retail (dead-stripped or inlined): called by ClimaxT1MaterialDataReadStream
extern "C" void ClimaxT1MaterialSetFrameBlendMode( RpMaterial* material, int frameBlendMode ) {
    int fbm = CLIMAXT1MATERIAL( material )->frameBlendMode & CLIMAXT1_FRAMEBLENDMODE_KEPT;
    CLIMAXT1MATERIAL( material )->frameBlendMode = fbm + frameBlendMode;
}

/*
====================
ClimaxT1MaterialSetAnimation
====================
*/
// no copy in retail (dead-stripped or inlined): called by ClimaxT1MaterialDataReadStream
extern "C" void ClimaxT1MaterialSetAnimation( RpMaterial* material, int animation ) {
    CLIMAXT1MATERIAL( material )->animation = animation;
}

/*
====================
ClimaxT1MaterialGetUVMatrices

The material's UV transform matrices, made (as identities) the first time they are asked for.
NULL when the material's effect has no UV transform, or when there is no memory. Both matrices
are made, the second for the dual-texture effect.
====================
*/
// 0x801DF984
extern "C" RwMatrix* ClimaxT1MaterialGetUVMatrices( RpMaterial* material ) {
    RwMatrix* mtx = CLIMAXT1MATERIAL( material )->UvMatrix;
    if ( mtx ) {
        return mtx;
    }

    RpMatFXMaterialFlags eflags = RpMatFXMaterialGetEffects( material );
    int UvAChans = 0;
    if ( eflags == rpMATFXEFFECTUVTRANSFORM ) {
        UvAChans = 1;
    }
    if ( eflags == rpMATFXEFFECTDUALUVTRANSFORM ) {
        UvAChans = 2;
    }
    if ( UvAChans == 0 ) {
        return NULL;
    }

    mtx = (RwMatrix*)RwMalloc( 2 * sizeof( RwMatrix ), CLIMAXT1_UVMATRIX_HINT );
    if ( !mtx ) {
        return NULL;
    }

    CLIMAXT1MATERIAL( material )->UvMatrix = mtx;
    RwMatrixSetIdentity( &mtx[0] );
    RwMatrixSetIdentity( &mtx[1] );

    switch ( UvAChans ) {
    case 1:
        RpMatFXMaterialSetUVTransformMatrices( material, mtx, NULL );
        break;
    case 2:
        RpMatFXMaterialSetUVTransformMatrices( material, mtx, &mtx[1] );
        break;
    }

    return CLIMAXT1MATERIAL( material )->UvMatrix;
}

/*
====================
ClimaxT1MaterialDataReadStream

Plugin stream reader: the three settings, then the interpolator for the animation (when the
stream names one) set to play it.
====================
*/
// 0x801DFB00
static RwStream* ClimaxT1MaterialDataReadStream( RwStream* stream, int length, void* material, int offset, int size ) {
    ClimaxT1Material binaryData;

    if ( !RwStreamReadInt32( stream, &binaryData.isDoubleSided, 4 ) ||
         !RwStreamReadInt32( stream, &binaryData.frameBlendMode, 4 ) ||
         !RwStreamReadInt32( stream, &binaryData.animation, 4 ) ) {
        return NULL;
    }

    ClimaxT1MaterialSetIsDoubleSided( (RpMaterial*)material, binaryData.isDoubleSided );
    ClimaxT1MaterialSetFrameBlendMode( (RpMaterial*)material, binaryData.frameBlendMode );
    ClimaxT1MaterialSetAnimation( (RpMaterial*)material, binaryData.animation );

    if ( binaryData.animation >= 0 ) {
        RtAnimAnimation* anim = ClimaxT1DictionarySchemaGetAnimation( binaryData.animation );
        if ( anim ) {
            CLIMAXT1MATERIAL( material )->interpolator =
                RtAnimInterpolatorCreate( RtAnimAnimationGetNumNodes( anim ), CLIMAXT1_KEYFRAME_SIZE );
            if ( !CLIMAXT1MATERIAL( material )->interpolator ) {
                return NULL;
            }
            if ( !RtAnimInterpolatorSetCurrentAnim( CLIMAXT1MATERIAL( material )->interpolator, anim ) ) {
                return NULL;
            }
        }
    }
    return stream;
}

/*
====================
ClimaxT1MaterialDataWriteStream

Plugin stream writer: the three settings.
====================
*/
// 0x801DFC2C
static RwStream* ClimaxT1MaterialDataWriteStream( RwStream* stream, int length, const void* material, int offset, int size ) {
    ClimaxT1Material binaryData;

    binaryData.isDoubleSided = CLIMAXT1MATERIAL( material )->isDoubleSided;
    binaryData.frameBlendMode = CLIMAXT1MATERIAL( material )->frameBlendMode;
    binaryData.animation = CLIMAXT1MATERIAL( material )->animation;

    if ( !RwStreamWriteInt32( stream, &binaryData.isDoubleSided, 4 ) ||
         !RwStreamWriteInt32( stream, &binaryData.frameBlendMode, 4 ) ||
         !RwStreamWriteInt32( stream, &binaryData.animation, 4 ) ) {
        return NULL;
    }
    return stream;
}

/*
====================
ClimaxT1MaterialDataGetStreamSize

The three settings of ClimaxT1MaterialDataWriteStream, in bytes.
====================
*/
// 0x801DFCC4
static int ClimaxT1MaterialDataGetStreamSize( const void* material, int offset, int size ) {
    return 12;
}

/*
====================
ClimaxT1MaterialPluginAttach

Registers the material plugin and its stream chunk with RenderWare. FALSE when RenderWare has
no room for it or the two registrations disagree on the plugin's offset.
====================
*/
// 0x801DFCCC
extern "C" int ClimaxT1MaterialPluginAttach() {
    int offset;

    LocalOffset = RpMaterialGetPluginOffset( rwID_CLIMAXT1MATERIAL );
    if ( LocalOffset >= 0 ) {
        return TRUE;
    }

    LocalOffset = RpMaterialRegisterPlugin( sizeof( ClimaxT1Material ), rwID_CLIMAXT1MATERIAL,
                                            ClimaxT1MaterialDataConstructor, ClimaxT1MaterialDataDestructor,
                                            ClimaxT1MaterialDataCopier );
    if ( LocalOffset < 0 ) {
        return FALSE;
    }

    offset = RpMaterialRegisterPluginStream( rwID_CLIMAXT1MATERIAL, ClimaxT1MaterialDataReadStream,
                                             ClimaxT1MaterialDataWriteStream, ClimaxT1MaterialDataGetStreamSize );
    return offset == LocalOffset;
}
