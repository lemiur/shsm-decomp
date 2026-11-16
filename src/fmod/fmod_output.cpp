/*
 * fmod_output.cpp: FMOD::Output, the base of the sound outputs. Allocation tags carry the original
 * file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_output.obj)
 */
// Unit name and boundary: retail string "fmod_output.cpp" and Gormiti link map (fmodex.a fmod_output.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_downmix.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_output.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
Output::Output

Initializes the output's fields and DSP tick counter.
====================
*/
Output::Output() {
    mEnumerated            = false;
    mPolling               = false;
    mSystem                = NULL;
    mChannelPool3D         = NULL;
    mMixAheadBlocks        = 0;
    mMusicChannelGroup     = NULL;
    mNum2DChannelsFromCaps = 0;
    mNum3DChannelsFromCaps = 0;
    mTotalChannelsFromCaps = 0;
    mDSPTick               = 1;
} // 0x80379E10

/*
====================
Output::release

Closes the output plugin if a close callback is registered, then frees this object.
====================
*/
FMOD_RESULT Output::release() {
    if ( mDescription.close ) {
        readfrommixer = mixCallback;

        mDescription.close( this );
    }

#line 96
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x80379E88

/*
====================
Output::mix

Runs the DSP graph starting at the sound card unit to fill buffer with numsamples samples, copies
back any redirected output buffer, invokes the post-mix callback, and advances the system and global
DSP clocks.
====================
*/
FMOD_RESULT Output::mix( void *buffer, unsigned int numsamples ) {
    FMOD_RESULT          result;
    unsigned int         blockalign = 0;
    unsigned int         offset     = 0;

    LocalCriticalSection criticalsection( mSystem->mDSPCrit );
    LocalCriticalSection lockdspcriticalsection( mSystem->mDSPLockCrit );

    FMOD_SOUND_FORMAT    outputformat;
    int                  outputchannels;
    int                  outputchannelsmixer;
    DSPI                *dsphead;

    if ( !buffer || !numsamples ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    result = mSystem->getSoftwareFormat( NULL, &outputformat, &outputchannelsmixer, NULL, NULL, NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    outputchannels = outputchannelsmixer;
    if ( mSystem->mDownmix ) {
        mSystem->mDownmix->getOutputChannels( &outputchannels );
    }

    result = SoundI::getBytesFromSamples( 1, &blockalign, outputchannels, outputformat );
    if ( result != FMOD_OK ) {
        return result;
    }

    dsphead = mSystem->mDSPSoundCard;
    if ( !dsphead ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mSystem->flushDSPConnectionRequests( false );

    lockdspcriticalsection.enter();
    criticalsection.enter();

    do {
        void         *origdest = (char *)buffer + offset * blockalign;
        void         *dest     = origdest;
        unsigned int  len      = numsamples;

        mSystem->mDSPActive        = true;
        mSystem->mDSPReadBuffIndex = 0;

        dsphead->execute( dest, &dest, &len, outputchannelsmixer, &outputchannelsmixer, mSystem->mSpeakerMode,
                          mDSPTick );

        mDSPTick++;
        mSystem->mDSPActive = false;

        if ( dest != origdest ) {
            memcpy( origdest, dest, len * blockalign );
        }

        numsamples -= len;
        offset     += len;
    } while ( numsamples );

    if ( mDescription.postmixcallback ) {
        mDescription.postmixcallback( this );
    }

    criticalsection.leave();
    lockdspcriticalsection.leave();

    mSystem->mDSPClock.mValue += offset;

    float delta = 1000.0f * ( (float)mSystem->mDSPBlockSize / (float)mSystem->mOutputRate );
    lbl_80529020->mDSPClock.mValue += (unsigned long long)( 4294967296.0f * delta );
    FMOD_OS_Time_GetMs( &lbl_80529020->mDSPClockTimeStamp );

    return FMOD_OK;
} // 0x80379F0C

/*
====================
Output::getFreeChannel

Allocates a free real channel from the 3D or 2D channel pool according to mode.
====================
*/
FMOD_RESULT Output::getFreeChannel( unsigned int mode, ChannelReal **realchannel, int numchannels, int maxchannels,
                                    int *found_out, bool ignorereserved ) {
    FMOD_RESULT result;

    if ( !realchannel ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( mode & FMOD_3D ) {
        if ( !mChannelPool3D ) {
            return FMOD_ERR_CHANNEL_ALLOC;
        }

        result = mChannelPool3D->allocateChannel( realchannel, -1, numchannels, found_out, ignorereserved );
        if ( result != FMOD_OK ) {
            return result;
        }
    } else {
        if ( !mChannelPool ) {
            return FMOD_ERR_CHANNEL_ALLOC;
        }

        result = mChannelPool->allocateChannel( realchannel, -1, numchannels, found_out, ignorereserved );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x8037A378

/*
====================
Output::mixCallback

Forwards an output plugin's mix callback to Output::mix.
====================
*/
FMOD_RESULT F_CALLBACK Output::mixCallback( FMOD_OUTPUT_STATE *output_state, void *buffer, unsigned int numsamples ) {
    return static_cast<Output *>( output_state )->mix( buffer, numsamples );
} // 0x8037A418

} // namespace FMOD
