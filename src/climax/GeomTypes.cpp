// Names and layout: Gormiti link map and DWARF (Climax GeomTypes.obj, game/SpiritFrameWork/CollisionBsp/GeomTypes.cpp)
// Unit name and boundary: Gormiti link map. Retail keeps 31 of the object's 43 functions: the others are dead-stripped
// or inlined. RefVertex, Face::Copy, Face::ClipByPlane, SplitListByPlane and DelFace are unpaired with Gormiti's
// copies (same size but for RefVertex; the relocations differ), so their bodies are retail's.

#include <climax/Heap.h>
#include <climax/GeomTypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bavector.h>
#include <string.h>

// The hint the free lists are created and allocated with; its meaning is unknown. Name invented.
#define GEOMTYPES_FREELIST_HINT 5

// How close planes must be to count as one: squared distance, and squared normal difference (the
// normals may differ by a thousandth). Names invented.
#define PLANE_DISTANCE_EPSILON 0.0001f
#define PLANE_NORMAL_EPSILON ( 0.001f * 0.001f )

// How far from a plane a vertex may lie and still count as on it. Name invented.
static const float PLANE_VERTEX_TOLERANCE = 0.0025f;

// Bits ClasifyPlane collects from the vertices' sides of the plane. Names invented.
enum {
    ON_PLANE = 1,
    IN_FRONT = 2,
    BEHIND_PLANE = 4
};

static RwFreeList* s_VertexFreeList;
static RwFreeList* s_FaceFreeList;
static RwFreeList* s_PlaneFreeList;

// The side of the plane a vertex lies on, from its signed distance d: 0 on the plane (within
// PLANE_VERTEX_TOLERANCE), 1 in front, 2 behind. Name invented. Gormiti's Debug build has the test
// inline at each use; retail evaluates the comparison into a bool, which a bool local here reproduces.
static inline int PlaneSide( float d ) {
    bool outside = d * d > PLANE_VERTEX_TOLERANCE * PLANE_VERTEX_TOLERANCE;
    return outside ? ( d > 0.0f ? 1 : 2 ) : 0;
}

// no copy in retail (inlined)
inline CcBSP_FaceList::Face::Vertex* CcBSP_FaceList::NewVertex() {
    return (Face::Vertex*)RwFreeListAlloc( s_VertexFreeList, GEOMTYPES_FREELIST_HINT );
}

// no copy in retail (inlined)
inline void CcBSP_FaceList::DelVertex( Face::Vertex* vertex ) {
    RwFreeListFree( s_VertexFreeList, vertex );
}

/*
====================
GeomTypes_Start

Creates the free lists of the faces, the vertices and the planes.
====================
*/
// 0x801D5D68
void GeomTypes_Start() {
    s_FaceFreeList = RwFreeListCreate( sizeof( CcBSP_FaceList::Face ), 128, 4, GEOMTYPES_FREELIST_HINT );
    s_VertexFreeList = RwFreeListCreate( sizeof( CcBSP_FaceList::Face::Vertex ), 512, 4, GEOMTYPES_FREELIST_HINT );
    s_PlaneFreeList = RwFreeListCreate( sizeof( CPlaneList::StPlane ), 128, 4, GEOMTYPES_FREELIST_HINT );
}

/*
====================
GeomTypes_End
====================
*/
// 0x801D5DCC
void GeomTypes_End() {
    RwFreeListDestroy( s_FaceFreeList );
    RwFreeListDestroy( s_VertexFreeList );
    RwFreeListDestroy( s_PlaneFreeList );
}

// no copy in retail (inlined)
inline float CPlaneList::StPlane::distanceTo( const RwV3d& point ) const {
    return D + RwV3dDotProductMacro( &point, &N );
}

/*
====================
CPlaneList::StPlane::Inverse

Turns the plane to face the other way.
====================
*/
// 0x801D5E00
void CPlaneList::StPlane::Inverse() {
    N.x *= -1.0f;
    N.y *= -1.0f;
    N.z *= -1.0f;
    D = -D;
}

