/*
 * FMOD::ChannelPool: the channels of an output (fmod_channelpool.cpp).
 * Layout: Gormiti DWARF (sibling.exe types ChannelPool), size 0x14. No vtable.
 */
#ifndef _FMOD_CHANNELPOOL_H
#define _FMOD_CHANNELPOOL_H

#include <fmod/fmod.h>

namespace FMOD {

class ChannelReal;
class DSPI;
class Output;
class SystemI;

class ChannelPool {
public:
    int            mNumChannels;        // 0x00
    int            mChannelsUsed;       // 0x04
    SystemI       *mSystem;             // 0x08
    Output        *mOutput;             // 0x0C
    ChannelReal  **mChannel;            // 0x10

    ChannelPool();                      // 0x8033CC4C
    FMOD_RESULT init( SystemI *system, Output *output, int numchannels );
    FMOD_RESULT release();
    FMOD_RESULT allocateChannel( ChannelReal **realchannel, int index, int numchannels, int *found_out,
                                 bool ignorereserved );
    FMOD_RESULT getNumChannels( int *numchannels );
    FMOD_RESULT getChannelsUsed( int *channelsused );
    FMOD_RESULT setChannel( int index, ChannelReal *channel, DSPI *dspmixtarget );
    FMOD_RESULT getChannel( int index, ChannelReal **channel );
};

} // namespace FMOD

#endif /* _FMOD_CHANNELPOOL_H */
