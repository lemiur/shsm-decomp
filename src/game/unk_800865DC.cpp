// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800865DC.h>
#include <math.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E7EE0 size 0x58 jumptable_803E7EE0
//   .data 0x803E7F38 size 0x58 __vt__12unk_80086F9C
//   .data 0x803E7F90 size 0x90 __vt__12unk_800865DC
//   .sdata 0x80527250 size 0x8 lbl_80527250
//   .sbss 0x80529670 size 0x4 lbl_80529670
//   .sbss 0x80529674 size 0x4 lbl_80529674
//   .sbss 0x80529678 size 0x4 lbl_80529678
//   .sbss 0x8052967C size 0x4 lbl_8052967C
//   .sdata2 0x8052C000 size 0x4 lbl_8052C000
//   .sdata2 0x8052C004 size 0x4 lbl_8052C004
//   .sdata2 0x8052C008 size 0x4 lbl_8052C008
//   .sdata2 0x8052C00C size 0x4 lbl_8052C00C
//   .sdata2 0x8052C010 size 0x4 lbl_8052C010
//   .sdata2 0x8052C014 size 0x4 lbl_8052C014
//   .sdata2 0x8052C018 size 0x4 lbl_8052C018
//   .sdata2 0x8052C01C size 0x4 lbl_8052C01C
//   .sdata2 0x8052C020 size 0x4 lbl_8052C020
//   .sdata2 0x8052C024 size 0x4 lbl_8052C024
//   .sdata2 0x8052C028 size 0x8 lbl_8052C028
//   .sdata2 0x8052C030 size 0x4 lbl_8052C030
//   .sdata2 0x8052C034 size 0x4 lbl_8052C034
//   .sdata2 0x8052C038 size 0x8 lbl_8052C038

unk_800865DC* lbl_80529670;
unk_80086F9C* lbl_8052967C;
bool lbl_80527250;

// the colour and intensity lerps that fn_80086D7C moves (their initial values are the retail ones)
float lbl_80528818 = 0.4f;
float lbl_8052881C = 0.3f;
float lbl_80528820 = 1.0f;
float lbl_80528824 = 1.0f;
float lbl_80528828 = 1.0f;
float lbl_8052882C = 1.0f;
float lbl_80528830 = 6.0f;
float lbl_80528834 = 1.0f;
float lbl_80528838 = 1.5f;
float lbl_8052A680;

/*
====================
unk_800865DC::unk_800865DC

Sets the timings and the colour, links the instance into the list.
====================
*/
// 0x800865DC
unk_800865DC::unk_800865DC( const unk_AttributePacket& packet ) {
    field_0x34 = 50.0f;
    field_0x38 = 0;
    field_0x3C = 0.4f;
    field_0x40 = 0.4f;
    field_0x44 = 0;
    field_0x48 = 6.0f;
    field_0x4C = 1.0f;
    field_0x50 = 1.5f;
    field_0x54 = 0.0f;
    field_0x58 = 0;
    field_0x5C = 1.0f;
    field_0x60 = 1.0f;
    field_0x64 = 1.0f;
    field_0x68 = 1.0f;
    field_0x6C = 0;
    field_0x70 = 2.0f;
    field_0x74 = field_0x70 * field_0x70;
    field_0x78 = 1.0f;
    field_0x7C = 0;
    field_0x80 = 40.0f;
    field_0x84.fn_801A92FC( NULL );
    field_0x8C.fn_801A92FC( NULL );
    field_0x94 = 0;
    field_0x98 = 1.0f;
    field_0x9C = 2;
    next = lbl_80529670;
    lbl_80529670 = this;
}

/*
====================
unk_800865DC::fn_8009CB1C

Runs the base handler, then advances the state when the first or second message arrives.
====================
*/
// 0x800867E0
void unk_800865DC::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == field_0x84.record && field_0x9C == 0 ) {
        field_0xA0 = 0.0f;
        field_0x9C = 1;
    }
    if ( msg.record == field_0x8C.record ) {
        if ( field_0x9C == 2 ) {
            field_0xA0 = 1.0f;
            field_0x9C = 3;
        }
        if ( field_0x9C == 1 ) {
            field_0x9C = 3;
        }
    }
}

/*
====================
unk_800865DC::~unk_800865DC

Unlinks the instance from the list, unregisters both messages, then the base destructor.
====================
*/
// 0x800866F0
unk_800865DC::~unk_800865DC() {
    CEventHandler::fn_801A9E00( field_0x84 );
    fn_801A9940( field_0x84 );
    CEventHandler::fn_801A9E00( field_0x8C );
    fn_801A9940( field_0x8C );

    unk_800865DC* next = lbl_80529670;
    lbl_80529670 = NULL;
    while ( next != NULL ) {
        unk_800865DC* node = next;
        next = node->next;
        if ( node != this ) {
            node->next = lbl_80529670;
            lbl_80529670 = node;
        }
    }
    lbl_80527250 = true;
}

