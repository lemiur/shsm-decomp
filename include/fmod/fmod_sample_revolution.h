/*
 * FMOD::SampleRevolution: the Wii (Revolution) sample, a Sample whose buffer lives in main memory and
 * is fed to an AX voice as ADPCM (fmod_sample_revolution.cpp). Layout: Gormiti DWARF
 * (sibling.exe types SampleRevolution), size 0x15C; the vtable is __vt__Q24FMOD16SampleRevolution.
 */
#ifndef _FMOD_SAMPLE_REVOLUTION_H
#define _FMOD_SAMPLE_REVOLUTION_H

#include "fmod.h"
#include "fmod_soundi.h"

namespace FMOD {

class OutputRevolution;
class ChannelRevolution;

class SampleRevolution : public Sample {
public:
    void               *mBuffer;                // 0x140
    void               *mBufferMemory;          // 0x144
    OutputRevolution   *mOutput;                // 0x148
    unsigned int        mLocked;                // 0x14C
    int                 mLockOffset;            // 0x150, hides Sample::mLockOffset (as in the original)
    ChannelRevolution  *mChannelRevolution;     // 0x154
    bool                mOwnMemory;             // 0x158

    SampleRevolution();

    // Overrides of Sample (retail vtable __vt__Q24FMOD16SampleRevolution); the other 48 slots are inherited.
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT lockInternal( unsigned int offset, unsigned int length, void **ptr1,
                                      void **ptr2, unsigned int *len1, unsigned int *len2 );
    virtual FMOD_RESULT unlockInternal( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 );
    virtual FMOD_RESULT setBufferData( void *data );
};

} // namespace FMOD

#endif /* _FMOD_SAMPLE_REVOLUTION_H */
