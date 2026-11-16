/*
 * FMOD::SpeakerLevelsPool: the system's pool of speaker level buffers, one per real channel
 * (fmod_speakerlevels_pool.cpp). A buffer holds one level per input and output channel.
 * Layout: Gormiti DWARF (sibling.exe types SpeakerLevelsPool, LevelsInfo), size 0x8. No vtable.
 */
#ifndef _FMOD_SPEAKERLEVELS_POOL_H
#define _FMOD_SPEAKERLEVELS_POOL_H

#include <fmod/fmod.h>

namespace FMOD {

class SystemI;

// One entry of the pool (name from the Gormiti DWARF).
class LevelsInfo {
public:
    bool    inuse;                      // 0x00
    float  *levelsmemory;               // 0x04, NULL until the entry is first handed out
};

class SpeakerLevelsPool {
public:
    LevelsInfo *mLevelsPool;            // 0x00, one entry per real channel
    SystemI    *mSystem;                // 0x04

    SpeakerLevelsPool();                // 0x80349CFC
    FMOD_RESULT alloc( float **levels );
    FMOD_RESULT free( float *levels );
    FMOD_RESULT release();
};

} // namespace FMOD

#endif /* _FMOD_SPEAKERLEVELS_POOL_H */
