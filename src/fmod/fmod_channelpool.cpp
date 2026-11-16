/*
 * fmod_channelpool.cpp: FMOD::ChannelPool, the pool of real channels owned by an output.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex fmod_channelpool.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_real.h>
#include <fmod/fmod_channelpool.h>
#include <fmod/fmod_memory.h>
#include <stddef.h>

// ChannelReal::mFlags bits used by ChannelPool::allocateChannel besides CHANNELREAL_FLAG_0x10 (meanings
// from use; 0x1000 is bypassed when ignorereserved is true).
#define CHANNELREAL_FLAG_0x80       0x00000080
#define CHANNELREAL_FLAG_0x100      0x00000100
#define CHANNELREAL_FLAG_RESERVED   0x00001000

namespace FMOD {

/*
====================
ChannelPool::ChannelPool

Starts empty with no channels or owner yet.
====================
*/
ChannelPool::ChannelPool() {
    mChannel      = NULL;
    mNumChannels  = 0;
    mChannelsUsed = 0;
    mSystem       = NULL;
    mOutput       = NULL;
} // 0x8033CC4C

/*
====================
ChannelPool::init

Allocates the channel pointer array for numchannels slots and binds the owning system and output.
====================
*/
FMOD_RESULT ChannelPool::init( SystemI *system, Output *output, int numchannels ) {
    if ( numchannels < 0 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( numchannels ) {
#line 64
        mChannel = (ChannelReal **)FMOD_Memory_Calloc( numchannels * sizeof( ChannelReal * ) );
        if ( !mChannel ) {
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    mNumChannels = numchannels;
    mSystem      = system;
    mOutput      = output;

    return FMOD_OK;
} // 0x8033CC68

/*
====================
ChannelPool::release

Closes every channel in the pool, frees the pointer array and frees the pool itself.
====================
*/
FMOD_RESULT ChannelPool::release() {
    if ( mChannel ) {
        for ( int count = 0; count < mNumChannels; count++ ) {
            if ( mChannel[count] ) {
                mChannel[count]->close();
            }
        }

#line 108
        FMOD_Memory_Free( mChannel );
    }

#line 111
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x8033CD0C

/*
====================
ChannelPool::allocateChannel

Allocates either numchannels free channels (index == -1) or a single specific channel by index.
====================
*/
FMOD_RESULT ChannelPool::allocateChannel( ChannelReal **realchannel, int index, int numchannels, int *found_out,
                                          bool ignorereserved ) {
    int count;
    int found = 0;

    if ( !realchannel ) {
        if ( found_out ) {
            *found_out = 0;
        }
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( index == -1 ) {
        for ( count = 0; count < mNumChannels; count++ ) {
            if ( !( mChannel[count]->mFlags & CHANNELREAL_FLAG_0x10 ) &&
                 !( mChannel[count]->mFlags & CHANNELREAL_FLAG_0x100 ) &&
                 ( !( mChannel[count]->mFlags & CHANNELREAL_FLAG_RESERVED ) || ignorereserved ) ) {
                FMOD_RESULT result;
                bool playing;

                result = mChannel[count]->isPlaying( &playing, true );
                if ( result == FMOD_OK && !playing ) {
                    mChannel[count]->mFlags |= CHANNELREAL_FLAG_0x10;
                    mChannel[count]->mFlags |= CHANNELREAL_FLAG_0x100;
                    mChannel[count]->mFlags &= ~CHANNELREAL_FLAG_0x80;
                    mChannel[count]->mFlags &= ~CHANNELREAL_FLAG_RESERVED;

                    realchannel[found] = mChannel[count];
                    found++;
                    if ( found == numchannels ) {
                        if ( found_out ) {
                            *found_out = found;
                        }
                        return FMOD_OK;
                    }
                }
            }
        }
    } else if ( index >= 0 && index < mNumChannels ) {
        if ( numchannels > 1 ) {
            return FMOD_ERR_CHANNEL_ALLOC;
        }

        mChannel[index]->mFlags |= CHANNELREAL_FLAG_0x10;
        mChannel[index]->mFlags |= CHANNELREAL_FLAG_0x100;
        mChannel[index]->mFlags &= ~CHANNELREAL_FLAG_0x80;

        *realchannel = mChannel[index];
        return FMOD_OK;
    }

    for ( count = 0; count < found; count++ ) {
        if ( realchannel[count] ) {
            realchannel[count]->mFlags &= ~CHANNELREAL_FLAG_0x10;
            realchannel[count]->mFlags &= ~CHANNELREAL_FLAG_0x100;
            realchannel[count]->mFlags |= CHANNELREAL_FLAG_0x80;
        }
    }

    if ( found_out ) {
        *found_out = found;
    }

    return FMOD_ERR_CHANNEL_ALLOC;
} // 0x8033CDD4

/*
====================
ChannelPool::getNumChannels

Returns the total number of channel slots in the pool.
====================
*/
FMOD_RESULT ChannelPool::getNumChannels( int *numchannels ) {
    if ( !numchannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numchannels = mNumChannels;

    return FMOD_OK;
} // 0x8033D00C

/*
====================
ChannelPool::getChannelsUsed

Returns the number of channels currently marked in use.
====================
*/
FMOD_RESULT ChannelPool::getChannelsUsed( int *channelsused ) {
    if ( !channelsused ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channelsused = mChannelsUsed;

    return FMOD_OK;
} // 0x8033D02C

/*
====================
ChannelPool::setChannel

Installs a channel at index, binds it to this pool and initializes it.
====================
*/
FMOD_RESULT ChannelPool::setChannel( int index, ChannelReal *channel, DSPI *dspmixtarget ) {
    if ( !channel || index < 0 || index >= mNumChannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mChannel[index]        = channel;
    mChannel[index]->mPool = this;

    return mChannel[index]->init( index, mSystem, mOutput, dspmixtarget );
} // 0x8033D04C

/*
====================
ChannelPool::getChannel

Returns the channel stored at index.
====================
*/
FMOD_RESULT ChannelPool::getChannel( int index, ChannelReal **channel ) {
    if ( !channel || index < 0 || index >= mNumChannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channel = mChannel[index];

    return FMOD_OK;
} // 0x8033D0B4

} // namespace FMOD
