/*
 * fmod_dsp_wavetable.cpp: FMOD::DSPWaveTable, the DSP unit that plays a sample's PCM data into the mix.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_wavetable.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_wavetable.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_real.h>
#include <fmod/fmod_dsp_resampler.h>
#include <fmod/fmod_dsp_wavetable.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_sample_software.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
DSPWaveTable::alloc

Sets the unit up: it starts forwards with no frequency, no pending seek and not finished, and reads
the system's output rate as its target frequency. The unit at the head of the tree defaults to the
system's sound card.
====================
*/
FMOD_RESULT DSPWaveTable::alloc( FMOD_DSP_DESCRIPTION_EX *description ) {
    FMOD_RESULT result;

    result = DSPI::alloc( description );
    if ( result != FMOD_OK ) {
        return result;
    }

    mFrequency = 0.0f;

    result = mSystem->getSoftwareFormat( &mTargetFrequency, NULL, NULL, NULL, NULL, NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    mDirection     = DSPWAVETABLE_SPEEDDIR_FORWARDS;
    mNewPosition   = (unsigned int)-1;
    mDSPFinishTick = (unsigned int)-1;
    mDSPTick       = (unsigned int)-1;

    if ( description->mDSPSoundCard ) {
        mDSPSoundCard = description->mDSPSoundCard;
    } else {
        mDSPSoundCard = mSystem->mDSPSoundCard;
    }

    return FMOD_OK;
} // 0x80364CEC

/*
====================
DSPWaveTable::execute

Mixes outlength samples of the sound into the output buffer: the sound is resampled from its read
position, stepping by the speed, until the buffer is full or the sound ends. At the end it loops
(forwards, or back and forth), goes on with the next subsound of a sentence, or finishes the unit and
silences the rest. A DSP clock start, pause or end that falls inside the block silences the
corresponding part of it.

The sentence walk indexes the list with a counter stepped before the read (Gormiti's DWARF lists only
`count` in that block): the compiler's own pointer induction variable is what gives retail's register
numbering, where a named walking pointer would not.

Quirks kept: the start test compares the clock with the start time plus the block length (not the
reverse), and this unit never stores mDSPTick, which its caller does.
====================
*/
FMOD_RESULT DSPWaveTable::execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                   int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick ) {
    void           *readbuffer;
    SampleSoftware *samplesoftware;
    unsigned int    outlength;
    unsigned int    readoffset;
    unsigned int    soundoffset;
    unsigned int    soundlength;
    FMOD_SINT64P    origspeed;
    unsigned int    starttime;
    unsigned int    endtime;

    *outbuffer = inbuffer;

    mFlags &= ~( DSPI_FLAG_FINISHED | DSPI_FLAG_0x10 );

    if ( mDSPTick != tick ) {
        outlength   = *length;
        readoffset  = 0;
        soundoffset = 0;

        starttime = 0;
        endtime   = 0;
        if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
            FMOD_OS_Time_GetNs( &starttime );
        }

        if ( mNewPosition != (unsigned int)-1 ) {
            mPosition.mHi = mNewPosition;
            mPosition.mLo = 0;
            mNewPosition  = (unsigned int)-1;
        }

        samplesoftware = (SampleSoftware *)mSound;

        if ( mSound->mSubSoundList ) {
            int count;

            soundoffset    = 0;
            samplesoftware = (SampleSoftware *)mSound->mSubSound[mSound->mSubSoundList[0].mIndex];
            count = 0;
            while ( count < mChannel->mSubSoundListCurrent ) {
                if ( samplesoftware ) {
                    soundoffset += samplesoftware->mLength;
                }
                count++;
                samplesoftware = (SampleSoftware *)mSound->mSubSound[mSound->mSubSoundList[count].mIndex];
            }
            if ( !samplesoftware ) {
                return FMOD_ERR_INVALID_PARAM;
            }
        } else {
            if ( !samplesoftware ) {
                return FMOD_ERR_INVALID_PARAM;
            }
        }

        soundlength = samplesoftware->mLength;

        if ( mBuffer ) {
            *outbuffer = mBuffer;
        }

        readbuffer = samplesoftware->mBuffer;
        if ( !readbuffer ) {
            memset( *outbuffer, 0, outlength * sizeof( float ) * mSound->mChannels );

            mDSPFinishTick = tick;
            mFlags |= DSPI_FLAG_FINISHED;

            *outchannels = mSound->mChannels;
            return FMOD_OK;
        }

        origspeed = mSpeed;

        // The unit ends at a DSP clock time inside this block: the rest of the block is silent.
        if ( mDSPClockEnd.mValue ) {
            if ( mDSPClockEnd.mValue < mSystem->mDSPClock.mValue + outlength ) {
                unsigned int diff = (unsigned int)( mSystem->mDSPClock.mValue + outlength - mDSPClockEnd.mValue );

                if ( diff > outlength ) {
                    diff = outlength;
                }

                memset( *outbuffer + mSound->mChannels * ( outlength - diff ), 0, diff * sizeof( float ) * mSound->mChannels );

                mDSPFinishTick = tick;
                mFlags |= DSPI_FLAG_FINISHED;

                outlength -= diff;
            }
        }

        // The unit pauses at a DSP clock time inside this block.
        if ( mDSPClockPause.mValue ) {
            if ( mDSPClockPause.mValue < mSystem->mDSPClock.mValue + outlength ) {
                unsigned int diff = (unsigned int)( mSystem->mDSPClock.mValue + outlength - mDSPClockPause.mValue );

                if ( diff > outlength ) {
                    diff = outlength;
                }

                memset( *outbuffer + mSound->mChannels * ( outlength - diff ), 0, diff * sizeof( float ) * mSound->mChannels );

                setActive( false );
                mDSPClockPause.mValue = 0;

                outlength -= diff;
            }
        }

        // The unit starts at a DSP clock time inside this block: the start of the block is silent.
        if ( mSystem->mDSPClock.mValue < mDSPClockStart.mValue ) {
            if ( mSystem->mDSPClock.mValue < mDSPClockStart.mValue + outlength ) {
                unsigned int diff = (unsigned int)( mDSPClockStart.mValue - mSystem->mDSPClock.mValue );

                if ( diff > outlength ) {
                    diff = outlength;
                }

                if ( diff ) {
                    memset( *outbuffer, 0, diff * sizeof( float ) * mSound->mChannels );

                    readoffset = diff;
                    outlength -= diff;
                }
            }
        }

        while ( outlength ) {
            unsigned int          rlength;
            FMOD_SINT64P          speed;
            FMOD_UINT64P          pos;
            FMOD_RESAMPLER_END    endflag = FMOD_RESAMPLER_END_MIXBUFFER;

            pos = mPosition;
            pos.mHi -= soundoffset;

            speed = origspeed;
            if ( mDirection == DSPWAVETABLE_SPEEDDIR_BACKWARDS ) {
                speed.mValue = -speed.mValue;
            }

            rlength = outlength;

            if ( origspeed.mValue > 0x100 ) {
                FMOD_UINT64P mixesleft;
                bool         eos = false;

                if ( mDirection == DSPWAVETABLE_SPEEDDIR_BACKWARDS ) {
                    if ( ( mChannel->mMode & ( FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI ) ) && mChannel->mLoopCount && pos.mHi >= mChannel->mLoopStart ) {
                        mixesleft.mHi = pos.mHi - mChannel->mLoopStart;
                    } else {
                        mixesleft.mHi = pos.mHi;
                    }
                    mixesleft.mLo = 0;

                    if ( mixesleft.mHi > soundlength ) {
                        eos = true;
                        mixesleft.mValue = pos.mValue;
                    }
                } else {
                    if ( ( mChannel->mMode & ( FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI ) ) && mChannel->mLoopCount ) {
                        mixesleft.mHi = mChannel->mLoopStart + mChannel->mLoopLength;
                    } else {
                        mixesleft.mHi = mSound->mLength;
                    }
                    mixesleft.mLo = 0;

                    if ( mixesleft.mValue > pos.mValue ) {
                        mixesleft.mValue -= pos.mValue;
                    } else {
                        mixesleft.mValue = 0;
                    }

                    if ( pos.mHi + mixesleft.mHi > soundlength ) {
                        mixesleft.mHi = soundlength - pos.mHi;
                        eos = true;
                    }
                }

                // Output samples until the end, rounded up.
                unsigned long long remainder = mixesleft.mValue % origspeed.mValue;

                mixesleft.mValue = mixesleft.mValue / origspeed.mValue;
                if ( remainder ) {
                    mixesleft.mValue++;
                }

                if ( mixesleft.mLo <= outlength ) {
                    rlength = mixesleft.mLo;
                    endflag = FMOD_RESAMPLER_END_SOUND;
                    if ( eos && mSound->mSubSoundList ) {
                        endflag = FMOD_RESAMPLER_END_SUBSOUND;
                    }
                }
            }

            if ( origspeed.mHi == 1 && speed.mLo == 0 ) {
                FMOD_Resampler_NoInterp( *outbuffer + readoffset * mSound->mChannels, rlength, readbuffer,
                                         mSound->mFormat, &pos, &speed, mSound->mChannels );
            } else {
                switch ( mSystem->mResampleMethod ) {
                    case FMOD_DSP_RESAMPLER_NOINTERP:
                        FMOD_Resampler_NoInterp( *outbuffer + readoffset * mSound->mChannels, rlength, readbuffer,
                                                 mSound->mFormat, &pos, &speed, mSound->mChannels );
                        break;
                    case FMOD_DSP_RESAMPLER_LINEAR:
                        FMOD_Resampler_Linear( *outbuffer + readoffset * mSound->mChannels, rlength, readbuffer,
                                               mSound->mFormat, &pos, &speed, mSound->mChannels );
                        break;
                    default:
                        FMOD_Resampler_Linear( *outbuffer + readoffset * mSound->mChannels, rlength, readbuffer,
                                               mSound->mFormat, &pos, &speed, mSound->mChannels );
                        break;
                }
            }

            outlength -= rlength;
            pos.mHi += soundoffset;
            readoffset += rlength;
            mPosition = pos;

            if ( endflag == FMOD_RESAMPLER_END_SOUND ) {
                if ( ( mChannel->mMode & FMOD_LOOP_BIDI ) && mChannel->mLoopCount ) {
                    // Back and forth: turn round at the end.
                    if ( (int)mPosition.mHi < 0 ) {
                        mPosition.mHi = 0;
                    }

                    if ( mDirection == DSPWAVETABLE_SPEEDDIR_FORWARDS ) {
                        mDirection = DSPWAVETABLE_SPEEDDIR_BACKWARDS;
                    } else {
                        mDirection = DSPWAVETABLE_SPEEDDIR_FORWARDS;
                    }
                } else if ( ( mChannel->mMode & FMOD_LOOP_NORMAL ) && mChannel->mLoopCount ) {
                    if ( mDirection == DSPWAVETABLE_SPEEDDIR_BACKWARDS ) {
                        mPosition.mHi += mChannel->mLoopLength;
                        while ( mPosition.mHi < mChannel->mLoopStart ) {
                            mPosition.mHi += mChannel->mLoopLength;
                        }

                        if ( mChannel->mLoopCount > 0 ) {
                            mChannel->mLoopCount--;
                        }

                        if ( mSound->mSubSoundList ) {
                            soundoffset = 0;
                            mChannel->mSubSoundListCurrent = 0;
                            while ( soundoffset + soundlength < mPosition.mHi ) {
                                if ( samplesoftware ) {
                                    soundoffset += samplesoftware->mLength;
                                }
                                mChannel->mSubSoundListCurrent++;
                                samplesoftware = (SampleSoftware *)mSound->mSubSound[mSound->mSubSoundList[mChannel->mSubSoundListCurrent].mIndex];
                                soundlength    = samplesoftware->mLength;
                                readbuffer     = samplesoftware->mBuffer;
                            }
                        }
                    } else {
                        if ( mPosition.mHi >= mChannel->mLoopLength ) {
                            mPosition.mHi -= mChannel->mLoopLength;
                        } else {
                            mPosition.mHi = 0;
                        }
                        while ( mPosition.mHi >= mChannel->mLoopStart + mChannel->mLoopLength ) {
                            mPosition.mHi -= mChannel->mLoopLength;
                        }

                        if ( mChannel->mLoopCount > 0 ) {
                            mChannel->mLoopCount--;
                        }

                        if ( mSound->mSubSoundList ) {
                            soundoffset = 0;
                            mChannel->mSubSoundListCurrent = 0;
                            while ( soundoffset + soundlength < mPosition.mHi ) {
                                if ( samplesoftware ) {
                                    soundoffset += samplesoftware->mLength;
                                }
                                mChannel->mSubSoundListCurrent++;
                                samplesoftware = (SampleSoftware *)mSound->mSubSound[mSound->mSubSoundList[mChannel->mSubSoundListCurrent].mIndex];
                                soundlength    = samplesoftware->mLength;
                                readbuffer     = samplesoftware->mBuffer;
                            }
                        }
                    }
                } else {
                    // No loop: the sound is over and the rest of the block is silent.
                    mPosition.mHi = samplesoftware->mLength;
                    mPosition.mLo = 0;

                    memset( *outbuffer + readoffset * mSound->mChannels, 0, outlength * mSound->mChannels * sizeof( float ) );

                    mDSPFinishTick = tick;
                    mFlags |= DSPI_FLAG_FINISHED;

                    break;
                }
            } else if ( endflag == FMOD_RESAMPLER_END_SUBSOUND ) {
                // On to the next subsound of the sentence.
                if ( mDirection == DSPWAVETABLE_SPEEDDIR_FORWARDS ) {
                    mChannel->mSubSoundListCurrent++;
                    soundoffset += samplesoftware->mLength;
                } else {
                    mChannel->mSubSoundListCurrent--;
                }

                samplesoftware = (SampleSoftware *)mSound->mSubSound[mSound->mSubSoundList[mChannel->mSubSoundListCurrent].mIndex];
                soundlength    = samplesoftware->mLength;
                readbuffer     = samplesoftware->mBuffer;

                if ( mDirection == DSPWAVETABLE_SPEEDDIR_BACKWARDS ) {
                    soundoffset -= soundlength;
                    mPosition.mHi--;
                }
            }
        }

        if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
            FMOD_OS_Time_GetNs( &endtime );
            mCPUUsage = endtime - starttime;
        }
    } else {
        *outbuffer = mBuffer;
    }

    *outchannels = mSound->mChannels;

    return FMOD_OK;
} // 0x80364D90

