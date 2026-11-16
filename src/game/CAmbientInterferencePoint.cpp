// CAmbientInterferencePoint: class name from the registration strings lbl_803EB38C and lbl_803EB3A8.
// The inline factory fn_800C2BF0 and the two thunks follow the static initialiser in .text.

#include <game/CAmbientInterferencePoint.h>
#include <game/DisplayHeight.h>
#include <game/VectorPS.h>
#include <game/unk_MessageInbox.h>
#include <math.h>
#include <stdio.h>

u32 lbl_80529A40 = fn_801A7A84( "CAmbientInterferencePoint" );
unk_801A8F6C lbl_80529A44( "CAmbientInterferencePoint", CAmbientInterferencePoint::fn_800C2BF0, sizeof( CAmbientInterferencePoint ) );

const char* lbl_80527480 = "AmbientInterferenceData";
const char* lbl_80527484 = "AmbientInterferenceID";
const char* lbl_80527488 = "SaveData";
bool lbl_8052748C = true;

#define FLOAT_MAX       3.4028235e+38f
#define RAD_TO_DEG      57.29578f

/*
====================
NormalizeFacing

Scales v to unit length, leaving a zero vector alone: the unit-local form (z loaded first, one
register for the squared length), used for the facing; the delta goes through V3dNormalize
(VectorPS.h). Name invented.
====================
*/
static inline void NormalizeFacing( register unk_RwV3d* v ) {
    register float half = 0.5f;
    register float three = 3.0f;
    register float xy;
    register float z;
    register float sum;
    register float sq;
    register float estimate;
    register float zero = 0.0f;
    asm {
        psq_l       z, 8(v), 1, 0
        psq_l       xy, 0(v), 0, 0
        ps_mul      sq, xy, xy
        ps_madd     sum, z, z, sq
        ps_sum0     sum, sum, z, sq
        fcmpu       cr0, sum, zero
        beq         done
        frsqrte     estimate, sum
        fmuls       sq, estimate, estimate
        fmuls       estimate, estimate, half
        fnmsubs     sq, sq, sum, three
        fmuls       estimate, sq, estimate
        ps_muls0    xy, xy, estimate
        ps_muls0    z, z, estimate
        psq_st      xy, 0(v), 0, 0
        psq_st      z, 8(v), 1, 0
    done:
    }
}

/*
====================
FacingDot

Name invented.
====================
*/
static inline float FacingDot( register const unk_RwV3d* a, register const unk_RwV3d* b ) {
    register float dot;
    asm {
        psq_l       fp0, 0(b), 0, 0
        psq_l       fp2, 0(a), 0, 0
        psq_l       fp1, 8(b), 1, 0
        ps_mul      fp0, fp0, fp2
        psq_l       fp3, 8(a), 1, 0
        fmuls       fp1, fp1, fp3
        ps_sum0     fp2, fp0, fp0, fp0
        fadds       dot, fp1, fp2
    }
    return dot;
}

/*
====================
CAmbientInterferencePoint::CAmbientInterferencePoint
====================
*/
// 0x800C1E48
CAmbientInterferencePoint::CAmbientInterferencePoint( const unk_AttributePacket& packet ) :
    field_0x34( false ),
    field_0x35( false ),
    field_0x36( true ),
    field_0x38( 1.0f ),
    field_0x3C( 0.5f ),
    field_0x40( 0.0f ),
    field_0x44( 2.0f ),
    field_0x48( FLOAT_MAX ),
    field_0x4C( 0.0f ),
    field_0x50( false ),
    field_0x54( 0 ),
    field_0x58( 0.0f ),
    field_0x5C( 0.0f ),
    field_0x70( 0 ),
    field_0x74( NULL ),
    field_0x78( NULL ),
    field_0xD4( true ),
    field_0xD5( true ) {
}

/*
====================
CAmbientInterferencePoint::StartListening
====================
*/
inline void CAmbientInterferencePoint::StartListening() {
    fn_801A9DE8( lbl_8052A788, NULL, 10 );
    PostMsg( lbl_8052A0B0, &field_0x54 );
    field_0xD5 = true;
}

/*
====================
CAmbientInterferencePoint::StopListening
====================
*/
inline void CAmbientInterferencePoint::StopListening() {
    fn_801A9E00( lbl_8052A788 );
    PostMsg( lbl_8052A0B8, &field_0x54 );
    field_0xD5 = false;
}

/*
====================
CAmbientInterferencePoint::~CAmbientInterferencePoint
====================
*/
// 0x800C1F30
CAmbientInterferencePoint::~CAmbientInterferencePoint() {
    if ( field_0xD5 ) {
        StopListening();
    }
    delete field_0x74;
    delete field_0x78;
    fn_801A9E00( field_0x60 );
    fn_801A9940( field_0x60 );
    fn_801A9940( field_0x68 );
    fn_801A9E00( field_0xE0 );
    fn_801A9940( field_0xE0 );
    fn_801A9E00( field_0xD8 );
    fn_801A9940( field_0xD8 );
}

