/*
 * fmod_output_software.cpp: FMOD::OutputSoftware, the output that mixes every voice in software.
 * Names and layout: Gormiti link map and DWARF (fmod_output_software.obj). Allocation tags carry the original
 * file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_output_software.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_software.h>
#include <fmod/fmod_channelgroupi.h>
#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_output_software.h>
#include <fmod/fmod_sample_software.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <new>
#include <string.h>

namespace FMOD {

/*
====================
GetSampleMemoryType

The kind of memory a sample's data is allocated from (name invented): the decode buffer of a stream, or normal
memory.
====================
*/
static inline FMOD_MEMORY_TYPE GetSampleMemoryType( unsigned int mode ) {
    return ( mode & FMOD_CREATESTREAM ) ? FMOD_MEMORY_STREAM_DECODE : FMOD_MEMORY_NORMAL;
}

/*
====================
OutputSoftware::OutputSoftware

An output with the description of the software mixer and no channels yet.
====================
*/
OutputSoftware::OutputSoftware() {
    memset( &mDescription, 0, sizeof( mDescription ) );

    mDescription.name                 = "FMOD Software Output";
    mDescription.version              = 0x00010100;
    mDescription.polling              = false;
    mDescription.getsamplemaxchannels = getSampleMaxChannelsCallback;
    mDescription.mType                = FMOD_OUTPUTTYPE_SOFTWARE;
    mDescription.mSize                = sizeof( OutputSoftware );

    mChannel     = NULL;
    mChannelPool = NULL;
} // 0x80372FF8

