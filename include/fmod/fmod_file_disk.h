/*
 * FMOD::DiskFile: see fmod_file_disk.cpp.
 */
#ifndef _FMOD_FILE_DISK_H
#define _FMOD_FILE_DISK_H

#include <fmod/fmod.h>
#include <fmod/fmod_file.h>

// Layout: Gormiti DWARF (sibling.exe types DiskFile)
// Virtuals: retail vtable __vt__Q24FMOD8DiskFile + Gormiti link map
namespace FMOD {

class DiskFile : public File {
public:
    void *mHandle;                                  // 0x194

    virtual FMOD_RESULT reallyOpen( const char *name_or_data, unsigned int *filesize ); // slot 0x10, override
    virtual FMOD_RESULT reallyClose();              // slot 0x14, override
    virtual FMOD_RESULT reallyRead( void *buffer, unsigned int size, unsigned int *rd ); // slot 0x18, override
    virtual FMOD_RESULT reallySeek( unsigned int pos ); // slot 0x1C, override
    virtual FMOD_RESULT reallyCancel();             // slot 0x20, override
}; // size 0x198 (Gormiti DWARF); retail ~0x198 (estimate from accesses)

} // namespace FMOD

#endif
