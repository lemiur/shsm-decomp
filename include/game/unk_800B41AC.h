#ifndef GAME_UNK_800B41AC_H
#define GAME_UNK_800B41AC_H

#include <revolution/types.h>
#include <climax/Frame.h>

/*
 * The object these functions work on. Its class name and constructor are not recovered; the
 * placeholder takes the address of the unit's first function (name invented). Only the fields the
 * functions in this unit touch are declared. The rest of the object is open.
 */

/* Byte buffer at +0x04: a data pointer and its count at +0x08. Name invented. */
struct unk_800B41AC_Buffer {
    u8*  data;          /* +0x00 (+0x04 in the object) */
    u16  count;         /* +0x04 (+0x08 in the object) */
    u16  field_0x06;    /* not touched by this unit */
};

/* The object at +0xEC of unk_800B41AC; its target at +0x08. Name invented. */
struct unk_800B41AC_Target {
    u8   field_0x00[0x80];
    u32  id;            /* +0x80 */
};

struct unk_800B41AC_Owner {
    u8                          field_0x00[0x08];
    unk_800B41AC_Target*        target;     /* +0x08 */
};

/* The source of the copy in fn_800B43B4: an id and a pointer to a matrix. Name invented. */
struct unk_800B41AC_Source {
    u32                 id;         /* +0x00 */
    unk_RwMatrix*       matrix;     /* +0x04 */
};

struct unk_800B41AC {
    u8                   field_0x00[0x04];
    unk_800B41AC_Buffer  buffer;        /* +0x04 */
    u16                  field_0x0C;    /* +0x0C: read first in the stream order 0x0C, 0x12, 0x0E, 0x10 */
    u16                  field_0x0E;    /* +0x0E */
    u16                  field_0x10;    /* +0x10 */
    u16                  field_0x12;    /* +0x12 */
    u8                   field_0x14[0x18];
    u8                   field_0x2C[0xC0];  /* +0x2C: the vptr of a vtable slot call in fn_800B4490; open */
    unk_800B41AC_Owner*  owner;         /* +0xEC */
    void*                field_0xF0;    /* +0xF0 */
    unk_RwMatrix         matrix;        /* +0xF4 */
};

extern "C" {
int  fn_800B41AC( unk_800B41AC* self, void* stream );
int  fn_800B422C( unk_800B41AC* self, void* stream );
int  fn_800B42AC( unk_800B41AC* self, const unk_800B41AC* source );
unk_800B41AC* fn_800B435C( unk_800B41AC* self, int flags );
void fn_800B43B4( unk_800B41AC* self, const unk_800B41AC_Source* source );
void fn_800B4490();
void fn_800B4578();
}

#endif