/*
====================
CPlaneList::CPlaneList
====================
*/
// 0x801D5E38
CPlaneList::CPlaneList() {
    m_List = NULL;
    m_PlaneCount = 0;
}

/*
====================
CPlaneList::~CPlaneList
====================
*/
// 0x801D5E48
CPlaneList::~CPlaneList() {
}

/*
====================
CPlaneList::RefPlane

The plane with this normal and distance, taking the first of the list that agrees within the
tolerances, or a new one put at the head of the list.
====================
*/
// 0x801D5E88
CPlaneList::StPlane* CPlaneList::RefPlane( RwV3d* normal, float distance ) {
    for ( StPlane* plane = m_List; plane != NULL; plane = plane->Next ) {
        float distanceDelta = distance - plane->D;
        if ( distanceDelta * distanceDelta < PLANE_DISTANCE_EPSILON ) {
            RwV3d delta;
            RwV3dSubMacro( &delta, normal, &plane->N );
            if ( RwV3dDotProductMacro( &delta, &delta ) < PLANE_NORMAL_EPSILON ) {
                return plane;
            }
        }
    }
    StPlane* plane = (StPlane*)RwFreeListAlloc( s_PlaneFreeList, GEOMTYPES_FREELIST_HINT );
    plane->N = *normal;
    plane->D = distance;
    plane->Next = m_List;
    plane->ID = m_PlaneCount;
    m_List = plane;
    m_PlaneCount++;
    return plane;
}

/*
====================
CPlaneList::GetCount
====================
*/
// 0x801D5FA8
int CPlaneList::GetCount() {
    return m_PlaneCount;
}

/*
====================
CVertexList::CVertexList
====================
*/
// 0x801D5FB0
CVertexList::CVertexList() {
    memset( m_List, 0, sizeof( m_List ) );
    m_Count = 0;
}

/*
====================
CVertexList::~CVertexList
====================
*/
// 0x801D5FF8
CVertexList::~CVertexList() {
}

/*
====================
CVertexList::RefVertex

The index of the vertex at this position, rounded to a thousandth; new positions are added to the
list. -1 when the list is full.
====================
*/
// 0x801D6038
int CVertexList::RefVertex( RwV3d* position ) {
    RwV3d snapped;
    int x = (int)( position->x * 1000.0f + 0.5f );
    int y = (int)( position->y * 1000.0f + 0.5f );
    int z = (int)( position->z * 1000.0f + 0.5f );
    snapped.x = (float)x / 1000.0f;
    snapped.y = (float)y / 1000.0f;
    snapped.z = (float)z / 1000.0f;

    int i;
    for ( i = 0; i < m_Count; i++ ) {
        if ( snapped.x == m_List[i].V.x && snapped.y == m_List[i].V.y && snapped.z == m_List[i].V.z ) {
            return i;
        }
    }
    m_Count++;
    if ( m_Count >= MAX_VERTICES ) {
        return -1;
    }
    m_List[i].V = snapped;
    return i;
}

/*
====================
CVertexList::GetVertex
====================
*/
// 0x801D6194
CVertexList::StVertex* CVertexList::GetVertex( int index ) {
    return &m_List[index];
}

/*
====================
CVertexList::GetCount
====================
*/
// 0x801D61A0
int CVertexList::GetCount() {
    return m_Count;
}

/*
====================
CcBSP_FaceList::Face::Copy

A copy of the face with its own vertices; NULL when an allocation fails.
====================
*/
// 0x801D61AC
CcBSP_FaceList::Face* CcBSP_FaceList::Face::Copy() {
    Face* face = CcBSP_FaceList::NewFace();
    if ( face == NULL ) {
        return NULL;
    }
    face->m_vcount = m_vcount;
    face->m_plane = m_plane;
    face->m_facetype = m_facetype;

    Vertex* last = NULL;
    Vertex* source = m_vertices;
    for ( int i = 0; i < m_vcount; i++ ) {
        Vertex* vertex = CcBSP_FaceList::NewVertex();
        if ( vertex == NULL ) {
            return NULL;
        }
        vertex->v = source->v;
        vertex->id = source->id;
        if ( last == NULL ) {
            face->m_vertices = vertex;
            vertex->next = vertex;
        } else {
            vertex->next = last->next;
            last->next = vertex;
        }
        source = source->next;
        last = vertex;
    }
    return face;
}

