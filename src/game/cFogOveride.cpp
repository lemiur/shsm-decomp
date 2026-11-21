// cFogOveride: class name from the registry strings "cFogOveride" (0x803E7EC4, 0x803E7ED0).
// The unit boundary follows the class's registry and static initialiser (0x800864A4).
// The constructor 0x80086504 is inline in the factory; the member functions are free functions
// (open work: the class form and the mangled names). Open wall: fn_80085DB8 (fog blend) and
// fn_80086294 (attribute switch) are not reconstructed.

#include <game/cFogOveride.h>
#include <game/CSystemCommands.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>
#include <climax/Heap.h>
#include <PowerPC_EABI_Support/Runtime/NMWException.h>

u32 fn_800681FC( CPlayer* player );     // the player's id, the key the setting search matches

u32 lbl_80529650 = fn_801A7A84( "cFogOveride" );
unk_801A8F6C lbl_80529654( "cFogOveride", fn_80086504, sizeof( cFogOveride ) );

unk_80085CBC* lbl_80529658;
unk_80085CBC* lbl_8052965C;
u32 lbl_80529660;
u8 lbl_80529664[3];
float lbl_80529668;
float lbl_8052966C;
unk_FogState lbl_8046A480;
float lbl_80527238 = 50.0f;
float lbl_8052723C = 0.3f;
float lbl_80527240 = 50.0f;
float lbl_80527244 = 0.3f;
float lbl_80527248 = 50.0f;
float lbl_8052724C = 0.3f;

/*
====================
fn_80085CBC

Pushes the record onto the list of fog settings with the default setting.
====================
*/
// 0x80085CBC
void fn_80085CBC( unk_80085CBC* self ) {
    self->next = lbl_80529658;
    lbl_80529658 = self;
    self->field_0x04 = 0.0f;
    self->field_0x08 = 0.0f;
    self->field_0x0C = 0.0f;
    self->field_0x14 = 0.0f;
    self->field_0x18 = 50.0f;
    self->field_0x1C = 0.3f;
    self->field_0x20 = 0.1f;
    self->field_0x24 = 0;
    self->field_0x28 = 0;
    fn_80085FB8();
}

/*
====================
fn_80085D30

Takes the record off the list, refreshes the fog and frees the record when deleteFlag is set.
====================
*/
// 0x80085D30
unk_80085CBC* fn_80085D30( unk_80085CBC* self, int deleteFlag ) {
    if ( self != NULL ) {
        unk_80085CBC* node = lbl_80529658;
        unk_80085CBC* prev = NULL;
        lbl_80529658 = NULL;
        while ( node != NULL ) {
            unk_80085CBC* cur = node;
            node = cur->next;
            if ( cur != self ) {
                cur->next = prev;
                prev = cur;
                lbl_80529658 = cur;
            }
        }
        fn_80085FB8();
        if ( deleteFlag > 0 ) {
            delete self;
        }
    }
    return self;
}

/*
====================
fn_80085DB8

Advances the fog blend by dt and writes the fog colour. Not reconstructed (open wall).
====================
*/
// 0x80085DB8
void fn_80085DB8( float dt ) {
    (void)dt;
}

