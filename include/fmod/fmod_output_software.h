/*
 * FMOD::OutputSoftware, the output that mixes every voice in software (fmod_output_software.cpp): it makes
 * the pool of software channels and the samples they play.
 * Virtuals: retail vtable lbl_8044F970 + Gormiti link map (__vt__Q24FMOD14OutputSoftware)
 * Layout: Gormiti DWARF (sibling.exe types OutputSoftware), size 0xF4
 */
#ifndef _FMOD_OUTPUT_SOFTWARE_H
#define _FMOD_OUTPUT_SOFTWARE_H

#include <fmod/fmod.h>
#include <fmod/fmod_codec.h>
#include <fmod/fmod_output.h>

// The type the software output registers itself with. It is not one of the public FMOD_OUTPUTTYPE values:
// the constant is the retail constructor's, the name is invented.
#define FMOD_OUTPUTTYPE_SOFTWARE ( (FMOD_OUTPUTTYPE)-2 )

namespace FMOD {

class ChannelSoftware;
class Sample;

class OutputSoftware : public Output {
public:
    ChannelSoftware *mChannel;          // 0xF0, the array of software channels

    OutputSoftware();                   // 0x80372FF8

    // Override of Output (vtable lbl_8044F970; getFreeChannel is inherited).
    virtual FMOD_RESULT release();      // 0x803731D4
    // Makes the pool of channels and the software channels in it.
    virtual FMOD_RESULT init( int maxchannels );    // 0x80373088

    // Makes a sample, or sets up one that is passed in, for the waveformat a codec found.
    FMOD_RESULT createSample( unsigned int mode, FMOD_CODEC_WAVEFORMAT *waveformat, Sample **sample ); // 0x80373250

    // How many channels a sample of this mode and format has at most: 16 whatever they are (parameter names
    // from the callback's signature, the member's from Gormiti's link map).
    // no copy in retail (inlined)
    int getSampleMaxChannels( unsigned int mode, FMOD_SOUND_FORMAT format ) {
        return 16;
    }

    // The callback registered in the description: the output is the state's owner.
    static int F_CALLBACK getSampleMaxChannelsCallback( FMOD_OUTPUT_STATE *output_state, unsigned int mode,
                                                        FMOD_SOUND_FORMAT format );     // 0x80373838
};

} // namespace FMOD

#endif /* _FMOD_OUTPUT_SOFTWARE_H */
