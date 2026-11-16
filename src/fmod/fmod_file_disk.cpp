/*
 * fmod_file_disk.cpp: FMOD::DiskFile, a file read through the operating system's file layer.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_file_disk.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_file_disk.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_string.h>
#include <fmod/fmod_systemi.h>

namespace FMOD {

/*
====================
DiskFile::reallyOpen

Opens the file named by name_or_data for reading and returns its size.
====================
*/
FMOD_RESULT DiskFile::reallyOpen( const char *name_or_data, unsigned int *filesize ) {
    FMOD_RESULT result;
    char        path[2048];

    if ( mFlags & FILE_FLAG_UNICODE ) {
        if ( !FMOD_strlenW( (const short *)name_or_data ) ) {
            return FMOD_ERR_FILE_NOTFOUND;
        }
    } else {
        if ( !FMOD_strlen( name_or_data ) ) {
            return FMOD_ERR_FILE_NOTFOUND;
        }
    }

    FMOD_strcpy( path, name_or_data );

    result = setName( path );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = FMOD_OS_File_Open( name_or_data, "rb", ( mFlags & FILE_FLAG_UNICODE ) ? 1 : 0, filesize, &mHandle );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x803753D8

/*
====================
DiskFile::reallyClose

Closes the file.
====================
*/
FMOD_RESULT DiskFile::reallyClose() {
    FMOD_RESULT result = FMOD_OK;

    if ( mHandle ) {
        result = FMOD_OS_File_Close( mHandle );
        mHandle = 0;
    }

    return result;
} // 0x803754A4

/*
====================
DiskFile::reallyRead

Reads size bytes into buffer; rd receives the number read. A short read with no error is an EOF.
The disk is flagged busy while a thread other than the system's main thread reads.
====================
*/
FMOD_RESULT DiskFile::reallyRead( void *buffer, unsigned int size, unsigned int *rd ) {
    FMOD_RESULT  result;
    unsigned int read;
    unsigned int threadid = 0;

    FMOD_OS_Thread_GetCurrentID( &threadid );

    if ( mSystem && threadid != mSystem->mMainThreadID ) {
        FMOD_File_SetDiskBusy( 1 );
    }

    result = FMOD_OS_File_Read( mHandle, buffer, size, &read );

    if ( mSystem && threadid != mSystem->mMainThreadID ) {
        FMOD_File_SetDiskBusy( 0 );
    }

    if ( rd ) {
        *rd = read;
    }

    if ( result == FMOD_OK && size != read ) {
        return FMOD_ERR_FILE_EOF;
    }

    return result;
} // 0x803754EC

/*
====================
DiskFile::reallySeek

Moves the file position to pos.
====================
*/
FMOD_RESULT DiskFile::reallySeek( unsigned int pos ) {
    return FMOD_OS_File_Seek( mHandle, pos );
} // 0x803755D8

/*
====================
DiskFile::reallyCancel

Cancels a pending read.
====================
*/
FMOD_RESULT DiskFile::reallyCancel() {
    return FMOD_OS_File_Cancel( mHandle );
} // 0x803755E0

} // namespace FMOD
