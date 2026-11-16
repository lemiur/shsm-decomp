/*
 * FMOD::ChannelRevolution: the Wii (Revolution) real channel, one AX voice (fmod_channel_revolution.cpp).
 * Layout: Gormiti DWARF (sibling.exe types ChannelRevolution), size 0xA8.
 */
#ifndef _FMOD_CHANNEL_REVOLUTION_H
#define _FMOD_CHANNEL_REVOLUTION_H

#include "fmod_channel_realmanual3d.h"

struct _AXVPB;

namespace FMOD {

class OutputRevolution;

class ChannelRevolution : public ChannelRealManual3D {
public:
    OutputRevolution   *mOutput;                // 0x68
    _AXVPB             *mVoice;                 // 0x6C
    bool                mVoiceDropped;          // 0x70
    unsigned char       field_0x71[0x74 - 0x71];
    int                 mVolume;                // 0x74
    int                 mLastVolume;            // 0x78
    float               mLastFrequency;         // 0x7C
    float               mLastPan;               // 0x80
    float               mLastFBPan;             // 0x84
    bool                mPaused;                // 0x88
    bool                mStartPending;          // 0x89
    bool                mStopPending;           // 0x8A
    bool                mSetPositionPending;    // 0x8B
    int                 mAUXALevel;             // 0x8C
    int                 mAUXBLevel;             // 0x90
    int                 mAUXCLevel;             // 0x94
    int                 mVoicePriority;         // 0x98
    unsigned int        mStartPosition;         // 0x9C
    unsigned int        mSeekAddress;           // 0xA0
    unsigned int        mLastControllerSpeaker; // 0xA4
};

} // namespace FMOD

#endif /* _FMOD_CHANNEL_REVOLUTION_H */
