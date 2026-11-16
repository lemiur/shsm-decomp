/*
 * FMOD::TimeStamp: times a section of code (the CPU usage meter of FMOD::System). Members and layout
 * from Gormiti's DWARF (sibling.exe types TimeStamp); times are in nanoseconds as FMOD_OS_Time_GetNs
 * returns them.
 */
#ifndef _FMOD_TIME_H
#define _FMOD_TIME_H

#include <fmod/fmod.h>

namespace FMOD {

class TimeStamp {
public:
    unsigned int mIn;           // 0x00, time of stampIn
    unsigned int mOut;          // 0x04, time of stampOut
    unsigned int mPausedIn;     // 0x08, time the timing was paused
    unsigned int mPausedOut;    // 0x0C, time it was resumed
    unsigned int mTotalIn;      // 0x10, start of the period being averaged
    unsigned int mTotalOut;     // 0x14
    float        mPercent;      // 0x18, smoothed share of the period spent between stampIn and stampOut
    float        mAvTotal;      // 0x1C, smoothed length of the period
    float        mAvCurrent;    // 0x20
    float        mCPUUsage;     // 0x24
    bool         mPaused;       // 0x28
    unsigned int mPausedTotal;  // 0x2C, time spent paused in this period
    int          mPausedRefCount; // 0x30
    bool         mTiming;       // 0x34, set by stampIn

    TimeStamp();

    FMOD_RESULT stampIn();
    FMOD_RESULT stampOut( int damp );
    FMOD_RESULT setPaused( bool paused );
};

} // namespace FMOD

#endif /* _FMOD_TIME_H */
