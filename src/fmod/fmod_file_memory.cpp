/*
 * fmod_file_memory.cpp: FMOD::MemoryFile, a file read out of a block of memory.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_file_memory.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_file_memory.h>
#include <string.h>

namespace FMOD {

/*
====================
MemoryFile::reallyOpen

Takes name_or_data as the block to read from. The block has no name and the position starts at 0.
====================
*/
FMOD_RESULT MemoryFile::reallyOpen( const char *name_or_data, unsigned int *filesize ) {
    mMem = (void *)name_or_data;
    mPosition = 0;
    mFlags &= ~FILE_FLAG_NAMED;

    return FMOD_OK;
} // 0x803755E8

/*
====================
MemoryFile::reallyClose

Nothing to release: the block belongs to the caller.
====================
*/
FMOD_RESULT MemoryFile::reallyClose() {
    return FMOD_OK;
} // 0x80375608

/*
====================
MemoryFile::reallyRead

Copies size bytes from the current position; a read past the end is cut short and reports an EOF.
====================
*/
FMOD_RESULT MemoryFile::reallyRead( void *buffer, unsigned int size, unsigned int *rd ) {
    FMOD_RESULT result = FMOD_OK;

    if ( mPosition + size > mFileSize ) {
        size   = mFileSize - mPosition;
        result = FMOD_ERR_FILE_EOF;
    }

    memcpy( buffer, (char *)mMem + mPosition, size );

    *rd = size;
    mPosition += size;

    return result;
} // 0x80375610

/*
====================
MemoryFile::reallySeek

Moves the position to pos, clamped to the end of the block.
====================
*/
FMOD_RESULT MemoryFile::reallySeek( unsigned int pos ) {
    mPosition = pos;
    if ( mPosition > mFileSize ) {
        mPosition = mFileSize;
    }

    return FMOD_OK;
} // 0x803756A0

} // namespace FMOD