/*
====================
CAmbientInterferencePoint::fn_8000CFCC

Restores the interference level while the sound plays.
====================
*/
// 0x800C207C
void CAmbientInterferencePoint::fn_8000CFCC() {
    if ( field_0x50 ) {
        fn_801580A8( 1.0f );
    }
}

/*
====================
CAmbientInterferencePoint::fn_801A7A88
====================
*/
// 0x800C2094
void CAmbientInterferencePoint::fn_801A7A88() {
    CBaseBehaviour::fn_801A7A88();
    if ( field_0xD5 ) {
        StartListening();
    }
}

/*
====================
CAmbientInterferencePoint::fn_8009CB1C

Each frame, checks the player; the three messages named by the packet stop, start and end the point.
====================
*/
// 0x800C2118
void CAmbientInterferencePoint::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == lbl_8052A788.record && !field_0x34 && field_0x36 == true ) {
        unk_80491B38* player = &lbl_80491B38;
        if ( player != NULL ) {
            if ( lbl_80529D20 != NULL ) {
                field_0x35 = lbl_80529D20->field_0xA8 != 0;
            }
            if ( !fn_800C2304( player->field_0x14 ) && lbl_8052748C ) {
                fn_800C25EC( player->field_0x8 );
            }
        }
    } else if ( msg.record == field_0x60.record && field_0x36 ) {
        PostMsg( lbl_8052A0B8, &field_0x54 );
        if ( field_0xD4 ) {
            lbl_8052A10C->field_0x14 = 0.0f;
        }
        fn_801580A8( 1.0f );
        field_0x36 = false;
    } else if ( msg.record == field_0xD8.record ) {
        if ( field_0x36 ) {
            StartListening();
        }
    } else if ( msg.record == field_0xE0.record && field_0x36 ) {
        StopListening();
    }
}

/*
====================
CAmbientInterferencePoint::fn_800C2304

Fires when the player is within the outer range and faces the point; 1 when it fired.
====================
*/
// MATCH-PENDING (R18, 2/186 rows): the vector helpers are inline asm (paired singles, which C
// cannot express; scheduled with the C code, so they were inline asm in the original too, but
// their header is not identified). The two normalisations are two different helpers: the facing's
// keeps the squared length in one register and reloads 0.0f with the stores before it, the delta's
// is VectorPS.h's V3dNormalize (separate registers, its own 0.0f load). The half angle is
// written `/ 2.0f` (folded to * 0.5f, but numbered before the constant's load, as retail).
// Residue: the order of FacingDot's two address computations (retail computes &delta, in r3,
// first; every inline form tried, pointer or reference parameters, copied into register locals
// in either order, evaluates the last argument first).
// Not a const-parameter effect: the rows that move are two address computations; no access
// through target or this crosses a store. VectorPS.h's V3dDot( &facing, &delta ) (the same asm
// with the operands swapped) gives retail's computation order but swaps r3 and r4 (6 rows);
// a reference-parameter wrapper around either helper, or swapped parameters, does not move it.
// 0x800C2304
int CAmbientInterferencePoint::fn_800C2304( CBaseBehaviour* target ) {
    if ( target != NULL && fn_80070284( target ) && field_0xD5 && target->GetZone() == GetZone() ) {
        const unk_RwV3d* myPos = fn_8009CD08();
        unk_RwV3d pos;
        pos.x = myPos->x;
        pos.y = myPos->y;
        pos.z = myPos->z;
        const unk_RwV3d* targetPos = target->fn_8009CD08();
        unk_RwV3d head;
        unk_RwV3d delta;
        unk_RwV3d facing;
        head.x = targetPos->x;
        head.y = targetPos->y;
        head.z = targetPos->z;
        head.y += 1.8f;
        delta.x = pos.x - head.x;
        delta.y = pos.y - head.y;
        delta.z = pos.z - head.z;
        delta.y *= field_0x44;
        field_0x48 = V3dLength( &delta );
        if ( field_0x48 <= field_0x38 ) {
            if ( field_0x35 ) {
                facing = *target->fn_8009CD78();
            } else {
                unk_Msg query( lbl_8052A120, NULL );
                if ( query.record != lbl_8052A530 && query.record->head.next != NULL ) {
                    fn_801AA2AC( query );
                }
                facing = static_cast< unk_CameraInfo* >( query.data )->direction;
            }
            facing.y = 0.0f;
            delta.y = 0.0f;
            NormalizeFacing( &facing );
            V3dNormalize( &delta );
            field_0x4C = RAD_TO_DEG * ( float )acos( FacingDot( &delta, &facing ) );
            if ( field_0x4C <= field_0x40 / 2.0f && field_0x48 <= field_0x3C ) {
                field_0x34 = true;
                field_0x36 = false;
                fn_800C2790();
                return 1;
            }
        }
    } else {
        field_0x48 = FLOAT_MAX;
    }
    return 0;
}

