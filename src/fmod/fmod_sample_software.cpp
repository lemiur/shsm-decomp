/*
 * fmod_sample_software.cpp: FMOD::SampleSoftware, software-mixed sample in main memory.
 * Class, member, function names, and line numbers come from Gormiti's DWARF
 * (sibling.exe cu fmod_sample_software, sibling.exe types SampleSoftware).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_sample_software.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_sample_software.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
SampleSoftware::SampleSoftware

Constructor: no sample buffer or saved loop-point offset yet.
====================
*/
// 0x80341018
SampleSoftware::SampleSoftware()
{
    mBuffer = NULL;
    mBufferMemory = NULL;
    mLoopPointDataEndOffset = 0;
}

/*
====================
SampleSoftware::release

Waits for any pending async open, stops the sound, frees the sample buffer (from secondary
RAM when configured) and any heap-allocated loop-point backup buffer, then releases Sample.
====================
*/
// 0x80341064
FMOD_RESULT SampleSoftware::release( bool freethis )
{
    FMOD_RESULT result;

    if ( mSystem == NULL )
    {
        return FMOD_ERR_UNINITIALIZED;
    }
    if ( mFlags & SOUNDI_FLAG_PRELOADED )
    {
        return FMOD_ERR_PRELOADED;
    }
    if ( mFlags & SOUNDI_FLAG_PROGRAMMERSOUND )
    {
        return FMOD_ERR_PROGRAMMERSOUND;
    }

    while ( ( mOpenState != FMOD_OPENSTATE_READY && mOpenState != FMOD_OPENSTATE_ERROR ) ||
            ( mFlags & SOUNDI_FLAG_ASYNCFINISHING ) )
    {
        FMOD_OS_Time_Sleep( 2 );
    }

    result = mSystem->stopSound( this );
    if ( result != FMOD_OK )
    {
        return result;
    }

    if ( mBufferMemory != NULL )
    {
        if ( ( mMode & FMOD_LOADSECONDARYRAM ) &&
             ( lbl_80529020->mMemoryTypeFlags & FMOD_MEMORY_SECONDARY ) )
        {
#line 105
            lbl_80529020->mMemPool->free( mBufferMemory, __FILE__, __LINE__, FMOD_MEMORY_SECONDARY );
        }
        else
        {
#line 109
            FMOD_Memory_Free( mBufferMemory );
        }
        mBufferMemory = NULL;
    }

    if ( mLoopPointDataEnd != NULL && mLoopPointDataEnd != mLoopPointDataEndMemory )
    {
#line 116
        FMOD_Memory_Free( mLoopPointDataEnd );
        mLoopPointDataEnd = NULL;
    }

    mBuffer = NULL;

    return Sample::release( freethis );
}

/*
====================
SampleSoftware::setMode

Sets the mode via Sample::setMode and updates the loop-point overflow data.
====================
*/
// 0x803411D0
FMOD_RESULT SampleSoftware::setMode( unsigned int mode )
{
    FMOD_RESULT result;

    result = Sample::setMode( mode );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = setLoopPointData();
    if ( result != FMOD_OK )
    {
        return result;
    }

    return FMOD_OK;
}

