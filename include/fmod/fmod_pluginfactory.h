/*
 * FMOD::PluginFactory: registers the DSP, codec and output units of a system (fmod_pluginfactory.cpp).
 * Only the members used so far are declared; the size is 0x2A8 (Gormiti DWARF, sibling.exe types
 * PluginFactory).
 */
#ifndef _FMOD_PLUGINFACTORY_H
#define _FMOD_PLUGINFACTORY_H

#include <fmod/fmod.h>

namespace FMOD {

class DSPI;
class FMOD_DSP_DESCRIPTION_EX;
class FMOD_OUTPUT_DESCRIPTION_EX;

class PluginFactory {
public:
    FMOD_RESULT registerOutput( FMOD_OUTPUT_DESCRIPTION_EX *description, unsigned int *handle );

    // The DSP units of the factory, by index and by handle (fmod_reverbi.cpp finds its reverb unit this way).
    FMOD_RESULT getNumDSPs( int *numdsps );
    FMOD_RESULT getDSPHandle( int index, unsigned int *handle );
    FMOD_RESULT getDSP( unsigned int handle, FMOD_DSP_DESCRIPTION_EX **description );
    FMOD_RESULT createDSP( FMOD_DSP_DESCRIPTION_EX *description, DSPI **dsp );   // 0x8033F704
};

} // namespace FMOD

#endif /* _FMOD_PLUGINFACTORY_H */
