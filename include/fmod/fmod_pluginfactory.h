/*
 * FMOD::PluginFactory: registers the DSP, codec and output units of a system and creates instances of
 * them (fmod_pluginfactory.cpp).
 * Layout: Gormiti DWARF (sibling.exe types PluginFactory), size 0x2A8.
 */
#ifndef _FMOD_PLUGINFACTORY_H
#define _FMOD_PLUGINFACTORY_H

#include <fmod/fmod.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_output.h>

namespace FMOD {

class SystemI;

class PluginFactory {
public:
    char                        mPluginPath[256];       // 0x00
    FMOD_DSP_DESCRIPTION_EX     mDSPHead;               // 0x100, the head of the list of DSP units
    FMOD_CODEC_DESCRIPTION_EX   mCodecHead;             // 0x18C, the head of the list of codecs
    FMOD_OUTPUT_DESCRIPTION_EX  mOutputHead;            // 0x1FC, the head of the list of outputs
    SystemI                    *mSystem;                // 0x2A0
    unsigned int                mCurrentPluginHandle;   // 0x2A4

    PluginFactory();                                    // 0x8033E85C

    FMOD_RESULT release();
    FMOD_RESULT setSystem( SystemI *system );
    FMOD_RESULT setPluginPath( const char *path );
    FMOD_RESULT unloadPlugin( unsigned int handle );

    FMOD_RESULT registerCodec( FMOD_CODEC_DESCRIPTION_EX *description, unsigned int *handle, unsigned int priority );
    FMOD_RESULT registerOutput( FMOD_OUTPUT_DESCRIPTION_EX *description, unsigned int *handle );

    FMOD_RESULT getNumCodecs( int *numcodecs );
    // The DSP units of the factory, by index and by handle (fmod_reverbi.cpp finds its reverb unit this way).
    FMOD_RESULT getNumDSPs( int *numdsps );
    FMOD_RESULT getNumOutputs( int *numoutputs );
    FMOD_RESULT getCodecHandle( int index, unsigned int *handle );
    FMOD_RESULT getDSPHandle( int index, unsigned int *handle );
    FMOD_RESULT getOutputHandle( int index, unsigned int *handle );
    FMOD_RESULT getCodec( unsigned int handle, FMOD_CODEC_DESCRIPTION_EX **description );
    FMOD_RESULT getDSP( unsigned int handle, FMOD_DSP_DESCRIPTION_EX **description );
    FMOD_RESULT getOutput( unsigned int handle, FMOD_OUTPUT_DESCRIPTION_EX **description );

    FMOD_RESULT createCodec( FMOD_CODEC_DESCRIPTION_EX *codecdesc, Codec **codec );
    FMOD_RESULT createDSP( FMOD_DSP_DESCRIPTION_EX *dspdesc, DSPI **dsp );
    FMOD_RESULT createOutput( FMOD_OUTPUT_DESCRIPTION_EX *outputdesc, Output **output );
};

} // namespace FMOD

#endif /* _FMOD_PLUGINFACTORY_H */
