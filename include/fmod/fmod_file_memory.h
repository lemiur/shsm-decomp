/*
 * FMOD::MemoryFile: see fmod_file_memory.cpp.
 */
#ifndef _FMOD_FILE_MEMORY_H
#define _FMOD_FILE_MEMORY_H

#include <fmod/fmod.h>
#include <fmod/fmod_file.h>

// Layout: Gormiti DWARF (sibling.exe types MemoryFile)
// Virtuals: retail vtable __vt__Q24FMOD10MemoryFile + Gormiti link map
namespace FMOD {

class MemoryFile : public File {
public:
    unsigned int mPosition;                         // 0x194
    void *mMem;                                     // 0x198

    virtual FMOD_RESULT reallyOpen( const char *name_or_data, unsigned int *filesize ); // slot 0x10, override
    virtual FMOD_RESULT reallyClose();              // slot 0x14, override
    virtual FMOD_RESULT reallyRead( void *buffer, unsigned int size, unsigned int *rd ); // slot 0x18, override
    virtual FMOD_RESULT reallySeek( unsigned int pos ); // slot 0x1C, override
}; // size 0x19C (Gormiti DWARF); retail ~0x19C (estimate from accesses)

} // namespace FMOD

#endif
