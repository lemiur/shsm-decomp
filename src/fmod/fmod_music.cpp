// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_music.obj)
//
// The Internal members and MusicVirtualChannel::cleanUp have no copy in retail: they are inlined into their
// callers. Retail's MusicSong::playSound is another revision than Gormiti's (same size, other code).

#include <new>
#include <string.h>

#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_music.h>
#include <fmod/fmod_systemi.h>

namespace FMOD {

// The placeholders of the music codecs: a sample, a voice, a channel and an instrument that every
// module that finds a slot empty can point at.
MusicSample gDummySample;
MusicVirtualChannel gDummyVirtualChannel;
MusicChannel gDummyChannel;
MusicInstrument gDummyInstrument;

// Returns a voice to the pool.
// no copy in retail (inlined)
inline void MusicVirtualChannel::cleanUp()
{
    if ( mSong->mLowPass )
    {
        mSong->mLowPass[mChannel.mIndex]->remove();
    }
    mNoteControl = 0;
    removeNode();
    mAllocated = false;
}

/*
====================
MusicSong::play

Starts the song from the top; fromopen is set when the codec opens, to let the first DSP tick through.
====================
*/
FMOD_RESULT MusicSong::play( bool fromopen )
{
    FMOD_RESULT result;
    int count;

    result = stop();
    if ( result != FMOD_OK )
    {
        return result;
    }

    mGlobalVolume = mDefaultGlobalVolume;
    mSpeed = mDefaultSpeed;
    mRow = 0;
    mOrder = 0;
    mNextOrder = 0;
    mNextRow = 0;
    mMixerSamplesLeft = 0;
    mTick = 0;
    mPatternDelay = 0;
    mPatternDelayTicks = 0;
    mPCMOffset = 0;
    mFinished = false;

    if ( fromopen )
    {
        mDSPTick = 1;
    }

    new ( &mChannelGroup ) ChannelGroupI;
    mChannelGroup.mDSPHead = mDSPHead;
    mChannelGroup.mDSPMixTarget = mDSPHead;
    mChannelGroup.mVolume = 1.0f;

    if ( mVisited )
    {
        memset( mVisited, 0, mNumOrders * 256 );
    }

    setBPM( mDefaultBPM );

    if ( mNumChannels && mMusicChannel )
    {
        for ( count = 0; count < mNumChannels; count++ )
        {
            MusicChannel *cptr = mMusicChannel[count];
            float oldmastervolume;

            oldmastervolume = cptr->mMasterVolume;
            memset( cptr, 0, sizeof( MusicChannel ) );
            cptr->mVirtualChannelHead.initNode();

            cptr->mGlobalVolume = mDefaultVolume[count];
            cptr->mPan = mDefaultPan[count];

            if ( fromopen )
            {
                cptr->mMasterVolume = 1.0f;
            }
            else
            {
                cptr->mMasterVolume = oldmastervolume;
            }
        }
    }

    if ( mNumVirtualChannels )
    {
        for ( count = 0; count < mNumVirtualChannels; count++ )
        {
            MusicVirtualChannel *vcptr = &mVirtualChannel[count];

            memset( vcptr, 0, sizeof( MusicVirtualChannel ) );
            vcptr->mChannel.init();
            vcptr->mChannel.mFlags |= CHANNELI_FLAG_0x800;
            vcptr->mIndex = count;
            vcptr->mChannel.mIndex = count;
            vcptr->mSong = this;
        }
    }

    mPlaying = true;

    return FMOD_OK;
} // 0x8033DA1C

/*
====================
MusicSong::spawnNewVirtualChannel

Takes the first free voice and links it into the voice ring of the channel.
====================
*/
FMOD_RESULT MusicSong::spawnNewVirtualChannel( MusicChannel *cptr, MusicSample *sptr, MusicVirtualChannel **newvcptr )
{
    MusicVirtualChannel *vcptr = 0;
    int count;

    for ( count = 0; count < mNumVirtualChannels; count++ )
    {
        if ( !mVirtualChannel[count].mAllocated )
        {
            vcptr = &mVirtualChannel[count];
            vcptr->mAllocated = true;
            break;
        }
    }

    if ( !vcptr )
    {
        return FMOD_ERR_INTERNAL;
    }

    vcptr->addAfter( &cptr->mVirtualChannelHead );

    vcptr->mBackground = false;
    vcptr->mEnvVolume.mTick = 0;
    vcptr->mEnvVolume.mPosition = 0;
    vcptr->mEnvVolume.mValue = 64;
    vcptr->mEnvVolume.mFraction = 0x400000;
    vcptr->mEnvVolume.mDelta = 0;
    vcptr->mEnvVolume.mStopped = false;
    vcptr->mEnvPan.mTick = 0;
    vcptr->mEnvPan.mPosition = 0;
    vcptr->mEnvPan.mValue = 128;
    vcptr->mEnvPan.mFraction = 0x800000;
    vcptr->mEnvPan.mDelta = 0;
    vcptr->mEnvPan.mStopped = false;
    vcptr->mEnvPitchTick = 0;
    vcptr->mEnvPitchPos = 0;
    vcptr->mEnvPitchFrac = 0;
    vcptr->mEnvPitch = 0;
    vcptr->mEnvPitchDelta = 0;
    vcptr->mEnvPitchStopped = false;
    vcptr->mFadeOutVolume = 1024;

    if ( newvcptr )
    {
        *newvcptr = vcptr;
    }

    return FMOD_OK;
} // 0x8033DCCC

/*
====================
MusicSong::setBPM

Sets the tempo and the number of mixer samples per tick that goes with it.
====================
*/
FMOD_RESULT MusicSong::setBPM( int bpm )
{
    float hz;

    if ( bpm < 1 )
    {
        bpm = 1;
    }

    mBPM = bpm;

    hz = (float)bpm * 2.0f / 5.0f;
    hz *= mMasterSpeed;

    if ( hz >= 0.01f )
    {
        mMixerSamplesPerTick = (int)( (float)waveformat->frequency / hz );
    }

    return FMOD_OK;
} // 0x8033DDC0

/*
====================
MusicSong::stop

Stops every voice of the song and gives them back.
====================
*/
FMOD_RESULT MusicSong::stop()
{
    int count;

    mPlaying = false;
    mFinished = true;

    if ( mMusicChannel )
    {
        for ( count = 0; count < mNumChannels; count++ )
        {
            MusicChannel *cptr = mMusicChannel[count];

            if ( cptr && cptr->mVirtualChannelHead.mNodeNext )
            {
                while ( !cptr->mVirtualChannelHead.isEmpty() )
                {
                    MusicVirtualChannel *vcptr = (MusicVirtualChannel *)cptr->mVirtualChannelHead.mNodeNext;

                    vcptr->mChannel.stopEx( CHANNELI_STOPFLAG_0x4 );
                    vcptr->mChannel.mRealChannel[0] = 0;

                    if ( mLowPass )
                    {
                        mLowPass[vcptr->mChannel.mIndex]->remove();
                    }

                    bool playing = false;

                    vcptr->mChannel.isPlaying( &playing );
                    if ( !playing )
                    {
                        vcptr->cleanUp();
                    }
                }
            }
        }
    }

    return FMOD_OK;
} // 0x8033DE54

/*
====================
MusicSong::playSound

Starts a sample on a voice: allocates a real channel for it, plays the sample paused and unpauses it once it is
routed through the song's group (or the mixer plug-in's), and through the low-pass filter if one is asked for.
====================
*/
FMOD_RESULT MusicSong::playSound( MusicSample *sample, MusicVirtualChannel *vcptr, bool addfilter, _SNDMIXPLUGIN *plugin )
{
    FMOD_RESULT result;
    ChannelI *channel = &vcptr->mChannel;
    ChannelReal *realchannel = channel->mRealChannel[0];

    result = mChannelPool->allocateChannel( &realchannel, vcptr->mFlip ? vcptr->mIndex + mNumVirtualChannels : vcptr->mIndex, 1, 0, false );
    if ( result != FMOD_OK )
    {
        return result;
    }

    if ( mChannelSoftware )
    {
        vcptr->mFlip = !vcptr->mFlip;
    }

    if ( channel->mRealChannel[0] )
    {
        vcptr->mChannel.setVolume( 0.0f, false );
    }
    channel->mRealChannel[0] = realchannel;

    if ( plugin )
    {
        channel->mChannelGroup = &plugin->mChannelGroup;
    }
    else
    {
        channel->mChannelGroup = &mChannelGroup;
    }

    result = channel->play( sample->mSound, true, true, false );
    if ( result != FMOD_OK )
    {
        channel->stopEx( CHANNELI_STOPFLAG_0x4 );
        return result;
    }

    if ( vcptr->mSampleOffset )
    {
        channel->setPosition( vcptr->mSampleOffset, FMOD_TIMEUNIT_PCM );
        vcptr->mSampleOffset = 0;
    }

    if ( mLowPass )
    {
        mLowPass[channel->mIndex]->remove();
        if ( addfilter )
        {
            channel->addDSP( mLowPass[channel->mIndex], 0 );
        }
    }

    channel->setPaused( false );

    if ( mChannelSoftware )
    {
        mSystem->flushDSPConnectionRequests( true );
    }

    return FMOD_OK;
} // 0x8033DFAC

// Lets the song's codec run its update while the group is not paused.
FMOD_RESULT ChannelMusic::updateStream()
{
    bool paused;

    mMusic->mChannelGroup.getPaused( &paused );
    if ( !paused )
    {
        mMusic->mDescription.update( mMusic );
    }

    return FMOD_OK;
} // 0x8033E160

FMOD_RESULT ChannelMusic::stop()
{
    return mMusic->stop();
} // 0x8033E1C8

FMOD_RESULT ChannelMusic::start()
{
    return mMusic->play( false );
} // 0x8033E1D0

// Pauses or resumes the song's group and the first voice of each channel.
FMOD_RESULT ChannelMusic::setPaused( bool paused )
{
    int count;

    if ( mMusic->mMusicChannel )
    {
        mMusic->mChannelGroup.setPaused( paused, true );

        for ( count = 0; count < mMusic->mNumChannels; count++ )
        {
            MusicChannel *cptr = mMusic->mMusicChannel[count];

            if ( cptr )
            {
                MusicVirtualChannel *vcptr = (MusicVirtualChannel *)cptr->mVirtualChannelHead.mNodeNext;

                if ( vcptr )
                {
                    vcptr->mChannel.setPaused( paused );
                }
            }
        }
    }

    return FMOD_OK;
} // 0x8033E1DC

FMOD_RESULT ChannelMusic::setVolume( float volume )
{
    if ( mMusic->mMusicChannel )
    {
        mMusic->mChannelGroup.setVolume( volume );
    }

    return FMOD_OK;
} // 0x8033E288

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::getLengthInternal( unsigned int *length, unsigned int lengthtype )
{
    if ( lengthtype == FMOD_TIMEUNIT_MODORDER )
    {
        *length = mNumOrders;
        return FMOD_OK;
    }
    else if ( lengthtype == FMOD_TIMEUNIT_MODPATTERN )
    {
        *length = mNumPatterns;
        return FMOD_OK;
    }
    else if ( lengthtype == FMOD_TIMEUNIT_MODROW )
    {
        *length = mPattern[mOrderList[mOrder]].mRows;
        return FMOD_OK;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::getLengthCallback( FMOD_CODEC_STATE *codec_state, unsigned int *length, unsigned int lengthtype )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->getLengthInternal( length, lengthtype );
} // 0x8033E2BC

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::getPositionInternal( unsigned int *position, unsigned int postype )
{
    if ( postype == FMOD_TIMEUNIT_MODORDER )
    {
        *position = mOrder;
        return FMOD_OK;
    }
    else if ( postype == FMOD_TIMEUNIT_MODPATTERN )
    {
        *position = mOrderList[mOrder];
        return FMOD_OK;
    }
    else if ( postype == FMOD_TIMEUNIT_MODROW )
    {
        *position = mRow;
        return FMOD_OK;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::getPositionCallback( FMOD_CODEC_STATE *codec_state, unsigned int *position, unsigned int postype )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->getPositionInternal( position, postype );
} // 0x8033E330

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::getMusicNumChannelsInternal( int *numchannels )
{
    if ( !numchannels )
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numchannels = mNumChannels;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::getMusicNumChannelsCallback( FMOD_CODEC_STATE *codec_state, int *numchannels )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->getMusicNumChannelsInternal( numchannels );
} // 0x8033E398

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::setMusicChannelVolumeInternal( int channel, float volume )
{
    if ( channel < 0 || channel >= mNumChannels || volume < 0.0f || volume > 1.0f )
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    mMusicChannel[channel]->mMasterVolume = volume;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::setMusicChannelVolumeCallback( FMOD_CODEC_STATE *codec_state, int channel, float volume )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->setMusicChannelVolumeInternal( channel, volume );
} // 0x8033E3C4

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::getMusicChannelVolumeInternal( int channel, float *volume )
{
    if ( channel < 0 || channel >= mNumChannels || !volume )
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    *volume = mMusicChannel[channel]->mMasterVolume;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::getMusicChannelVolumeCallback( FMOD_CODEC_STATE *codec_state, int channel, float *volume )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->getMusicChannelVolumeInternal( channel, volume );
} // 0x8033E420

// no copy in retail (inlined)
inline FMOD_RESULT MusicSong::getHardwareMusicChannel( ChannelReal **realchannel )
{
    if ( !realchannel )
    {
        return FMOD_ERR_INVALID_PARAM;
    }

    new ( &mHardwareMusicChannel ) ChannelMusic;

    *realchannel = &mHardwareMusicChannel;
    mHardwareMusicChannel.mFlags |= CHANNELREAL_FLAG_0x10;
    mHardwareMusicChannel.mMusic = this;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK MusicSong::getHardwareMusicChannelCallback( FMOD_CODEC_STATE *codec_state, ChannelReal **realchannel )
{
    MusicSong *cmusic = static_cast<MusicSong *>( codec_state );

    return cmusic->getHardwareMusicChannel( realchannel );
} // 0x8033E470

} // namespace FMOD
