// Unit name and boundary: class-name string literal "cWind" in the registration (sinit,
// lbl_805272A0, interned into lbl_80529728). The factory and the class code share the unit.

#include <game/cWind.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>
#include <math.h>

u32 lbl_80529728 = fn_801A7A84( "cWind" );
unk_801A8F6C lbl_8052972C( "cWind", cWind::fn_80091BA4, sizeof( cWind ) );
cWind* lbl_80529730;

/*
====================
cWind::cWind

The packet is not read here: fn_801A73A8 applies it once the object is linked.
====================
*/
// 0x800914A8
cWind::cWind( const unk_AttributePacket& packet ) : CBaseBehaviour() {
    fn_801A9DE8( lbl_8052A788 );
    active = true;
    rampingUp = false;
    rampingDown = false;
    maxStrength = 1.0f;
    strength = 1.0f;
    mode = MODE_Directional;
    level = 1.0f;
    radius = 5.0f;
    duration = 3.0f;
    next = NULL;
    if ( lbl_80529730 == NULL ) {
        lbl_80529730 = this;
    } else {
        cWind* last = lbl_80529730;
        while ( last->next != NULL ) {
            last = last->next;
        }
        last->next = this;
    }
}

/*
====================
cWind::~cWind

Unregisters the messages and unlinks the wind from the list.
====================
*/
// 0x80091588
cWind::~cWind() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( onMessage );
    fn_801A9E00( offMessage );
    fn_801A9940( onMessage );
    fn_801A9940( offMessage );
    cWind* head = lbl_80529730;
    if ( head == this ) {
        lbl_80529730 = head->next;
    } else {
        cWind* prev = head;
        while ( prev->next != this ) {
            prev = prev->next;
        }
        prev->next = next;
    }
}

/*
====================
cWind::fn_8009CB1C

The frame message ramps the level toward 1 or 0 by the frame time over duration. The on and off
messages start a ramp in either direction.
====================
*/
// 0x8009167C
void cWind::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788 ) {
        const unk_80491B38* timing = &lbl_80491B38;
        if ( active ) {
            if ( rampingUp ) {
                level += timing->field_0x8 / duration;
                if ( level >= 1.0f ) {
                    level = 1.0f;
                    rampingUp = false;
                }
                strength = maxStrength * level;
            } else if ( rampingDown ) {
                level -= timing->field_0x8 / duration;
                if ( level <= 0.0f ) {
                    level = 0.0f;
                    rampingDown = false;
                    active = false;
                }
                strength = maxStrength * level;
            }
        }
    } else if ( msg.record == onMessage ) {
        active = true;
        rampingUp = true;
        rampingDown = false;
    } else if ( msg.record == offMessage ) {
        active = true;
        rampingUp = false;
        rampingDown = true;
    }
}

/*
====================
fn_800917C8

Sums the force of the active winds of zone at pos into out.
Wall (register/schedule): retail keeps four callee-saved registers here and zeroes out in
a different order; this compiles with five.
====================
*/
// 0x800917C8
void fn_800917C8( CZone* zone, const unk_RwV3d* pos, unk_RwV3d* out ) {
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    for ( cWind* wind = lbl_80529730; wind != NULL; wind = wind->next ) {
        if ( !wind->active || wind->mZone != zone ) {
            continue;
        }
        switch ( wind->mode ) {
        case cWind::MODE_Point: {
            unk_RwMatrix* matrix = RwFrameGetMatrix( wind->field_0x30 );
            unk_RwV3d diff;
            diff.x = matrix->pos.x - pos->x;
            diff.y = matrix->pos.y - pos->y;
            diff.z = matrix->pos.z - pos->z;
            float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if ( distSq < wind->radiusSq ) {
                float t = sqrt( distSq ) / wind->radius;
                out->x = out->x * t + wind->strength * ( 1.0f - t ) * RwFrameGetMatrix( wind->field_0x30 )->at.x;
                out->y = out->y * t + wind->strength * ( 1.0f - t ) * RwFrameGetMatrix( wind->field_0x30 )->at.y;
                out->z = out->z * t + wind->strength * ( 1.0f - t ) * RwFrameGetMatrix( wind->field_0x30 )->at.z;
            }
            break;
        }
        case cWind::MODE_Directional: {
            out->x += wind->strength * RwFrameGetMatrix( wind->field_0x30 )->at.x;
            out->y += wind->strength * RwFrameGetMatrix( wind->field_0x30 )->at.y;
            out->z += wind->strength * RwFrameGetMatrix( wind->field_0x30 )->at.z;
            break;
        }
        }
    }
}

/*
====================
cWind::fn_801A73A8

Applies the attribute commands of the packet.
Wall (register/schedule, 98.9%): retail reloads the iterator current at the body entry; the
compiled body reuses the load from the loop condition.
====================
*/
// 0x800919B4
void cWind::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529728 );
    while ( it->next != 0 ) {
        switch ( it->id ) {
        case CMD_SetMode:
            mode = it->GetU32();
            break;
        case CMD_SetRadius:
            radius = it->GetFloat();
            radiusSq = radius * radius;
            break;
        case CMD_SetStrength:
            maxStrength = it->GetFloat();
            strength = maxStrength;
            break;
        case CMD_SetDuration:
            duration = it->GetFloat();
            break;
        case CMD_SetActive:
            if ( it->GetU32() != 0 ) {
                active = true;
                level = 1.0f;
            } else {
                active = false;
                level = 0.0f;
            }
            rampingUp = false;
            rampingDown = false;
            strength = maxStrength * level;
            break;
        case CMD_SetOnMessage:
            fn_801AA218( onMessage, it->GetString(), NULL );
            break;
        case CMD_SetOffMessage:
            fn_801AA218( offMessage, it->GetString(), NULL );
            break;
        }
        ++it;
    }
}
