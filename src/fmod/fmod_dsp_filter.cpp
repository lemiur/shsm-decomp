/*
 * fmod_dsp_filter.cpp: FMOD::DSPFilter, the base DSP unit that mixes its inputs and runs a
 * filter callback. Allocation tags carry the original file's line numbers (docs/exceptions.md,
 * "FMOD rulings (fmod-1)").
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_filter.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_filter.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_dsp.h>
#include <fmod/fmod_dsp_connectioni.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_linkedlist.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
DSPFilter::release

Stops history buffering and releases the base DSPI unit.
====================
*/
FMOD_RESULT DSPFilter::release( bool freethis ) {
    stopBuffering();

    return DSPI::release( freethis );
} // 0x8035ECC0

/*
====================
DSPFilter::execute

Pulls and mixes active inputs into mixbuffer (passing a single unity-gain input straight through
when possible), invokes the filter's read callback unless bypassed, and copies the output into the
circular history buffer when buffering is active.

Gormiti's copy is a different revision (0x6DC bytes vs 0x6D4 in retail) and does not list
`historybuffer` in its DWARF; holding `mHistoryBuffer` in `historybuffer` keeps it in r16 across
the memcpy calls as in retail.
====================
*/
FMOD_RESULT DSPFilter::execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick ) {
    FMOD_RESULT  result    = FMOD_OK;
    unsigned int starttime = 0;
    unsigned int endtime   = 0;

    if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
        FMOD_OS_Time_GetNs( &starttime );
    }

    if ( *length > mSystem->mDSPBlockSize ) {
        *length = mSystem->mDSPBlockSize;
    }

    *outbuffer   = inbuffer;
    *outchannels = inchannels;

    mFlags |= DSPI_FLAG_0x10;

    if ( mDSPTick != tick ) {
        float          *inputworkbuff;
        float          *mixbuffer;
        LinkedListNode *current;
        int             connectionnumber;
        int             inputworkbuffused;

        mixbuffer = mBuffer;
        if ( mixbuffer ) {
            mBufferChannels = inchannels;
        } else {
            mixbuffer = inbuffer;
        }

        inputworkbuffused = mSystem->mDSPReadBuffIndex;
        inputworkbuff     = mSystem->mDSPReadBuff[inputworkbuffused];
        current           = mInputHead.getNext();
        connectionnumber  = 0;

        if ( current == &mInputHead ) {
            memset( mixbuffer, 0, *length * inchannels * sizeof( float ) );
        }

        while ( current != &mInputHead ) {
            DSPConnectionI *connection = (DSPConnectionI *)current->getData();

            if ( !( connection->mInputUnit->mFlags & DSPI_FLAG_ACTIVE ) ||
                 ( connection->mInputUnit->mFlags & ( DSPI_FLAG_FINISHED | DSPI_FLAG_0x100 ) ) ) {
                if ( mLastChannels > 0 ) {
                    *outchannels = mLastChannels;
                } else {
                    *outchannels = inchannels;
                }

                if ( connectionnumber == 0 ) {
                    memset( mixbuffer, 0, *length * *outchannels * sizeof( float ) );
                }

                *outbuffer = mixbuffer;
                connectionnumber++;
                connection->mInputUnit->mDSPTick = tick;
                current = current->getNext();
                continue;
            }

            mSystem->mDSPReadBuffIndex = 1 - inputworkbuffused;

            if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
                FMOD_OS_Time_GetNs( &endtime );
                mCPUUsageTemp += endtime - starttime;
            }

            result = connection->mInputUnit->execute( inputworkbuff, outbuffer, length, inchannels,
                                                      outchannels, speakermode, tick );
            if ( result != FMOD_OK ) {
                break;
            }

            if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
                FMOD_OS_Time_GetNs( &starttime );
            }

            if ( connection->mInputUnit->mFlags & DSPI_FLAG_0x10 ) {
                if ( mLastChannels > 0 ) {
                    *outchannels = mLastChannels;
                } else {
                    *outchannels = inchannels;
                }

                if ( connectionnumber == 0 ) {
                    memset( mixbuffer, 0, *length * *outchannels * sizeof( float ) );
                }

                *outbuffer = mixbuffer;
                connectionnumber++;
                connection->mInputUnit->mDSPTick = tick;
                current = current->getNext();
                continue;
            }

            mFlags &= ~DSPI_FLAG_0x10;

            bool passthru = false;
            if ( mNumInputs <= 1 && connection->mVolume == 1.0f ) {
                passthru = true;

                if ( mDescription.mCategory == FMOD_DSP_CATEGORY_SOUNDCARD &&
                     connection->checkUnity( *outchannels, inchannels ) != FMOD_OK ) {
                    passthru = false;
                } else if ( connection->mSetLevelsUsed &&
                            connection->checkUnity( *outchannels, inchannels ) != FMOD_OK ) {
                    passthru = false;
                } else if ( mDescription.read && mDescription.channels &&
                            mDescription.channels != *outchannels ) {
                    passthru = false;
                }
            }

            if ( passthru ) {
                if ( mNumOutputs > 1 && mDescription.mCategory != FMOD_DSP_CATEGORY_RESAMPLER ) {
                    if ( *outbuffer ) {
                        memcpy( mBuffer, *outbuffer, *length * *outchannels * sizeof( float ) );
                    } else {
                        memset( mBuffer, 0, *length * *outchannels * sizeof( float ) );
                    }

                    *outbuffer      = mBuffer;
                    mBufferChannels = *outchannels;
                }

                if ( connection->mRampCount ) {
                    int count, count2;

                    for ( count = 0; count < connection->mMaxOutputLevels; count++ ) {
                        for ( count2 = 0; count2 < connection->mMaxInputLevels; count2++ ) {
                            connection->mLevelCurrent[count][count2] =
                                connection->mVolume * connection->mLevel[count][count2];
                            connection->mLevelDelta[count][count2] = 0.0f;
                        }
                    }

                    connection->mRampCount = 0;
                }
            } else {
                if ( !connection->mSetLevelsUsed ) {
                    if ( inchannels == *outchannels && connection->mVolume == 1.0f ) {
                        connection->setUnity();
                    } else {
                        connection->setPan( 0.0f, inchannels, *outchannels, speakermode );
                    }
                    connection->mSetLevelsUsed = 1;
                }

                if ( *outbuffer == mixbuffer ) {
                    if ( mixbuffer == mSystem->mDSPReadBuff[0] ) {
                        mixbuffer = mSystem->mDSPReadBuff[1];
                    } else {
                        mixbuffer = mSystem->mDSPReadBuff[0];
                    }

                    inputworkbuffused = 1 - inputworkbuffused;
                    inputworkbuff     = mSystem->mDSPReadBuff[inputworkbuffused];
                }

                if ( connectionnumber == 0 ) {
                    memset( mixbuffer, 0, *length * inchannels * sizeof( float ) );
                }

                connection->mix( mixbuffer, *outbuffer, inchannels, *outchannels, *length );

                *outbuffer   = mixbuffer;
                *outchannels = inchannels;
            }

            connectionnumber++;
            connection->mInputUnit->mDSPTick = tick;
            current = current->getNext();
        }

        if ( mDescription.read && !( mFlags & DSPI_FLAG_BYPASS ) ) {
            inchannels = *outchannels;

            if ( inbuffer == *outbuffer && inbuffer != mixbuffer && mNumInputs ) {
                inbuffer = mixbuffer;
            }

            if ( mDescription.channels ) {
                *outchannels = mDescription.channels;
                memset( inbuffer, 0, *length * mDescription.channels * sizeof( float ) );
            } else if ( !mNumInputs ) {
                memset( *outbuffer, 0, *length * inchannels * sizeof( float ) );
            }

            instance = (FMOD_DSP *)this;
            mDescription.read( this, *outbuffer, inbuffer, *length, inchannels, *outchannels );

            *outbuffer = inbuffer;
            mFlags &= ~DSPI_FLAG_0x10;
        }

        float *historybuffer = mHistoryBuffer;
        if ( historybuffer ) {
            float *srcptr = *outbuffer;
            int    len    = *length;

            while ( len ) {
                int size = len;
                if ( mHistoryPosition + len > mHistoryLength ) {
                    size = mHistoryLength - mHistoryPosition;
                }

                memcpy( historybuffer + ( mHistoryPosition * *outchannels ), srcptr,
                        size * *outchannels * sizeof( float ) );

                len              -= size;
                srcptr           += size * *outchannels;
                mHistoryPosition += size;
                if ( mHistoryPosition >= mHistoryLength ) {
                    mHistoryPosition = 0;
                }
            }
        }
    } else {
        *outbuffer   = mBuffer;
        *outchannels = mBufferChannels;

        mFlags &= ~DSPI_FLAG_0x10;
    }

    mLastChannels = *outchannels;

    if ( mSystem->mFlags & FMOD_INIT_ENABLE_PROFILE ) {
        FMOD_OS_Time_GetNs( &endtime );
        mCPUUsage     = mCPUUsageTemp + ( endtime - starttime );
        mCPUUsageTemp = 0;
    }

    return result;
} // 0x8035ED10

