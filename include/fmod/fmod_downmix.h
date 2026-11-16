/*
 * FMOD::Downmix, the speaker-mode downmixer the system holds a pointer to (fmod_systemi.cpp). The Wii
 * build never creates one (retail's pointer stays NULL); only the slots the sound card's DSP unit and
 * the system's shutdown call are declared, so the vtable is incomplete (retail has no vtable to read).
 * Layout: Gormiti DWARF (sibling.exe types Downmix), size 0x14. Method names are invented (FMOD
 * convention), except shutdown (Gormiti link map).
 */
#ifndef _FMOD_DOWNMIX_H
#define _FMOD_DOWNMIX_H

#include <fmod/fmod.h>

namespace FMOD {

class Downmix {
public:
    // The vtable pointer (0x00) sits where the first virtual is declared, ahead of the data members.
    virtual FMOD_RESULT unk_Slot0();        // slot 0, no caller in the retail DOL
    virtual FMOD_RESULT shutdown();         // slot 1
    // Mixes length samples of srcbuffer down into destbuffer.
    virtual FMOD_RESULT encode( float *srcbuffer, void *destbuffer, unsigned int length );

    // The number of channels the downmix writes (name invented); outputchannels may be NULL.
    void getOutputChannels( int *outputchannels ) {
        if ( outputchannels ) {
            *outputchannels = mNumOutputChannels;
        }
    }

    void             *mMixBuffer;           // 0x04, where the sound card's input is mixed
    int               mNumInputChannels;    // 0x08
    int               mNumOutputChannels;   // 0x0C
    FMOD_SPEAKERMODE  mOutputSpeakerMode;   // 0x10
};

} // namespace FMOD

#endif /* _FMOD_DOWNMIX_H */