/*
====================
CcBSP_FaceList::Face::CalcPlane

Finds the face's plane from its ring of vertices: the normal is the cross product of the corner
with the largest area (Normal3), the point on the plane is the centroid; the plane comes from the
list. -1 for no vertices, -2 for fewer than three, -3 when a normal degenerates.

Original bug kept: Normal2, the sum of the corners' normals, is meant to be Normal2 + nrm, but the
code adds nrm to itself, so Normal2 only holds the last corner's doubled normal; it is only tested
for being non-zero.
====================
*/
// 0x801D62C8
int CcBSP_FaceList::Face::CalcPlane( CPlaneList* PlaneList ) {
    Vertex* tv = m_vertices;
    if ( tv == NULL ) {
        return -1;
    }
    if ( m_vcount < 3 ) {
        return -2;
    }
    RwV3d Normal2 = { 0.0f, 0.0f, 0.0f };
    RwV3d Normal3 = { 0.0f, 0.0f, 0.0f };
    RwV3d pos = { 0.0f, 0.0f, 0.0f };
    float BestLength = -1.0f;

    for ( int ind = 0; ind < m_vcount; ind++ ) {
        RwV3d e1;
        RwV3d e2;
        RwV3d nrm;
        RwV3dAddMacro( &pos, &pos, &tv->v );
        RwV3dSubMacro( &e1, &tv->next->v, &tv->next->next->v );
        RwV3dSubMacro( &e2, &tv->v, &tv->next->next->v );
        RwV3dCrossProductMacro( &nrm, &e1, &e2 );
        float l = RwV3dLength( &nrm );
        if ( l > 0.0f && l > BestLength ) {
            BestLength = l;
            Normal3 = nrm;
            RwV3dNormalize( &Normal3, &Normal3 );
        }
        if ( l > 0.0f ) {
            RwV3dNormalize( &nrm, &nrm );
            RwV3dAddMacro( &Normal2, &nrm, &nrm );
        }
        tv = tv->next;
    }

    RwV3dNormalize( &Normal2, &Normal2 );
    if ( RwV3dDotProductMacro( &Normal2, &Normal2 ) == 0.0f ) {
        return -3;
    }
    if ( RwV3dDotProductMacro( &Normal3, &Normal3 ) == 0.0f ) {
        return -3;
    }
    RwV3dScaleMacro( &pos, &pos, 1.0f / (float)m_vcount );
    float PlanerD = -RwV3dDotProductMacro( &Normal3, &pos );
    m_plane = PlaneList->RefPlane( &Normal3, PlanerD );
    return 0;
}

/*
====================
CcBSP_FaceList::Face::CalculateArea

The area of the polygon as a fan of triangles about its first vertex.
====================
*/
// 0x801D65EC
float CcBSP_FaceList::Face::CalculateArea() {
    Vertex* first = m_vertices;
    if ( first == NULL ) {
        return 0.0f;
    }
    if ( m_vcount < 3 ) {
        return 0.0f;
    }
    Vertex* vertex = first->next;
    float area = 0.0f;
    for ( int i = 0; i < m_vcount - 2; i++ ) {
        RwV3d edge1;
        RwV3d edge2;
        RwV3d cross;
        RwV3dSubMacro( &edge1, &vertex->v, &first->v );
        RwV3dSubMacro( &edge2, &vertex->next->v, &first->v );
        RwV3dCrossProductMacro( &cross, &edge1, &edge2 );
        area += 0.5f * RwV3dLength( &cross );
        vertex = vertex->next;
    }
    return area;
}

