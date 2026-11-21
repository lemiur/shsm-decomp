#ifndef GAME_UNK_8000D83C_H
#define GAME_UNK_8000D83C_H

#include <revolution/types.h>
#include <climax/Frame.h>
#include <climax/Physics.h>

/*
 * Component of a vector by axis index (0 = x, 1 = y, 2 = z). Any other index reads as 0.0f.
 */
inline float unk_AxisComponent( const unk_RwV3d& v, int axis ) {
    switch ( axis ) {
    case 0:
        return v.x;
    case 1:
        return v.y;
    case 2:
        return v.z;
    }
    return 0.0f;
}

/*
 * The sweep record fn_8000EAB8 builds on its stack and hands to every node of the manager. It holds
 * the segment (pos to prevPos), its unit direction and length (scale), the contact point and normal
 * of the best hit so far, the hit time, the node that was hit (owner: non-null means a hit), the
 * node the sweep ignores and a flag the node callback reads. Names invented; the offsets are the
 * accesses in fn_8000EAB8, fn_8000E89C and fn_8000DED8.
 */
struct unk_SweepRecord {
    unk_RwV3d   pos;            // 0x00
    unk_RwV3d   prevPos;        // 0x0C
    unk_RwV3d   dir;            // 0x18
    float       scale;          // 0x24
    unk_RwV3d   contact;        // 0x28
    unk_RwV3d   normal;         // 0x34
    float       bestT;          // 0x40
    void*       ignored;        // 0x44: the node the sweep does not test
    void*       owner;          // 0x48: the node of the best hit, null when none
    bool        flag;           // 0x4C
};

/*
 * The object fn_8000EAB8 sweeps against. Its member at 0xC0 leads, through two pointer loads, to the
 * manager that RwpMgrForAllObjs walks. Names invented.
 */
struct unk_SweepOwner {
    char        field_0x00[0xC0];
    void**      field_0xc0;     // 0xC0
};

extern "C" {
bool fn_8000D83C( const unk_RwV3d* start, const unk_RwV3d* end, const unk_RwV3d* center, const unk_RwV3d* halfExtent, float* outT );
bool fn_8000DB14( const unk_RwV3d* base, const unk_RwV3d* top, const unk_RwV3d* from, const unk_RwV3d* to, float radius, float* outT );
bool fn_8000DED8( void* owner, const void* unused, const unk_RwV3d* origin, unk_SweepRecord* rec, float radius, float halfHeight );
void fn_8000E244( unk_RwpBody* body, unk_RwpShape* shape, unk_SweepRecord* rec );
void fn_8000E89C( unk_RwpBody* body, unk_SweepRecord* rec );
bool fn_8000EAB8( unk_SweepOwner* owner, const unk_RwV3d* from, const unk_RwV3d* to, unk_RwV3d* move, unk_RwV3d* normalOut, void* ignored, bool flag );
void RwpMgrForAllObjs( void* mgr, void (*callback)( unk_RwpBody*, unk_SweepRecord* ), unk_SweepRecord* data );
}

#endif