/*
====================
unk_800865DC::fn_801A73A8

Reads the attribute commands of the packet into the fields.
Open (register only): retail keeps this in r29 and the constants in r26-r31; the body is complete.
====================
*/
// 0x80086884
void unk_800865DC::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_80529674 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetField34:
            field_0x34 = it->GetFloat();
            lbl_80527250 = true;
            break;
        case CMD_SetField38:
            field_0x38 = it->GetU32();
            break;
        case CMD_SetField3C:
            field_0x3C = it->GetFloat();
            break;
        case CMD_SetField40:
            field_0x40 = it->GetFloat();
            break;
        case CMD_SetField44:
            field_0x44 = it->GetU32();
            break;
        case CMD_SetField48:
            field_0x48 = it->GetFloat();
            break;
        case CMD_SetField4C:
            field_0x4C = it->GetFloat();
            break;
        case CMD_SetField50:
            field_0x50 = it->GetFloat();
            break;
        case CMD_SetField54:
            field_0x54 = it->GetFloat();
            break;
        case CMD_SetField58:
            field_0x58 = it->GetU32();
            break;
        case CMD_SetColor: {
            s32 color = it->GetU32();
            field_0x5C = (float)( color & 0xFF ) / 255.0f;
            field_0x60 = (float)( ( color >> 8 ) & 0xFF ) / 255.0f;
            field_0x64 = (float)( ( color >> 16 ) & 0xFF ) / 255.0f;
            break;
        }
        case CMD_SetField68:
            field_0x68 = it->GetFloat();
            break;
        case CMD_SetField6C:
            field_0x6C = it->GetU32();
            break;
        case CMD_SetField70:
            field_0x70 = it->GetFloat();
            field_0x74 = field_0x70 * field_0x70;
            break;
        case CMD_SetField78:
            field_0x78 = it->GetFloat();
            break;
        case CMD_SetField7C:
            field_0x7C = it->GetU32();
            break;
        case CMD_SetField80:
            field_0x80 = it->GetFloat();
            break;
        case CMD_SetState:
            if ( it->GetU32() != 0 ) {
                field_0x9C = 2;
            } else {
                field_0x9C = 0;
            }
            break;
        case CMD_SetFieldMsg84:
            fn_801AA218( field_0x84, it->GetString(), NULL );
            break;
        case CMD_SetFieldMsg8C:
            fn_801AA218( field_0x8C, it->GetString(), NULL );
            break;
        case CMD_SetField94:
            field_0x94 = it->GetU32();
            break;
        case CMD_SetField98:
            field_0x98 = it->GetFloat();
            break;
        }
        ++it;
    }
    if ( field_0x70 < field_0x78 ) {
        field_0x70 = field_0x78 + 0.01f;
    }
}

/*
====================
unk_800865DC::fn_80086B0C

A timed falloff. The state machine at field_0x9C advances the timer at field_0xA0 by dt times the
rate at field_0x98 and maps it through sin( pi / 2 * timer ); state 0 returns 0. The factor is
then scaled by the distance falloff between the radii (field_0x78 and field_0x70) when field_0x6C
is set, and by the target's facing (its at vector against the direction to the frame) raised to
field_0x80 when field_0x7C is set. Either falloff returns 0 outside its range.
Open (schedule only): retail loads each offset component's pair in turn; here the loads of the next
component are hoisted, so 9 rows differ. The body is complete.
====================
*/
// 0x80086B0C
float unk_800865DC::fn_80086B0C( float dt, const unk_RwMatrix* target ) {
    float factor = 1.0f;

    switch ( field_0x9C ) {
    case 0:
        return 0.0f;
    case 1:
        field_0xA0 += dt * field_0x98;
        if ( field_0xA0 >= 1.0f || field_0x98 == 0.0f ) {
            field_0xA0 = 1.0f;
            field_0x9C = 2;
        }
        factor = sin( 0.5f * ( 3.14159f * field_0xA0 ) );
        break;
    case 2:
        if ( field_0x94 != 0 ) {
            field_0xA0 = 1.0f;
            field_0x9C = 3;
        }
        break;
    case 3:
        field_0xA0 -= dt * field_0x98;
        if ( field_0xA0 <= 0.0f || field_0x98 == 0.0f ) {
            field_0xA0 = 0.0f;
            field_0x9C = 0;
        }
        factor = sin( 0.5f * ( 3.14159f * field_0xA0 ) );
        break;
    }

    unk_RwV3d offset;
    if ( field_0x6C != 0 || field_0x7C != 0 ) {
        const unk_RwV3d& framePos = RwFrameGetMatrix( field_0x30 )->pos;
        offset.x = framePos.x - target->pos.x;
        offset.y = framePos.y - target->pos.y;
        offset.z = framePos.z - target->pos.z;
    }

    if ( field_0x6C != 0 ) {
        float distanceSq = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
        if ( distanceSq < field_0x74 ) {
            float distance = (float)sqrt( distanceSq );
            float t = ( distance - field_0x78 ) / ( field_0x70 - field_0x78 );
            factor *= 1.0f - t;
        } else {
            return 0.0f;
        }
    }

    if ( field_0x7C != 0 ) {
        RwV3dNormalize( &offset, &offset );
        float facing = offset.x * target->at.x + offset.y * target->at.y + offset.z * target->at.z;
        if ( facing < 0.0f ) {
            return 0.0f;
        }
        factor *= (float)pow( facing, field_0x80 );
    }

    return factor;
}