/*
====================
CcBSP_FaceList::Face::InsertVertex

Puts a vertex in the ring after the last one inserted.
====================
*/
// 0x801D673C
int CcBSP_FaceList::Face::InsertVertex( const RwV3d& position, int id ) {
    Vertex* vertex = CcBSP_FaceList::NewVertex();
    if ( vertex == NULL ) {
        return -1;
    }
    vertex->v = position;
    vertex->id = id;
    if ( m_vertices == NULL ) {
        vertex->next = vertex;
        m_vertices = vertex;
        m_vcount = 1;
    } else {
        vertex->next = m_vertices->next;
        m_vertices->next = vertex;
        m_vertices = vertex;
        m_vcount++;
    }
    return 0;
}

/*
====================
CcBSP_FaceList::Face::GetVertex

The position of the index'th vertex from the ring's current one.
====================
*/
// 0x801D6804
RwV3d CcBSP_FaceList::Face::GetVertex( int index ) {
    Vertex* vertex = m_vertices;
    for ( ; index > 0; index-- ) {
        vertex = vertex->next;
    }
    return vertex->v;
}

/*
====================
CcBSP_FaceList::Face::GetVertexID

The identifier of the index'th vertex from the ring's current one.
====================
*/
// 0x801D6870
int CcBSP_FaceList::Face::GetVertexID( int index ) {
    Vertex* vertex = m_vertices;
    for ( ; index > 0; index-- ) {
        vertex = vertex->next;
    }
    return vertex->id;
}

/*
====================
CcBSP_FaceList::Face::ClasifyPlane

Where the face lies against the plane (the name keeps the original's spelling): every vertex is
measured by its signed distance, taken as on the plane within PLANE_VERTEX_TOLERANCE.
====================
*/
// 0x801D68C8
CcBSP_FaceList::Face::EePlaneClasification CcBSP_FaceList::Face::ClasifyPlane( const CPlaneList::StPlane* plane ) {
    Vertex* tv = m_vertices;
    int clas = 0;

    if ( plane == m_plane ) {
        return COPLANAR;
    }
    float normdot = RwV3dDotProductMacro( &plane->N, &m_plane->N );
    bool MaybeCoplanar = false;
    if ( normdot > 0.99f ) {
        MaybeCoplanar = true;
    }
    if ( normdot < -0.99f ) {
        MaybeCoplanar = true;
    }

    int pfront = 0;
    int pback = 0;
    int vp_total = 0;
    int vp_total_f = 0;
    int vp_total_b = 0;
    for ( int ind = 0; ind < m_vcount; ind++ ) {
        float d = plane->distanceTo( tv->v );
        if ( d > 0.0f ) {
            pfront++;
        }
        if ( d < 0.0f ) {
            pback++;
        }
        if ( d * d < PLANE_VERTEX_TOLERANCE * PLANE_VERTEX_TOLERANCE ) {
            clas |= ON_PLANE;
        } else {
            clas |= ( d > 0.0f ) ? IN_FRONT : BEHIND_PLANE;
        }
        vp_total++;
        if ( PLANE_VERTEX_TOLERANCE + d > 0.0f ) {
            vp_total_f++;
        }
        if ( d - PLANE_VERTEX_TOLERANCE < 0.0f ) {
            vp_total_b++;
        }
        tv = tv->next;
    }

    if ( clas == ON_PLANE && MaybeCoplanar ) {
        return COPLANAR;
    }
    clas &= IN_FRONT | BEHIND_PLANE;
    if ( clas == IN_FRONT ) {
        return INFRONT;
    }
    if ( clas == BEHIND_PLANE ) {
        return BEHIND;
    }
    if ( pback == 0 ) {
        return INFRONT;
    }
    if ( pfront == 0 ) {
        return BEHIND;
    }
    if ( vp_total_f == vp_total ) {
        return INFRONT;
    }
    if ( vp_total_b == vp_total ) {
        return BEHIND;
    }
    return INTERSECT;
}

