/*
 * FMOD::SampleSoftware: software-mixed sample in main memory (fmod_sample_software.cpp).
 * Layout: Gormiti DWARF (sibling.exe types SampleSoftware), size 0x158; the vtable is
 * __vt__Q24FMOD14SampleSoftware (lbl_8044CF60).
 */
#ifndef _FMOD_SAMPLE_SOFTWARE_H
#define _FMOD_SAMPLE_SOFTWARE_H

#include "fmod.h"
#include "fmod_soundi.h"

#ifndef FMOD_LOADSECONDARYRAM
#define FMOD_LOADSECONDARYRAM 0x20000000
#endif


namespace FMOD {

class SampleSoftware : public Sample {
public:
    void         *mBuffer;                      // 0x140
    void         *mBufferMemory;                // 0x144
    char         *mLoopPointDataEnd;            // 0x148
    char          mLoopPointDataEndMemory[8];   // 0x14C
    unsigned int  mLoopPointDataEndOffset;      // 0x154

    SampleSoftware();

    // Overrides of Sample (retail vtable lbl_8044CF60); the other 47 slots are inherited.
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT setMode( unsigned int mode );
    virtual FMOD_RESULT lockInternal( unsigned int offset, unsigned int length, void **ptr1,
                                      void **ptr2, unsigned int *len1, unsigned int *len2 );
    virtual FMOD_RESULT unlockInternal( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 );
    virtual FMOD_RESULT setBufferData( void *data );

    FMOD_RESULT setLoopPointData();
    FMOD_RESULT restoreLoopPointData();
};

} // namespace FMOD

#endif /* _FMOD_SAMPLE_SOFTWARE_H */
