// Unit name and boundary: Gormiti link map (ClimaxT1Geometry.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1Geometry.obj).
// ClimaxT1GeometryPrimeOffset, ClimaxT1GeometrySortMaterials and ClimaxT1GeometryPrintDebug have no copy in
// retail (dead-stripped); ClimaxT1GeometryCheck and ClimaxT1GeometrySetAlphaVertex are inlined.
/*
 * ClimaxT1Geometry.cpp: the ClimaxT1 plugin of geometries. Each geometry carries a flag that says
 * whether it has vertex colours and materials with animations, and streams it with the geometry.
 */

#include <climax/ClimaxT1Geometry.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>

// The plugin ID of the geometry plugin.
#define CLIMAXT1_GEOMETRY_PLUGIN_ID 0xA03

static int GlobalOffset = -1;

// The plugin data of a geometry.
#define CLIMAXT1GEOMETRY( _geometry ) RWPLUGINOFFSET( ClimaxT1Geometry, ( _geometry ), GlobalOffset )

static void* ClimaxT1GeometryDataConstructor( void* geometry, int offset, int size );
static void* ClimaxT1GeometryDataDestructor( void* geometry, int offset, int size );
static void* ClimaxT1GeometryDataCopier( void* dstGeometry, const void* srcGeometry, int offset, int size );
static RwStream* ClimaxT1GeometryDataReadStream( RwStream* stream, int binaryLength, void* geometry, int offset,
                                                 int size );
static RwStream* ClimaxT1GeometryDataWriteStream( RwStream* stream, int binaryLength, const void* geometry,
                                                  int offset, int size );
static int ClimaxT1GeometryDataGetStreamSize( const void* geometry, int offset, int size );

/*
====================
ClimaxT1GeometryHasAnimatedMaterials
====================
*/
// 0x801DF000
extern "C" int ClimaxT1GeometryHasAnimatedMaterials( RpGeometry* geometry ) {
    return CLIMAXT1GEOMETRY( geometry )->alphaVertex;
}

/*
====================
ClimaxT1GeometryDataConstructor
====================
*/
// 0x801DF00C
static void* ClimaxT1GeometryDataConstructor( void* geometry, int offset, int size ) {
    if ( GlobalOffset > 0 ) {
        CLIMAXT1GEOMETRY( geometry )->alphaVertex = 0;
    }
    return geometry;
}

/*
====================
ClimaxT1GeometryDataDestructor
====================
*/
// 0x801DF024
static void* ClimaxT1GeometryDataDestructor( void* geometry, int offset, int size ) {
    return geometry;
}

/*
====================
ClimaxT1GeometryDataCopier
====================
*/
// 0x801DF028
static void* ClimaxT1GeometryDataCopier( void* dstGeometry, const void* srcGeometry, int offset, int size ) {
    CLIMAXT1GEOMETRY( dstGeometry )->alphaVertex =
        RWPLUGINOFFSET( const ClimaxT1Geometry, srcGeometry, GlobalOffset )->alphaVertex;
    return dstGeometry;
}

/*
====================
ClimaxT1GeometryDataReadStream
====================
*/
// 0x801DF038
static RwStream* ClimaxT1GeometryDataReadStream( RwStream* stream, int binaryLength, void* geometry, int offset,
                                                 int size ) {
    ClimaxT1Geometry binaryData;

    if ( !RwStreamReadInt32( stream, (RwInt32*)&binaryData, sizeof( ClimaxT1Geometry ) ) ) {
        return NULL;
    }
    CLIMAXT1GEOMETRY( geometry )->alphaVertex = binaryData.alphaVertex;
    return stream;
}

/*
====================
ClimaxT1GeometrySetAlphaVertex
====================
*/
// no copy in retail (dead-stripped or inlined): the stores of ClimaxT1GeometryCheck, which the Final
// build's ClimaxT1GeometryDataWriteStream carries inline, and no other caller keeps it out of line.
static void ClimaxT1GeometrySetAlphaVertex( RpGeometry* geometry, int alphaVertex ) {
    CLIMAXT1GEOMETRY( geometry )->alphaVertex = alphaVertex;
}

/*
====================
ClimaxT1GeometryCheck

Works the flag out before it is streamed: set when the geometry has vertex colours and any of its
materials has an animation.
====================
*/
// no copy in retail (dead-stripped or inlined): ClimaxT1GeometryDataWriteStream, its only caller, carries
// the body inline (one source line in the Final build's line table, locals m and mmax in r30 and r29).
static void ClimaxT1GeometryCheck( RpGeometry* geometry ) {
    RwRGBA* color;
    int m;
    int mmax;

    ClimaxT1GeometrySetAlphaVertex( geometry, 0 );
    color = geometry->preLitLum;
    if ( color != NULL ) {
        mmax = geometry->matList.numMaterials;
        for ( m = 0; m < mmax; m++ ) {
            if ( ClimaxT1MaterialGetAnimation( geometry->matList.materials[m] ) >= 0 ) {
                break;
            }
        }
        if ( m != mmax ) {
            ClimaxT1GeometrySetAlphaVertex( geometry, 1 );
        }
    }
}

/*
====================
ClimaxT1GeometryDataWriteStream
====================
*/
// 0x801DF098
static RwStream* ClimaxT1GeometryDataWriteStream( RwStream* stream, int binaryLength, const void* geometry,
                                                  int offset, int size ) {
    RpGeometry* rpGeometry = (RpGeometry*)geometry;
    ClimaxT1Geometry binaryData;

    ClimaxT1GeometryCheck( rpGeometry );
    binaryData.alphaVertex = CLIMAXT1GEOMETRY( rpGeometry )->alphaVertex;
    if ( !RwStreamWriteInt32( stream, (RwInt32*)&binaryData, sizeof( ClimaxT1Geometry ) ) ) {
        return NULL;
    }
    return stream;
}

/*
====================
ClimaxT1GeometryDataGetStreamSize
====================
*/
// 0x801DF154
static int ClimaxT1GeometryDataGetStreamSize( const void* geometry, int offset, int size ) {
    return sizeof( ClimaxT1Geometry );
}

/*
====================
ClimaxT1GeometryUpdate
====================
*/
// 0x801DF15C
extern "C" void ClimaxT1GeometryUpdate() {
}

/*
====================
ClimaxT1GeometryPluginAttach

Registers the plugin with geometries and their streams. Returns false when either fails.
====================
*/
// 0x801DF160
extern "C" int ClimaxT1GeometryPluginAttach() {
    int offset;

    GlobalOffset = RpGeometryGetPluginOffset( CLIMAXT1_GEOMETRY_PLUGIN_ID );
    if ( GlobalOffset >= 0 ) {
        return TRUE;
    }
    GlobalOffset = RpGeometryRegisterPlugin( sizeof( ClimaxT1Geometry ), CLIMAXT1_GEOMETRY_PLUGIN_ID,
                                             ClimaxT1GeometryDataConstructor, ClimaxT1GeometryDataDestructor,
                                             ClimaxT1GeometryDataCopier );
    if ( GlobalOffset < 0 ) {
        return FALSE;
    }
    offset = RpGeometryRegisterPluginStream( CLIMAXT1_GEOMETRY_PLUGIN_ID, ClimaxT1GeometryDataReadStream,
                                             ClimaxT1GeometryDataWriteStream, ClimaxT1GeometryDataGetStreamSize );
    return offset == GlobalOffset;
}
