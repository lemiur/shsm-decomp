/*
 * fmod_output_emulated.cpp: FMOD::OutputEmulated, the output of a system that has no sound device.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_output_emulated.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_emulated.h>
#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_output_emulated.h>
#include <new>
#include <string.h>

namespace FMOD {

/*
====================
OutputEmulated::OutputEmulated

An output with no description and no channels yet.
====================
*/
OutputEmulated::OutputEmulated() {
    memset( &mDescription, 0, sizeof( mDescription ) );

    mChannel = NULL;
} // 0x80372534

/*
====================
OutputEmulated::init

Makes the pool of channels and the emulated channels in it.
====================
*/
FMOD_RESULT OutputEmulated::init( int maxchannels ) {
    FMOD_RESULT result;

    if ( !mSystem ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    if ( maxchannels ) {
#line 71
        mChannelPool = mChannelPool3D = new ( FMOD_Memory_Alloc( sizeof( ChannelPool ) ) ) ChannelPool;
        if ( !mChannelPool ) {
            return FMOD_ERR_MEMORY;
        }

        result = mChannelPool->init( mSystem, this, maxchannels );
        if ( result != FMOD_OK ) {
            return result;
        }

#line 83
        mChannel = (ChannelEmulated *)FMOD_Memory_Calloc( maxchannels * sizeof( ChannelEmulated ) );
        if ( !mChannel ) {
            return FMOD_ERR_MEMORY;
        }

        for ( int count = 0; count < maxchannels; count++ ) {
            new ( &mChannel[count] ) ChannelEmulated;

            mChannelPool->setChannel( count, &mChannel[count], NULL );
        }
    }

    return FMOD_OK;
} // 0x80372588

/*
====================
OutputEmulated::release

Releases the pool and the emulated channels.
====================
*/
FMOD_RESULT OutputEmulated::release() {
    if ( mChannelPool ) {
        mChannelPool->release();
        mChannelPool = NULL;
    }

    if ( mChannel ) {
#line 126
        FMOD_Memory_Free( mChannel );
        mChannel = NULL;
    }

    return Output::release();
} // 0x803726C8

/*
====================
OutputEmulated::update

Nothing to update.
====================
*/
FMOD_RESULT OutputEmulated::update() {
    return FMOD_OK;
} // 0x80372744

} // namespace FMOD