/*
====================
DSPWaveTable::setFrequency

Sets the playback frequency. A negative frequency plays the sound backwards, otherwise a sound that does
not bounce between its loop points plays forwards. The speed is the frequency over the target frequency
as 32.32 fixed point.
====================
*/
FMOD_RESULT DSPWaveTable::setFrequency( float frequency ) {
    if ( frequency < 0.0f ) {
        frequency  = -frequency;
        mDirection = DSPWAVETABLE_SPEEDDIR_BACKWARDS;
    } else if ( mSound && !( mChannel->mMode & FMOD_LOOP_BIDI ) ) {
        mDirection = DSPWAVETABLE_SPEEDDIR_FORWARDS;
    }

    mFrequency = frequency;

    mSpeed.mValue = (long long)( 4294967296.0f * ( mFrequency / (float)mTargetFrequency ) );

    return FMOD_OK;
} // 0x80365744

/*
====================
DSPWaveTable::setFinished

Marks the unit as not finished, or finished from the next mix of the sound card on (or at once when forced).
====================
*/
FMOD_RESULT DSPWaveTable::setFinished( bool finished, bool force ) {
    if ( !finished ) {
        mDSPFinishTick = (unsigned int)-1;
        mFlags &= ~DSPI_FLAG_FINISHED;
        return FMOD_OK;
    }

    if ( force ) {
        FMOD_OS_CriticalSection_Enter( mSystem->mDSPCrit );
        mDSPFinishTick = 0;
        FMOD_OS_CriticalSection_Leave( mSystem->mDSPCrit );
    }

    if ( mDSPSoundCard ) {
        mDSPFinishTick = mDSPSoundCard->mDSPTick + 1;
    }

    return FMOD_OK;
} // 0x803657F0

