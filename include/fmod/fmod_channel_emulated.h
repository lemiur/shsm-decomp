/*
 * FMOD::ChannelEmulated: the real channel of a virtual voice, tracked in software
 * (fmod_channel_emulated.cpp). It keeps a DSP head so the voice can be mixed when it becomes audible.
 * Layout: Gormiti DWARF (sibling.exe types ChannelEmulated), size 0x180; the vtable is lbl_8044BE58.
 */
#ifndef _FMOD_CHANNEL_EMULATED_H
#define _FMOD_CHANNEL_EMULATED_H

#include "fmod_channel_real.h"
#include "fmod_dspi.h"

namespace FMOD {

class ChannelEmulated : public ChannelReal {
public:
    DSPFilter   mDSPHeadMemory;             // 0x64
    DSPI       *mDSPHead;                   // 0x17C

    ChannelEmulated();                      // 0x8032C6B0

    // Overrides of the ChannelReal slots.
    virtual             ~ChannelEmulated() {}
    virtual FMOD_RESULT init( int index, SystemI *system, Output *output, DSPI *dspmixtarget );
    virtual FMOD_RESULT close();
    virtual FMOD_RESULT alloc();
    virtual FMOD_RESULT update( int delta );
    virtual FMOD_RESULT stop();
    virtual FMOD_RESULT setSpeakerLevels( int speaker, float *levels, int numlevels );
    virtual FMOD_RESULT isVirtual( bool *isvirtual );
    virtual FMOD_RESULT getDSPHead( DSPI **dsp );
};

} // namespace FMOD

#endif /* _FMOD_CHANNEL_EMULATED_H */
