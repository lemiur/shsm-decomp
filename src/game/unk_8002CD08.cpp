// Unit name unconfirmed: no string or class-name evidence names this file, apart from the
// "ZoneLink" debug tag of fn_8002CD08 (the class is ZoneLink, game/ZoneLink.h). Rename this file when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/EventHandler.h>
#include <game/Log.h>
#include <game/unk_8002CD08.h>

/*
====================
CPointOfInterest::fn_8002CD9C
====================
*/
// 0x8002CD9C
char* CPointOfInterest::fn_8002CD9C() {
    return field_0x40;
}

/*
====================
CPointOfInterest::fn_8002CDA4
====================
*/
// 0x8002CDA4
bool CPointOfInterest::fn_8002CDA4() {
    return name != NULL && name[0] != '\0';
}

/*
====================
CPointOfInterest::fn_8002CDC8
====================
*/
// 0x8002CDC8
bool CPointOfInterest::fn_8002CDC8() {
    return field_0x1C;
}

/*
====================
CPointOfInterest::fn_8002CDD0
====================
*/
// 0x8002CDD0
// Stub: the call goes through slot 0x34 of other's vtable (lbl_803EBA7C, slot 13), which is bool
// fn_8002CDC8() in the base class, so `other` is not a CPointOfInterest or the slot is not this one.
void CPointOfInterest::fn_8002CDD0( CPointOfInterest* other, unk_Vec3* out ) {
}

/*
====================
CPointOfInterest::fn_8002CDF0
====================
*/
// 0x8002CDF0
bool CPointOfInterest::fn_8002CDF0() {
    return true;
}

/*
====================
fn_8002CD08
====================
*/
// 0x8002CD08
void fn_8002CD08( ZoneLink* self ) {
    if ( self->field_0x3A8.registrations != 0 ) {
        const char* linkName = self->field_0x3A8.record != NULL ? self->field_0x3A8.record->name : NULL;
        fn_80028D0C( 4, "ZoneLink[%s]: (Traversal End) Sends %s", NULL, linkName );
        PostMsg( self->field_0x3A8, NULL );
    }
}

/*
====================
fn_8002CDF8
====================
*/
// 0x8002CDF8
void* fn_8002CDF8( ZoneLink* self, int index, int flag ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( flag == 0 ) {
        return target->field_0xAC[index];
    }
    return target->field_0xCC[index];
}

/*
====================
fn_8002CE54
====================
*/
// 0x8002CE54
void* fn_8002CE54( ZoneLink* self, int index, int flag ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( flag == 0 ) {
        return target->field_0xA0[index];
    }
    return target->field_0xC0[index];
}

/*
====================
fn_8002CEB0
====================
*/
// 0x8002CEB0
void* fn_8002CEB0( ZoneLink* self, int flag ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( flag == 0 ) {
        return target->field_0x9C;
    }
    return target->field_0xBC;
}

/*
====================
fn_8002CEF0
====================
*/
// 0x8002CEF0
void* fn_8002CEF0( ZoneLink* self, int flag ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( flag == 0 ) {
        return target->field_0x98;
    }
    return target->field_0xB8;
}

/*
====================
fn_8002CF30
====================
*/
// 0x8002CF30
void* fn_8002CF30( ZoneLink* self, int index, int flag ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    return target->field_0x88[index][flag];
}

/*
====================
fn_8002CF7C
====================
*/
// 0x8002CF7C
void* fn_8002CF7C( ZoneLink* self, int index, int flag, int alt ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( alt != 0 ) {
        return target->field_0xE0;
    }
    return target->field_0x78[index][flag];
}

/*
====================
fn_8002CFE4
====================
*/
// 0x8002CFE4
void* fn_8002CFE4( ZoneLink* self, int index, int flag, int alt ) {
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( alt != 0 ) {
        return target->field_0xDC;
    }
    return target->field_0x68[index][flag];
}

/*
====================
fn_8002D04C
====================
*/
// 0x8002D04C
void* fn_8002D04C( ZoneLink* self, int index, int flag ) {
    u8 alt = self->field_0x334;
    unk_ZoneTarget* target = fn_8018309C( self );
    if ( alt != 0 ) {
        return target->field_0xD8;
    }
    return target->field_0x58[index][flag];
}

/*
====================
fn_8002D0B4
====================
*/
// 0x8002D0B4
bool fn_8002D0B4( ZoneLink* self ) {
    bool result = true;
    if ( self->field_0x2D8 != 1 ) {
        if ( fn_8018309C( self )->field_0x54 == 0 ) {
            result = false;
        }
    }
    return result;
}

/*
====================
fn_8002D100
====================
*/
// 0x8002D100
// Stub: the object that holds the embedded member at 0x44 is not recovered.
void fn_8002D100( unk_Vec3* out, void* owner ) {
}
