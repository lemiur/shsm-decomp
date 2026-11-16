/*
 * fmod_sound_sample.cpp: FMOD::Sample, in-memory sample and multi-subsample container.
 * Class, member, function names, and line numbers come from Gormiti's DWARF
 * (sibling.exe cu fmod_sound_sample, sibling.exe types Sample, SoundI).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_sound_sample.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
Sample::Sample

Initialises a sample with no sub-samples and readable lock buffer.
====================
*/
// 0x80341FE4
Sample::Sample() {
    mNumSubSamples = 0;
    mLockCanRead = true;
}

/*
====================
Sample::lock

Locks a byte range of the sample's buffer, or de-interleaves multi-subsample data into a
shared lock buffer when sub-samples are present.
====================
*/
// 0x80342030
FMOD_RESULT Sample::lock( unsigned int offset, unsigned int length, void **ptr1, void **ptr2,
                          unsigned int *len1, unsigned int *len2 ) {
    int count;
    unsigned int samples = 0;
    unsigned int lenbytes;

    if ( !ptr1 || !len1 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( mNumSubSamples < 1 ) {
        return lockInternal( offset, length, ptr1, ptr2, len1, len2 );
    }

    if ( mMode & FMOD_CREATECOMPRESSEDSAMPLE ) {
        return FMOD_ERR_FORMAT;
    }

    getBytesFromSamples( mLength, &lenbytes );

    if ( length + offset > lenbytes ) {
        length = lenbytes - offset;
    }

    length = ( length > 16384 ) ? 16384 : length;

    FMOD_OS_CriticalSection_Enter( mSystem->mMultiSubSampleLockBufferCrit );

    *ptr1 = mLockBuffer;
    if ( ptr2 ) {
        *ptr2 = NULL;
    }

    *len1 = length;
    if ( len2 ) {
        *len2 = 0;
    }

    mLockLength = length;
    mLockOffset = offset;

    SoundI::getSamplesFromBytes( length, &samples, mChannels, mFormat );

    offset /= mNumSubSamples;
    length /= mNumSubSamples;

    for ( count = 0; count < mNumSubSamples; count++ ) {
        if ( mSubSample[count]->mLockCanRead ) {
            unsigned int count2;
            unsigned int in_len1, in_len2;
            void *in_ptr1, *in_ptr2;

            mSubSample[count]->lock( offset, length, &in_ptr1, &in_ptr2, &in_len1, &in_len2 );

            switch ( mFormat ) {
                case FMOD_SOUND_FORMAT_PCM8: {
                    signed char *dest = (signed char *)( *ptr1 ) + count;
                    signed char *src = (signed char *)in_ptr1;

                    count2 = samples >> 3;
                    while ( count2 ) {
                        dest[0] = src[0];
                        dest[1 * mNumSubSamples] = src[1];
                        dest[2 * mNumSubSamples] = src[2];
                        dest[3 * mNumSubSamples] = src[3];
                        dest[4 * mNumSubSamples] = src[4];
                        dest[5 * mNumSubSamples] = src[5];
                        dest[6 * mNumSubSamples] = src[6];
                        dest[7 * mNumSubSamples] = src[7];

                        src += 8;
                        dest += 8 * mNumSubSamples;
                        count2--;
                    }

                    count2 = samples & 7;
                    while ( count2 ) {
                        *dest = *src++;
                        dest += mNumSubSamples;
                        count2--;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_GCADPCM: {
                    samples = length / 2;
                }
                case FMOD_SOUND_FORMAT_PCM16: {
                    short *dest = (short *)( *ptr1 ) + count;
                    short *src = (short *)in_ptr1;

                    count2 = samples >> 3;
                    while ( count2 ) {
                        dest[0] = src[0];
                        dest[1 * mNumSubSamples] = src[1];
                        dest[2 * mNumSubSamples] = src[2];
                        dest[3 * mNumSubSamples] = src[3];
                        dest[4 * mNumSubSamples] = src[4];
                        dest[5 * mNumSubSamples] = src[5];
                        dest[6 * mNumSubSamples] = src[6];
                        dest[7 * mNumSubSamples] = src[7];

                        src += 8;
                        dest += 8 * mNumSubSamples;
                        count2--;
                    }

                    count2 = samples & 7;
                    while ( count2 ) {
                        *dest = *src++;
                        dest += mNumSubSamples;
                        count2--;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_PCM24: {
                    for ( count2 = 0; count2 < samples; count2++ ) {
                        ( (char *)( *ptr1 ) )[( mNumSubSamples * count2 + count ) * 3] = ( (char *)in_ptr1 )[count2 * 3];
                        ( (char *)( *ptr1 ) )[( mNumSubSamples * count2 + count ) * 3 + 1] = ( (char *)in_ptr1 )[count2 * 3 + 1];
                        ( (char *)( *ptr1 ) )[( mNumSubSamples * count2 + count ) * 3 + 2] = ( (char *)in_ptr1 )[count2 * 3 + 2];
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_PCM32:
                case FMOD_SOUND_FORMAT_PCMFLOAT: {
                    unsigned int *dest = (unsigned int *)( *ptr1 ) + count;
                    unsigned int *src = (unsigned int *)in_ptr1;

                    count2 = samples >> 3;
                    while ( count2 ) {
                        dest[0] = src[0];
                        dest[1 * mNumSubSamples] = src[1];
                        dest[2 * mNumSubSamples] = src[2];
                        dest[3 * mNumSubSamples] = src[3];
                        dest[4 * mNumSubSamples] = src[4];
                        dest[5 * mNumSubSamples] = src[5];
                        dest[6 * mNumSubSamples] = src[6];
                        dest[7 * mNumSubSamples] = src[7];

                        src += 8;
                        dest += 8 * mNumSubSamples;
                        count2--;
                    }

                    count2 = samples & 7;
                    while ( count2 ) {
                        *dest = *src++;
                        dest += mNumSubSamples;
                        count2--;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_IMAADPCM: {
                    if ( mNumSubSamples == 2 ) {
                        unsigned int *dest = (unsigned int *)( *ptr1 ) + count;
                        unsigned int *src = (unsigned int *)in_ptr1;

                        samples = length / 4;

                        count2 = samples >> 3;
                        while ( count2 ) {
                            dest[0] = src[0];
                            dest[1 * mNumSubSamples] = src[1];
                            dest[2 * mNumSubSamples] = src[2];
                            dest[3 * mNumSubSamples] = src[3];
                            dest[4 * mNumSubSamples] = src[4];
                            dest[5 * mNumSubSamples] = src[5];
                            dest[6 * mNumSubSamples] = src[6];
                            dest[7 * mNumSubSamples] = src[7];

                            src += 8;
                            dest += 8 * mNumSubSamples;
                            count2--;
                        }

                        count2 = samples & 7;
                        while ( count2 ) {
                            *dest = *src++;
                            dest += mNumSubSamples;
                            count2--;
                        }
                        break;
                    }
                }
                case FMOD_SOUND_FORMAT_VAG: {
                    unsigned int blocksize = 0;
                    unsigned int blocks;
                    char *dest = (char *)( *ptr1 );
                    char *src = (char *)in_ptr1;

                    getBytesFromSamples( 1, &blocksize, 1, mFormat );

                    blocks = length / blocksize;
                    dest += count * blocksize;

                    for ( count2 = 0; count2 < blocks; count2++ ) {
                        memcpy( dest, src, blocksize );
                        src += blocksize;
                        dest += blocksize * mNumSubSamples;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_XMA: {
                    signed char *dest = (signed char *)( *ptr1 ) + count;
                    signed char *src = (signed char *)in_ptr1;

                    count2 = samples >> 3;
                    while ( count2 ) {
                        dest[0] = src[0];
                        dest[1 * mNumSubSamples] = src[1];
                        dest[2 * mNumSubSamples] = src[2];
                        dest[3 * mNumSubSamples] = src[3];
                        dest[4 * mNumSubSamples] = src[4];
                        dest[5 * mNumSubSamples] = src[5];
                        dest[6 * mNumSubSamples] = src[6];
                        dest[7 * mNumSubSamples] = src[7];

                        src += 8;
                        dest += 8 * mNumSubSamples;
                        count2--;
                    }

                    count2 = samples & 7;
                    while ( count2 ) {
                        *dest = *src++;
                        dest += mNumSubSamples;
                        count2--;
                    }
                    break;
                }
                default: {
                    FMOD_OS_CriticalSection_Leave( mSystem->mMultiSubSampleLockBufferCrit );
                    return FMOD_ERR_FORMAT;
                }
            }

            mSubSample[count]->unlock( in_ptr1, in_ptr2, in_len1, in_len2 );
        }
    }

    return FMOD_OK;
}

/*
====================
Sample::unlock

Unlocks a sample buffer, interleaving modified multi-subsample data back into each
sub-sample and releasing the shared lock buffer critical section. The VAG branch computes
the block count (`blocks = length / 16`) before looping; the sibling's DWARF omitted it.
====================
*/
// 0x80342EF8
FMOD_RESULT Sample::unlock( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 ) {
    int count;
    unsigned int length, offset, samples = 0;

    if ( !ptr1 || !len1 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( mNumSubSamples < 1 ) {
        return unlockInternal( ptr1, ptr2, len1, len2 );
    }

    getSamplesFromBytes( mLockLength, &samples );

    length = mLockLength / mNumSubSamples;
    offset = mLockOffset / mNumSubSamples;

    for ( count = 0; count < mNumSubSamples; count++ ) {
        unsigned int count2;
        unsigned int in_len1, in_len2;
        void *in_ptr1, *in_ptr2;

        mSubSample[count]->lock( offset, length, &in_ptr1, &in_ptr2, &in_len1, &in_len2 );

        switch ( mFormat ) {
            case FMOD_SOUND_FORMAT_PCM8: {
                signed char *src = (signed char *)ptr1 + count;
                signed char *dest = (signed char *)in_ptr1;

                count2 = samples >> 3;
                while ( count2 ) {
                    dest[0] = src[0];
                    dest[1] = src[1 * mNumSubSamples];
                    dest[2] = src[2 * mNumSubSamples];
                    dest[3] = src[3 * mNumSubSamples];
                    dest[4] = src[4 * mNumSubSamples];
                    dest[5] = src[5 * mNumSubSamples];
                    dest[6] = src[6 * mNumSubSamples];
                    dest[7] = src[7 * mNumSubSamples];

                    dest += 8;
                    src += 8 * mNumSubSamples;
                    count2--;
                }

                count2 = samples & 7;
                while ( count2 ) {
                    *dest++ = *src;
                    src += mNumSubSamples;
                    count2--;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_GCADPCM: {
                samples = length / 2;
            }
            case FMOD_SOUND_FORMAT_PCM16: {
                short *src = (short *)ptr1 + count;
                short *dest = (short *)in_ptr1;

                count2 = samples >> 3;
                while ( count2 ) {
                    dest[0] = src[0];
                    dest[1] = src[1 * mNumSubSamples];
                    dest[2] = src[2 * mNumSubSamples];
                    dest[3] = src[3 * mNumSubSamples];
                    dest[4] = src[4 * mNumSubSamples];
                    dest[5] = src[5 * mNumSubSamples];
                    dest[6] = src[6 * mNumSubSamples];
                    dest[7] = src[7 * mNumSubSamples];

                    dest += 8;
                    src += 8 * mNumSubSamples;
                    count2--;
                }

                count2 = samples & 7;
                while ( count2 ) {
                    *dest++ = *src;
                    src += mNumSubSamples;
                    count2--;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_PCM24: {
                for ( count2 = 0; count2 < samples; count2++ ) {
                    ( (char *)in_ptr1 )[count2 * 3] = ( (char *)ptr1 )[( mNumSubSamples * count2 + count ) * 3];
                    ( (char *)in_ptr1 )[count2 * 3 + 1] = ( (char *)ptr1 )[( mNumSubSamples * count2 + count ) * 3 + 1];
                    ( (char *)in_ptr1 )[count2 * 3 + 2] = ( (char *)ptr1 )[( mNumSubSamples * count2 + count ) * 3 + 2];
                }
                break;
            }
            case FMOD_SOUND_FORMAT_PCM32:
            case FMOD_SOUND_FORMAT_PCMFLOAT: {
                unsigned int *src = (unsigned int *)ptr1 + count;
                unsigned int *dest = (unsigned int *)in_ptr1;

                count2 = samples >> 3;
                while ( count2 ) {
                    dest[0] = src[0];
                    dest[1] = src[1 * mNumSubSamples];
                    dest[2] = src[2 * mNumSubSamples];
                    dest[3] = src[3 * mNumSubSamples];
                    dest[4] = src[4 * mNumSubSamples];
                    dest[5] = src[5 * mNumSubSamples];
                    dest[6] = src[6 * mNumSubSamples];
                    dest[7] = src[7 * mNumSubSamples];

                    dest += 8;
                    src += 8 * mNumSubSamples;
                    count2--;
                }

                count2 = samples & 7;
                while ( count2 ) {
                    *dest++ = *src;
                    src += mNumSubSamples;
                    count2--;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_IMAADPCM: {
                if ( mNumSubSamples == 2 ) {
                    samples = length / 4;

                    unsigned int *src = (unsigned int *)ptr1 + count;
                    unsigned int *dest = (unsigned int *)in_ptr1;

                    count2 = samples >> 3;
                    while ( count2 ) {
                        dest[0] = src[0];
                        dest[1] = src[1 * mNumSubSamples];
                        dest[2] = src[2 * mNumSubSamples];
                        dest[3] = src[3 * mNumSubSamples];
                        dest[4] = src[4 * mNumSubSamples];
                        dest[5] = src[5 * mNumSubSamples];
                        dest[6] = src[6 * mNumSubSamples];
                        dest[7] = src[7 * mNumSubSamples];

                        dest += 8;
                        src += 8 * mNumSubSamples;
                        count2--;
                    }

                    count2 = samples & 7;
                    while ( count2 ) {
                        *dest++ = *src;
                        src += mNumSubSamples;
                        count2--;
                    }
                    break;
                }
            }
            case FMOD_SOUND_FORMAT_VAG: {
                unsigned int *dest = (unsigned int *)in_ptr1;
                unsigned int *src = (unsigned int *)ptr1 + count * 4;
                unsigned int blocks = length / 16;

                for ( count2 = 0; count2 < blocks; count2++ ) {
                    dest[0] = src[0];
                    dest[1] = src[1];
                    dest[2] = src[2];
                    dest[3] = src[3];

                    dest += 4;
                    src += 4 * mNumSubSamples;
                }
                break;
            }
            case FMOD_SOUND_FORMAT_XMA: {
                signed char *src = (signed char *)ptr1 + count;
                signed char *dest = (signed char *)in_ptr1;

                count2 = samples >> 3;
                while ( count2 ) {
                    dest[0] = src[0];
                    dest[1] = src[1 * mNumSubSamples];
                    dest[2] = src[2 * mNumSubSamples];
                    dest[3] = src[3 * mNumSubSamples];
                    dest[4] = src[4 * mNumSubSamples];
                    dest[5] = src[5 * mNumSubSamples];
                    dest[6] = src[6 * mNumSubSamples];
                    dest[7] = src[7 * mNumSubSamples];

                    dest += 8;
                    src += 8 * mNumSubSamples;
                    count2--;
                }

                count2 = samples & 7;
                while ( count2 ) {
                    *dest++ = *src;
                    src += mNumSubSamples;
                    count2--;
                }
                break;
            }
            default: {
                FMOD_OS_CriticalSection_Leave( mSystem->mMultiSubSampleLockBufferCrit );
                return FMOD_ERR_FORMAT;
            }
        }

        mSubSample[count]->unlock( in_ptr1, in_ptr2, in_len1, in_len2 );
    }

    FMOD_OS_CriticalSection_Leave( mSystem->mMultiSubSampleLockBufferCrit );

    return FMOD_OK;
}

/*
====================
Sample::release

Stops the sound, frees any shared multi-subsample lock buffer, releases sub-samples, and
delegates to SoundI::release.
====================
*/
// 0x80343C60
FMOD_RESULT Sample::release( bool freethis ) {
    int count;

    if ( mFlags & SOUNDI_FLAG_PRELOADED ) {
        return FMOD_ERR_PRELOADED;
    }
    if ( mFlags & SOUNDI_FLAG_PROGRAMMERSOUND ) {
        return FMOD_ERR_PROGRAMMERSOUND;
    }

    if ( mSystem ) {
        mSystem->stopSound( this );

        if ( mLockBuffer ) {
            mSystem->mMultiSubSampleLockBuffer.free( "", 0 );
        }
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        if ( mSubSample[count] ) {
            mSubSample[count]->mCodec = NULL;
            mSubSample[count]->release( true );
            mSubSample[count] = NULL;
        }
    }

    return SoundI::release( freethis );
}

/*
====================
Sample::setDefaults

Sets default playback parameters on this sample and all of its sub-samples.
====================
*/
// 0x80343D48
FMOD_RESULT Sample::setDefaults( float frequency, float volume, float pan, int priority ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::setDefaults( frequency, volume, pan, priority );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->setDefaults( frequency, volume, pan, priority );
    }

    return FMOD_OK;
}

/*
====================
Sample::setVariations

Sets default parameter variations on this sample and all of its sub-samples.
====================
*/
// 0x80343E08
FMOD_RESULT Sample::setVariations( float frequencyvar, float volumevar, float panvar ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::setVariations( frequencyvar, volumevar, panvar );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->setVariations( frequencyvar, volumevar, panvar );
    }

    return FMOD_OK;
}

/*
====================
Sample::set3DMinMaxDistance

Sets 3D min/max distance on this sample and all of its sub-samples.
====================
*/
// 0x80343EB8
FMOD_RESULT Sample::set3DMinMaxDistance( float min, float max ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::set3DMinMaxDistance( min, max );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->set3DMinMaxDistance( min, max );
    }

    return FMOD_OK;
}

/*
====================
Sample::set3DConeSettings

Sets 3D cone settings on this sample and all of its sub-samples.
====================
*/
// 0x80343F58
FMOD_RESULT Sample::set3DConeSettings( float insideconeangle, float outsideconeangle, float outsidevolume ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::set3DConeSettings( insideconeangle, outsideconeangle, outsidevolume );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->set3DConeSettings( insideconeangle, outsideconeangle, outsidevolume );
    }

    return FMOD_OK;
}

/*
====================
Sample::setMode

Sets the sound mode on this sample and all of its sub-samples.
====================
*/
// 0x80344008
FMOD_RESULT Sample::setMode( unsigned int mode ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::setMode( mode );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->setMode( mode );
    }

    return FMOD_OK;
}

/*
====================
Sample::setLoopCount

Sets the loop count on this sample and all of its sub-samples.
====================
*/
// 0x80344098
FMOD_RESULT Sample::setLoopCount( int loopcount ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::setLoopCount( loopcount );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->setLoopCount( loopcount );
    }

    return FMOD_OK;
}

/*
====================
Sample::setLoopPoints

Sets loop start and end points on this sample and all of its sub-samples.
====================
*/
// 0x80344128
FMOD_RESULT Sample::setLoopPoints( unsigned int loopstart, unsigned int loopstarttype,
                                   unsigned int loopend, unsigned int loopendtype ) {
    FMOD_RESULT result;
    int count;

    result = SoundI::setLoopPoints( loopstart, loopstarttype, loopend, loopendtype );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < mNumSubSamples; count++ ) {
        mSubSample[count]->setLoopPoints( loopstart, loopstarttype, loopend, loopendtype );
    }

    return FMOD_OK;
}

} // namespace FMOD