/*
====================
DSPFilter::startBuffering

Allocates the circular history buffer for this filter unit.
====================
*/
FMOD_RESULT DSPFilter::startBuffering( unsigned int length ) {
    FMOD_RESULT          result;
    int                  channels;
    LocalCriticalSection criticalsection( mSystem->mDSPCrit );

    if ( mHistoryBuffer && length == mHistoryLength ) {
        return FMOD_OK;
    }

    criticalsection.enter();

    result = mSystem->getSoftwareFormat( NULL, NULL, &channels, NULL, NULL, NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mHistoryBuffer ) {
#line 438
        FMOD_Memory_Free( mHistoryBuffer );
    }

    mHistoryLength   = length;
    mHistoryPosition = 0;

    if ( channels < mSystem->mMaxInputChannels ) {
        channels = mSystem->mMaxInputChannels;
    }

#line 449
    mHistoryBuffer = (float *)FMOD_Memory_Calloc( mHistoryLength * channels * sizeof( float ) );
    if ( !mHistoryBuffer ) {
        return FMOD_ERR_MEMORY;
    }

    criticalsection.leave();

    return FMOD_OK;
} // 0x8035F3E4

/*
====================
DSPFilter::getHistoryBuffer

Returns the history buffer pointer, write cursor position, and total length.
====================
*/
FMOD_RESULT DSPFilter::getHistoryBuffer( float **buffer, unsigned int *position, unsigned int *length ) {
    if ( buffer ) {
        *buffer = mHistoryBuffer;
    }

    if ( position ) {
        *position = mHistoryPosition;
    }

    if ( length ) {
        *length = mHistoryLength;
    }

    return FMOD_OK;
} // 0x8035F55C

/*
====================
DSPFilter::stopBuffering

Frees the circular history buffer under the DSP critical section.
====================
*/
FMOD_RESULT DSPFilter::stopBuffering() {
    if ( mHistoryBuffer ) {
        LocalCriticalSection criticalsection( mSystem->mDSPCrit );

        criticalsection.enter();
#line 520
        FMOD_Memory_Free( mHistoryBuffer );
        mHistoryBuffer = NULL;
    }

    return FMOD_OK;
} // 0x8035F594

} // namespace FMOD