/*
====================
CcBSP_FaceList::Face::ClipByPlane

Cuts the face to the part in front of the plane (or on it): a vertex is put on every edge that
crosses the plane, then the vertices behind the plane are dropped. -1 for no vertices, -2 when a
vertex cannot be allocated.
====================
*/
// 0x801D6A88
int CcBSP_FaceList::Face::ClipByPlane( const CPlaneList::StPlane* plane ) {
    Vertex* cv = m_vertices;
    Vertex* nv = NULL;
    if ( cv == NULL ) {
        return -1;
    }

    // Split the edges that cross the plane. The new vertex becomes the next one visited.
    int ind;
    for ( ind = 0; ind < m_vcount; ind++, cv = cv->next ) {
        float nd;
        float cd;
        nv = cv->next;
        cd = plane->distanceTo( cv->v );
        int cside = PlaneSide( cd );
        nd = plane->distanceTo( nv->v );
        int nside = PlaneSide( nd );
        if ( ( cside | nside ) == 3 ) {
            Vertex* newv = NewVertex();
            if ( newv == NULL ) {
                return -2;
            }
            newv->next = nv;
            cv->next = newv;
            m_vcount++;
            float lerp = nd / ( nd - cd );
            newv->v.x = nv->v.x + lerp * ( cv->v.x - nv->v.x );
            newv->v.y = nv->v.y + lerp * ( cv->v.y - nv->v.y );
            newv->v.z = nv->v.z + lerp * ( cv->v.z - nv->v.z );
            newv->id = -1;
        }
    }

    // Drop the vertices behind the plane.
    int delv = 0;
    for ( ind = 0; ind < m_vcount; ind++ ) {
        float nd;
        nv = cv->next;
        nd = plane->distanceTo( nv->v );
        int nside = PlaneSide( nd );
        if ( nside == 2 ) {
            cv->next = nv->next;
            DelVertex( nv );
            if ( m_vertices == nv ) {
                m_vertices = cv;
            }
            delv++;
        } else {
            cv = cv->next;
        }
    }
    m_vcount -= delv;
    return 0;
}

/*
====================
CcBSP_FaceList::CcBSP_FaceList
====================
*/
// 0x801D6D7C
CcBSP_FaceList::CcBSP_FaceList() {
    m_fcount = 0;
    m_flist = NULL;
}

/*
====================
CcBSP_FaceList::~CcBSP_FaceList

Gives every face back to the free list.
====================
*/
// 0x801D6D8C
CcBSP_FaceList::~CcBSP_FaceList() {
    while ( m_flist != NULL ) {
        Face* face = m_flist;
        m_flist = face->m_next;
        DelFace( face );
    }
}

/*
====================
CcBSP_FaceList::AddFace

Puts the face at the head of the list; -1 for no face.
====================
*/
// 0x801D6E08
int CcBSP_FaceList::AddFace( Face* face ) {
    if ( face == NULL ) {
        return -1;
    }
    face->m_next = m_flist;
    m_flist = face;
    m_fcount++;
    return 0;
}

/*
====================
CcBSP_FaceList::RemoveFace

Unlinks the face from the list without freeing it; -1 when the list does not hold it.
====================
*/
// 0x801D6E38
int CcBSP_FaceList::RemoveFace( Face* face ) {
    if ( face == NULL ) {
        return -1;
    }
    Face* current = m_flist;
    if ( current == NULL ) {
        return -1;
    }
    if ( current == face ) {
        m_flist = face->m_next;
        face->m_next = NULL;
        m_fcount--;
        return 0;
    }
    while ( current->m_next != NULL ) {
        if ( current->m_next == face ) {
            current->m_next = face->m_next;
            face->m_next = NULL;
            m_fcount--;
            return 0;
        }
        current = current->m_next;
    }
    return -1;
}

/*
====================
CcBSP_FaceList::Copy

A new list of copies of the faces; AddFace skips a copy that failed.
====================
*/
// 0x801D6ED0
CcBSP_FaceList* CcBSP_FaceList::Copy() {
    CcBSP_FaceList* copy = new( GAME_HEAP, GAME_HEAP_FLAGS ) CcBSP_FaceList();
    for ( Face* face = m_flist; face != NULL; face = face->m_next ) {
        copy->AddFace( face->Copy() );
    }
    return copy;
}

