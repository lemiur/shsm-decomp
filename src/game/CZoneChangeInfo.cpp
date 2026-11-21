// Unit name and boundary: unit_plan (single constructor range; class name from lbl_803E2788 string CZoneChangeInfo*)

#include <game/CZoneChangeInfo.h>
#include <climax/Utils.h>

// The handle constructor and FindListedZone are defined here, not in the header, so that the other
// units including CZoneChangeInfo.h (climax/cattributehandler, matched) keep their code unchanged.
inline CZone* unk_ZoneHandle::FindListedZone( CZone* candidate ) {
    CZone* listed[8];
    s32 count = lbl_80529260->fn_80030284( listed, 8 );
    for ( s32 i = 0; i < count; i++ ) {
        if ( candidate == listed[i] ) {
            return candidate;
        }
    }
    return NULL;
}

inline unk_ZoneHandle::unk_ZoneHandle( CZone* candidate ) {
    CZone* kept = FindListedZone( candidate );
    zone = kept;
    if ( kept != NULL ) {
        fn_80028170( kept );
    }
    lbl_80529260->fn_80031CE0( this );
}

/*
====================
CZoneChangeInfo::CZoneChangeInfo
====================
*/
// 0x80029164
// Not exact (94.70%): the retail keeps the second handle's address in its own register (this in r30,
// addi r31, r30, 4) where the compiled body folds that offset into the stores (register/schedule).
CZoneChangeInfo::CZoneChangeInfo() : oldZone( NULL ), newZone( NULL ) {
    field_0x8 = NULL;
    field_0xC = NULL;
    field_0x10 = NULL;
    field_0x94 = 0;
    field_0x95 = 0;
    field_0x96 = 0;
    field_0x97 = 0;
    RwMatrixSetIdentity( &field_0x14 );
    RwMatrixSetIdentity( &field_0x54 );
}
/*
====================
CZoneChangeInfo::~CZoneChangeInfo
====================
*/
// 0x8002930C
// NonMatching (inline/call set): retail inlines the handle destructor (0x800290F0, same translation
// unit in the original; it is out of line here, a unit boundary the lane plan cuts), so the bytes differ.
CZoneChangeInfo::~CZoneChangeInfo() {
    delete[] field_0x8;
    delete[] field_0xC;
    delete[] field_0x10;
}

/*
====================
CZoneChangeInfo::fn_800293D4
====================
*/
// 0x800293D4
void CZoneChangeInfo::fn_800293D4( const char* a, const char* b, const char* c ) {
    field_0x8 = fn_801B67FC( a, 0 );
    field_0xC = fn_801B67FC( b, 0 );
    field_0x10 = fn_801B67FC( c, 0 );
}
