// Names and layout: Gormiti link map and DWARF (Climax GeometryData.obj, game/SpiritFrameWork/GeometryPlugin/GeometryData.cpp)
// Unit name and boundary: Gormiti link map. The object starts at RpGeometryGetData; the two functions before it in
// the first split of this unit belong to Gormiti's NewFontCache.obj (Font::CFont::UnpackChar and CacheChar).
// Retail is another revision than Gormiti's: its cGeometryData has no argument constructor, AddRef or Release out of
// line, and its read callback builds the object with the default constructor (those are inlined or absent here).

#include <climax/Heap.h>
#include <climax/GeometryData.h>
#include <string.h>

// The plugin ID of the geometry data plugin and of its stream chunk.
#define GEOMETRYDATA_PLUGIN_ID 0x1101

#define GEOMDATAPLUGIN( _geometry ) RWPLUGINOFFSET( StGeomDataPlugin, ( _geometry ), cGeometryData::sRpGeometryOffset )

int cGeometryData::sRpGeometryOffset = -1;

/*
====================
RpGeometryGetData

The geometry's data. The first request builds it from the geometry's vertices.
Gormiti: RpGeometryGetData (another revision: its constructor was out of line).
====================
*/
// 0x801D2E00
cGeometryData* RpGeometryGetData( RpGeometry* geometry ) {
    if ( cGeometryData::sRpGeometryOffset < 0 ) {
        return NULL;
    }
    StGeomDataPlugin* GeomData = GEOMDATAPLUGIN( geometry );
    if ( GeomData->GeometryData == NULL ) {
        GeomData->GeometryData = new( GAME_HEAP, GAME_HEAP_FLAGS ) cGeometryData();
        GeometryDataBuild( GeomData->GeometryData, geometry );
    }
    return GeomData->GeometryData;
}

/*
====================
cGeometryData::PluginObjectConstructor

A new geometry has no data yet.
====================
*/
// 0x801D2EB8
void* cGeometryData::PluginObjectConstructor( void* object, int offsetInObject, int sizeInObject ) {
    StGeomDataPlugin* GeomData = RWPLUGINOFFSET( StGeomDataPlugin, object, offsetInObject );
    GeomData->GeometryData = NULL;
    return object;
}

/*
====================
cGeometryData::PluginObjectCopy

The copy shares the source's data.
====================
*/
// 0x801D2EC4
void* cGeometryData::PluginObjectCopy( void* dstObject, const void* srcObject, int offsetInObject, int sizeInObject ) {
    StGeomDataPlugin* GeomDataDst = RWPLUGINOFFSET( StGeomDataPlugin, dstObject, offsetInObject );
    const StGeomDataPlugin* GeomDataSrc = RWPLUGINOFFSET( const StGeomDataPlugin, srcObject, offsetInObject );
    GeomDataDst->GeometryData = GeomDataSrc->GeometryData;
    if ( GeomDataDst->GeometryData != NULL ) {
        GeomDataDst->GeometryData->AddRef();
    }
    return dstObject;
}

/*
====================
cGeometryData::PluginObjectDestructor

Lets go of the geometry's data, deleting it with its last reference.
====================
*/
// 0x801D2EE4
void* cGeometryData::PluginObjectDestructor( void* object, int offsetInObject, int sizeInObject ) {
    StGeomDataPlugin* GeomData = RWPLUGINOFFSET( StGeomDataPlugin, object, offsetInObject );
    cGeometryData* data = GeomData->GeometryData;
    if ( data != NULL ) {
        data->Release();
    }
    GeomData->GeometryData = NULL;
    return object;
}

