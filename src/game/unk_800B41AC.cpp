// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800B41AC.h>

#include <string.h>
#include <climax/Heap.h>

// Defined in other units, not yet reconstructed (C linkage). The buffer helpers take the buffer
// (or a field) and the stream; fn_800AF088 reads n bytes from the stream, fn_800AF188 writes them.
extern "C" void fn_800AC078( void* buffer, void* stream );
extern "C" void fn_800ABFE4( void* buffer, void* stream );
extern "C" void fn_800AF088( void* stream, void* bytes, int size );
extern "C" void fn_800AF188( void* stream, void* bytes, int size );
extern "C" void fn_80112D94( void* self, int flags );
extern "C" void fn_801147AC( void* self );

/*
====================
fn_800B41AC

Reads the object from the stream: the buffer through fn_800AC078, then the four 16-bit fields in
the order 0x0C, 0x12, 0x0E, 0x10, each two bytes. Returns 0.
====================
*/
// 0x800B41AC
extern "C" int fn_800B41AC( unk_800B41AC* self, void* stream ) {
    fn_800AC078( &self->buffer, stream );
    fn_800AF088( stream, &self->field_0x0C, 2 );
    fn_800AF088( stream, &self->field_0x12, 2 );
    fn_800AF088( stream, &self->field_0x0E, 2 );
    fn_800AF088( stream, &self->field_0x10, 2 );
    return 0;
}

/*
====================
fn_800B422C

Writes the object to the stream: the mirror of fn_800B41AC, with fn_800ABFE4 for the buffer and
fn_800AF188 for each field in the same order. Returns 0.
====================
*/
// 0x800B422C
extern "C" int fn_800B422C( unk_800B41AC* self, void* stream ) {
    fn_800ABFE4( &self->buffer, stream );
    fn_800AF188( stream, &self->field_0x0C, 2 );
    fn_800AF188( stream, &self->field_0x12, 2 );
    fn_800AF188( stream, &self->field_0x0E, 2 );
    fn_800AF188( stream, &self->field_0x10, 2 );
    return 0;
}

/*
====================
fn_800B42AC

Copy assignment. Frees the destination's buffer if it has one, takes the source's count, and when
the source has data and a count, allocates a new buffer of that count and copies the bytes. Then
copies the four 16-bit fields.
====================
*/
// 0x800B42AC
extern "C" int fn_800B42AC( unk_800B41AC* self, const unk_800B41AC* source ) {
    if ( self->buffer.data != NULL ) {
        delete[] self->buffer.data;
        self->buffer.data = NULL;
        self->buffer.count = 0;
    }
    self->buffer.count = source->buffer.count;
    if ( source->buffer.data != NULL && source->buffer.count != 0 ) {
        self->buffer.data = (u8*)operator new[]( self->buffer.count, GAME_HEAP, GAME_HEAP_FLAGS );
        memcpy( self->buffer.data, source->buffer.data, self->buffer.count );
    }
    self->field_0x0C = source->field_0x0C;
    self->field_0x12 = source->field_0x12;
    self->field_0x0E = source->field_0x0E;
    self->field_0x10 = source->field_0x10;
    return 0;
}

/*
====================
fn_800B435C

Deleting destructor. Runs the base destructor, fn_80112D94 with flag 0, and frees the object when
the flag is positive. Returns the object.
====================
*/
// 0x800B435C
extern "C" unk_800B41AC* fn_800B435C( unk_800B41AC* self, int flags ) {
    if ( self != NULL ) {
        fn_80112D94( self, 0 );
        if ( flags > 0 ) {
            operator delete( self );
        }
    }
    return self;
}

/*
====================
fn_800B43B4

Calls fn_801147AC on the object first. Then, when the object has an owner whose target's id equals
the source's id, copies the source's matrix into the object's own matrix at +0xF4.
Open: 99.64%. The compare's registers differ (retail holds the target in r4 and the id in r3; the
compile keeps the target in r3), a register residue not yet closed; the body is the source's shape.
====================
*/
// 0x800B43B4
extern "C" void fn_800B43B4( unk_800B41AC* self, const unk_800B41AC_Source* source ) {
    fn_801147AC( self );
    if ( self->owner != NULL ) {
        unk_800B41AC_Target* target = self->owner->target;
        if ( source->id == target->id ) {
            self->matrix = *source->matrix;
        }
    }
}

/*
====================
fn_800B4490

Open: transforms a matrix from the frame at +0x30 and calls a virtual through the vptr at +0x2C.
The vptr-at-+0x2C layout is not recovered, so no match is attempted and the body is still a stub.
====================
*/
// 0x800B4490
extern "C" void fn_800B4490() {
    // TODO
}

/*
====================
fn_800B4578

Open: releases the handler at +0xF0 and clears the pointer at +0xEC. The handler's type and the
object's fields past +0x14 are not recovered, so no match is attempted and the body is still a stub.
====================
*/
// 0x800B4578
extern "C" void fn_800B4578() {
    // TODO
}
