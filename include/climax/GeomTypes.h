// Names and layout: Gormiti link map and DWARF (Climax GeomTypes.obj, game/SpiritFrameWork/CollisionBsp/GeomTypes.cpp)
#ifndef CLIMAX_GEOMTYPES_H
#define CLIMAX_GEOMTYPES_H

#include <rwsdk/plcore/batypes.h>

/*
====================
CPlaneList

A list of the planes of a collision BSP; planes are taken from a free list, one per distinct plane.
====================
*/
class CPlaneList {
public:
    class StPlane {
    public:
        float distanceTo( const RwV3d& point ) const;
        void Inverse();

        RwV3d N;            // 0x00: the unit normal
        float D;            // 0x0C: the distance from the origin along the normal
        StPlane* Next;      // 0x10
        int ID;             // 0x14
    };

    CPlaneList();
    ~CPlaneList();
    StPlane* RefPlane( RwV3d* normal, float distance );
    int GetCount();

    StPlane* m_List;        // 0x0
    int m_PlaneCount;       // 0x4
};

/*
====================
CVertexList

The distinct vertices of a collision BSP, in a fixed array.
====================
*/
class CVertexList {
public:
    enum { MAX_VERTICES = 5000 };

    class StVertex {
    public:
        RwV3d V;            // 0x0
        int ref;            // 0xC: how many faces use the vertex
    };

    CVertexList();
    ~CVertexList();
    int RefVertex( RwV3d* position );
    StVertex* GetVertex( int index );
    int GetCount();

    StVertex m_List[MAX_VERTICES];  // 0x00
    int m_Count;                    // 0x13880
};

/*
====================
CcBSP_FaceList

A singly linked list of the convex faces of a collision BSP, each a ring of vertices on a plane.
====================
*/
class CcBSP_FaceList {
public:
    class Face {
    public:
        // Where a face lies against a plane.
        enum EePlaneClasification {
            INFRONT = 0,
            BEHIND = 1,
            COPLANAR = 2,
            INTERSECT = 3
        };

        class Vertex {
        public:
            RwV3d v;            // 0x00
            int id;             // 0x0C
            Vertex* next;       // 0x10: the ring's next vertex
        };

        Face* Copy();
        int CalcPlane( CPlaneList* PlaneList );
        float CalculateArea();
        int InsertVertex( const RwV3d& position, int id );
        RwV3d GetVertex( int index );
        int GetVertexID( int index );
        EePlaneClasification ClasifyPlane( const CPlaneList::StPlane* plane );
        int ClipByPlane( const CPlaneList::StPlane* plane );

        CPlaneList::StPlane* m_plane;   // 0x00
        Vertex* m_vertices;             // 0x04
        int m_vcount;                   // 0x08
        Face* m_next;                   // 0x0C
        float m_weight;                 // 0x10
        int m_facetype;                 // 0x14
    };

    CcBSP_FaceList();
    ~CcBSP_FaceList();
    int AddFace( Face* face );
    int RemoveFace( Face* face );
    CcBSP_FaceList* Copy();
    int CountFaces();
    void Sort();
    void SortFromSize();
    int SplitListByPlane( const CPlaneList::StPlane* plane, CcBSP_FaceList*& Front, CcBSP_FaceList*& Back,
                          bool DiscardCoPlanar );
    static Face* NewFace();
    static void DelFace( Face* face );
    static Face::Vertex* NewVertex();
    static void DelVertex( Face::Vertex* vertex );

    Face* m_flist;      // 0x0
    int m_fcount;       // 0x4
};

void GeomTypes_Start();
void GeomTypes_End();

#endif
