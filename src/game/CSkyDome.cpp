// CSkyDome: class name from the registration string "CSkyDome" (the static initialiser at 0x800A720C).
// The inline factory fn_800A726C follows the static initialiser in .text.

#include <game/CSkyDome.h>
#include <game/Messages.h>

extern "C" void fn_80024B80( unk_80024158* self, int mode );
extern "C" float fn_80024A8C( unk_80024158* self );
extern "C" void fn_80024A94( unk_80024158* self, float value );
extern "C" void RwClimaxAtomicSetPass( unk_RpAtomic* atomic, u32 pass );

u32 lbl_80529820 = fn_801A7A84( "CSkyDome" );
unk_801A8F6C lbl_80529824( "CSkyDome", CSkyDome::fn_800A726C, sizeof( CSkyDome ) );
static CSkyDome* lbl_80529828;  // head of the list of live sky domes

/*
====================
CSkyDome::CSkyDome

Wall: the this-to-base conversion for fn_80024B80( this, 2 ) inside the constructor body makes MWCC emit a
null check (cmpwi/beq on r31) that retail does not have; the other statements match.
====================
*/
// 0x800A6A60
CSkyDome::CSkyDome( const unk_AttributePacket& packet ) : unk_8009D2E4( packet ) {
    fn_80024B80( this, 2 );
    bounds = fn_80025674( fn_80024A14() );
    fn_80024C74( 0.0f );
    field_0x3C = 1;
    field_0x14C = 0;
    next = NULL;
    if ( lbl_80529828 == NULL ) {
        lbl_80529828 = this;
    } else {
        CSkyDome* last = lbl_80529828;
        while ( last->next != NULL ) {
            last = last->next;
        }
        last->next = this;
    }
}

/*
====================
CSkyDome::~CSkyDome
====================
*/
// 0x800A6B34
CSkyDome::~CSkyDome() {
    CEventHandler::fn_801A9E00( field_0x15C );
    fn_801A9940( field_0x15C );
    CEventHandler::fn_801A9E00( field_0x154 );
    fn_801A9940( field_0x154 );
    if ( bounds != NULL ) {
        delete bounds;
        bounds = NULL;
    }
    if ( lbl_80529828 == this ) {
        lbl_80529828 = lbl_80529828->next;
    } else {
        CSkyDome* last = lbl_80529828;
        while ( last->next != this ) {
            last = last->next;
        }
        last->next = next;
    }
}

/*
====================
fn_800A6C4C

RpClumpForAllAtomics callback of CSkyDome::fn_801A73A8: sets the atomic's pass to 0 and returns it so the
walk goes on. The data argument is unused.
====================
*/
// 0x800A6C4C
extern "C" unk_RpAtomic* fn_800A6C4C( unk_RpAtomic* atomic, void* data ) {
    RwClimaxAtomicSetPass( atomic, 0 );
    return atomic;
}

/*
====================
CSkyDome::fn_801A73A8

Applies the packet: the base first, then the commands (two float fields, two event ids), then the
clump's atomics are reset to pass 0.
====================
*/
// 0x800A6C80
void CSkyDome::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_8009D2E4::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_80529820 ); !it.IsFinished(); ++it ) {
        switch ( it->id ) {
        case 0:
            field_0x144 = it->GetFloat();
            break;
        case 1:
            field_0x148 = it->GetFloat();
            break;
        case 3:
            CEventHandler::fn_801AA218( field_0x15C, it->GetString(), NULL );
            break;
        case 2:
            CEventHandler::fn_801AA218( field_0x154, it->GetString(), NULL );
            break;
        }
    }
    if ( field_0x130 != NULL ) {
        RpClumpForAllAtomics( field_0x130, fn_800A6C4C, NULL );
    }
}

/*
====================
CSkyDome::fn_8009CB1C

The message handler. The per-frame message moves the clump's position (fn_80024C74) against the two
limits from the attribute commands, the flag at 0x14C says which limit applies; the two event ids
set and clear that flag. The names of the flag and the limits are invented.

Wall: retail branches on the compares with cror-encoded conditions and lays the else block out after the
first test; the compiled form has two extra blocks (control flow), and the `this` passed to fn_80024A8C is
the one that tests for null otherwise.
====================
*/
// 0x800A6D84
void CSkyDome::fn_8009CB1C( unk_Msg& msg ) {
    unk_8009D2E4::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        float pos = fn_80024A8C( &static_cast<unk_80024158&>( *this ) );
        float limit = unk_80024158::fn_80024A58();
        if ( field_0x14C == 0 ) {
            if ( pos < field_0x144 ) {
                float offset = pos - field_0x144;
                if ( offset < 0.0f ) {
                    offset = 0.0f;
                }
                unk_80024158::fn_80024C74( offset );
            } else if ( pos < limit ) {
                float offset = pos - limit;
                if ( offset < 0.0f ) {
                    offset = 0.0f;
                }
                unk_80024158::fn_80024C74( offset );
            }
        } else if ( pos < limit ) {
            float low = field_0x148;
            float offset = pos - ( limit - low );
            if ( offset < low ) {
                offset = low;
            }
            unk_80024158::fn_80024C74( offset );
        }
        if ( bounds != NULL ) {
            bounds->fn_80025750();
        }
    } else if ( msg.record == field_0x15C.record ) {
        field_0x14C = 1;
    } else if ( msg.record == field_0x154.record ) {
        fn_80024A94( this, 0.0f );
        unk_80024158::fn_80024C74( 0.0f );
        field_0x14C = 0;
    }
}
