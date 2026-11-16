/*
 * FMOD::OutputEmulated, the output that mixes nothing: it only provides emulated channels, so the
 * system can track virtual voices (fmod_output_emulated.cpp).
 * Virtuals: retail vtable __vt__Q24FMOD14OutputEmulated + Gormiti link map
 * Layout: Gormiti DWARF (sibling.exe types OutputEmulated), size 0xF4
 */
#ifndef _FMOD_OUTPUT_EMULATED_H
#define _FMOD_OUTPUT_EMULATED_H

#include <fmod/fmod.h>
#include <fmod/fmod_output.h>

namespace FMOD {

class ChannelEmulated;

class OutputEmulated : public Output {
public:
    ChannelEmulated *mChannel;          // 0xF0, the array of emulated channels

    OutputEmulated();                   // 0x80372534

    FMOD_RESULT init( int maxchannels );
    FMOD_RESULT update();

    // Override of Output (vtable __vt__Q24FMOD14OutputEmulated; getFreeChannel is inherited).
    virtual FMOD_RESULT release();
};

} // namespace FMOD

#endif /* _FMOD_OUTPUT_EMULATED_H */
