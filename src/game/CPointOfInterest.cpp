// Unit name: the class name string "CPointOfInterest" (0x803EBAD0), hashed by the unit's static
// initialiser; the file name is the class name. Boundary: the data ranges (vtable lbl_803EBA7C)
// and the unit plan.

#include <game/CBaseBehaviour.h>
#include <game/CPointOfInterest.h>
#include <game/SaveData.h>
#include <game/unk_PointOfInterestManager.h>
#include <climax/Utils.h>

const char* lbl_80527498 = "SaveData";

u32 lbl_80529A80 = fn_801A7A84( "CPointOfInterest" );

/*
====================
CPointOfInterest::CPointOfInterest
====================
*/
// 0x800C4F80
CPointOfInterest::CPointOfInterest( const unk_AttributePacket& packet ) {
    fn_800C5050( packet, true );
}

/*
====================
CPointOfInterest::~CPointOfInterest
====================
*/
// 0x800C4FC0
CPointOfInterest::~CPointOfInterest() {
    delete saveDataA;
    delete saveDataB;
    lbl_80529A88->fn_800C5704( this );
    if ( field_0x40 != NULL ) {
        delete field_0x40;
        field_0x40 = NULL;
    }
}

/*
====================
CPointOfInterest::fn_800C5050

Sets the defaults and files the point with the point manager.
====================
*/
// 0x800C5050
void CPointOfInterest::fn_800C5050( const unk_AttributePacket& packet, bool addToManager ) {
    matrix = NULL;
    name = NULL;
    field_0xC = 2.0f;
    offset.x = offset.y = offset.z = 0.0f;
    field_0x1C = true;
    field_0x1D = false;
    field_0x20 = 0.0f;
    extent = 0.0f;
    field_0x28 = 0.0f;
    field_0x2C = 0.0f;
    field_0x30 = 0;
    field_0x34 = 6.0f;
    field_0x40 = NULL;
    if ( addToManager ) {
        lbl_80529A88->fn_800C56A4( this );
    }
    saveDataA = NULL;
    saveDataB = NULL;
}

/*
====================
CPointOfInterest::fn_801A73A8

Takes the shape, flags and save-data keys from the attribute packet.
====================
*/
// 0x800C50E8
void CPointOfInterest::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_AttributeCommandIterator it( packet, lbl_80529A80 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetField0xC:
            field_0xC = it->GetFloat();
            break;
        case CMD_SetOffset:
            offset = it->GetVector();
            break;
        case CMD_SetField0x1C:
            if ( it->GetU32() != 0 ) {
                field_0x1C = true;
            } else {
                field_0x1C = false;
            }
            break;
        case CMD_SetField0x1D:
            if ( it->GetU32() != 0 ) {
                field_0x1D = true;
            } else {
                field_0x1D = false;
            }
            break;
        case CMD_SetField0x20:
            field_0x20 = it->GetFloat();
            break;
        case CMD_SetExtent:
            extent = it->GetFloat();
            break;
        case CMD_SetField0x28:
            field_0x28 = it->GetFloat();
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField0x40:
            field_0x40 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField0x2C:
            field_0x2C = it->GetFloat();
            break;
        case CMD_SetField0x34:
            field_0x34 = it->GetFloat();
            break;
        case CMD_SetField0x30:
            field_0x30 = it->GetU32();
            break;
        }
        ++it;
    }
}

/*
====================
CPointOfInterest::fn_800C5280

The point on the line nearest the behaviour's position.
====================
*/
// 0x800C5280
unk_Vector3 CPointOfInterest::fn_800C5280( const CBaseBehaviour* behaviour ) {
    unk_Vector3 nearest;
    fn_800C53F4( behaviour->fn_8009CD08(), &nearest );
    return nearest;
}

/*
====================
CPointOfInterest::fn_800C52D8

The position along the line nearest the behaviour.
====================
*/
// 0x800C52D8
float CPointOfInterest::fn_800C52D8( const CBaseBehaviour* behaviour ) {
    return fn_800C5320( behaviour->fn_8009CD08() );
}

/*
====================
CPointOfInterest::fn_800C5320

The position along the line (from the centre, along the right vector, within extent) nearest
point; 0 for a point with no extent.

Parked, wall class register/schedule: the body is complete and the call set and control flow
are retail's; the compile differs in 12 rows, all one consistent renaming (r5 and r7 swapped:
retail holds the matrix in r5 and the address of toPoint in r7, this build the reverse) and
the place of the stwu. Tried: scalar and operator forms of centre and toPoint, declaration order
of the three locals, early return, a named matrix local; none moves the allocation. Also worse:
a GetCentre() inline shared with fn_800C53F4 (67%, and breaks fn_800C53F4), toPoint built by the
three-float constructor (76%), the locals declared before the extent test (75%).
====================
*/
// 0x800C5320
float CPointOfInterest::fn_800C5320( const unk_RwV3d* point ) {
    if ( extent > 0.0f ) {
        unk_Vector3 centre = offset + matrix->pos;
        unk_Vector3 axis = matrix->right;
        unk_Vector3 toPoint;
        toPoint.x = point->x - centre.x;
        toPoint.y = point->y - centre.y;
        toPoint.z = point->z - centre.z;
        float along = V3dDot( &toPoint, &axis );
        if ( along > extent ) {
            along = extent;
        } else if ( along < -extent ) {
            along = -extent;
        }
        return along;
    }
    return 0.0f;
}

/*
====================
CPointOfInterest::fn_800C53F4

The point on the line nearest point.
====================
*/
// 0x800C53F4
void CPointOfInterest::fn_800C53F4( const unk_RwV3d* point, unk_RwV3d* out ) {
    unk_Vector3 right = matrix->right;
    unk_Vector3 centre = offset + matrix->pos;
    *out = centre;
    unk_Vector3 delta = right * fn_800C5320( point );
    V3dAdd( out, out, &delta );
}

/*
====================
CPointOfInterest::fn_800C54E4
====================
*/
// 0x800C54E4
bool CPointOfInterest::fn_800C54E4() {
    return false;
}

/*
====================
CPointOfInterest::fn_800C54EC

Whether the save-data value is set.
====================
*/
// 0x800C54EC
int CPointOfInterest::fn_800C54EC() {
    return fn_80206348( saveDataA, saveDataB, lbl_80527498 );
}

// Compiler-generated, no source of their own: __sinit_CPointOfInterest_cpp (0x800C5500).
