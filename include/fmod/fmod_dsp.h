/*
 * FMOD Ex public DSP types (fmod_dsp.h of the FMOD Ex programmer's API): the plugin description a
 * DSP unit is created from, and the state it is run with. Callbacks are declared as in the public
 * API; the ones nothing here sets are left opaque.
 */
#ifndef _FMOD_DSP_H
#define _FMOD_DSP_H

#include <fmod/fmod.h>

typedef struct FMOD_DSP FMOD_DSP;
typedef struct FMOD_DSP_PARAMETERDESC FMOD_DSP_PARAMETERDESC;
typedef struct FMOD_DSP_STATE FMOD_DSP_STATE;

/* FMOD_DSP_CATEGORY (internal: Gormiti DWARF) and FMOD_DSP_TYPE (public API; values: Gormiti DWARF). */
typedef enum {
    FMOD_DSP_CATEGORY_FILTER,
    FMOD_DSP_CATEGORY_DSPCODECMPEG,
    FMOD_DSP_CATEGORY_DSPCODECADPCM,
    FMOD_DSP_CATEGORY_DSPCODECXMA,
    FMOD_DSP_CATEGORY_DSPCODECRAW,
    FMOD_DSP_CATEGORY_SOUNDCARD,
    FMOD_DSP_CATEGORY_WAVETABLE,
    FMOD_DSP_CATEGORY_RESAMPLER
} FMOD_DSP_CATEGORY;

typedef enum {
    FMOD_DSP_TYPE_UNKNOWN,
    FMOD_DSP_TYPE_MIXER,
    FMOD_DSP_TYPE_OSCILLATOR,
    FMOD_DSP_TYPE_LOWPASS,
    FMOD_DSP_TYPE_ITLOWPASS,
    FMOD_DSP_TYPE_HIGHPASS,
    FMOD_DSP_TYPE_ECHO,
    FMOD_DSP_TYPE_FLANGE,
    FMOD_DSP_TYPE_DISTORTION,
    FMOD_DSP_TYPE_NORMALIZE,
    FMOD_DSP_TYPE_PARAMEQ,
    FMOD_DSP_TYPE_PITCHSHIFT,
    FMOD_DSP_TYPE_CHORUS,
    FMOD_DSP_TYPE_REVERB,
    FMOD_DSP_TYPE_VSTPLUGIN,
    FMOD_DSP_TYPE_WINAMPPLUGIN,
    FMOD_DSP_TYPE_ITECHO,
    FMOD_DSP_TYPE_COMPRESSOR,
    FMOD_DSP_TYPE_SFXREVERB,
    FMOD_DSP_TYPE_LOWPASS_SIMPLE,
    FMOD_DSP_TYPE_DELAY,
    FMOD_DSP_TYPE_TREMOLO,
    FMOD_DSP_TYPE_FORCEINT = 65536
} FMOD_DSP_TYPE;

struct FMOD_DSP_STATE {
    FMOD_DSP       *instance;
    void           *plugindata;
    unsigned short  speakermask;
};

typedef struct FMOD_DSP_DESCRIPTION {
    char                     name[32];
    unsigned int             version;
    int                      channels;
    void                    *create;
    void                    *release;
    void                    *reset;
    FMOD_RESULT            (*read)( FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer,
                                    unsigned int length, int inchannels, int outchannels );
    void                    *setposition;
    int                      numparameters;
    FMOD_DSP_PARAMETERDESC  *paramdesc;
    void                    *setparameter;
    void                    *getparameter;
    void                    *config;
    int                      configwidth;
    int                      configheight;
    void                    *userdata;
} FMOD_DSP_DESCRIPTION;

#endif /* _FMOD_DSP_H */
