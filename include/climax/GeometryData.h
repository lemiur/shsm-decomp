// Names and layout: Gormiti link map and DWARF (Climax GeometryData.obj)
#ifndef CLIMAX_GEOMETRYDATA_H
#define CLIMAX_GEOMETRYDATA_H

#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/world/bageomet.h>

/*
====================
StBoneData

A bone's identity and the radius of the bounds around it.
====================
*/
class StBoneData {
public:
    int BoneID;     // 0x0
    float Radius;   // 0x4
};

/*
====================
cGeometryData

The geometry plugin's data: the bounds of a geometry and the bones that influence them. The plugin
slot of each geometry holds one, shared by reference count. Gormiti declares the class in
SpiritFrameWork/GeometryPlugin/GeometryData.cpp; the layout is read from its DWARF.
====================
*/
class cGeometryData {
public:
    // Retail's default constructor: one reference, bounds zeroed, no bones.
    cGeometryData() {
        m_RefCount = 1;
        m_BoundMin.x = 0.0f;
        m_BoundMin.y = 0.0f;
        m_BoundMin.z = 0.0f;
        m_BoundMax.x = 0.0f;
        m_BoundMax.y = 0.0f;
        m_BoundMax.z = 0.0f;
        m_BoundingSphere.center.x = 0.0f;
        m_BoundingSphere.center.y = 0.0f;
        m_BoundingSphere.center.z = 0.0f;
        m_BoundingSphere.radius = 0.0f;
        m_BoneCount = 0;
        m_BoneData = NULL;
    }

    ~cGeometryData() {
        if ( m_BoneData != NULL ) {
            delete[] m_BoneData;
            m_BoneData = NULL;
        }
    }

    // Gormiti: AddRef and Release, out of line there; retail has them inlined.
    void AddRef() {
        m_RefCount++;
    }
    void Release() {
        if ( --m_RefCount == 0 ) {
            delete this;
        }
    }

    // The plugin's RenderWare callbacks.
    static void* PluginObjectConstructor( void* object, int offsetInObject, int sizeInObject );
    static void* PluginObjectCopy( void* dstObject, const void* srcObject, int offsetInObject, int sizeInObject );
    static void* PluginObjectDestructor( void* object, int offsetInObject, int sizeInObject );
    static RwStream* PluginDataChunkReadCallBack( RwStream* stream, int binaryLength, void* object,
                                                  int offsetInObject, int sizeInObject );
    static RwStream* PluginDataChunkWriteCallBack( RwStream* stream, int binaryLength, const void* object,
                                                   int offsetInObject, int sizeInObject );
    static int PluginDataChunkGetSizeCallBack( const void* object, int offsetInObject, int sizeInObject );

    static int sRpGeometryOffset;   // the plugin offset RenderWare assigned, negative until attached

    int m_RefCount;                 // 0x00
    RwV3d m_BoundMin;               // 0x04
    RwV3d m_BoundMax;               // 0x10
    RwSphere m_BoundingSphere;      // 0x1C
    int m_BoneCount;                // 0x2C
    StBoneData* m_BoneData;         // 0x30: m_BoneCount entries, or NULL
};

/*
====================
StGeomDataPlugin

What the plugin keeps in each geometry: the geometry's data, created when first asked for.
====================
*/
class StGeomDataPlugin {
public:
    cGeometryData* GeometryData;    // 0x0
};

// The geometry's data, built from the geometry on first use; NULL when the plugin is not attached.
cGeometryData* RpGeometryGetData( RpGeometry* geometry );

// Registers the plugin and its stream chunk with RenderWare; returns nonzero on success.
int GeometryDataPluginAttach();

// Gormiti: GeometryDataBuild.cpp (GeometryDataBuild.obj), defined in a unit not yet reconstructed.
void GeometryDataBuild( cGeometryData* data, RpGeometry* geometry );

#endif
