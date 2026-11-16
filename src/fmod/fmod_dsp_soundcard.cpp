/*
 * fmod_dsp_soundcard.cpp: FMOD::DSPSoundCard, the DSP unit at the head of the DSP tree. Allocation tags
 * carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_soundcard.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_downmix.h>
#include <fmod/fmod_dsp_soundcard.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_systemi.h>

namespace FMOD {

/*
====================
DSPSoundCard::alloc

Sets the unit up. A unit whose output is not float needs a buffer to convert the mix from float in.
====================
*/
FMOD_RESULT DSPSoundCard::alloc( FMOD_DSP_DESCRIPTION_EX *description ) {
    FMOD_RESULT result;

    result = DSPI::alloc( description );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( description->mFormat == FMOD_SOUND_FORMAT_PCMFLOAT ) {
        mConversionBuffer = NULL;
    } else {
#line 48
        mConversionBufferMem = (float *)FMOD_Memory_Calloc( mSystem->mDSPBlockSize * description->channels * sizeof( float ) + 16 );
        if ( !mConversionBufferMem ) {
            return FMOD_ERR_MEMORY;
        }

        mConversionBuffer = (float *)( ( (unsigned int)mConversionBufferMem + 15 ) & ~15 );
    }

    updateTreeLevel( 0 );

    return FMOD_OK;
} // 0x803649A4

/*
====================
DSPSoundCard::release

Frees the conversion buffer.
====================
*/
FMOD_RESULT DSPSoundCard::release( bool freethis ) {
    if ( mConversionBufferMem ) {
#line 84
        FMOD_Memory_Free( mConversionBufferMem );
        mConversionBuffer    = NULL;
        mConversionBufferMem = NULL;
    }

    return DSPFilter::release( freethis );
} // 0x80364A64

/*
====================
DSPSoundCard::execute

Runs the filter. The result is downmixed when a downmixer is installed on this unit's system, then
converted to the unit's sample format if that is not float.

The output channel count goes through the inlined Downmix::getOutputChannels, which tests the
optional pointer itself: the downmix pointer is loaded before that test (Gormiti's line table puts
the load, the test and the store on one line).
====================
*/
FMOD_RESULT DSPSoundCard::execute( void *inbuffer, void **outbuffer, unsigned int *length, int inchannels,
                                   int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick ) {
    FMOD_RESULT result;

    if ( mSystem->mDownmix ) {
        inbuffer = mSystem->mDownmix->mMixBuffer;
    }

    if ( mConversionBuffer ) {
        float *readbuffer = NULL;
        float *src;

        result = DSPFilter::execute( mConversionBuffer, &readbuffer, length, inchannels, outchannels, speakermode, tick );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( mSystem->mDownmix && this == mSystem->mDSPSoundCard ) {
            src = mSystem->mDSPReadBuff[0];
            if ( readbuffer == src ) {
                src = mSystem->mDSPReadBuff[1];
            }

            mSystem->mDownmix->getOutputChannels( outchannels );

            mSystem->mDownmix->encode( readbuffer, src, *length );
        } else {
            src = readbuffer;
        }

        result = DSPI::convert( *outbuffer, src, mDescription.mFormat, FMOD_SOUND_FORMAT_PCMFLOAT, *length * *outchannels, 1, 1, 1.0f );
        if ( result != FMOD_OK ) {
            return result;
        }
    } else {
        float *readbuffer = NULL;

        result = DSPFilter::execute( (float *)inbuffer, &readbuffer, length, inchannels, outchannels, speakermode, tick );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( mSystem->mDownmix && this == mSystem->mDSPSoundCard ) {
            if ( *outbuffer == readbuffer ) {
                if ( *outbuffer == mSystem->mDSPReadBuff[0] ) {
                    *outbuffer = mSystem->mDSPReadBuff[1];
                } else {
                    *outbuffer = mSystem->mDSPReadBuff[0];
                }
            }

            mSystem->mDownmix->getOutputChannels( outchannels );

            mSystem->mDownmix->encode( readbuffer, *outbuffer, *length );
        } else {
            *outbuffer = readbuffer;
        }
    }

    mDSPTick = tick;

    return FMOD_OK;
} // 0x80364ADC

} // namespace FMOD
