/*
 * fmod_sample_revolution.cpp: FMOD::SampleRevolution, the Wii (Revolution) sample. Class, member and
 * function names come from Gormiti's DWARF (sibling.exe types and pair).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_sample_revolution.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_revolution.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_sample_revolution.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <revolution/ax.h>          // AXSetVoiceAdpcmLoop
#include <revolution/os/OSCache.h>  // DCFlushRange

namespace FMOD {

/*
====================
SampleRevolution::SampleRevolution

Constructor: the sample owns no buffer and no AX voice yet; it owns its buffer memory.
====================
*/
// 0x8035AB74
SampleRevolution::SampleRevolution()
{
    mChannelRevolution = NULL;
    mLockOffset = 0;
    mLocked = 0;
    mOwnMemory = true;
}

/*
====================
SampleRevolution::release

Stops the sample's sound in the system, frees the buffer memory when the sample owns it, then releases the
Sample. The original returns FMOD_OK, not the stop error, when stopSound fails.
====================
*/
// 0x8035ABC8
FMOD_RESULT SampleRevolution::release( bool freethis )
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
    while ( mOpenState != FMOD_OPENSTATE_READY && mOpenState != FMOD_OPENSTATE_ERROR )
    {
        FMOD_OS_Time_Sleep( 2 );
    }
    result = mSystem->stopSound( this );
    if ( result != FMOD_OK )
    {
        return FMOD_OK;
    }
    if ( mBufferMemory != NULL && mOwnMemory )
    {
#line 94
        FMOD_Memory_Free( mBufferMemory );
        mBuffer = NULL;
        mBufferMemory = NULL;
    }
    return Sample::release( freethis );
}

/*
====================
SampleRevolution::lockInternal

Locks a range of the sample's buffer. A range that runs past the end of the buffer is returned in two
parts, the second starting at the buffer's beginning. The byte length is mLengthBytes when it is set, else
the sample count converted by SoundI::getBytesFromSamples (rounded up to whole frames).
====================
*/
// 0x8035ACBC
FMOD_RESULT SampleRevolution::lockInternal( unsigned int offset, unsigned int length, void **ptr1,
                                            void **ptr2, unsigned int *len1, unsigned int *len2 )
{
    FMOD_RESULT result;
    unsigned int lenbytes;
    char *src;

    if ( mLengthBytes )
    {
        lenbytes = mLengthBytes;
    }
    else
    {
        result = getBytesFromSamples( mLength, &lenbytes );
        if ( result != FMOD_OK )
        {
            return result;
        }
    }

    src = (char *)mBuffer;
    if ( src == NULL )
    {
        *ptr1 = NULL;
        *ptr2 = NULL;
        *len1 = 0;
        *len2 = 0;
        return FMOD_ERR_INVALID_PARAM;
    }
    if ( length > lenbytes )
    {
        length = lenbytes;
    }
    if ( offset >= lenbytes )
    {
        *ptr1 = NULL;
        *ptr2 = NULL;
        *len1 = 0;
        *len2 = 0;
        return FMOD_ERR_INVALID_PARAM;
    }
    if ( offset + length <= lenbytes )
    {
        *ptr1 = src + offset;
        *len1 = length;
        *ptr2 = NULL;
        *len2 = 0;
    }
    else
    {
        *ptr1 = src + offset;
        *len1 = lenbytes - offset;
        *ptr2 = src;
        *len2 = length - ( lenbytes - offset );
    }

    return FMOD_OK;
}

/*
====================
SampleRevolution::unlockInternal

Flushes the cache over the locked parts that were written, then, for an ADPCM sample that has its own AX
voice and starts at its beginning, sets the voice's ADPCM loop context from the buffer's first frame.
====================
*/
// 0x8035AEF8
FMOD_RESULT SampleRevolution::unlockInternal( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 )
{
    if ( len1 != 0 && ptr1 != NULL )
    {
        DCFlushRange( ptr1, len1 );
    }
    if ( len2 != 0 && ptr2 != NULL )
    {
        DCFlushRange( ptr2, len2 );
    }
    if ( mChannelRevolution != NULL && mLockOffset == 0 && mFormat == FMOD_SOUND_FORMAT_GCADPCM )
    {
        AXPBADPCMLOOP loop;

        loop.loop_pred_scale = *(unsigned char *)mBuffer;
        loop.loop_yn1 = 0;
        loop.loop_yn2 = 0;
        AXSetVoiceAdpcmLoop( mChannelRevolution->mVoice, &loop );
    }
    return FMOD_OK;
}

/*
====================
SampleRevolution::setBufferData

Sets the sample's buffer to caller-owned memory. The buffer must be 8-byte aligned for the DMA.
====================
*/
// 0x8035AFBC
FMOD_RESULT SampleRevolution::setBufferData( void *data )
{
    FMOD_RESULT result = FMOD_OK;

    mOwnMemory = false;
    mBuffer = data;
    if ( ( (unsigned int)data & 7 ) != 0 )
    {
        result = FMOD_ERR_INVALID_ADDRESS;
    }
    return result;
}

} // namespace FMOD