/*
====================
SampleSoftware::lockInternal

Restores overwritten loop-point overflow bytes if the locked offset overlaps them, then
returns one or two pointers into the sample buffer covering the requested byte range.
====================
*/
// 0x80341224
FMOD_RESULT SampleSoftware::lockInternal( unsigned int offset, unsigned int length, void **ptr1,
                                          void **ptr2, unsigned int *len1, unsigned int *len2 )
{
    FMOD_RESULT result;
    char *src;
    unsigned int overflowbytes, pointB;

    result = getBytesFromSamples( 4, &overflowbytes );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = getBytesFromSamples( mLoopStart + mLoopLength, &pointB );
    if ( result != FMOD_OK )
    {
        return result;
    }

    if ( offset >= pointB && offset < pointB + overflowbytes )
    {
        result = restoreLoopPointData();
        if ( result != FMOD_OK )
        {
            return result;
        }
    }

    src = (char *)mBuffer;

    if ( offset >= mLengthBytes || length > mLengthBytes )
    {
        *ptr1 = NULL;
        if ( ptr2 )
        {
            *ptr2 = NULL;
        }
        *len1 = 0;
        if ( len2 )
        {
            *len2 = 0;
        }
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( offset + length <= mLengthBytes )
    {
        *ptr1 = src + offset;
        *len1 = length;
        if ( ptr2 )
        {
            *ptr2 = NULL;
        }
        if ( len2 )
        {
            *len2 = 0;
        }
    }
    else
    {
        *ptr1 = src + offset;
        *len1 = mLengthBytes - offset;
        *ptr2 = src;
        *len2 = length - ( mLengthBytes - offset );
    }

    return FMOD_OK;
}

/*
====================
SampleSoftware::unlockInternal

Reapplies loop-point overflow bytes after unlocking the sample buffer.
====================
*/
// 0x803415DC
FMOD_RESULT SampleSoftware::unlockInternal( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 )
{
    FMOD_RESULT result;

    result = setLoopPointData();
    if ( result != FMOD_OK )
    {
        return result;
    }

    return FMOD_OK;
}

/*
====================
SampleSoftware::setBufferData

Sets the sample's buffer pointer directly.
====================
*/
// 0x80341610
FMOD_RESULT SampleSoftware::setBufferData( void *data )
{
    mBuffer = data;
    return FMOD_OK;
}

/*
====================
SampleSoftware::setLoopPointData

Writes 4 overflow samples past the loop end for software resampler interpolation (mirrored
for bidirectional loops, or copied from loop start for forward loops), saving the original
bytes so they can be restored when looping is disabled or the region is locked.
====================
*/
// 0x8034161C
FMOD_RESULT SampleSoftware::setLoopPointData()
{
    FMOD_RESULT result;
    unsigned int overflowbytes, pointA, pointB;

    if ( mFormat < FMOD_SOUND_FORMAT_PCM8 || mFormat > FMOD_SOUND_FORMAT_PCMFLOAT || !mLoopPointDataEnd )
    {
        return FMOD_OK;
    }

    result = getBytesFromSamples( 4, &overflowbytes );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = getBytesFromSamples( mLoopStart, &pointA );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = getBytesFromSamples( mLoopStart + mLoopLength, &pointB );
    if ( result != FMOD_OK )
    {
        return result;
    }

    if ( mMode & FMOD_LOOP_BIDI )
    {
        int count;

        memcpy( mLoopPointDataEnd, (char *)mBuffer + pointB, overflowbytes );
        mLoopPointDataEndOffset = pointB;

        switch ( mFormat )
        {
            case FMOD_SOUND_FORMAT_PCM8:
            {
                char *srcptr, *destptr;
                destptr = (char *)mBuffer + pointB;
                srcptr = destptr - mChannels;
                for ( count = 0; count < mChannels * 4; count++ )
                {
                    *destptr = *srcptr;
                    destptr++;
                    srcptr--;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_PCM16:
            {
                short *srcptr, *destptr;
                destptr = (short *)( (char *)mBuffer + pointB );
                srcptr = destptr - mChannels;
                for ( count = 0; count < mChannels * 4; count++ )
                {
                    *destptr = *srcptr;
                    destptr++;
                    srcptr--;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_PCM24:
            {
                char *destptr = (char *)mBuffer + pointB;
                char *srcptr = destptr - ( mChannels * 3 );
                for ( count = 0; count < mChannels * 4; count++ )
                {
                    destptr[0] = srcptr[0];
                    destptr[1] = srcptr[1];
                    destptr[2] = srcptr[2];
                    destptr += 3;
                    srcptr -= 3;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_PCM32:
            case FMOD_SOUND_FORMAT_PCMFLOAT:
            {
                int *srcptr, *destptr;
                destptr = (int *)( (char *)mBuffer + pointB );
                srcptr = destptr - mChannels;
                for ( count = 0; count < mChannels * 4; count++ )
                {
                    *destptr = *srcptr;
                    destptr++;
                    srcptr--;
                }
                break;
            }
        }
    }
    else if ( mMode & FMOD_LOOP_NORMAL )
    {
        if ( mLoopPointDataEndOffset )
        {
            memcpy( (char *)mBuffer + mLoopPointDataEndOffset, mLoopPointDataEnd, overflowbytes );
        }
        memcpy( mLoopPointDataEnd, (char *)mBuffer + pointB, overflowbytes );
        mLoopPointDataEndOffset = pointB;
        memcpy( (char *)mBuffer + pointB, (char *)mBuffer + pointA, overflowbytes );
    }
    else if ( mMode & FMOD_LOOP_OFF )
    {
        if ( mLoopPointDataEndOffset )
        {
            memcpy( (char *)mBuffer + mLoopPointDataEndOffset, mLoopPointDataEnd, overflowbytes );
            mLoopPointDataEndOffset = 0;
        }
    }

    return FMOD_OK;
}

/*
====================
SampleSoftware::restoreLoopPointData

Restores the original bytes saved at the loop-point overflow offset.
====================
*/
// 0x80341C88
FMOD_RESULT SampleSoftware::restoreLoopPointData()
{
    FMOD_RESULT result;
    unsigned int overflowbytes, pointA, pointB;

    if ( mFormat < FMOD_SOUND_FORMAT_PCM8 || mFormat > FMOD_SOUND_FORMAT_PCMFLOAT || !mLoopPointDataEnd )
    {
        return FMOD_OK;
    }

    result = getBytesFromSamples( 4, &overflowbytes );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = getBytesFromSamples( mLoopStart, &pointA );
    if ( result != FMOD_OK )
    {
        return result;
    }

    result = getBytesFromSamples( mLoopStart + mLoopLength, &pointB );
    if ( result != FMOD_OK )
    {
        return result;
    }

    if ( mLoopPointDataEndOffset )
    {
        memcpy( (char *)mBuffer + mLoopPointDataEndOffset, mLoopPointDataEnd, overflowbytes );
        mLoopPointDataEndOffset = 0;
    }

    return FMOD_OK;
}

} // namespace FMOD
