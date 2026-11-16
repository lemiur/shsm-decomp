/*
 * fmod_file_user.cpp: FMOD::UserFile, a file read through the application's file callbacks
 * (FMOD_System_SetFileSystem, or the callbacks of the file itself).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_file_user.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_file_user.h>

namespace FMOD {

FMOD_FILE_OPENCALLBACK  UserFile::gOpenCallback;
FMOD_FILE_CLOSECALLBACK UserFile::gCloseCallback;
FMOD_FILE_READCALLBACK  UserFile::gReadCallback;
FMOD_FILE_SEEKCALLBACK  UserFile::gSeekCallback;

/*
====================
UserFile::reallyOpen

Opens through the file's own open callback, or the global one. A file that cannot seek to its start
is kept, but no longer marked seekable.
====================
*/
FMOD_RESULT UserFile::reallyOpen( const char *name_or_data, unsigned int *filesize ) {
    FMOD_RESULT result = FMOD_OK;

    if ( mOpenCallback ) {
        result = mOpenCallback( name_or_data, ( mFlags & FILE_FLAG_UNICODE ) ? 1 : 0, filesize, &mHandle, &mUserData );
    } else if ( gOpenCallback ) {
        result = gOpenCallback( name_or_data, ( mFlags & FILE_FLAG_UNICODE ) ? 1 : 0, filesize, &mHandle, &mUserData );
    }

    if ( result == FMOD_OK ) {
        result = reallySeek( 0 );
        if ( result == FMOD_ERR_FILE_COULDNOTSEEK ) {
            mFlags &= ~FILE_FLAG_SEEKABLE;
            result = FMOD_OK;
        }
    }

    return result;
} // 0x8037572C

/*
====================
UserFile::reallyClose

Closes through the file's own close callback, or the global one.
====================
*/
FMOD_RESULT UserFile::reallyClose() {
    if ( mCloseCallback ) {
        mCloseCallback( mHandle, mUserData );
    } else if ( gCloseCallback ) {
        gCloseCallback( mHandle, mUserData );
    }

    return FMOD_OK;
} // 0x803757E4

/*
====================
UserFile::reallyRead

Reads through the file's own read callback, or the global one.
====================
*/
FMOD_RESULT UserFile::reallyRead( void *buffer, unsigned int size, unsigned int *rd ) {
    FMOD_RESULT result = FMOD_OK;

    if ( mReadCallback ) {
        result = mReadCallback( mHandle, buffer, size, rd, mUserData );
    } else if ( gReadCallback ) {
        result = gReadCallback( mHandle, buffer, size, rd, mUserData );
    }

    return result;
} // 0x80375844

/*
====================
UserFile::reallySeek

Seeks through the file's own seek callback, or the global one. With neither, the seek succeeds.
====================
*/
FMOD_RESULT UserFile::reallySeek( unsigned int pos ) {
    if ( mSeekCallback ) {
        FMOD_RESULT result = mSeekCallback( mHandle, pos, mUserData );
        if ( result != FMOD_OK ) {
            return result;
        }
    } else if ( gSeekCallback ) {
        FMOD_RESULT result = gSeekCallback( mHandle, pos, mUserData );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x803758B0

} // namespace FMOD
