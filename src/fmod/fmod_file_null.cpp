/*
 * fmod_file_null.cpp: FMOD::NullFile, a file of a given length with no data behind it.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_file_null.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_file_null.h>

namespace FMOD {

/*
====================
NullFile::reallyOpen

Starts at position 0 with a size of 0.
====================
*/
FMOD_RESULT NullFile::reallyOpen( const char *name_or_data, unsigned int *filesize ) {
    mPosition = 0;
    *filesize = 0;

    return FMOD_OK;
} // 0x803756BC

/*
====================
NullFile::reallyClose

Nothing to release.
====================
*/
FMOD_RESULT NullFile::reallyClose() {
    return FMOD_OK;
} // 0x803756D0

/*
====================
NullFile::reallyRead

Reads nothing: it only advances the position, cut short at the end of the file, and reports the cut as
an invalid parameter.
====================
*/
FMOD_RESULT NullFile::reallyRead( void *buffer, unsigned int size, unsigned int *rd ) {
    FMOD_RESULT result = FMOD_OK;

    if ( mPosition + size > mLength ) {
        size   = mLength - mPosition;
        result = FMOD_ERR_INVALID_PARAM;
    }

    *rd = size;
    mPosition += size;

    return result;
} // 0x803756D8

/*
====================
NullFile::reallySeek

Moves the position to pos, clamped to the end of the file.
====================
*/
FMOD_RESULT NullFile::reallySeek( unsigned int pos ) {
    mPosition = pos;
    if ( mPosition > mLength ) {
        mPosition = mLength;
    }

    return FMOD_OK;
} // 0x80375710

} // namespace FMOD
