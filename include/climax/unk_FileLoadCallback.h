#ifndef CLIMAX_UNK_FILELOADCALLBACK_H
#define CLIMAX_UNK_FILELOADCALLBACK_H

// unit name unconfirmed: header and type names invented.

#include <revolution/types.h>

/*
 * The interface of an object told when a file it asked for has been read (0x801B250C-0x801B2628,
 * unit not yet reconstructed). fn_801B250C keeps the caller's pointer in field_0x4 and, when the
 * file is read at once, calls fn_800CC364 with its data, name and size. It has no virtual
 * destructor: its one slot is fn_800CC364. No vtable of its own survives in the binary; the slot
 * is declared, not pure, because MWCC emits a weak copy of the vtable of a class whose virtuals
 * are all pure in every unit that constructs a derived class (here before the unit's strings),
 * which the retail .data does not have. Parameter names invented.
 */
class unk_FileLoadCallback {
public:
    /* Called with the file's data once it has been read. */
    virtual void    fn_800CC364( void* data, int a, const char* name, int b, u32 size );

    /* Asks for the file name; the request, or NULL when it could not be made. */
    void*           fn_801B250C( const char* name, void* user, float priority );

    void*           field_0x4;      // the user pointer passed to fn_801B250C
};

#endif
