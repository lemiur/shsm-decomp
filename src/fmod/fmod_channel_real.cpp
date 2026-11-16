// Unit name and boundary: Gormiti link map (fmodex.a fmod_channel_real.obj)
/*
 * fmod_channel_real.cpp: FMOD::ChannelReal, the base class of every real voice (hardware, software
 * or emulated). Names and layout: Gormiti link map and DWARF (fmodex.a fmod_channel_real.obj).
 */

#include <fmod/fmod.h>
#include <fmod/fmod_channel_real.h>
#include <fmod/fmod_channeli.h>
#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_reverbi.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_speakerlevels_pool.h>
#include <fmod/fmod_systemi.h>
#include <math.h>
#include <stddef.h>

#define CHANNELREAL_FLAG_0x80 0x00000080

namespace FMOD {

/*
====================
ChannelReal::ChannelReal

Starts unbound with default loop count and frequency range.
====================
*/
ChannelReal::ChannelReal() {
    mSound        = NULL;
    mSystem       = NULL;
    mOutput       = NULL;
    mPool         = NULL;
    mLoopCount    = -1;
    mMinFrequency = 100.0f;
    mMaxFrequency = 1000000.0f;
} // 0x8032CD28

/*
====================
ChannelReal::init

Resets playback, filter and speaker state and binds the owning system, output and channel index.
====================
*/
FMOD_RESULT ChannelReal::init( int index, SystemI *system, Output *output, DSPI *dspmixtarget ) {
    mSound             = NULL;
    mFlags             = 0;
    mMode              = 0;
    mPosition          = 0;
    mDirection         = 0;
    mLoopCount         = -1;
    mOutput            = output;
    mSystem            = system;
    mIndex             = index;
    mControllerSpeaker = 0;
    mBiquadActive      = false;
    mBiquadB0          = 0;
    mBiquadB1          = 0;
    mBiquadB2          = 0;
    mBiquadA1          = 0;
    mBiquadA2          = 0;
    mLPFCutoff         = -1;

    return FMOD_OK;
} // 0x8032CD64

/*
====================
ChannelReal::close

Base close stops the channel.
====================
*/
FMOD_RESULT ChannelReal::close() {
    return stop();
} // 0x8032CDB8

/*
====================
ChannelReal::alloc

Resets position and accounts for the voice in its pool.
====================
*/
FMOD_RESULT ChannelReal::alloc() {
    mPosition = 0;

    if ( mPool ) {
        mPool->mChannelsUsed++;
    }

    return FMOD_OK;
} // 0x8032CDC8

/*
====================
ChannelReal::alloc

DSP-backed allocation overload: accounts for the voice in its pool and resets position.
====================
*/
FMOD_RESULT ChannelReal::alloc( DSPI *dsp ) {
    if ( mPool ) {
        mPool->mChannelsUsed++;
    }

    mPosition = 0;

    return FMOD_OK;
} // 0x8032CDF0

/*
====================
ChannelReal::set2DFreqVolumePanFor3D

No-op base hook for 2D parameter updates on 3D channels.
====================
*/
FMOD_RESULT ChannelReal::set2DFreqVolumePanFor3D() {
    return FMOD_OK;
} // 0x8032CE18

/*
====================
ChannelReal::update

No-op base per-tick update.
====================
*/
FMOD_RESULT ChannelReal::update( int delta ) {
    return FMOD_OK;
} // 0x8032CE20

/*
====================
ChannelReal::updateStream

No-op base stream update.
====================
*/
FMOD_RESULT ChannelReal::updateStream() {
    return FMOD_OK;
} // 0x8032CE28

/*
====================
ChannelReal::start

No-op base start.
====================
*/
FMOD_RESULT ChannelReal::start() {
    return FMOD_OK;
} // 0x8032CE30

/*
====================
ChannelReal::stop

Releases the channel back to its pool and resets filter and playback flags.
====================
*/
FMOD_RESULT ChannelReal::stop() {
    if ( mPool ) {
        mPool->mChannelsUsed--;
    }

    mFlags &= ~( CHANNELREAL_FLAG_PLAYING | CHANNELREAL_FLAG_PAUSED | CHANNELREAL_FLAG_0x10 );
    mFlags |= CHANNELREAL_FLAG_0x80;

    mControllerSpeaker = 0;

    mBiquadActive = false;
    mBiquadB0     = 0;
    mBiquadB1     = 0;
    mBiquadB2     = 0;
    mBiquadA1     = 0;
    mBiquadA2     = 0;
    mLPFCutoff    = -1;

    return FMOD_OK;
} // 0x8032CE38

/*
====================
ChannelReal::setPaused

Updates the channel's paused flag.
====================
*/
FMOD_RESULT ChannelReal::setPaused( bool paused ) {
    if ( paused ) {
        mFlags |= CHANNELREAL_FLAG_PAUSED;
    } else {
        mFlags &= ~CHANNELREAL_FLAG_PAUSED;
    }

    return FMOD_OK;
} // 0x8032CE90

/*
====================
ChannelReal::getPaused

Returns whether the channel's paused flag is set.
====================
*/
FMOD_RESULT ChannelReal::getPaused( bool *paused ) {
    if ( !paused ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *paused = ( mFlags & CHANNELREAL_FLAG_PAUSED ) ? true : false;

    return FMOD_OK;
} // 0x8032CEBC

/*
====================
ChannelReal::setVolume

No-op base volume setter.
====================
*/
FMOD_RESULT ChannelReal::setVolume( float volume ) {
    return FMOD_OK;
} // 0x8032CEE0

/*
====================
ChannelReal::setFrequency

No-op base frequency setter.
====================
*/
FMOD_RESULT ChannelReal::setFrequency( float frequency ) {
    return FMOD_OK;
} // 0x8032CEE8

/*
====================
ChannelReal::setPan

No-op base pan setter.
====================
*/
FMOD_RESULT ChannelReal::setPan( float pan, float fbpan ) {
    return FMOD_OK;
} // 0x8032CEF0

/*
====================
ChannelReal::setDSPClockDelay

No-op base DSP clock delay setter.
====================
*/
FMOD_RESULT ChannelReal::setDSPClockDelay() {
    return FMOD_OK;
} // 0x8032CEF8

/*
====================
ChannelReal::setSpeakerMix

Converts 8-speaker mix levels into pan and volume for the base voice or its subchannel.
====================
*/
FMOD_RESULT ChannelReal::setSpeakerMix( float frontleft, float frontright, float center, float lfe,
                                        float backleft, float backright, float sideleft, float sideright ) {
    int numsubchannels;

    if ( mSound ) {
        if ( mSound->mSubSampleParent ) {
            numsubchannels = mSound->mSubSampleParent->mChannels;
        } else {
            numsubchannels = mSound->mChannels;
        }
    } else if ( mDSP ) {
        numsubchannels = mDSP->mDescription.channels;
    } else {
        numsubchannels = 0;
    }

    if ( numsubchannels > 1 ) {
        if ( mSound->mDefaultChannelMask & SOUNDI_CHANNELMASK_ALLMONO ) {
            setPan( 0.0f, 1.0f );
            setVolume( center * mParent->mVolume );
        } else if ( mSound->mDefaultChannelMask & SOUNDI_CHANNELMASK_ALLSTEREO ) {
            switch ( mSubChannelIndex % 2 ) {
                case 0:
                    setPan( -1.0f, 1.0f );
                    setVolume( frontleft * mParent->mVolume );
                    break;
                case 1:
                    setPan( 1.0f, 1.0f );
                    setVolume( frontright * mParent->mVolume );
                    break;
            }
        } else {
            switch ( mSubChannelIndex ) {
                case 0:
                    setPan( -1.0f, 1.0f );
                    setVolume( frontleft * mParent->mVolume );
                    break;
                case 1:
                    setPan( 1.0f, 1.0f );
                    setVolume( frontright * mParent->mVolume );
                    break;
                case 2:
                    setPan( 0.0f, 1.0f );
                    setVolume( center * mParent->mVolume );
                    break;
                case 3:
                    setPan( 0.0f, 0.0f );
                    setVolume( lfe * mParent->mVolume );
                    break;
                case 4:
                    setPan( -1.0f, -1.0f );
                    setVolume( backleft * mParent->mVolume );
                    break;
                case 5:
                    setPan( 1.0f, -1.0f );
                    setVolume( backright * mParent->mVolume );
                    break;
                case 6:
                    setPan( -1.0f, 0.0f );
                    setVolume( sideleft * mParent->mVolume );
                    break;
                case 7:
                    setPan( 1.0f, 0.0f );
                    setVolume( sideright * mParent->mVolume );
                    break;
            }
        }
    } else {
        float vol   = 0.0f;
        float lrpan = 0.0f;
        float fbpan = 0.0f;

        vol += frontleft;
        vol += frontright;
        vol += center;
        vol += lfe;
        vol += backleft;
        vol += backright;
        vol += sideleft;
        vol += sideright;

        lrpan -= frontleft;
        lrpan += frontright;
        lrpan -= backleft;
        lrpan += backright;
        lrpan -= sideleft;
        lrpan += sideright;

        fbpan += frontleft;
        fbpan += frontright;
        fbpan += center;
        fbpan -= backleft;
        fbpan -= backright;

        vol = ( vol > 1.0f ) ? 1.0f : vol;
        setVolume( vol * mParent->mVolume );
        setPan( lrpan < -1.0f ? -1.0f : ( lrpan > 1.0f ? 1.0f : lrpan ),
                fbpan < -1.0f ? -1.0f : ( fbpan > 1.0f ? 1.0f : fbpan ) );
    }

    return FMOD_OK;
} // 0x8032CF00

/*
====================
ChannelReal::setSpeakerLevels

Stores clamped per-speaker input levels in the parent channel's matrix and refreshes pan/volume.
====================
*/
FMOD_RESULT ChannelReal::setSpeakerLevels( int speaker, float *levels, int numlevels ) {
    int count;

    if ( !mParent ) {
        return FMOD_OK;
    }

    if ( !mParent->mLevels ) {
        mSystem->mSpeakerLevelsPool.alloc( &mParent->mLevels );
        if ( !mParent->mLevels ) {
            return FMOD_ERR_MEMORY;
        }
    }

    for ( count = 0; count < numlevels; count++ ) {
        float volume = *levels;

        if ( volume < 0.0f ) {
            volume = 0.0f;
        }
        if ( volume > 1.0f ) {
            volume = 1.0f;
        }

        levels++;

        mParent->mLevels[speaker * mSystem->mMaxInputChannels + count] = volume;
    }

    return updateSpeakerLevels( mParent->mVolume );
} // 0x8032D418

/*
====================
ChannelReal::updateSpeakerLevels

Collapses the parent channel's speaker matrix row into volume and 2D pan.

`system` and `levels` (names invented) keep `mSystem` (r31) and `mParent->mLevels` (r30) live across
the `sqrt` call; Gormiti's DWARF does not record them separately, though retail and Gormiti both hold
them in r31 and r30 across `sqrt`.
====================
*/
FMOD_RESULT ChannelReal::updateSpeakerLevels( float volume ) {
    if ( mParent ) {
        SystemI *system;
        float   *levels = mParent->mLevels;

        if ( levels ) {
            system = mSystem;
            int   count;
            float vol   = 0.0f;
            float pan   = 0.0f;
            float fbpan = 0.0f;
            float sumsq = 0.0f;
            float sqrtsumsq;
            int   outputchannels = system->mMaxOutputChannels;

            if ( system->mSpeakerMode == FMOD_SPEAKERMODE_PROLOGIC ) {
                outputchannels = 6;
            }

            for ( count = 0; count < outputchannels; count++ ) {
                if ( count != 2 && count != 3 ) {
                    float val = (float)fabs( levels[count * system->mMaxInputChannels + mSubChannelIndex] );
                    sumsq += val * val;
                }
            }

            sqrtsumsq = (float)sqrt( sumsq );

            for ( count = 0; count < outputchannels; count++ ) {
                float val = (float)fabs( levels[count * system->mMaxInputChannels + mSubChannelIndex] );

                if ( sqrtsumsq ) {
                    val /= sqrtsumsq;
                } else {
                    val = 0.0f;
                }

                if ( count == 0 || count == 4 || count == 6 ) {
                    pan -= val;
                } else if ( count == 1 || count == 5 || count == 7 ) {
                    pan += val;
                }

                if ( count == 0 || count == 1 ) {
                    fbpan += val;
                } else if ( count == 4 || count == 5 ) {
                    fbpan -= val;
                }
            }

            vol = ( sqrtsumsq > 1.0f ) ? 1.0f : sqrtsumsq;
            setVolume( vol * volume );
            setPan( pan < -1.0f ? -1.0f : ( pan > 1.0f ? 1.0f : pan ),
                    fbpan < -1.0f ? -1.0f : ( fbpan > 1.0f ? 1.0f : fbpan ) );
        }
    }

    return FMOD_OK;
} // 0x8032D52C

/*
====================
ChannelReal::setPosition

Converts position in ms, PCM samples or PCM bytes to clamped PCM samples.
====================
*/
FMOD_RESULT ChannelReal::setPosition( unsigned int position, unsigned int postype ) {
    if ( postype != FMOD_TIMEUNIT_MS && postype != FMOD_TIMEUNIT_PCM && postype != FMOD_TIMEUNIT_PCMBYTES ) {
        return FMOD_ERR_FORMAT;
    }

    if ( mSound ) {
        FMOD_RESULT  result;
        unsigned int lengthpcm;
        unsigned int pcm = 0;

        result = mSound->getLength( &lengthpcm, FMOD_TIMEUNIT_PCM );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( postype == FMOD_TIMEUNIT_PCM ) {
            pcm = position;
        } else if ( postype == FMOD_TIMEUNIT_PCMBYTES ) {
            mSound->getSamplesFromBytes( position, &pcm );
        } else if ( postype == FMOD_TIMEUNIT_MS ) {
            pcm = (unsigned int)( ( (float)position / 1000.0f ) * mSound->mDefaultFrequency );
        }

        if ( pcm >= lengthpcm ) {
            pcm = lengthpcm;
        }

        mPosition = pcm;
    }

    return FMOD_OK;
} // 0x8032D778

/*
====================
ChannelReal::getPosition

Converts the current PCM position to the requested time unit or sentence subsound position.
====================
*/
FMOD_RESULT ChannelReal::getPosition( unsigned int *position, unsigned int postype ) {
    bool getsubsoundtime = false;

    if ( !position ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !mSound ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    postype &= ~FMOD_TIMEUNIT_BUFFERED;

    if ( postype == FMOD_TIMEUNIT_SENTENCE_MS ) {
        postype         = FMOD_TIMEUNIT_MS;
        getsubsoundtime = true;
    } else if ( postype == FMOD_TIMEUNIT_SENTENCE_PCM ) {
        postype         = FMOD_TIMEUNIT_PCM;
        getsubsoundtime = true;
    } else if ( postype == FMOD_TIMEUNIT_SENTENCE_PCMBYTES ) {
        postype         = FMOD_TIMEUNIT_PCMBYTES;
        getsubsoundtime = true;
    } else if ( postype == FMOD_TIMEUNIT_SENTENCE_SOUND || postype == FMOD_TIMEUNIT_SENTENCE_SUBSOUND ) {
        getsubsoundtime = true;
    }

    if ( getsubsoundtime && !mSound->mSubSoundList ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( postype == FMOD_TIMEUNIT_MS || postype == FMOD_TIMEUNIT_PCM || postype == FMOD_TIMEUNIT_PCMBYTES
         || postype == FMOD_TIMEUNIT_SENTENCE_SOUND || postype == FMOD_TIMEUNIT_SENTENCE_SUBSOUND ) {
        int          currentsubsoundid = 0;
        int          currentsentenceid = 0;
        unsigned int pcmcurrent        = mPosition;

        if ( getsubsoundtime ) {
            for ( currentsubsoundid = 0; currentsubsoundid < mSound->mSubSoundListNum; currentsubsoundid++ ) {
                SoundI *sound = mSound->mSubSound[mSound->mSubSoundList[currentsubsoundid].mIndex];

                if ( !sound || pcmcurrent < sound->mLength ) {
                    break;
                }

                pcmcurrent -= sound->mLength;
                currentsentenceid++;
            }
        }

        if ( postype == FMOD_TIMEUNIT_SENTENCE_SOUND ) {
            *position = currentsentenceid;
        } else if ( postype == FMOD_TIMEUNIT_SENTENCE_SUBSOUND ) {
            *position = currentsubsoundid;
        } else if ( postype == FMOD_TIMEUNIT_PCM ) {
            *position = pcmcurrent;
        } else if ( postype == FMOD_TIMEUNIT_PCMBYTES ) {
            SoundI::getBytesFromSamples( pcmcurrent, position, mSound->mChannels, mSound->mFormat );
        } else if ( postype == FMOD_TIMEUNIT_MS ) {
            *position = (unsigned int)( ( (float)pcmcurrent / mSound->mDefaultFrequency ) * 1000.0f );
        }
    } else {
        return FMOD_ERR_FORMAT;
    }

    return FMOD_OK;
} // 0x8032D9B0

/*
====================
ChannelReal::setLoopPoints

Validates and stores the loop start and length in samples.
====================
*/
FMOD_RESULT ChannelReal::setLoopPoints( unsigned int loopstart, unsigned int looplength ) {
    if ( !mSound ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( loopstart >= mSound->mLength ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( loopstart + looplength > mSound->mLength ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mLoopStart  = loopstart;
    mLoopLength = looplength;

    return FMOD_OK;
} // 0x8032DD18

/*
====================
ChannelReal::setLoopCount

Stores the remaining loop count.
====================
*/
FMOD_RESULT ChannelReal::setLoopCount( int loopcount ) {
    mLoopCount = loopcount;
    return FMOD_OK;
} // 0x8032DD64

/*
====================
ChannelReal::setLowPassGain

No-op base low-pass gain setter.
====================
*/
FMOD_RESULT ChannelReal::setLowPassGain( float gain ) {
    return FMOD_OK;
} // 0x8032DD70

/*
====================
ChannelReal::set3DAttributes

No-op base 3D attributes setter.
====================
*/
FMOD_RESULT ChannelReal::set3DAttributes() {
    return FMOD_OK;
} // 0x8032DD78

/*
====================
ChannelReal::set3DMinMaxDistance

No-op base 3D min/max distance setter.
====================
*/
FMOD_RESULT ChannelReal::set3DMinMaxDistance() {
    return FMOD_OK;
} // 0x8032DD80

/*
====================
ChannelReal::set3DOcclusion

Refreshes the channel volume when occlusion changes.
====================
*/
FMOD_RESULT ChannelReal::set3DOcclusion( float directocclusion, float reverbocclusion ) {
    if ( mParent ) {
        return setVolume( mParent->mVolume );
    }

    return FMOD_OK;
} // 0x8032DD88

/*
====================
ChannelReal::setReverbProperties

Applies per-instance reverb channel properties across the four global reverb instances.
====================
*/
FMOD_RESULT ChannelReal::setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !mParent ) {
        return FMOD_OK;
    }

    FMOD_RESULT result;
    int         instance;
    int         numinstances = 0;

    for ( instance = 0; instance < FMOD_REVERB_MAXINSTANCES; instance++ ) {
        if ( prop->Flags & ( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance ) ) {
            numinstances++;
        }
    }

    for ( instance = 0; instance < FMOD_REVERB_MAXINSTANCES; instance++ ) {
        if ( ( prop->Flags & ( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance ) )
             || ( numinstances == 0 && instance == 0 ) ) {
            result = mSystem->mReverbGlobal.setChanProperties( instance, mParent->mIndex, prop, NULL );
            if ( numinstances <= 1 && result != FMOD_OK ) {
                return result;
            }
        } else {
            FMOD_REVERB_CHANNELPROPERTIES cprop;

            mSystem->mReverbGlobal.getChanProperties( instance, mParent->mIndex, &cprop, NULL );
            cprop.Direct = prop->Direct;
            mSystem->mReverbGlobal.setChanProperties( instance, mParent->mIndex, &cprop, NULL );
        }
    }

    return FMOD_OK;
} // 0x8032DDB0

/*
====================
ChannelReal::getReverbProperties

Reads per-channel reverb properties for the selected reverb instance. Retail emits a trailing `blr`
after the tail call (0x74 bytes total, matching Gormiti's size).
====================
*/
FMOD_RESULT ChannelReal::getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !mParent ) {
        return FMOD_OK;
    }

    return mSystem->mReverbGlobal.getChanProperties(
        ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE1 ) ? 1 :
        ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE2 ) ? 2 :
        ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE3 ) ? 3 : 0,
        mParent->mIndex, prop, NULL );
} // 0x8032DF18