/*
====================
CcBSP_FaceList::CountFaces
====================
*/
// 0x801D6F68
int CcBSP_FaceList::CountFaces() {
    int count = 0;
    for ( Face* face = m_flist; face != NULL; face = face->m_next ) {
        count++;
    }
    return count;
}

/*
====================
CcBSP_FaceList::Sort

Insertion sort of the faces by m_weight.
====================
*/
// 0x801D6F88
void CcBSP_FaceList::Sort() {
    Face* head;
    Face* unsorted = m_flist;
    m_flist = NULL;
    while ( unsorted != NULL ) {
        head = m_flist;
        Face* face = unsorted;
        unsorted = unsorted->m_next;
        if ( head == NULL || face->m_weight < head->m_weight ) {
            face->m_next = head;
            m_flist = face;
        } else {
            Face* previous = head;
            while ( previous->m_next != NULL && previous->m_weight > head->m_weight ) {
                previous = previous->m_next;
            }
            face->m_next = previous->m_next;
            previous->m_next = face;
        }
    }
}

/*
====================
CcBSP_FaceList::SortFromSize

Sorts the faces by area, largest first.
====================
*/
// 0x801D7004
void CcBSP_FaceList::SortFromSize() {
    for ( Face* face = m_flist; face != NULL; face = face->m_next ) {
        face->m_weight = -face->CalculateArea();
    }
    Sort();
}

/*
====================
CcBSP_FaceList::SplitListByPlane

Sorts the list's faces into two new lists, those in front of the plane and those behind it, taking
every face out of this list; a face crossing the plane is cut in two. A face on the plane goes in
front, or is freed when DiscardCoPlanar is set. Returns 1.
====================
*/
// 0x801D7060
int CcBSP_FaceList::SplitListByPlane( const CPlaneList::StPlane* plane, CcBSP_FaceList*& Front,
                                      CcBSP_FaceList*& Back, bool DiscardCoPlanar ) {
    Front = new( GAME_HEAP, GAME_HEAP_FLAGS ) CcBSP_FaceList();
    Back = new( GAME_HEAP, GAME_HEAP_FLAGS ) CcBSP_FaceList();

    while ( m_flist != NULL ) {
        Face* tface = m_flist;
        RemoveFace( tface );
        switch ( tface->ClasifyPlane( plane ) ) {
        case Face::INFRONT:
            Front->AddFace( tface );
            break;
        case Face::BEHIND:
            Back->AddFace( tface );
            break;
        case Face::COPLANAR:
            if ( DiscardCoPlanar ) {
                DelFace( tface );
            } else {
                Front->AddFace( tface );
            }
            break;
        case Face::INTERSECT: {
            Face* f_back = tface->Copy();
            CPlaneList::StPlane RevPlane = *plane;
            RevPlane.Inverse();
            tface->ClipByPlane( plane );
            f_back->ClipByPlane( &RevPlane );
            Front->AddFace( tface );
            Back->AddFace( f_back );
            break;
        }
        }
    }
    return 1;
}

/*
====================
CcBSP_FaceList::NewFace

A cleared face from the free list.
====================
*/
// 0x801D72BC
CcBSP_FaceList::Face* CcBSP_FaceList::NewFace() {
    Face* face = (Face*)RwFreeListAlloc( s_FaceFreeList, GEOMTYPES_FREELIST_HINT );
    face->m_next = NULL;
    face->m_vcount = 0;
    face->m_vertices = NULL;
    face->m_facetype = 0;
    return face;
}

/*
====================
CcBSP_FaceList::DelFace

Returns the face to the free list.
====================
*/
// 0x801D7304
void CcBSP_FaceList::DelFace( Face* face ) {
    RwFreeListFree( s_FaceFreeList, face );
}