/*
====================
CAmbientInterferencePoint::fn_800C25EC

Fades the interference sound with the player's distance.
====================
*/
// 0x800C25EC
void CAmbientInterferencePoint::fn_800C25EC( float dt ) {
    if ( field_0x48 <= field_0x3C ) {
        field_0x58 = 1.0f;
        if ( field_0xD4 ) {
            lbl_8052A10C->field_0x14 = 9.0f;
        }
        field_0x7C.fn_80157F7C( dt, 1.0f );
        fn_801580A8( field_0x7C.fn_80157D08( field_0x7C.field_0x48, 0.5f ) );
        field_0x50 = true;
    } else if ( field_0x48 <= field_0x38 ) {
        float level = 1.0f - ( field_0x48 - field_0x3C ) / ( field_0x38 - field_0x3C );
        field_0x58 = level;
        float distortion = level * ( 9.0f - lbl_80530080 );
        if ( field_0xD4 ) {
            lbl_8052A10C->field_0x14 = distortion;
        }
        field_0x7C.fn_80157F7C( dt, level );
        fn_801580A8( field_0x7C.fn_80157D08( field_0x7C.field_0x48, 0.5f * level ) );
        field_0x50 = true;
    } else {
        field_0x58 = 0.0f;
        if ( field_0x50 ) {
            if ( field_0xD4 ) {
                lbl_8052A10C->field_0x14 = lbl_80530080;
            }
            fn_801580A8( 1.0f );
            field_0x50 = false;
        }
    }
    if ( field_0x48 <= field_0x38 ) {
        field_0x5C = ( 1.0f - field_0x4C / 120.0f ) * ( 0.25f + 0.75f * ( field_0x58 * field_0x58 ) );
    } else {
        field_0x5C = 0.0f;
    }
}

/*
====================
CAmbientInterferencePoint::fn_800C2790

Fires the point: stops the sound and sends the message named by the packet; 1 when it fired.
====================
*/
// 0x800C2790
int CAmbientInterferencePoint::fn_800C2790() {
    int fired = 0;
    if ( fn_80206348( field_0x74, field_0x78, lbl_80527488 ) && field_0xD5 ) {
        PostMsg( lbl_8052A0B8, &field_0x54 );
        PostMsg( field_0x68, NULL );
        if ( field_0xD4 ) {
            lbl_8052A10C->field_0x14 = 0.0f;
        }
        fn_801580A8( 1.0f );
        fired = 1;
    }
    return fired;
}

/*
====================
CAmbientInterferencePoint::fn_801A73A8

Takes the ranges, messages and flags from the packet's commands.
====================
*/
// 0x800C2870
void CAmbientInterferencePoint::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_80529A40 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetFieldMsg60:
            fn_801AA218( field_0x60, it->GetString(), NULL );
            break;
        case CMD_SetField38:
            field_0x38 = it->GetFloat();
            break;
        case CMD_SetField3C:
            field_0x3C = it->GetFloat();
            break;
        case CMD_SetField40:
            field_0x40 = it->GetFloat();
            break;
        case CMD_SetFieldMsg68:
            fn_801AA1C4( field_0x68, it->GetString(), NULL );
            break;
        case CMD_SetField74:
            field_0x74 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField78:
            field_0x78 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField44:
            field_0x44 = it->GetFloat();
            break;
        case CMD_SetFieldD5:
            field_0xD5 = it->GetU32();
            break;
        case CMD_SetFieldMsgD8:
            fn_801AA218( field_0xD8, it->GetString(), NULL );
            break;
        case CMD_SetFieldMsgE0:
            fn_801AA218( field_0xE0, it->GetString(), NULL );
            break;
        case CMD_SetFieldD4:
            field_0xD4 = it->GetU32();
            break;
        }
        ++it;
    }
    if ( field_0x38 < field_0x3C ) {
        field_0x38 = 1.0f + field_0x3C;
    }
}

/*
====================
CAmbientInterferencePoint::fn_800C2A0C

Saves (arg true) or loads the fired and armed flags under names built from text.
====================
*/
// 0x800C2A0C
void CAmbientInterferencePoint::fn_800C2A0C( const char* text, bool arg ) {
    void* save = fn_802050AC( fn_80204C04(), lbl_80527480, 1, 0, 0, 0 );
    if ( !fn_80206348( field_0x74, field_0x78, lbl_80527488 ) && field_0xD5 ) {
        field_0x36 = false;
    } else if ( save != NULL && text != NULL ) {
        char firedName[128];
        char activeName[128];
        sprintf( firedName, "%s%s", lbl_80527484, text );
        sprintf( activeName, "%s%s_active", lbl_80527484, text );
        if ( arg ) {
            u8 fired = field_0x34;
            fn_80205354( save, firedName, 1, &fired, 1 );
            u8 armed = field_0x36;
            fn_80205354( save, activeName, 1, &armed, 1 );
        } else {
            fn_802057FC( save, firedName, 1, &field_0x34, 0 );
            fn_802057FC( save, activeName, 1, &field_0x36, 0 );
        }
    }
}