/*
====================
ChannelReal::isPlaying

Returns true if the channel is playing or currently allocated.
====================
*/
FMOD_RESULT ChannelReal::isPlaying( bool *isplaying, bool includethreadlatency ) {
    if ( !isplaying ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( ( mFlags & CHANNELREAL_FLAG_PLAYING ) || ( mFlags & CHANNELREAL_FLAG_0x10 ) ) {
        *isplaying = true;
    } else {
        *isplaying = false;
    }

    return FMOD_OK;
} // 0x8032DF8C

/*
====================
ChannelReal::isVirtual

Base real channels are not virtual.
====================
*/
FMOD_RESULT ChannelReal::isVirtual( bool *isvirtual ) {
    if ( !isvirtual ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *isvirtual = false;

    return FMOD_OK;
} // 0x8032DFCC

/*
====================
ChannelReal::getSpectrum

Requires a software voice.
====================
*/
FMOD_RESULT ChannelReal::getSpectrum( float *spectrumarray, int numvalues, int channeloffset,
                                      FMOD_DSP_FFT_WINDOW windowtype ) {
    return FMOD_ERR_NEEDSSOFTWARE;
} // 0x8032DFEC

/*
====================
ChannelReal::getWaveData

Requires a software voice.
====================
*/
FMOD_RESULT ChannelReal::getWaveData( float *wavearray, int numvalues, int channeloffset ) {
    return FMOD_ERR_NEEDSSOFTWARE;
} // 0x8032DFF4

/*
====================
ChannelReal::getDSPHead

Base real channels have no DSP head.
====================
*/
FMOD_RESULT ChannelReal::getDSPHead( DSPI **dsp ) {
    *dsp = NULL;
    return FMOD_ERR_NEEDSSOFTWARE;
} // 0x8032DFFC

/*
====================
ChannelReal::setMode

Updates loop, 3D rolloff/relativity, virtual and 2D/3D mode flags.
====================
*/
FMOD_RESULT ChannelReal::setMode( FMOD_MODE mode ) {
    if ( mode & ( FMOD_LOOP_OFF | FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI ) ) {
        mMode &= ~( FMOD_LOOP_OFF | FMOD_LOOP_NORMAL | FMOD_LOOP_BIDI );

        if ( mode & FMOD_LOOP_OFF ) {
            mMode |= FMOD_LOOP_OFF;
        } else if ( mode & FMOD_LOOP_NORMAL ) {
            mMode |= FMOD_LOOP_NORMAL;
        } else if ( mode & FMOD_LOOP_BIDI ) {
            mMode |= FMOD_LOOP_BIDI;
        }
    }

    if ( mode & FMOD_3D_HEADRELATIVE ) {
        mMode &= ~FMOD_3D_WORLDRELATIVE;
        mMode |= FMOD_3D_HEADRELATIVE;
    } else if ( mode & FMOD_3D_WORLDRELATIVE ) {
        mMode &= ~FMOD_3D_HEADRELATIVE;
        mMode |= FMOD_3D_WORLDRELATIVE;
    }

    if ( mode & FMOD_3D_LOGROLLOFF ) {
        mMode &= ~FMOD_3D_LINEARROLLOFF;
        mMode &= ~FMOD_3D_CUSTOMROLLOFF;
        mMode |= FMOD_3D_LOGROLLOFF;
    } else if ( mode & FMOD_3D_LINEARROLLOFF ) {
        mMode &= ~FMOD_3D_LOGROLLOFF;
        mMode &= ~FMOD_3D_CUSTOMROLLOFF;
        mMode |= FMOD_3D_LINEARROLLOFF;
    } else if ( mode & FMOD_3D_CUSTOMROLLOFF ) {
        mMode &= ~FMOD_3D_LOGROLLOFF;
        mMode &= ~FMOD_3D_LINEARROLLOFF;
        mMode |= FMOD_3D_CUSTOMROLLOFF;
    }

    if ( mode & FMOD_3D_IGNOREGEOMETRY ) {
        mMode |= FMOD_3D_IGNOREGEOMETRY;
    } else {
        mMode &= ~FMOD_3D_IGNOREGEOMETRY;
    }

    if ( mode & FMOD_VIRTUAL_PLAYFROMSTART ) {
        mMode |= FMOD_VIRTUAL_PLAYFROMSTART;
    } else {
        mMode &= ~FMOD_VIRTUAL_PLAYFROMSTART;
    }

    if ( mode & FMOD_2D ) {
        if ( !mParent ) {
            return FMOD_OK;
        }

        mMode &= ~FMOD_3D;
        mMode |= FMOD_2D;
        mParent->mVolume3D        = 1.0f;
        mParent->mDirectOcclusion = 0.0f;
        mParent->mReverbDryVolume = 1.0f;
        mParent->mConeVolume3D    = 1.0f;
        mParent->mPitch3D         = 1.0f;
    } else if ( mode & FMOD_3D ) {
        mMode &= ~FMOD_2D;
        mMode |= FMOD_3D;
    }

    return FMOD_OK;
} // 0x8032E00C

/*
====================
ChannelReal::setLowPassFilter

Stores the low-pass filter cutoff.
====================
*/
FMOD_RESULT ChannelReal::setLowPassFilter( int cutoff ) {
    mLPFCutoff = cutoff;
    return FMOD_OK;
} // 0x8032E1A8

/*
====================
ChannelReal::getLowPassFilter

Returns the low-pass filter cutoff.
====================
*/
FMOD_RESULT ChannelReal::getLowPassFilter( int *cutoff ) {
    *cutoff = mLPFCutoff;
    return FMOD_OK;
} // 0x8032E1B4

/*
====================
ChannelReal::setBiquadFilter

Stores the biquad filter state and coefficients.
====================
*/
FMOD_RESULT ChannelReal::setBiquadFilter( bool active, unsigned short b0, unsigned short b1, unsigned short b2,
                                          unsigned short a1, unsigned short a2 ) {
    mBiquadActive = active;
    mBiquadB0     = b0;
    mBiquadB1     = b1;
    mBiquadB2     = b2;
    mBiquadA1     = a1;
    mBiquadA2     = a2;

    return FMOD_OK;
} // 0x8032E1C4

/*
====================
ChannelReal::getBiquadFilter

Returns the biquad filter state and coefficients.
====================
*/
FMOD_RESULT ChannelReal::getBiquadFilter( bool *active, unsigned short *b0, unsigned short *b1,
                                          unsigned short *b2, unsigned short *a1, unsigned short *a2 ) {
    *active = mBiquadActive;
    *b0     = mBiquadB0;
    *b1     = mBiquadB1;
    *b2     = mBiquadB2;
    *a1     = mBiquadA1;
    *a2     = mBiquadA2;

    return FMOD_OK;
} // 0x8032E1E4

/*
====================
ChannelReal::setControllerSpeaker

Stores the Wii controller speaker mask.
====================
*/
FMOD_RESULT ChannelReal::setControllerSpeaker( unsigned int speaker, int controller ) {
    mControllerSpeaker = speaker;
    return FMOD_OK;
} // 0x8032E21C

/*
====================
ChannelReal::getControllerSpeaker

Returns the Wii controller speaker mask.
====================
*/
FMOD_RESULT ChannelReal::getControllerSpeaker( unsigned int *speaker ) {
    *speaker = mControllerSpeaker;
    return FMOD_OK;
} // 0x8032E228

} // namespace FMOD
