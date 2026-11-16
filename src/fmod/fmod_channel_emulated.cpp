// Unit name and boundary: Gormiti link map (fmodex.a fmod_channel_emulated.obj)
/*
 * fmod_channel_emulated.cpp: ChannelEmulated, the real channel of a virtual voice. It tracks the
 * voice's position in software, ending or looping it, and keeps a DSP head so the voice can be mixed
 * when it turns audible. Reconstructed from retail with the sibling build's names, layouts and line
 * table (Gormiti link map, fmodex.a fmod_channel_emulated.obj).
 */

#include <fmod/fmod.h>
#include <fmod/fmod_channel_emulated.h>
#include <fmod/fmod_channel_stream.h>
#include <fmod/fmod_channelgroupi.h>
#include <fmod/fmod_channeli.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_reverbi.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

extern "C" char *FMOD_strcpy( char *dest, const char *src );

namespace FMOD {

/*
====================
ChannelEmulated::ChannelEmulated

Starts with no DSP head.
====================
*/
ChannelEmulated::ChannelEmulated() {
    mDSPHead = NULL;
} // 0x8032C6B0

/*
====================
ChannelEmulated::init

Creates the DSP head the voice is mixed through when it becomes audible. Nothing is created when
software mixing is disabled.
====================
*/
FMOD_RESULT ChannelEmulated::init( int index, SystemI *system, Output *output, DSPI *dspmixtarget ) {
    FMOD_RESULT result;

    result = ChannelReal::init( index, system, output, dspmixtarget );

    if ( !( mSystem->mFlags & FMOD_INIT_SOFTWARE_DISABLE ) ) {
        FMOD_DSP_DESCRIPTION_EX descriptionex;

        memset( &descriptionex, 0, sizeof( FMOD_DSP_DESCRIPTION_EX ) );

        FMOD_strcpy( descriptionex.name, "EmulatedChannel DSPHead Unit" );
        descriptionex.version   = 0x00010100;    // plugin version 1.01.00 (major.minor.patch in bytes)
        descriptionex.mCategory = FMOD_DSP_CATEGORY_FILTER;
        descriptionex.mFormat   = FMOD_SOUND_FORMAT_PCMFLOAT;

        mDSPHead = &mDSPHeadMemory;

        result = mSystem->createDSP( &descriptionex, &mDSPHead, false );
        if ( result != FMOD_OK ) {
            return result;
        }

        mMinFrequency = -mMaxFrequency;
    }

    return FMOD_OK;
} // 0x8032C748

/*
====================
ChannelEmulated::alloc

Connects the DSP head under the channel group's head.
====================
*/
FMOD_RESULT ChannelEmulated::alloc() {
    FMOD_RESULT result;

    result = ChannelReal::alloc();
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mDSPHead ) {
        result = mDSPHead->disconnectFrom( NULL, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = mParent->mChannelGroup->mDSPMixTarget->addInputQueued( mDSPHead, false, NULL, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x8032C810

/*
====================
ChannelEmulated::stop

Marks a stream's channel as finished, disconnects the DSP head and releases the reverb connections.
====================
*/
FMOD_RESULT ChannelEmulated::stop() {
    if ( mSound && mSound->isStream() ) {
        Stream *stream = static_cast< Stream * >( mSound );

        if ( stream->mChannel ) {
            stream->mChannel->mFinished = true;
        }
    }

    if ( mDSPHead ) {
        mDSPHead->mFlags &= ~0x2;
        mDSPHead->disconnectAll( false, true );
    }

    if ( mParent ) {
        int instance;

        for ( instance = 0; instance < 4; instance++ ) {
            mSystem->mReverbGlobal.resetConnectionPointer( instance, mParent->mIndex );
        }
    }

    return ChannelReal::stop();
} // 0x8032C89C

/*
====================
SoundI::isStream

Out-of-line copy of the base answer: a sound is not a stream.
====================
*/
bool SoundI::isStream() {
    return false;
} // 0x8032C974

/*
====================
ChannelEmulated::close

Releases the DSP head.
====================
*/
FMOD_RESULT ChannelEmulated::close() {
    FMOD_RESULT result;

    result = ChannelReal::close();
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mDSPHead ) {
        mDSPHead->release( false );
        mDSPHead = NULL;
    }

    return FMOD_OK;
} // 0x8032C97C

/*
====================
ChannelEmulated::update

Advances the voice's position by the time elapsed, looping or ending it at the sound's boundaries.

The loop test reads the mode through a local (name invented): a named local is numbered before the
compiler's temporaries and coloured after mSound's load, which puts mSound in r3 and the mode in r4 as in
retail and in Gormiti; reading mMode directly swaps the two. Gormiti's DWARF has a lexical block with no
listed variable over the if ( mSound ) body, and two source lines between that test and the loop test:
the mode local is one the DWARF did not record (MWCC drops locals the optimiser eliminates).
====================
*/
FMOD_RESULT ChannelEmulated::update( int delta ) {
    FMOD_RESULT result;

    result = ChannelReal::update( delta );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( ( mFlags & CHANNELREAL_FLAG_PAUSED ) || !( mFlags & CHANNELREAL_FLAG_PLAYING ) ) {
        return FMOD_OK;
    }

    if ( ( ( mParent->mFlags & CHANNELI_FLAG_PAUSED ) && !( mParent->mFlags & CHANNELI_FLAG_PLAYINGPAUSED ) )
         || ( mMode & FMOD_VIRTUAL_PLAYFROMSTART ) ) {
        return FMOD_OK;
    }

    if ( mSystem->mDSPClock.mValue < mParent->mDSPClockDelay.mValue ) {
        return FMOD_OK;
    }

    // The elapsed time in samples, backwards when the voice plays backwards.
    delta = delta * (int)( mParent->mFrequency * mParent->mPitch3D ) / 1000;
    if ( mDirection == 1 ) {
        delta = -delta;
    }

    if ( (int)( mPosition + delta ) > 0 ) {
        mPosition += delta;
    } else {
        mPosition = 0;
    }

    if ( mSound ) {
        FMOD_MODE mode = mMode;

        if ( ( mode & FMOD_LOOP_NORMAL ) || ( ( mode & FMOD_LOOP_BIDI ) && mLoopCount != 0 ) ) {
            while ( ( mParent->mFrequency > 0.0f && mPosition >= mLoopStart + mLoopLength )
                    || ( mParent->mFrequency < 0.0f && mPosition <= mLoopStart ) ) {
                if ( mLoopCount == 0 ) {
                    // Out of loops: park at the end of the loop and stop playing.
                    if ( mDirection == 0 && mParent->mFrequency > 0.0f ) {
                        mPosition = mLoopStart + mLoopLength;
                    } else {
                        mPosition = mLoopStart;
                    }
                    mFlags &= ~CHANNELREAL_FLAG_PLAYING;
                    break;
                }

                if ( mMode & FMOD_LOOP_NORMAL ) {
                    if ( mParent->mFrequency > 0.0f ) {
                        mPosition -= mLoopLength;
                    } else {
                        mPosition += mLoopLength;
                    }
                } else if ( mMode & FMOD_LOOP_BIDI ) {
                    if ( mDirection == 0 ) {
                        mDirection = 1;
                    } else {
                        mDirection = 0;
                    }
                    mPosition -= delta;
                }

                if ( mLoopCount >= 0 ) {
                    mLoopCount--;
                }
            }
        } else {
            if ( mPosition >= mSound->mLength ) {
                mPosition = mSound->mLength;
                mFlags &= ~CHANNELREAL_FLAG_PLAYING;
            }
        }
    }

    return FMOD_OK;
} // 0x8032C9E0

/*
====================
ChannelEmulated::isVirtual

An emulated channel is always virtual.
====================
*/
FMOD_RESULT ChannelEmulated::isVirtual( bool *isvirtual ) {
    if ( !isvirtual ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *isvirtual = true;

    return FMOD_OK;
} // 0x8032CC80

/*
====================
ChannelEmulated::getDSPHead

Returns the voice's DSP head.
====================
*/
FMOD_RESULT ChannelEmulated::getDSPHead( DSPI **dsp ) {
    if ( !dsp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *dsp = mDSPHead;

    return FMOD_OK;
} // 0x8032CCA0

/*
====================
ChannelEmulated::setSpeakerLevels

Not used by an emulated voice.
====================
*/
FMOD_RESULT ChannelEmulated::setSpeakerLevels( int speaker, float *levels, int numlevels ) {
    return FMOD_OK;
} // 0x8032CCC0

} // namespace FMOD