/*
====================
OutputSoftware::init

Makes the pool of channels and the software channels in it. Every channel mixes into the master group.
====================
*/
FMOD_RESULT OutputSoftware::init( int maxchannels ) {
    FMOD_RESULT result;

    if ( !mSystem ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    if ( maxchannels ) {
#line 87
        mChannelPool = mChannelPool3D = new ( FMOD_Memory_Alloc( sizeof( ChannelPool ) ) ) ChannelPool;
        if ( !mChannelPool ) {
            return FMOD_ERR_MEMORY;
        }

        result = mChannelPool->init( mSystem, this, maxchannels );
        if ( result != FMOD_OK ) {
            return result;
        }

#line 99
        mChannel = (ChannelSoftware *)FMOD_Memory_Calloc( maxchannels * sizeof( ChannelSoftware ) );
        if ( !mChannel ) {
            return FMOD_ERR_MEMORY;
        }

        for ( int count = 0; count < maxchannels; count++ ) {
            new ( &mChannel[count] ) ChannelSoftware;

            mChannelPool->setChannel( count, &mChannel[count], mSystem->mChannelGroup->mDSPHead );
        }
    }

    return FMOD_OK;
} // 0x80373088

/*
====================
OutputSoftware::release

Releases the pool and the software channels in it.
====================
*/
FMOD_RESULT OutputSoftware::release() {
    if ( mChannelPool ) {
        mChannelPool->release();
        mChannelPool = NULL;
    }

    if ( mChannel ) {
#line 141
        FMOD_Memory_Free( mChannel );
        mChannel = NULL;
    }

    return Output::release();
} // 0x803731D4

/*
====================
OutputSoftware::createSample

Makes a software sample (or takes the one the caller passed in) and sizes its buffer for the waveformat a
codec found: the length in bytes, the room for the overflow bytes that the loop points are kept in, and the
sample data itself. A format without a bit width (a compressed one) is refused up front: GC ADPCM needs the
hardware output. The branch below for IMA ADPCM, XMA and MPEG can therefore not be reached (as shipped).
====================
*/
FMOD_RESULT OutputSoftware::createSample( unsigned int mode, FMOD_CODEC_WAVEFORMAT *waveformat, Sample **sample ) {
    FMOD_RESULT     result;
    int             bits          = 0;
    SampleSoftware *newsample     = NULL;
    unsigned int    overflowbytes = 0;
    bool            allocedhere   = false;

    if ( !sample ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( waveformat ) {
        result = SoundI::getBitsFromFormat( waveformat->format, &bits );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( !bits && waveformat->format ) {
            if ( waveformat->format == FMOD_SOUND_FORMAT_GCADPCM ) {
                return FMOD_ERR_NEEDSHARDWARE;
            }
            return FMOD_ERR_FORMAT;
        }
    }

    newsample = (SampleSoftware *)*sample;
    if ( !newsample ) {
#line 222
        newsample = new ( FMOD_Memory_Calloc( sizeof( SampleSoftware ) ) ) SampleSoftware;
        if ( !newsample ) {
            return FMOD_ERR_MEMORY;
        }

        allocedhere = true;
    }

    if ( !waveformat ) {
        *sample = newsample;
        return FMOD_OK;
    }

    newsample->mFormat = waveformat->format;

    if ( waveformat->format == FMOD_SOUND_FORMAT_IMAADPCM || waveformat->format == FMOD_SOUND_FORMAT_XMA ||
         waveformat->format == FMOD_SOUND_FORMAT_MPEG ) {
        newsample->mLengthBytes      = waveformat->lengthbytes;
        newsample->mLoopPointDataEnd = NULL;
        overflowbytes                = 0;
    } else {
        result = SoundI::getBytesFromSamples( waveformat->lengthpcm, &newsample->mLengthBytes,
                                              waveformat->channels, waveformat->format );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = SoundI::getBytesFromSamples( 4, &overflowbytes, waveformat->channels, waveformat->format );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( overflowbytes <= sizeof( newsample->mLoopPointDataEndMemory ) ) {
            newsample->mLoopPointDataEnd = newsample->mLoopPointDataEndMemory;
        } else {
#line 269
            newsample->mLoopPointDataEnd = (char *)FMOD_Memory_Calloc( overflowbytes );
            if ( !newsample->mLoopPointDataEnd ) {
                return FMOD_ERR_MEMORY;
            }
        }
    }

    if ( mode & FMOD_OPENMEMORY_POINT ) {
        newsample->mBufferMemory = NULL;
        newsample->mBuffer       = NULL;
    } else if ( ( mode & FMOD_LOADSECONDARYRAM ) && ( lbl_80529020->mMemoryTypeFlags & FMOD_MEMORY_SECONDARY ) ) {
#line 318
        newsample->mBufferMemory = FMOD_Memory_CallocType( newsample->mLengthBytes + overflowbytes * 2 + 16, FMOD_MEMORY_SECONDARY | GetSampleMemoryType( mode ) );
        if ( !newsample->mBufferMemory ) {
            if ( allocedhere ) {
#line 323
                FMOD_Memory_Free( newsample );
            }
            return FMOD_ERR_MEMORY;
        }

        newsample->mBuffer = (void *)( ( (unsigned int)newsample->mBufferMemory + overflowbytes + 15 ) & ~15 );
    } else {
#line 333
        newsample->mBufferMemory = FMOD_Memory_CallocType( newsample->mLengthBytes + overflowbytes * 2 + 16, GetSampleMemoryType( mode ) );
        if ( !newsample->mBufferMemory ) {
            if ( allocedhere ) {
#line 338
                FMOD_Memory_Free( newsample );
            }
            return FMOD_ERR_MEMORY;
        }

        newsample->mBuffer = (void *)( ( (unsigned int)newsample->mBufferMemory + overflowbytes + 15 ) & ~15 );
    }

    newsample->mFormat = waveformat->format;
    newsample->mLength = waveformat->lengthpcm;

    *sample = newsample;

    return FMOD_OK;
} // 0x80373250

/*
====================
OutputSoftware::getSampleMaxChannelsCallback

Every sample may have 16 channels.
====================
*/
int OutputSoftware::getSampleMaxChannelsCallback( FMOD_OUTPUT_STATE *output_state, unsigned int mode,
                                                  FMOD_SOUND_FORMAT format ) {
    OutputSoftware *output = (OutputSoftware *)output_state;

    return output->getSampleMaxChannels( mode, format );
} // 0x80373838

} // namespace FMOD
