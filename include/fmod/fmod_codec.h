/*
 * FMOD Ex 4.x codec public API types (fmod_codec.h).
 */
#ifndef _FMOD_CODEC_H
#define _FMOD_CODEC_H

#include "fmod.h"

typedef struct FMOD_CODEC_STATE      FMOD_CODEC_STATE;
typedef struct FMOD_CODEC_WAVEFORMAT FMOD_CODEC_WAVEFORMAT;

typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_OPENCALLBACK)        ( FMOD_CODEC_STATE *codec_state, FMOD_MODE usermode, FMOD_CREATESOUNDEXINFO *userexinfo );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_CLOSECALLBACK)       ( FMOD_CODEC_STATE *codec_state );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_READCALLBACK)        ( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_GETLENGTHCALLBACK)   ( FMOD_CODEC_STATE *codec_state, unsigned int *length, FMOD_TIMEUNIT lengthtype );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_SETPOSITIONCALLBACK) ( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, FMOD_TIMEUNIT postype );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_GETPOSITIONCALLBACK) ( FMOD_CODEC_STATE *codec_state, unsigned int *position, FMOD_TIMEUNIT postype );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_SOUNDCREATECALLBACK) ( FMOD_CODEC_STATE *codec_state, int subsound, FMOD_SOUND *sound );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_GETWAVEFORMAT)       ( FMOD_CODEC_STATE *codec_state, int index, FMOD_CODEC_WAVEFORMAT *waveformat );
typedef FMOD_RESULT (F_CALLBACK *FMOD_CODEC_METADATACALLBACK)    ( FMOD_CODEC_STATE *codec_state, FMOD_TAGTYPE tagtype, char *name, void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype, int unique );

typedef struct FMOD_CODEC_DESCRIPTION {
    const char                     *name;
    unsigned int                    version;
    int                             defaultasstream;
    FMOD_TIMEUNIT                   timeunits;
    FMOD_CODEC_OPENCALLBACK         open;
    FMOD_CODEC_CLOSECALLBACK        close;
    FMOD_CODEC_READCALLBACK         read;
    FMOD_CODEC_GETLENGTHCALLBACK    getlength;
    FMOD_CODEC_SETPOSITIONCALLBACK  setposition;
    FMOD_CODEC_GETPOSITIONCALLBACK  getposition;
    FMOD_CODEC_SOUNDCREATECALLBACK  soundcreate;
    FMOD_CODEC_GETWAVEFORMAT        getwaveformat;
} FMOD_CODEC_DESCRIPTION;

struct FMOD_CODEC_WAVEFORMAT {
    char              name[256];
    FMOD_SOUND_FORMAT format;
    int               channels;
    int               frequency;
    unsigned int      lengthbytes;
    unsigned int      lengthpcm;
    int               blockalign;
    int               loopstart;
    int               loopend;
    FMOD_MODE         mode;
    unsigned int      channelmask;
};

struct FMOD_CODEC_STATE {
    int                          numsubsounds;
    FMOD_CODEC_WAVEFORMAT       *waveformat;
    void                        *plugindata;
    void                        *filehandle;
    unsigned int                 filesize;
    FMOD_FILE_READCALLBACK       fileread;
    FMOD_FILE_SEEKCALLBACK       fileseek;
    FMOD_CODEC_METADATACALLBACK  metadata;
};

#endif /* _FMOD_CODEC_H */
