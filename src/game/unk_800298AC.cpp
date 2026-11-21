// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800298AC.h>

#include <string.h>
#include <rwsdk/plcore/babinary.h>

// Defined in other units, not yet reconstructed (C linkage).
extern "C" void* fn_8012DED4( void* parent );
extern "C" void fn_801304A0( void* handle, unk_RwMatrix* matrix );
extern "C" void fn_801308C8( void* handle, u16 value );

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E16D0 size 0xC lbl_803E16D0
//   .sdata2 0x8052B8A8 size 0x4 lbl_8052B8A8
//   .sdata2 0x8052B8AC size 0x4 lbl_8052B8AC

/*
====================
fn_800298AC

Loads the spawn point data ("spawn_point" is its string). The first argument is the loaded file: its
header is at +0x4 (group count at +0x2), the group pointer array at +0x8 and the base that the string
offsets are added to at +0xC. The second argument receives the result. Open: the record layouts and
the helpers it calls (fn_8012DC4C, fn_8012E410, fn_8012E530, fn_80130414 and others, in other units)
are not recovered, so no match is attempted and the body is still a stub.
====================
*/
// 0x800298AC
extern "C" void fn_800298AC() {
    // TODO
}

/*
====================
fn_8002A584

Open: a reserve on a pointer-sized element vector (begin, end and capacity end at +0, +4, +8;
operator new[] with flags 0x40000). The container's identity is not recovered. The destroy loop over the
old elements means the element type has a non-trivial destructor, which no evidence names, so the
element type cannot be written.
====================
*/
// 0x8002A584
extern "C" void fn_8002A584() {
    // TODO
}

/*
====================
fn_8002A684

While the handle is still NULL, copies the source's four vectors into a local RwMatrix, updates it,
creates the handle from the parent and hands the matrix and the source's value to it. Returns the
handle. The class and the parent's type are not recovered (names invented).
====================
*/
// 0x8002A684
extern "C" void* fn_8002A684( unk_Placement* self, void* parent ) {
    if ( self->handle == NULL ) {
        unk_RwMatrix matrix;
        matrix.right = self->source->right;
        matrix.up = self->source->up;
        matrix.at = self->source->at;
        matrix.pos = self->source->pos;
        RwMatrixUpdate( &matrix );
        self->handle = fn_8012DED4( parent );
        fn_801304A0( self->handle, &matrix );
        fn_801308C8( self->handle, self->source->field_0xA );
    }
    return self->handle;
}

/*
====================
fn_8002A758

Finds the entry of a group whose name equals the name at nameIndex in the file. Returns the
entry, or NULL when no entry of the group has that name.
Open (96.7%, register/schedule wall): retail reloads file->nameOffsets inside the loop, this build
keeps the first load in a register across the loop.
====================
*/
// 0x8002A758
extern "C" unk_SpawnPointEntry* fn_8002A758( unk_SpawnPointGroup* group, unk_SpawnPointFile* file, int nameIndex ) {
    u32 offset = file->nameOffsets[ nameIndex ];
    RwMemNative32( &offset, sizeof( offset ) );
    const char* name = (const char*)file->nameOffsets + offset;

    for ( int i = 0; i < group->header->groupCount; i++ ) {
        u32 entryOffset = file->nameOffsets[ ( group->entries + i )->nameIndex ];
        RwMemNative32( &entryOffset, sizeof( entryOffset ) );
        if ( strcmp( (const char*)file->nameOffsets + entryOffset, name ) == 0 ) {
            return group->entries + i;
        }
    }
    return NULL;
}

/*
====================
fn_8002A828

Open: push_back on the same pointer-sized element vector as fn_8002A584 (growth, copy of the old
elements, store of the new one). Not matched: the container and its element type (the same non-trivial
element destructor as fn_8002A584) are not recovered.
====================
*/
// 0x8002A828
extern "C" void fn_8002A828() {
    // TODO
}

/*
====================
fn_8002A964

Deleting-destructor shape: deletes the object when the pointer is not NULL and the flag is
positive, and returns the pointer. The class is not placed (no constructor stores a vtable
here, and lbl_803E6194 slot 12 is a plain table entry), so this is a free function.
====================
*/
// 0x8002A964
extern "C" void* fn_8002A964( void* self, int flags ) {
    if ( self != NULL && flags > 0 ) {
        operator delete( self );
    }
    return self;
}
