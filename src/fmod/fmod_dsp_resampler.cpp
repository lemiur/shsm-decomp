/*
 * fmod_dsp_resampler.cpp: FMOD::DSPResampler, the DSP unit that reads a sound at a variable speed into the mix.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_resampler.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_resampler.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_dsp_codec.h>
#include <fmod/fmod_dsp_resampler.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
DSPResampler::DSPResampler

Starts with no resample buffer, no speed, no pending position and a two-block fill.
====================
*/
DSPResampler::DSPResampler() {
    mResampleBufferMemory = NULL;
    mResampleBuffer       = NULL;
    mTargetFrequency      = 0;
    mSpeed.mValue         = 0;

    mOverflowLength = 4;

    mResampleBufferPos = 0;
    mResampleFinishPos = (unsigned int)-1;
    mFill              = 2;
    mResamplePosition.mValue = 0;
    mPosition.mValue         = 0;
    mNoDMA                   = NULL;

    memset( &mNoDMAMemory, 0, sizeof( mNoDMAMemory ) );
} // 0x8035F62C

/*
====================
DSPResampler::release

Frees the resample buffer and, when asked, the unit itself.
====================
*/
FMOD_RESULT DSPResampler::release( bool freethis ) {
    DSPI::release( false );

    if ( mResampleBufferMemory ) {
#line 92
        FMOD_Memory_Free( mResampleBufferMemory );
        mResampleBufferMemory = NULL;
    }

    if ( freethis ) {
#line 115
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x8035F6C0

/*
====================
DSPResampler::alloc

Sets the unit up: the block length comes from the description or the system, and the buffer is sized
for two blocks plus the overflow the interpolator reads past the end. A raw codec's buffer sits right
after the codec object; every other unit allocates its own. The unit at the head of the tree defaults
to the system's sound card.
====================
*/
FMOD_RESULT DSPResampler::alloc( FMOD_DSP_DESCRIPTION_EX *description ) {
    FMOD_RESULT  result;
    int          channels;
    unsigned int bytespersample = 0;

    result = DSPI::alloc( description );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = mSystem->getSoftwareFormat( &mTargetFrequency, NULL, NULL, NULL, NULL, NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    mNoDMA = &mNoDMAMemory;

    if ( description->mResamplerBlockLength ) {
        mResampleBlockLength = description->mResamplerBlockLength;
        channels             = description->channels;
    } else {
        result = mSystem->getDSPBufferSize( &mResampleBlockLength, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        channels = mSystem->mMaxInputChannels;
    }

    mResampleBufferLength = mResampleBlockLength * 2;

    if ( mDescription.mFormat == FMOD_SOUND_FORMAT_NONE ) {
        mDescription.mFormat = FMOD_SOUND_FORMAT_PCMFLOAT;
    }

    SoundI::getBytesFromSamples( 1, &bytespersample, channels, mDescription.mFormat );

    if ( mDescription.mCategory == FMOD_DSP_CATEGORY_DSPCODECRAW ) {
        mResampleBuffer = (void *)( ( (unsigned int)( (DSPCodec *)this + 1 ) + 15 ) & ~15 );
    } else {
#line 231
        mResampleBufferMemory = FMOD_Memory_Calloc( bytespersample * ( mResampleBufferLength + mOverflowLength * 4 ) + 16 );
        if ( !mResampleBufferMemory ) {
            return FMOD_ERR_MEMORY;
        }

        mResampleBuffer = (void *)( ( (unsigned int)mResampleBufferMemory + 15 ) & ~15 );
    }

    mResampleBuffer = (char *)mResampleBuffer + mOverflowLength * bytespersample;
    mResampleBuffer = (void *)( ( (unsigned int)mResampleBuffer + 15 ) & ~15 );

    mResampleBufferPos = 0;
    mResampleFinishPos = (unsigned int)-1;
    mFill              = 2;
    mPosition.mValue         = 0;
    mResamplePosition.mValue = 0;
    mDSPTick                 = 0;
    mNoDMA->mDSPFinishTick   = 0;

    if ( description->mDSPSoundCard ) {
        mDSPSoundCard = description->mDSPSoundCard;
    } else {
        mDSPSoundCard = mSystem->mDSPSoundCard;
    }

    return FMOD_OK;
} // 0x8035F758

/*
====================
DSPResampler::execute

Fills the output with the sound read through the resample buffer. The buffer is topped up block by block
from the unit's read callback (silence once the input has run dry), and the resampler then steps through
it at the speed to produce len samples. A DSP clock end, pause or start that falls inside the block
silences the matching part of it. Afterwards the sound position is advanced (and wrapped at the loop
points of the sound).

Quirks kept: the pause test adds the read position to the clock, the end test does not; the loop length is
clamped to the sound twice on the forward path.

Not exact (99.56%, wall: register/schedule): in the two position-wrap tails retail reloads mNoDMA after the
loop-flag test where this source reuses the first load (register numbering of the tail only).
a loopstart local on the backward path was kept (it fixed the head of that branch); the tail is unresolved.
The reload is a const-qualified read: reading mNoDMA through a pointer to const DSPResampler in either the
tail condition or the tail body (not in the heads) is exact, so one of the two tail reads is a separate
expression to the common-subexpression pass. No spelling for it is evidenced (Gormiti's DWARF lists no inline
member of DSPResampler or DSPResampler_NODMA); nested ifs for the tails change nothing.
====================
*/
FMOD_RESULT DSPResampler::execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                   int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick ) {
    FMOD_RESULT  result;
    float       *output_buffer;
    int          output_channels;
    unsigned int bytespersample;

    if ( tick >= mNoDMA->mDSPFinishTick ) {
        mFlags |= DSPI_FLAG_FINISHED;
        return FMOD_OK;
    }

    output_channels = mDescription.channels;

    mFlags &= ~( DSPI_FLAG_FINISHED | DSPI_FLAG_0x10 );

    SoundI::getBytesFromSamples( 1, &bytespersample, output_channels, mDescription.mFormat );

    if ( mDSPTick != tick ) {
        float       *readbuffer;
        int          len, inlength;
        int          readoffset;
        FMOD_SINT64P speed;
        unsigned int starttime;
        unsigned int endtime;

        inlength   = *length;
        readoffset = 0;

        starttime = 0;
        endtime   = 0;
        if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
            FMOD_OS_Time_GetNs( &starttime );
        }

        if ( mBuffer ) {
            readbuffer = mBuffer;
        } else {
            readbuffer = inbuffer;
        }

        speed.mValue = mSpeed.mValue;

        if ( mNoDMA ) {
            // The unit ends at a DSP clock time inside this block: the rest of the block is silent.
            if ( mNoDMA->mDSPClockEnd.mValue && mNoDMA->mDSPClockEnd.mValue < mSystem->mDSPClock.mValue + inlength ) {
                unsigned int diff = (unsigned int)( mSystem->mDSPClock.mValue + inlength - mNoDMA->mDSPClockEnd.mValue );

                if ( diff > inlength ) {
                    diff = inlength;
                }

                memset( readbuffer + mDescription.channels * ( inlength - diff ), 0, diff * sizeof( float ) * mDescription.channels );

                inlength -= diff;
                mNoDMA->mDSPFinishTick = tick;
                mFlags |= DSPI_FLAG_FINISHED;
            }

            // The unit pauses at a DSP clock time inside this block.
            if ( mNoDMA->mDSPClockPause.mValue && mNoDMA->mDSPClockPause.mValue < mSystem->mDSPClock.mValue + mReadPosition + inlength ) {
                unsigned int diff = (unsigned int)( mSystem->mDSPClock.mValue + mReadPosition + inlength - mNoDMA->mDSPClockPause.mValue );

                if ( diff > inlength ) {
                    diff = inlength;
                }

                memset( readbuffer + ( mReadPosition + mDescription.channels * ( inlength - diff ) ), 0, diff * sizeof( float ) * mDescription.channels );

                inlength -= diff;
                setActive( false );
                mNoDMA->mDSPClockPause.mValue = 0;
            }

            // The unit starts at a DSP clock time inside this block: the start of the block is silent.
            if ( mNoDMA->mDSPClockStart.mValue > mSystem->mDSPClock.mValue && mNoDMA->mDSPClockStart.mValue + inlength > mSystem->mDSPClock.mValue ) {
                unsigned int diff = (unsigned int)( mNoDMA->mDSPClockStart.mValue - mSystem->mDSPClock.mValue );

                if ( diff > inlength ) {
                    diff = inlength;
                }

                if ( diff ) {
                    memset( readbuffer, 0, diff * sizeof( float ) * mDescription.channels );

                    inlength -= diff;
                    readoffset = diff;
                }
            }
        }

        len = inlength;
        while ( len ) {
            unsigned int       rlength;
            FMOD_RESAMPLER_END endflag = FMOD_RESAMPLER_END_MIXBUFFER;

            while ( mFill ) {
                unsigned int readpos      = mResampleBufferPos;
                unsigned int readlen      = mResampleBlockLength;
                void        *resamplebuff = (char *)mResampleBuffer + readpos * bytespersample;

                if ( (int)mResampleFinishPos == -1 ) {
                    instance = (FMOD_DSP *)this;
                    result = mDescription.read( this, NULL, (float *)resamplebuff, readlen, mDescription.channels, mDescription.channels );
                } else {
                    memset( resamplebuff, 0, readlen * mDescription.channels * 2 );
                    result = FMOD_OK;
                }

                mResampleBufferPos += readlen;
                if ( mResampleBufferPos >= mResampleBufferLength ) {
                    mResampleBufferPos = 0;
                }

                if ( (int)mResampleFinishPos == -1 && result != FMOD_OK ) {
                    mResampleFinishPos = mResampleBufferPos;
                    if ( mResampleFinishPos == 0 ) {
                        mResampleFinishPos = mResampleBufferLength;
                    }
                }

                if ( readpos == 0 ) {
                    unsigned int count;

                    for ( count = 0; count < bytespersample * ( mOverflowLength * 2 ); count++ ) {
                        ( (char *)mResampleBuffer )[count + mResampleBufferLength * bytespersample] = ( (char *)mResampleBuffer )[count];
                    }
                }

                mFill--;
            }

            rlength = len;

            if ( speed.mValue > 0x100 ) {
                FMOD_UINT64P mixesleft;
                int          nextpos;
                bool         endofsound = false;

                nextpos = mResamplePosition.mHi - mOverflowLength;
                nextpos = nextpos / (int)mResampleBlockLength;
                nextpos++;
                nextpos *= mResampleBlockLength;
                nextpos += mOverflowLength;

                mixesleft.mHi = nextpos;
                mixesleft.mLo = 0;
                mixesleft.mValue -= mResamplePosition.mValue;

                if ( mResampleFinishPos != (unsigned int)-1 ) {
                    FMOD_UINT64P mixeslefttofinish;

                    mixeslefttofinish.mHi = mResampleFinishPos;
                    mixeslefttofinish.mLo = 0;
                    mixeslefttofinish.mValue -= mResamplePosition.mValue;

                    if ( mixeslefttofinish.mValue <= mixesleft.mValue ) {
                        mixesleft.mValue = mixeslefttofinish.mValue;
                        endofsound       = true;
                    }
                }

                // Output samples until then, rounded up.
                unsigned long long remainder = mixesleft.mValue % speed.mValue;

                mixesleft.mValue = mixesleft.mValue / speed.mValue;
                if ( remainder ) {
                    mixesleft.mValue++;
                }

                if ( mixesleft.mValue <= (unsigned int)len ) {
                    endflag = FMOD_RESAMPLER_END_RESAMPLEBUFFER;
                    if ( endofsound ) {
                        endflag = FMOD_RESAMPLER_END_SOUND;
                    }
                    rlength = mixesleft.mLo;
                }
            }

            if ( speed.mHi == 1 && speed.mLo == 0 ) {
                FMOD_Resampler_NoInterp( readbuffer + readoffset * mDescription.channels, rlength, mResampleBuffer,
                                         mDescription.mFormat, &mResamplePosition, &speed, mDescription.channels );
            } else {
                switch ( mSystem->mResampleMethod ) {
                    case FMOD_DSP_RESAMPLER_LINEAR:
                        FMOD_Resampler_Linear( readbuffer + readoffset * mDescription.channels, rlength, mResampleBuffer,
                                               mDescription.mFormat, &mResamplePosition, &speed, mDescription.channels );
                        break;
                    default:
                        FMOD_Resampler_Linear( readbuffer + readoffset * mDescription.channels, rlength, mResampleBuffer,
                                               mDescription.mFormat, &mResamplePosition, &speed, mDescription.channels );
                        break;
                }
            }

            if ( mResamplePosition.mHi >= mResampleBufferLength + mOverflowLength ) {
                mResamplePosition.mHi -= mResampleBufferLength;
            }

            len -= rlength;
            readoffset += rlength;

            if ( endflag == FMOD_RESAMPLER_END_SOUND ) {
                memset( readbuffer + readoffset * mDescription.channels, 0, len * mDescription.channels * sizeof( float ) );

                mNoDMA->mDSPFinishTick = tick;
                mFlags |= DSPI_FLAG_FINISHED;
                break;
            } else if ( endflag == FMOD_RESAMPLER_END_RESAMPLEBUFFER ) {
                mFill++;
            }
        }

        if ( mLength < mNoDMA->mLoopStart + mNoDMA->mLoopLength ) {
            mNoDMA->mLoopLength = mLength - mNoDMA->mLoopStart;
        }

        if ( mDirection == DSPRESAMPLER_SPEEDDIR_BACKWARDS ) {
            int loopstart;

            if ( ( mNoDMA->mMode & FMOD_LOOP_NORMAL ) && mLoopCount ) {
                loopstart = mNoDMA->mLoopStart;
            } else {
                loopstart = 0;
            }

            mPosition.mValue += -speed.mValue * inlength;

            if ( (int)mPosition.mHi <= loopstart ) {
                if ( ( ( mNoDMA->mMode & FMOD_LOOP_NORMAL ) && mLoopCount ) || mLength == (unsigned int)-1 ) {
                    mPosition.mHi += mNoDMA->mLoopLength;
                } else {
                    mPosition.mHi = 0;
                }
            }
        } else {
            FMOD_UINT64P endpoint;

            if ( mLength < mNoDMA->mLoopStart + mNoDMA->mLoopLength ) {
                mNoDMA->mLoopLength = mLength - mNoDMA->mLoopStart;
            }

            if ( ( mNoDMA->mMode & FMOD_LOOP_NORMAL ) && mLoopCount ) {
                endpoint.mHi = mNoDMA->mLoopStart + mNoDMA->mLoopLength - 1;
                endpoint.mLo = (unsigned int)-1;
            } else {
                endpoint.mHi = mLength - 1;
                endpoint.mLo = (unsigned int)-1;
            }

            mPosition.mValue += speed.mValue * inlength;

            if ( mPosition.mValue > endpoint.mValue ) {
                if ( ( ( mNoDMA->mMode & FMOD_LOOP_NORMAL ) && mLoopCount ) || mLength == (unsigned int)-1 ) {
                    mPosition.mHi -= mNoDMA->mLoopLength;
                } else {
                    mPosition.mHi = mLength;
                }
            }
        }

        output_buffer   = readbuffer;
        output_channels = mDescription.channels;

        if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
            FMOD_OS_Time_GetNs( &endtime );
            mCPUUsage = endtime - starttime;
        }
    } else {
        mFlags &= ~DSPI_FLAG_0x10;
        output_buffer   = mBuffer;
    }

    *outbuffer   = output_buffer;
    *outchannels = output_channels;

    return FMOD_OK;
} // 0x8035FA04

/*
====================
DSPResampler::addInput

A resampler is a source: nothing can be connected to its input.
====================
*/
FMOD_RESULT DSPResampler::addInput( DSPI *target ) {
    return FMOD_ERR_DSP_CONNECTION;
} // 0x8036036C

/*
====================
DSPResampler::setFrequency

Sets the playback frequency. A negative frequency plays the sound backwards. The speed is the
frequency over the target frequency as 32.32 fixed point.
====================
*/
FMOD_RESULT DSPResampler::setFrequency( float frequency ) {
    if ( frequency < 0.0f ) {
        frequency  = -frequency;
        mDirection = DSPRESAMPLER_SPEEDDIR_BACKWARDS;
    } else {
        mDirection = DSPRESAMPLER_SPEEDDIR_FORWARDS;
    }

    mFrequency = frequency;

    mSpeed.mValue = (long long)( 4294967296.0f * ( mFrequency / (float)mTargetFrequency ) );

    return FMOD_OK;
} // 0x80360374

/*
====================
DSPResampler::setPosition

Seeks: the buffer is emptied and refilled from the new position.
====================
*/
FMOD_RESULT DSPResampler::setPosition( unsigned int position, bool processinputs ) {
    FMOD_RESULT result;

    result = DSPI::setPosition( position, processinputs );
    if ( result != FMOD_OK ) {
        return result;
    }

    mResampleBufferPos = 0;
    mResampleFinishPos = (unsigned int)-1;
    mFill              = 2;
    mResamplePosition.mValue = 0;
    mPosition.mHi            = position;
    mPosition.mLo            = 0;

    return FMOD_OK;
} // 0x80360404

/*
====================
DSPResampler::setFinished

Marks the unit as not finished, or finished from the next mix of the sound card on (or at once when forced).
====================
*/
FMOD_RESULT DSPResampler::setFinished( bool finished, bool force ) {
    if ( !finished ) {
        mNoDMA->mDSPFinishTick = (unsigned int)-1;
        mFlags &= ~DSPI_FLAG_FINISHED;
        return FMOD_OK;
    }

    if ( force ) {
        FMOD_OS_CriticalSection_Enter( mSystem->mDSPCrit );
        mNoDMA->mDSPFinishTick = 0;
        FMOD_OS_CriticalSection_Leave( mSystem->mDSPCrit );
    } else if ( mDSPSoundCard ) {
        mNoDMA->mDSPFinishTick = mDSPSoundCard->mDSPTick + 1;
    }

    return FMOD_OK;
} // 0x80360474

/*
====================
DSPResampler::getFinished

The unit is finished once the sound card has mixed past its finish tick, or when it has flagged itself
(flag 0x100 holds it unfinished).
====================
*/
FMOD_RESULT DSPResampler::getFinished( bool *finished ) {
    if ( !mDSPSoundCard ) {
        *finished = true;
        return FMOD_OK;
    }

    if ( ( mNoDMA->mDSPFinishTick >= mDSPSoundCard->mDSPTick && !( mFlags & DSPI_FLAG_FINISHED ) ) ||
         ( mFlags & DSPI_FLAG_0x100 ) ) {
        *finished = false;
    } else {
        *finished = true;
    }

    return FMOD_OK;
} // 0x80360514

} // namespace FMOD
