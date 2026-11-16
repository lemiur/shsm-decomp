/*
 * fmod_output_polled.cpp: FMOD::OutputPolled, the base of the outputs that are polled for their play position.
 * Names and layout: Gormiti link map and DWARF (fmod_output_polled.obj).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_output_polled.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_output_polled.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <fmod/fmod_time.h>

namespace FMOD {

/*
====================
OutputPolled::OutputPolled

Starts at the first block of the ring buffer.
====================
*/
OutputPolled::OutputPolled() {
    mCursorBlock = 0;
} // 0x8037274C

/*
====================
OutputPolled::threadFunc

One pass of the mixer. Finds the block the device is playing, then mixes every block between the cursor and
it: locks the matching bytes of the ring buffer, has the output mix into them and unlocks them again.
====================
*/
FMOD_RESULT OutputPolled::threadFunc() {
    FMOD_RESULT        result;
    unsigned int       pcm;
    unsigned int       blocksize;
    int                numblocks;
    FMOD_SOUND_FORMAT  outputformat;
    int                outputchannels;

    result = mSystem->getDSPBufferSize( &blocksize, &numblocks );
    if ( result != FMOD_OK ) {
        return result;
    }

    numblocks += mMixAheadBlocks;

    result = mSystem->getSoftwareFormat( NULL, &outputformat, &outputchannels, NULL, NULL, NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mDescription.getposition ) {
        result = mDescription.getposition( this, &pcm );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    mSystem->mDSPTimeStamp.stampIn();

    pcm = ( pcm / blocksize ) % numblocks;

    while ( mCursorBlock != (int)pcm ) {
        void         *ptr1 = NULL;
        void         *ptr2 = NULL;
        unsigned int  len1 = 0;
        unsigned int  len2 = 0;
        unsigned int  offbytes = 0;
        unsigned int  lenbytes = 0;
        unsigned int  numsamples;
        int           block = mCursorBlock;

        block -= mMixAheadBlocks;
        if ( block < 0 ) {
            block += numblocks;
        }

        if ( mSystem->mDownmix ) {
            mSystem->mDownmix->getOutputChannels( &outputchannels );
        }

        result = SoundI::getBytesFromSamples( blocksize, &lenbytes, outputchannels, outputformat );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = SoundI::getBytesFromSamples( block * blocksize, &offbytes, outputchannels, outputformat );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( mDescription.lock ) {
            result = mDescription.lock( this, offbytes, lenbytes, &ptr1, &ptr2, &len1, &len2 );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        result = SoundI::getSamplesFromBytes( len1, &numsamples, outputchannels, outputformat );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = mix( ptr1, numsamples );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( mDescription.unlock ) {
            result = mDescription.unlock( this, ptr1, ptr2, len1, len2 );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        mCursorBlock++;
        if ( mCursorBlock >= numblocks ) {
            mCursorBlock = 0;
        }
    }

    if ( mFinishedSema ) {
        FMOD_OS_Semaphore_Signal( mFinishedSema, false );
    }

    mSystem->mDSPTimeStamp.stampOut( 95 );

    return FMOD_OK;
} // 0x803727A0

/*
====================
OutputPolled::start

Starts the mixer thread. Where the system is updated by the game (FMOD_INIT_SYNCMIXERWITHUPDATE) the thread
is woken by a semaphore and the system waits on mFinishedSema; otherwise it sleeps for a third of the length
of a DSP block between passes (10ms for long blocks, at least 1ms).
====================
*/
FMOD_RESULT OutputPolled::start() {
    FMOD_RESULT result;

    if ( mSystem->mFlags & FMOD_INIT_SYNCMIXERWITHUPDATE ) {
        mPolledFromMainThread = true;
    }

    if ( mPolledFromMainThread ) {
        result = initThread( "FMOD mixer thread", NULL, NULL, PRIORITY_CRITICAL, NULL, 16384, true, 0,
                             mSystem );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = FMOD_OS_Semaphore_Create( &mFinishedSema );
        if ( result != FMOD_OK ) {
            return result;
        }
    } else {
        unsigned int blocksize;
        int          rate;
        float        ms;

        result = mSystem->getDSPBufferSize( &blocksize, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = mSystem->getSoftwareFormat( &rate, NULL, NULL, NULL, NULL, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        ms = 1000.0f * (float)blocksize / (float)rate;
        if ( ms < 20.0f ) {
            ms /= 3.0f;
            if ( ms < 1.0f ) {
                ms = 1.0f;
            }
        } else {
            ms = 10.0f;
        }

        result = initThread( "FMOD mixer thread", NULL, NULL, PRIORITY_CRITICAL, NULL, 16384, false,
                             (int)ms, mSystem );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x80372DC4

/*
====================
OutputPolled::stop

Stops the thread and frees the semaphore the system waits on.
====================
*/
FMOD_RESULT OutputPolled::stop() {
    FMOD_RESULT result;

    result = closeThread();
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mFinishedSema ) {
        result = FMOD_OS_Semaphore_Free( mFinishedSema );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x80372F4C

} // namespace FMOD