/*
====================
fn_80085FB8

Picks the active setting that the player's id and weight select, and when it changes, starts a
blend from the old setting to it.
====================
*/
// 0x80085FB8
void fn_80085FB8() {
    u32 key = 0;
    if ( GetPlayerRecord().field_0x14 != NULL ) {
        key = fn_800681FC( GetPlayerRecord().field_0x14 );
    }
    lbl_80529660 = key;

    float bestWeight = -1.0f;
    unk_80085CBC* best = NULL;
    for ( unk_80085CBC* node = lbl_80529658; node != NULL; node = node->next ) {
        if ( node->field_0x24 != 0 ) {
            float weight = node->field_0x20;
            if ( node->field_0x28 == key ) {
                weight += 1.0f;
            }
            if ( best == NULL || weight > bestWeight ) {
                bestWeight = weight;
                best = node;
            }
        }
    }

    if ( best != lbl_8052965C ) {
        lbl_8052965C = best;
        lbl_8046A480.field_0x20 = lbl_8046A480.field_0x10;
        lbl_8046A480.field_0x24 = lbl_8046A480.field_0x14;
        lbl_8046A480.field_0x28 = lbl_8046A480.field_0x18;
        lbl_8046A480.field_0x2C = lbl_8046A480.field_0x1C;
        lbl_80527240 = lbl_80527238;
        lbl_80527244 = lbl_8052723C;
        if ( best != NULL ) {
            lbl_8046A480.field_0x30 = best->field_0x04;
            lbl_8046A480.field_0x34 = best->field_0x08;
            lbl_8046A480.field_0x38 = best->field_0x0C;
            lbl_8046A480.field_0x3C = best->field_0x10;
            lbl_80527248 = best->field_0x18;
            lbl_8052724C = best->field_0x1C;
            lbl_8052966C = best->field_0x14;
        } else {
            lbl_8046A480.field_0x30 = 0.0f;
            lbl_8046A480.field_0x34 = 0.0f;
            lbl_8046A480.field_0x38 = 0.0f;
            lbl_80527248 = 50.0f;
            lbl_8052724C = 0.3f;
            lbl_8052966C = 0.0f;
        }
        lbl_80529668 = 0.0f;
    }
}

/*
====================
fn_80086118

The destructor: takes the object's two message ids and its base apart, frees it when deleteFlag
is set.
====================
*/
// 0x80086118
void fn_80086118( cFogOveride* self, int deleteFlag ) {
    if ( self != NULL ) {
        self->field_0x3C.~unk_801A92A8();
        self->field_0x34.~unk_801A92A8();
        self->CBaseBehaviour::~CBaseBehaviour();
        if ( deleteFlag > 0 ) {
            delete self;
        }
    }
}

/*
====================
fn_8008620C

The message handler: the id in field_0x34 turns the override on, the one in field_0x3C turns it off.
====================
*/
// 0x8008620C
void fn_8008620C( cFogOveride* self, unk_Msg& msg ) {
    self->CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == self->field_0x34.GetRecord() ) {
        if ( self->setting.field_0x24 != 1 ) {
            self->setting.field_0x24 = 1;
            fn_80085FB8();
        }
    } else if ( msg.record == self->field_0x3C.GetRecord() ) {
        if ( self->setting.field_0x24 != 0 ) {
            self->setting.field_0x24 = 0;
            fn_80085FB8();
        }
    }
}

/*
====================
fn_80086294

Reads the attribute commands of the packet. Open wall: the switch over the eight command ids is
not reconstructed.
====================
*/
// 0x80086294
void fn_80086294( cFogOveride* self, const unk_AttributePacket& packet ) {
    (void)self;
    (void)packet;
}

/*
====================
fn_80086498

Open wall: copies a base field into the 0x84 word; not reconstructed.
====================
*/
// 0x80086498
void fn_80086498( cFogOveride* self ) {
    (void)self;
}

/*
====================
fn_80086504

The factory: the constructor body is inline in the constructor below.
====================
*/
// 0x80086504
CAttributeHandler* fn_80086504( const unk_AttributePacket& packet ) {
    (void)packet;
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) cFogOveride();
}

/*
====================
cFogOveride::cFogOveride

Pushes the setting onto the list, sets its defaults and runs the search.
====================
*/
inline cFogOveride::cFogOveride() {
    setting.next = lbl_80529658;
    lbl_80529658 = &setting;
    setting.field_0x04 = 0.0f;
    setting.field_0x08 = 0.0f;
    setting.field_0x0C = 0.0f;
    setting.field_0x14 = 0.0f;
    setting.field_0x18 = 50.0f;
    setting.field_0x1C = 0.3f;
    setting.field_0x20 = 0.1f;
    setting.field_0x24 = 0;
    setting.field_0x28 = 0;
    fn_80085FB8();
    field_0x54 = 0.0f;
}

/*
====================
fn_800865CC

Thunk: the message handler reached through the CEventHandler base.
====================
*/
// 0x800865CC
void fn_800865CC( cFogOveride* self, unk_Msg& msg ) {
    fn_8008620C( self, msg );
}

/*
====================
fn_800865D4

Thunk: the destructor reached through the CEventHandler base.
====================
*/
// 0x800865D4
void fn_800865D4( cFogOveride* self, int deleteFlag ) {
    fn_80086118( self, deleteFlag );
}
