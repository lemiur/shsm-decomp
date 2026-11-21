// Unit name and boundary: the class name is registered as "CSmashableObject" (DOL string);
// the unit's data and boundary come from the unit planner.

#include <game/CSmashableObject.h>
#include <game/CSystemCommands.h>
#include <game/unk_PhysicsManager.h>
#include <game/Vec4.h>

// The class id of this class's attribute commands; written by fn_800A7BF8.
u32 lbl_80529830;

/*
====================
CSmashableObject::CSmashableObject
====================
*/
// 0x800A72E4
CSmashableObject::CSmashableObject( const unk_AttributePacket& packet ) : unk_8009D2E4( packet ),
    field_0x148( 0 ), field_0x158( 0 ), field_0x15C( 0 ), field_0x160( 0 ), field_0x164( 0 ),
    field_0x1B4( 0 ), field_0x1B8( 0 ), field_0x1BC( 0 ), field_0x1C0( 0 ), field_0x228( 0 ),
    field_0x22C( 0 ) {
    unk_803E8E64::field_0x4 = 4;
}

/*
====================
CSmashableObject::~CSmashableObject
====================
*/
// 0x800A7380
CSmashableObject::~CSmashableObject() {
    if ( field_0x228 != 0 ) {
        fn_800A7AF4();
    }
    fn_801A9940( field_0x230 );
}

/*
====================
CSmashableObject::fn_801A73A8

Attribute handler: attribute 0 of the packet sets the hit limit; attribute 1 registers the
event id; the matrix attributes are passed to the CBaseBehaviour slot 0x54 (fn_8009CE2C).
====================
*/
// 0x800A742C
void CSmashableObject::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_8009D2E4::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_80529830 ); !it.IsFinished(); ++it ) {
        switch ( it->id ) {
        case 0:
            field_0x144 = it->GetU32();
            break;
        case 1:
            fn_801AA1C4( field_0x230, it->GetString(), NULL );
            break;
        }
    }
    for ( unk_AttributeCommandIterator sys( packet, lbl_805297E8 ); !sys.IsFinished(); ++sys ) {
        switch ( sys->id ) {
        case 1: {
            unk_RwMatrix matrix = *( const unk_RwMatrix* )sys->data;
            fn_8009CE2C( &matrix );
            break;
        }
        }
    }
}

/*
====================
CSmashableObject::fn_8009E6D4
====================
*/
// 0x800A75D4
void CSmashableObject::fn_8009E6D4() {
    unk_8009D2E4::fn_8009E6D4();
}

/*
====================
CSmashableObject::fn_8009CB1C
====================
*/
// 0x800A75D8
void CSmashableObject::fn_8009CB1C( unk_Msg& msg ) {
    unk_8009D2E4::fn_8009CB1C( msg );
}

/*
====================
CSmashableObject::fn_8000CFD0
====================
*/
// 0x800A75DC
void CSmashableObject::fn_8000CFD0() {
    if ( field_0x228 == 0 ) {
        fn_800A7704();
    }
}

/*
====================
CSmashableObject::fn_8000CFCC
====================
*/
// 0x800A75F0
void CSmashableObject::fn_8000CFCC() {
    if ( field_0x228 != 0 ) {
        fn_800A7AF4();
    }
}

/*
====================
CSmashableObject::fn_80017558
====================
*/
// 0x800A7604
void CSmashableObject::fn_80017558() {
    unk_Msg msg( field_0x230, this );
    if ( field_0x230.record != lbl_8052A530.record && field_0x230.record->head.next != NULL ) {
        fn_801AA2AC( msg );
    }
}

/*
====================
CSmashableObject::fn_80017554
====================
*/
// 0x800A764C
void CSmashableObject::fn_80017554() {
    fn_800A7AF4();
    fn_80024BB4();
}

/*
====================
CSmashableObject::fn_8001755C
====================
*/
// 0x800A7680
void CSmashableObject::fn_8001755C() {
    if ( fn_80024BAC() == 1 && field_0x228 != 0 ) {
        fn_800A7AF4();
    }
}

/*
====================
CSmashableObject::fn_800A76CC

Counts a hit; at the limit, sets the flag at 0x78 and starts the base's timer at 0.
====================
*/
// 0x800A76CC
void CSmashableObject::fn_800A76CC() {
    field_0x148++;
    if ( field_0x148 != field_0x144 ) {
        return;
    }
    unk_80024158::field_0x3C = 1;
    fn_80024C74( 0.0f );
}

/*
====================
CSmashableObject::fn_8000F5DC
====================
*/
// 0x800A76FC
void CSmashableObject::fn_8000F5DC() {
}

/*
====================
CSmashableObject::fn_8000F5E0
====================
*/
// 0x800A7700
void CSmashableObject::fn_8000F5E0() {
}

/*
====================
CSmashableObject::fn_800A7AF4
====================
*/
// 0x800A7AF4
void CSmashableObject::fn_800A7AF4() {
    if ( field_0x228 != 0 ) {
        unk_PhysicsManager* manager = lbl_80529170;
        field_0x164->managerNode = NULL;
        if ( manager != NULL ) {
            manager->fn_8000F278( field_0x164 );
            manager->fn_8000F71C( field_0x164 );
        }
        field_0x1C0->managerNode = NULL;
        if ( manager != NULL ) {
            manager->fn_8000F278( field_0x1C0 );
            manager->fn_8000F71C( field_0x1C0 );
        }
        fn_802FA9D4( field_0x160 );
        fn_802FA9D4( field_0x1BC );
        field_0x158 = 0;
        field_0x1B4 = 0;
        field_0x15C = 0;
        field_0x1B8 = 0;
        field_0x160 = NULL;
        field_0x1BC = NULL;
        field_0x164 = NULL;
        field_0x1C0 = NULL;
        field_0x228 = 0;
    }
}

/*
====================
CSmashableObject::fn_800A7BC4
====================
*/
// 0x800A7BC4
extern const f32 lbl_8052C398;

void CSmashableObject::fn_800A7BC4( unk_Vec4* out ) {
    out->x = field_0x204;
    out->y = field_0x208;
    out->z = field_0x20C;
    f32 sum = field_0x154 + field_0x14C;
    out->w = sum * lbl_8052C398;
}

/*
====================
fn_800A7C58
====================
*/
// 0x800A7C58
extern "C" void fn_800A7C58( const unk_AttributePacket& packet ) {
    new ( 2, 0x40000 ) CSmashableObject( packet );
}