/*
====================
fn_80086D7C

Keeps the instance list sorted by field_0x34 (ascending) when an attribute command has set
lbl_80527250, then moves the colour and intensity lerps of each instance by its falloff at target
(an instance whose falloff is not positive does not move them).
====================
*/
// 0x80086D7C
void fn_80086D7C( float dt, const unk_RwMatrix* target ) {
    if ( lbl_80527250 ) {
        unk_800865DC* unsorted = lbl_80529670;
        lbl_80529670 = NULL;
        while ( unsorted != NULL ) {
            unk_800865DC* node = unsorted;
            unsorted = node->next;
            if ( lbl_80529670 == NULL || lbl_80529670->field_0x34 > node->field_0x34 ) {
                node->next = lbl_80529670;
                lbl_80529670 = node;
            } else {
                unk_800865DC* prev = lbl_80529670;
                while ( prev->next != NULL && prev->next->field_0x34 < node->field_0x34 ) {
                    prev = prev->next;
                }
                node->next = prev->next;
                prev->next = node;
            }
        }
        lbl_80527250 = false;
    }

    for ( unk_800865DC* instance = lbl_80529670; instance != NULL; instance = instance->next ) {
        float falloff = instance->fn_80086B0C( dt, target );
        if ( falloff > 0.0f ) {
            if ( instance->field_0x38 != 0 ) {
                lbl_80528818 = lbl_80528818 + falloff * ( instance->field_0x3C - lbl_80528818 );
                lbl_8052881C = lbl_8052881C + falloff * ( instance->field_0x40 - lbl_8052881C );
            }
            if ( instance->field_0x58 != 0 ) {
                lbl_80528820 = lbl_80528820 + falloff * ( instance->field_0x5C - lbl_80528820 );
                lbl_80528824 = lbl_80528824 + falloff * ( instance->field_0x60 - lbl_80528824 );
                lbl_80528828 = lbl_80528828 + falloff * ( instance->field_0x64 - lbl_80528828 );
                lbl_8052882C = lbl_8052882C + falloff * ( instance->field_0x68 - lbl_8052882C );
            }
            if ( instance->field_0x44 != 0 ) {
                lbl_80528830 = lbl_80528830 + falloff * ( instance->field_0x48 - lbl_80528830 );
                lbl_80528834 = lbl_80528834 + falloff * ( instance->field_0x4C - lbl_80528834 );
                lbl_80528838 = lbl_80528838 + falloff * ( instance->field_0x50 - lbl_80528838 );
                lbl_8052A680 = lbl_8052A680 + falloff * ( instance->field_0x54 - lbl_8052A680 );
            }
        }
    }
}

/*
====================
unk_80086F9C::unk_80086F9C

Links the instance into its list with a 2.5 timer and state 2.
====================
*/
// 0x80086F9C
unk_80086F9C::unk_80086F9C( const unk_AttributePacket& packet ) {
    field_0x34 = 1.0f;
    field_0x38.fn_801A92FC( NULL );
    field_0x40.fn_801A92FC( NULL );
    field_0x48 = 0;
    field_0x4C = 0.0f;
    field_0x50 = 2.5f;
    field_0x54 = 2;
    next = lbl_8052967C;
    lbl_8052967C = this;
}

