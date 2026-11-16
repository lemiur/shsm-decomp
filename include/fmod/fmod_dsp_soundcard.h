/*
 * FMOD::DSPSoundCard, the DSP unit at the head of the DSP tree: it runs the filter and hands the mix
 * to the output, converted to the output's sample format when that is not float
 * (fmod_dsp_soundcard.cpp).
 * Virtuals: retail vtable __vt__Q24FMOD12DSPSoundCard + Gormiti link map
 * Layout: Gormiti DWARF (sibling.exe types DSPSoundCard), size 0x120
 */
#ifndef _FMOD_DSP_SOUNDCARD_H
#define _FMOD_DSP_SOUNDCARD_H

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>

namespace FMOD {

class DSPSoundCard : public DSPFilter {
public:
    float *mConversionBuffer;                   // 0x118, 16-byte aligned inside the allocation below
    float *mConversionBufferMem;                // 0x11C

    // Overrides of the slots of DSPI (vtable __vt__Q24FMOD12DSPSoundCard).
    virtual FMOD_RESULT execute( void *inbuffer, void **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick );
    virtual FMOD_RESULT alloc( FMOD_DSP_DESCRIPTION_EX *description );
    virtual FMOD_RESULT release( bool freethis );
};

} // namespace FMOD

#endif /* _FMOD_DSP_SOUNDCARD_H */