/*
====================
DSPWaveTable::getFinished

The unit is finished once the sound card has mixed past its finish tick, or when it has flagged itself.
====================
*/
FMOD_RESULT DSPWaveTable::getFinished( bool *finished ) {
    if ( !mDSPSoundCard ) {
        *finished = true;
        return FMOD_OK;
    }

    if ( mDSPFinishTick >= mDSPSoundCard->mDSPTick && !( mFlags & DSPI_FLAG_FINISHED ) ) {
        *finished = false;
    } else {
        *finished = true;
    }

    return FMOD_OK;
} // 0x80365880

/*
====================
DSPWaveTable::setPositionCallback

The description's setposition callback: a seek takes effect at the next mix, clamped to the sound.
====================
*/
FMOD_RESULT DSPWaveTable::setPositionCallback( FMOD_DSP_STATE *dsp_state, unsigned int position ) {
    DSPWaveTable *wavetable = (DSPWaveTable *)dsp_state;

    if ( !wavetable->mSound ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( position >= wavetable->mSound->mLength ) {
        position = wavetable->mSound->mLength;
    }

    wavetable->mNewPosition = position;

    return FMOD_OK;
} // 0x803658D4

/*
====================
DSPWaveTable::setParameterCallback

The unit has no parameters.
====================
*/
FMOD_RESULT DSPWaveTable::setParameterCallback( FMOD_DSP_STATE *dsp_state, int index, float value ) {
    return FMOD_ERR_INVALID_PARAM;
} // 0x80365910

/*
====================
DSPWaveTable::getParameterCallback

The unit has no parameters.
====================
*/
FMOD_RESULT DSPWaveTable::getParameterCallback( FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr ) {
    return FMOD_ERR_INVALID_PARAM;
} // 0x80365918

/*
====================
DSPWaveTable::resetCallback

The description's reset callback: drops a pending seek.
====================
*/
FMOD_RESULT DSPWaveTable::resetCallback( FMOD_DSP_STATE *dsp_state ) {
    DSPWaveTable *wavetable = (DSPWaveTable *)dsp_state;

    wavetable->mNewPosition = (unsigned int)-1;

    return FMOD_OK;
} // 0x80365920

} // namespace FMOD
