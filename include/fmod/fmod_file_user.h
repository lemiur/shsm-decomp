/*
 * FMOD::UserFile: see fmod_file_user.cpp.
 */
#ifndef _FMOD_FILE_USER_H
#define _FMOD_FILE_USER_H

#include <fmod/fmod.h>
#include <fmod/fmod_file.h>

// Layout: Gormiti DWARF (sibling.exe types UserFile)
// Virtuals: retail vtable __vt__Q24FMOD8UserFile + Gormiti link map
namespace FMOD {

class UserFile : public File {
public:
    FMOD_FILE_OPENCALLBACK  mOpenCallback;          // 0x194
    FMOD_FILE_CLOSECALLBACK mCloseCallback;         // 0x198
    FMOD_FILE_READCALLBACK  mReadCallback;          // 0x19C
    FMOD_FILE_SEEKCALLBACK  mSeekCallback;          // 0x1A0
    void *mHandle;                                  // 0x1A4
    void *mUserData;                                // 0x1A8

    // The callbacks of FMOD_System_SetFileSystem, used by files that have none of their own.
    static FMOD_FILE_OPENCALLBACK  gOpenCallback;
    static FMOD_FILE_CLOSECALLBACK gCloseCallback;
    static FMOD_FILE_READCALLBACK  gReadCallback;
    static FMOD_FILE_SEEKCALLBACK  gSeekCallback;

    virtual FMOD_RESULT reallyOpen( const char *name_or_data, unsigned int *filesize ); // slot 0x10, override
    virtual FMOD_RESULT reallyClose();              // slot 0x14, override
    virtual FMOD_RESULT reallyRead( void *buffer, unsigned int size, unsigned int *rd ); // slot 0x18, override
    virtual FMOD_RESULT reallySeek( unsigned int pos ); // slot 0x1C, override
}; // size 0x1AC (Gormiti DWARF); retail ~0x1AC (estimate from accesses)

} // namespace FMOD

#endif
