/*
 * FMOD::NullFile: see fmod_file_null.cpp.
 */
#ifndef _FMOD_FILE_NULL_H
#define _FMOD_FILE_NULL_H

#include <fmod/fmod.h>
#include <fmod/fmod_file.h>

// Layout: retail accesses (no NullFile type in Gormiti DWARF; mPosition as in MemoryFile)
// Virtuals: retail vtable __vt__Q24FMOD8NullFile + Gormiti link map
namespace FMOD {

class NullFile : public File {
public:
    unsigned int mPosition;                         // 0x194

    virtual FMOD_RESULT reallyOpen( const char *name_or_data, unsigned int *filesize ); // slot 0x10, override
    virtual FMOD_RESULT reallyClose();              // slot 0x14, override
    virtual FMOD_RESULT reallyRead( void *buffer, unsigned int size, unsigned int *rd ); // slot 0x18, override
    virtual FMOD_RESULT reallySeek( unsigned int pos ); // slot 0x1C, override
}; // size 0x198

} // namespace FMOD

#endif
