/*
 * FMOD::ChannelGroupI, the internal channel group (fmod.cpp). Only the DSP mix target is named.
 * Layout: Gormiti DWARF (sibling.exe types ChannelGroupI), size 0x5C.
 */
#ifndef _FMOD_CHANNELGROUPI_H
#define _FMOD_CHANNELGROUPI_H

#include "fmod.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class ChannelGroup;
class Channel;
class DSPI;
class DSPConnectionI;
class SystemI;

class ChannelGroupI : public LinkedListNode {
public:
    SystemI        *mSystem;                // 0x0C
    void           *mUserData;              // 0x10
    DSPI           *mDSPHead;               // 0x14
    DSPI           *mDSPMixTarget;          // 0x18
    ChannelGroupI  *mParent;                // 0x1C
    ChannelGroupI  *mGroupHead;             // 0x20
    LinkedListNode  mChannelHead;           // 0x24
    int             mNumChannels;           // 0x30
    char           *mName;                  // 0x34
    float           mVolume;                // 0x38
    float           mRealVolume;            // 0x3C
    float           mDirectOcclusion;       // 0x40
    float           mReverbOcclusion;       // 0x44
    float           mRealDirectOcclusionVolume; // 0x48
    float           mRealReverbOcclusionVolume; // 0x4C
    float           mPitch;                 // 0x50
    float           mRealPitch;             // 0x54
    bool            mMute;                  // 0x58
    bool            mPaused;                // 0x59

    // Inline constructor; the volumes and pitches start at 1, the occlusions at 0 (MusicSong::play builds
    // its group in place with it).
    ChannelGroupI() {
        mVolume = mRealVolume = 1.0f;
        mPitch = mRealPitch = 1.0f;
        mDirectOcclusion = mReverbOcclusion = 0.0f;
        mRealDirectOcclusionVolume = mRealReverbOcclusionVolume = 1.0f;
    }

    // Called by the public FMOD::ChannelGroup methods (fmod_channelgroup.cpp).
    static FMOD_RESULT validate( ChannelGroup *channelgroup, ChannelGroupI **channelgroupi );

    FMOD_RESULT release();
    FMOD_RESULT releaseInternal( bool releasechildren );
    FMOD_RESULT setVolume( float volume );
    FMOD_RESULT setVolumeInternal();
    FMOD_RESULT getVolume( float *volume );
    FMOD_RESULT setPitch( float pitch );
    FMOD_RESULT setPitchInternal();
    // Second flag of setPaused and setMute (name invented): when set, the group keeps the new state itself;
    // the child groups are called with false.
    FMOD_RESULT setPaused( bool paused, bool storeState );
    FMOD_RESULT getPaused( bool *paused );
    FMOD_RESULT setMute( bool mute, bool storeState );
    FMOD_RESULT getMute( bool *mute );
    FMOD_RESULT stop();
    FMOD_RESULT overrideReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    FMOD_RESULT overrideSpeakerMix( float frontleft, float frontright, float center, float lfe,
                                    float backleft, float backright, float sideleft, float sideright );
    FMOD_RESULT updateChildMixTarget( DSPI *dsp );
    FMOD_RESULT addGroup( ChannelGroupI *group );
    FMOD_RESULT getDSPHead( DSPI **dsp );
    FMOD_RESULT addDSP( DSPI *dsp, DSPConnectionI **connection );
    FMOD_RESULT getNumChannels( int *numchannels );
    FMOD_RESULT getChannel( int index, Channel **channel );
};

} // namespace FMOD

#endif /* _FMOD_CHANNELGROUPI_H */