/*
====================
unk_80086F9C::~unk_80086F9C

Unlinks the instance from its list, unregisters both messages, then the base destructor.
====================
*/
// 0x80087044
unk_80086F9C::~unk_80086F9C() {
    CEventHandler::fn_801A9E00( field_0x38 );
    fn_801A9940( field_0x38 );
    CEventHandler::fn_801A9E00( field_0x40 );
    fn_801A9940( field_0x40 );

    unk_80086F9C* next = lbl_8052967C;
    lbl_8052967C = NULL;
    while ( next != NULL ) {
        unk_80086F9C* node = next;
        next = node->next;
        if ( node != this ) {
            node->next = lbl_8052967C;
            lbl_8052967C = node;
        }
    }
}

/*
====================
unk_80086F9C::fn_8009CB1C

Runs the base handler, then advances the state when the first or second message arrives.
====================
*/
// 0x8008712C
void unk_80086F9C::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == field_0x38.record && field_0x54 == 0 ) {
        field_0x58 = 0.0f;
        field_0x54 = 1;
    }
    if ( msg.record == field_0x40.record ) {
        if ( field_0x54 == 2 ) {
            field_0x58 = 1.0f;
            field_0x54 = 3;
        }
        if ( field_0x54 == 1 ) {
            field_0x54 = 3;
        }
    }
}

/*
====================
unk_800865DC::fn_80087564

Registry factory of the class: a new instance, returned as its attribute handler.
====================
*/
// 0x80087564
CAttributeHandler* unk_800865DC::fn_80087564( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_800865DC( packet );
}

/*
====================
unk_80086F9C::fn_801A73A8

Runs the base handler, then applies each attribute command addressed to the class.
====================
*/
// 0x800871D0
void unk_80086F9C::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_80529680 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetField34:
            field_0x34 = it->GetFloat();
            break;
        case CMD_SetState:
            if ( it->GetU32() != 0 ) {
                field_0x54 = 2;
            } else {
                field_0x54 = 0;
            }
            break;
        case CMD_SetFieldMsg38:
            fn_801AA218( field_0x38, it->GetString(), NULL );
            break;
        case CMD_SetFieldMsg40:
            fn_801AA218( field_0x40, it->GetString(), NULL );
            break;
        case CMD_SetField48:
            field_0x48 = it->GetU32();
            break;
        case CMD_SetField4C:
            field_0x4C = it->GetFloat();
            break;
        case CMD_SetField50:
            field_0x50 = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
unk_80086F9C::fn_80087314

Advances the instance's envelope by dt. State 1 rises, 2 holds, 3 falls, 0 is idle; the
envelope is a half sine of the timer, scaled by field_0x34.
====================
*/
// 0x80087314
float unk_80086F9C::fn_80087314( float dt ) {
    float envelope = 1.0f;
    switch ( field_0x54 ) {
    case 0:
        return 0.0f;
    case 1:
        field_0x58 += dt * field_0x4C;
        if ( field_0x58 >= 1.0f || field_0x4C == 0.0f ) {
            field_0x58 = 1.0f;
            field_0x54 = 2;
        }
        envelope = ( float )sin( 0.5f * ( 3.14159f * field_0x58 ) );
        break;
    case 2:
        if ( field_0x48 != 0 ) {
            field_0x58 = 1.0f;
            field_0x54 = 3;
        }
        break;
    case 3:
        field_0x58 -= dt * field_0x50;
        if ( field_0x58 <= 0.0f || field_0x50 == 0.0f ) {
            field_0x58 = 0.0f;
            field_0x54 = 0;
        }
        envelope = ( float )sin( 0.5f * ( 3.14159f * field_0x58 ) );
        break;
    }
    return envelope * field_0x34;
}

/*
====================
fn_80087448

Sums the envelope factors of every instance in the list, each advanced by dt.
====================
*/
// 0x80087448
float fn_80087448( float dt ) {
    float total = 0.0f;
    for ( unk_80086F9C* node = lbl_8052967C; node != NULL; node = node->next ) {
        total += node->fn_80087314( dt );
    }
    return total;
}

/*
====================
unk_80086F9C::fn_800875AC

Registry factory of the class: a new instance, returned as its attribute handler.
====================
*/
// 0x800875AC
CAttributeHandler* unk_80086F9C::fn_800875AC( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_80086F9C( packet );
}

// Registration of the two classes, in the retail order of the static initialiser. The names are
// the class-name string literals of the DOL (the class registry); each occurs twice, as in retail.
u32 lbl_80529674 = fn_801A7A84( "CBloomCfg" );
unk_801A8F6C lbl_80529678( "CBloomCfg", unk_800865DC::fn_80087564, sizeof( unk_800865DC ) );
u32 lbl_80529680 = fn_801A7A84( "CMotionBlurCfg" );
unk_801A8F6C lbl_80529684( "CMotionBlurCfg", unk_80086F9C::fn_800875AC, sizeof( unk_80086F9C ) );
