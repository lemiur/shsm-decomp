#include <game/CBasicAnimatedObject.h>
#include <game/Messages.h>


/*
====================
CBasicAnimatedObject::CBasicAnimatedObject
====================
*/
// 0x80020B04
CBasicAnimatedObject::CBasicAnimatedObject( const unk_AttributePacket& packet ) :
    unk_8009D2E4( packet ),
    field_0x148( 0 ),
    field_0x17C( NULL ),
    field_0x1C0( 0 ),
    field_0x1C1( 1 ),
    field_0x1C2( 1 ),
    field_0x1C3( 1 ),
    field_0x1C4( 0 ),
    field_0x1C5( 0 ),
    field_0x1C6( 0 ),
    field_0x1C7( 0 ),
    field_0x1C8( 0 ),
    field_0x1C9( 0 ),
    field_0x1CC( 0 ),
    field_0x1D0( 0 )
{
    field_0x144 = fn_80025674( unk_80024158::fn_80024A14() );
    CEventHandler::fn_801A9DE8( lbl_8052A790 );
    RwMatrixSetIdentity( &field_0x180 );
}

/*
====================
CBasicAnimatedObject::~CBasicAnimatedObject
====================
*/
// 0x80020C4C
CBasicAnimatedObject::~CBasicAnimatedObject() {
    // TODO: the field_0x1C9 branch is not reconstructed. Wall (referent): it clears bit field_0x1CC
    // in the bitmap at lbl_80529D20+0x71 and calls fn_8014B07C(lbl_8052A078), whose owner is unreconstructed.
    if ( field_0x144 != NULL ) {
        delete field_0x144;
        field_0x144 = NULL;
    }
    if ( field_0x148 != NULL ) {
        for ( int i = 0; i < 12; i++ ) {
            if ( field_0x148[i].field_0xB4 ) {
                CEventHandler::fn_801A9E00( field_0x148[i].field_0xB4 );
                fn_801A9940( field_0x148[i].field_0xB4 );
            }
        }
        delete[] field_0x148;
    }
    if ( field_0x17C != NULL && *field_0x17C != 0 ) {
        CEventHandler::fn_801A9E00( lbl_805298A0 );
        CEventHandler::fn_801A9E00( lbl_805298B0 );
    }
    CEventHandler::fn_801A9E00( lbl_8052A790 );
    CEventHandler::fn_801A9E00( field_0x14C );
    fn_801A9940( field_0x14C );
    CEventHandler::fn_801A9E00( field_0x154 );
    fn_801A9940( field_0x154 );
    CEventHandler::fn_801A9E00( field_0x15C );
    fn_801A9940( field_0x15C );
    CEventHandler::fn_801A9E00( field_0x164 );
    fn_801A9940( field_0x164 );
    fn_801A9940( field_0x16C );
    CEventHandler::fn_801A9E00( field_0x174 );
    fn_801A9940( field_0x174 );
}

// Climax clump rendering switches (climax/ClimaxT1Effect.h, not included: its RW headers clash with this unit).
extern "C" void ClimaxT1ClumpStartRendering( unk_RpClump* clump );
extern "C" void ClimaxT1ClumpStopRendering( unk_RpClump* clump );
extern "C" void fn_8009E894( CBasicAnimatedObject* self, unk_RpClump* clump, u8 flag );
extern "C" void fn_80024B80( unk_80024158* self, int mode );
extern "C" void fn_80024B90( unk_80024158* self, int id, u8 value );
extern "C" void fn_80021410( CBasicAnimatedObject* self );

/*
====================
fn_80020EA8
====================
*/
// 0x80020EA8
extern "C" void fn_80020EA8( CBasicAnimatedObject* self, bool flag ) {
    unk_RpClump* clump = self->unk_80024158::fn_80024A14();
    if ( clump != NULL ) {
        if ( flag ) {
            ClimaxT1ClumpStartRendering( clump );
        } else {
            ClimaxT1ClumpStopRendering( clump );
        }
    }
    if ( self->field_0x130 != NULL ) {
        fn_8009E894( self, self->field_0x130, flag );
    }
}

/*
====================
CBasicAnimatedObject::fn_801A7A88
====================
*/
// 0x80020F18
void CBasicAnimatedObject::fn_801A7A88() {
    // Open: the byte store to 0x78 (untyped member of unk_80024158) and the field_0x1C9 bitmap branch
    // (bit field_0x1CC of the bitmap at lbl_80529D20+0x71, then fn_8014AF1C and fn_8014B1E0; their owner is
    // unreconstructed, the same wall as the destructor).
    CAttributeHandler::fn_801A7A88();
    u8 flag = field_0x1C1;
    unk_RpClump* clump = unk_80024158::fn_80024A14();
    if ( clump != NULL ) {
        if ( flag ) {
            ClimaxT1ClumpStartRendering( clump );
        } else {
            ClimaxT1ClumpStopRendering( clump );
        }
    }
    if ( field_0x130 != NULL ) {
        fn_8009E894( this, field_0x130, flag );
    }
    if ( field_0x1C3 ) {
        fn_80024B80( this, 0 );
    } else {
        fn_80024B80( this, 2 );
    }
    fn_80024B90( this, 1, field_0x1C5 );
    if ( field_0x1C2 ) {
        field_0x1C8 = 0;
        if ( field_0x1C0 ) {
            clump = unk_80024158::fn_80024A14();
            if ( clump != NULL ) {
                ClimaxT1ClumpStartRendering( clump );
            }
            if ( field_0x130 != NULL ) {
                fn_8009E894( this, field_0x130, true );
            }
        }
        if ( field_0x1C3 ) {
            fn_80024B80( this, 0 );
        }
        unk_80024158::fn_80024C74( 0.0f );
    }
    fn_80021410( this );
    // TODO: the field_0x1C9 bitmap branch (see the comment above).
}

/*
====================
CBasicAnimatedObject::fn_801A73A8
====================
*/
// 0x800210AC
void CBasicAnimatedObject::fn_801A73A8( const unk_AttributePacket& ) {
    // TODO
}

/*
====================
fn_80021410
====================
*/
// 0x80021410
extern "C" void fn_80021410( CBasicAnimatedObject* self ) {
    // TODO
}

/*
====================
fn_800216A0
====================
*/
// 0x800216A0
extern "C" void fn_800216A0() {
    // TODO
}

/*
====================
fn_8002187C
====================
*/
// 0x8002187C
extern "C" void fn_8002187C() {
    // TODO
}

/*
====================
CBasicAnimatedObject::fn_8009CB1C
====================
*/
// 0x8002192C
void CBasicAnimatedObject::fn_8009CB1C( unk_Msg& ) {
    // TODO
}

/*
====================
CBasicAnimatedObject::fn_80017554
====================
*/
// 0x80022024
void CBasicAnimatedObject::fn_80017554() {
    // TODO
}

/*
====================
fn_80022178

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80022178
extern "C" void fn_80022178() {
    // TODO
}

// Compiler-generated thunks with no source of their own (the this-adjusting thunks of the secondary bases).