/*
====================
cGeometryData::PluginDataChunkReadCallBack

Reads the bounds and the bone list of a geometry's data chunk.
====================
*/
// 0x801D2F6C
RwStream* cGeometryData::PluginDataChunkReadCallBack( RwStream* stream, int binaryLength, void* object,
                                                      int offsetInObject, int sizeInObject ) {
    StGeomDataPlugin* GeomData = RWPLUGINOFFSET( StGeomDataPlugin, object, offsetInObject );
    cGeometryData* data = new( GAME_HEAP, GAME_HEAP_FLAGS ) cGeometryData();
    int Version = -1; // read and not checked

    RwStreamReadInt32( stream, &Version, sizeof( int ) );
    RwStreamReadReal( stream, &data->m_BoundMin.x, sizeof( RwV3d ) );
    RwStreamReadReal( stream, &data->m_BoundMax.x, sizeof( RwV3d ) );
    RwStreamReadReal( stream, &data->m_BoundingSphere.center.x, sizeof( RwSphere ) );
    RwStreamReadInt32( stream, &data->m_BoneCount, sizeof( int ) );
    if ( data->m_BoneCount > 0 ) {
        data->m_BoneData = new( GAME_HEAP, GAME_HEAP_FLAGS ) StBoneData[data->m_BoneCount];
        memset( data->m_BoneData, 0, data->m_BoneCount * sizeof( StBoneData ) );
        for ( int ind = 0; ind < data->m_BoneCount; ind++ ) {
            RwStreamReadInt32( stream, &data->m_BoneData[ind].BoneID, sizeof( int ) );
            RwStreamReadReal( stream, &data->m_BoneData[ind].Radius, sizeof( float ) );
        }
    }
    GeomData->GeometryData = data;
    return stream;
}

/*
====================
cGeometryData::PluginDataChunkWriteCallBack

Writes the data chunk: the version, the bounds and the bone list.
====================
*/
// 0x801D30DC
RwStream* cGeometryData::PluginDataChunkWriteCallBack( RwStream* stream, int binaryLength, const void* object,
                                                       int offsetInObject, int sizeInObject ) {
    const StGeomDataPlugin* GeomData = RWPLUGINOFFSET( const StGeomDataPlugin, object, offsetInObject );
    cGeometryData* Data = GeomData->GeometryData;
    int Version = 1;

    RwStreamWriteInt32( stream, &Version, sizeof( int ) );
    RwStreamWriteReal( stream, &Data->m_BoundMin.x, sizeof( RwV3d ) );
    RwStreamWriteReal( stream, &Data->m_BoundMax.x, sizeof( RwV3d ) );
    RwStreamWriteReal( stream, &Data->m_BoundingSphere.center.x, sizeof( RwSphere ) );
    RwStreamWriteInt32( stream, &Data->m_BoneCount, sizeof( int ) );
    for ( int ind = 0; ind < Data->m_BoneCount; ind++ ) {
        RwStreamWriteInt32( stream, &Data->m_BoneData[ind].BoneID, sizeof( int ) );
        RwStreamWriteReal( stream, &Data->m_BoneData[ind].Radius, sizeof( float ) );
    }
    return stream;
}

/*
====================
cGeometryData::PluginDataChunkGetSizeCallBack

The chunk's size in bytes, or -1 for a geometry without data: the version, the bounds, the bone
count and the bone list.
====================
*/
// 0x801D31C4
int cGeometryData::PluginDataChunkGetSizeCallBack( const void* object, int offsetInObject, int sizeInObject ) {
    const StGeomDataPlugin* GeomData = RWPLUGINOFFSET( const StGeomDataPlugin, object, offsetInObject );
    if ( GeomData->GeometryData == NULL ) {
        return -1;
    }
    return (int)( sizeof( int ) + 2 * sizeof( RwV3d ) + sizeof( RwSphere ) + sizeof( int ) ) +
           GeomData->GeometryData->m_BoneCount * sizeof( StBoneData );
}

/*
====================
GeometryDataPluginAttach

Registers the plugin and its stream chunk with the geometry module.
====================
*/
// 0x801D31E8
int GeometryDataPluginAttach() {
    if ( cGeometryData::sRpGeometryOffset >= 0 ) {
        return 0;
    }
    cGeometryData::sRpGeometryOffset = RpGeometryRegisterPlugin(
        sizeof( StGeomDataPlugin ), GEOMETRYDATA_PLUGIN_ID, cGeometryData::PluginObjectConstructor,
        cGeometryData::PluginObjectDestructor, cGeometryData::PluginObjectCopy );
    if ( cGeometryData::sRpGeometryOffset < 0 ) {
        return 0;
    }
    return RpGeometryRegisterPluginStream( GEOMETRYDATA_PLUGIN_ID, cGeometryData::PluginDataChunkReadCallBack,
                                           cGeometryData::PluginDataChunkWriteCallBack,
                                           cGeometryData::PluginDataChunkGetSizeCallBack ) >= 0;
}
