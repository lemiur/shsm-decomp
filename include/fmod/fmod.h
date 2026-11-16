/*
 * FMOD Ex 4.x public C API: the declarations this project needs, written from the public API
 * (fmod.h of the FMOD Ex programmer's API). Only what reconstructed units use is declared.
 */
#ifndef _FMOD_H
#define _FMOD_H

#define F_API
#define F_CALLBACK

/*
 * Error codes. Every FMOD function returns one. The values below run in the public header's order
 * from FMOD_OK; retail confirms FMOD_ERR_INITIALIZED (32), FMOD_ERR_INVALID_PARAM (37) and
 * FMOD_ERR_MEMORY (44). The remaining codes follow the public header (Gormiti DWARF, sibling.exe types FMOD_RESULT).
 */
typedef enum {
    FMOD_OK,
    FMOD_ERR_ALREADYLOCKED,
    FMOD_ERR_BADCOMMAND,
    FMOD_ERR_CDDA_DRIVERS,
    FMOD_ERR_CDDA_INIT,
    FMOD_ERR_CDDA_INVALID_DEVICE,
    FMOD_ERR_CDDA_NOAUDIO,
    FMOD_ERR_CDDA_NODEVICES,
    FMOD_ERR_CDDA_NODISC,
    FMOD_ERR_CDDA_READ,
    FMOD_ERR_CHANNEL_ALLOC,
    FMOD_ERR_CHANNEL_STOLEN,
    FMOD_ERR_COM,
    FMOD_ERR_DMA,
    FMOD_ERR_DSP_CONNECTION,
    FMOD_ERR_DSP_FORMAT,
    FMOD_ERR_DSP_NOTFOUND,
    FMOD_ERR_DSP_RUNNING,
    FMOD_ERR_DSP_TOOMANYCONNECTIONS,
    FMOD_ERR_FILE_BAD,
    FMOD_ERR_FILE_COULDNOTSEEK,
    FMOD_ERR_FILE_DISKEJECTED,
    FMOD_ERR_FILE_EOF,
    FMOD_ERR_FILE_NOTFOUND,
    FMOD_ERR_FILE_UNWANTED,
    FMOD_ERR_FORMAT,
    FMOD_ERR_HTTP,
    FMOD_ERR_HTTP_ACCESS,
    FMOD_ERR_HTTP_PROXY_AUTH,
    FMOD_ERR_HTTP_SERVER_ERROR,
    FMOD_ERR_HTTP_TIMEOUT,
    FMOD_ERR_INITIALIZATION,
    FMOD_ERR_INITIALIZED,           /* 32 */
    FMOD_ERR_INTERNAL,
    FMOD_ERR_INVALID_ADDRESS,
    FMOD_ERR_INVALID_FLOAT,
    FMOD_ERR_INVALID_HANDLE,
    FMOD_ERR_INVALID_PARAM,         /* 37 */
    FMOD_ERR_INVALID_POSITION,
    FMOD_ERR_INVALID_SPEAKER,
    FMOD_ERR_INVALID_SYNCPOINT,
    FMOD_ERR_INVALID_VECTOR,
    FMOD_ERR_IRX,
    FMOD_ERR_MAXAUDIBLE,
    FMOD_ERR_MEMORY,                /* 44 */
    FMOD_ERR_MEMORY_CANTPOINT,
    FMOD_ERR_MEMORY_IOP,
    FMOD_ERR_MEMORY_SRAM,
    FMOD_ERR_NEEDS2D,
    FMOD_ERR_NEEDS3D,
    FMOD_ERR_NEEDSHARDWARE,
    FMOD_ERR_NEEDSSOFTWARE,
    FMOD_ERR_NET_CONNECT,
    FMOD_ERR_NET_SOCKET_ERROR,
    FMOD_ERR_NET_URL,
    FMOD_ERR_NET_WOULD_BLOCK,
    FMOD_ERR_NOTREADY,
    FMOD_ERR_OUTPUT_ALLOCATED,
    FMOD_ERR_OUTPUT_CREATEBUFFER,
    FMOD_ERR_OUTPUT_DRIVERCALL,
    FMOD_ERR_OUTPUT_ENUMERATION,
    FMOD_ERR_OUTPUT_FORMAT,
    FMOD_ERR_OUTPUT_INIT,
    FMOD_ERR_OUTPUT_NOHARDWARE,
    FMOD_ERR_OUTPUT_NOSOFTWARE,
    FMOD_ERR_PAN,
    FMOD_ERR_PLUGIN,
    FMOD_ERR_PLUGIN_INSTANCES,
    FMOD_ERR_PLUGIN_MISSING,
    FMOD_ERR_PLUGIN_RESOURCE,
    FMOD_ERR_PRELOADED,
    FMOD_ERR_PROGRAMMERSOUND,
    FMOD_ERR_RECORD,
    FMOD_ERR_REVERB_INSTANCE,
    FMOD_ERR_SUBSOUND_ALLOCATED,
    FMOD_ERR_SUBSOUND_CANTMOVE,
    FMOD_ERR_SUBSOUND_MODE,
    FMOD_ERR_SUBSOUNDS,
    FMOD_ERR_TAGNOTFOUND,
    FMOD_ERR_TOOMANYCHANNELS,
    FMOD_ERR_UNIMPLEMENTED,
    FMOD_ERR_UNINITIALIZED,
    FMOD_ERR_UNSUPPORTED,
    FMOD_ERR_UPDATE,
    FMOD_ERR_VERSION,
    FMOD_ERR_EVENT_FAILED,
    FMOD_ERR_EVENT_INFOONLY,
    FMOD_ERR_EVENT_INTERNAL,
    FMOD_ERR_EVENT_MAXSTREAMS,
    FMOD_ERR_EVENT_MISMATCH,
    FMOD_ERR_EVENT_NAMECONFLICT,
    FMOD_ERR_EVENT_NOTFOUND,
    FMOD_ERR_EVENT_NEEDSSIMPLE,
    FMOD_ERR_EVENT_GUIDCONFLICT,
    FMOD_ERR_EVENT_ALREADY_LOADED,
    FMOD_ERR_MUSIC_UNINITIALIZED
} FMOD_RESULT;

typedef struct FMOD_SYSTEM    FMOD_SYSTEM;
typedef struct FMOD_SOUND     FMOD_SOUND;
typedef struct FMOD_CHANNEL   FMOD_CHANNEL;
typedef struct FMOD_SYNCPOINT FMOD_SYNCPOINT;
typedef enum {
    FMOD_TAGTYPE_UNKNOWN = 0,
    FMOD_TAGTYPE_ID3V1,
    FMOD_TAGTYPE_ID3V2,
    FMOD_TAGTYPE_VORBISCOMMENT,
    FMOD_TAGTYPE_SHOUTCAST,
    FMOD_TAGTYPE_ICECAST,
    FMOD_TAGTYPE_ASF,
    FMOD_TAGTYPE_MIDI,
    FMOD_TAGTYPE_PLAYLIST,
    FMOD_TAGTYPE_FMOD,
    FMOD_TAGTYPE_USER,
    FMOD_TAGTYPE_MAX,
    FMOD_TAGTYPE_FORCEINT = 65536
} FMOD_TAGTYPE;

typedef enum {
    FMOD_TAGDATATYPE_BINARY = 0,
    FMOD_TAGDATATYPE_INT,
    FMOD_TAGDATATYPE_FLOAT,
    FMOD_TAGDATATYPE_STRING,
    FMOD_TAGDATATYPE_STRING_UTF16,
    FMOD_TAGDATATYPE_STRING_UTF16BE,
    FMOD_TAGDATATYPE_STRING_UTF8,
    FMOD_TAGDATATYPE_CDTOC,
    FMOD_TAGDATATYPE_MAX,
    FMOD_TAGDATATYPE_FORCEINT = 65536
} FMOD_TAGDATATYPE;

typedef struct FMOD_TAG {
    FMOD_TAGTYPE     type;
    FMOD_TAGDATATYPE datatype;
    char            *name;
    void            *data;
    unsigned int     datalen;
    int              updated;
} FMOD_TAG;

typedef struct FMOD_VECTOR {
    float x;
    float y;
    float z;
} FMOD_VECTOR;

/* ---- added for climax/Audio/Source: the channel argument of System::playSound (FMOD Ex public API values) ---- */
typedef enum {
    FMOD_CHANNEL_FREE  = -1,
    FMOD_CHANNEL_REUSE = -2
} FMOD_CHANNELINDEX;

typedef unsigned int FMOD_TIMEUNIT;
typedef unsigned int FMOD_MODE;
typedef unsigned int FMOD_INITFLAGS;

/* FMOD_MODE bit tested by the emulated channel: virtual voices restart from the beginning. */
#define FMOD_VIRTUAL_PLAYFROMSTART 0x80000000
/* Loop modes (FMOD_MODE). */
#define FMOD_LOOP_OFF              0x00000001
#define FMOD_LOOP_NORMAL           0x00000002
#define FMOD_LOOP_BIDI             0x00000004
/* ---- added for fmod_eventimpl_simple: FMOD_MODE bits (FMOD Ex public API values) ---- */
#define FMOD_3D                    0x00000010
#define FMOD_HARDWARE              0x00000020
#define FMOD_SOFTWARE              0x00000040
#define FMOD_CREATESTREAM          0x00000080
#define FMOD_CREATECOMPRESSEDSAMPLE 0x00000200
/* ---- added for fmod_eventgroupi: FMOD_MODE bit (FMOD Ex public API value) ---- */
#define FMOD_2D                    0x00000008
/* ---- added for fmod_channel_real: 3D mode bits (FMOD Ex public API values) ---- */
#define FMOD_3D_HEADRELATIVE       0x00040000
#define FMOD_3D_WORLDRELATIVE      0x00080000
#define FMOD_3D_LOGROLLOFF         0x00100000
#define FMOD_3D_LINEARROLLOFF      0x00200000
#define FMOD_3D_CUSTOMROLLOFF      0x04000000
#define FMOD_3D_IGNOREGEOMETRY     0x40000000
/* FMOD_INITFLAGS bit: no software mixing. */
#define FMOD_INIT_SOFTWARE_DISABLE 0x00000004
/* FMOD_INITFLAGS bit: the DSP units time themselves (CPU usage per unit). */
#define FMOD_INIT_ENABLE_PROFILE 0x00000020
/* FMOD_INITFLAGS bit: the mixer runs from System::update instead of its own thread (Wii). */
#define FMOD_INIT_SYNCMIXERWITHUPDATE 0x00400000

typedef enum {
    FMOD_DSP_RESAMPLER_NOINTERP,
    FMOD_DSP_RESAMPLER_LINEAR,
    FMOD_DSP_RESAMPLER_CUBIC,
    FMOD_DSP_RESAMPLER_SPLINE,
    FMOD_DSP_RESAMPLER_MAX,
    FMOD_DSP_RESAMPLER_FORCEINT = 65536
} FMOD_DSP_RESAMPLER;

typedef enum {
    FMOD_SOUND_FORMAT_NONE,
    FMOD_SOUND_FORMAT_PCM8,
    FMOD_SOUND_FORMAT_PCM16,
    FMOD_SOUND_FORMAT_PCM24,
    FMOD_SOUND_FORMAT_PCM32,
    FMOD_SOUND_FORMAT_PCMFLOAT, /* 5, the format of an FMOD DSP unit */
    FMOD_SOUND_FORMAT_GCADPCM,
    FMOD_SOUND_FORMAT_IMAADPCM,
    FMOD_SOUND_FORMAT_VAG,
    FMOD_SOUND_FORMAT_XMA,
    FMOD_SOUND_FORMAT_MPEG,
    FMOD_SOUND_FORMAT_MAX,
    FMOD_SOUND_FORMAT_FORCEINT = 65536
} FMOD_SOUND_FORMAT;

/* Output types of FMOD_System_SetOutput. */
typedef enum {
    FMOD_OUTPUTTYPE_AUTODETECT,
    FMOD_OUTPUTTYPE_UNKNOWN,
    FMOD_OUTPUTTYPE_NOSOUND,
    FMOD_OUTPUTTYPE_WAVWRITER,
    FMOD_OUTPUTTYPE_NOSOUND_NRT,
    FMOD_OUTPUTTYPE_WAVWRITER_NRT,
    FMOD_OUTPUTTYPE_DSOUND,
    FMOD_OUTPUTTYPE_WINMM,
    FMOD_OUTPUTTYPE_OPENAL,
    FMOD_OUTPUTTYPE_WASAPI,
    FMOD_OUTPUTTYPE_ASIO,
    FMOD_OUTPUTTYPE_OSS,
    FMOD_OUTPUTTYPE_ALSA,
    FMOD_OUTPUTTYPE_ESD,
    FMOD_OUTPUTTYPE_SOUNDMANAGER,
    FMOD_OUTPUTTYPE_COREAUDIO,
    FMOD_OUTPUTTYPE_XBOX,
    FMOD_OUTPUTTYPE_PS2,
    FMOD_OUTPUTTYPE_PS3,
    FMOD_OUTPUTTYPE_GC,
    FMOD_OUTPUTTYPE_XBOX360,
    FMOD_OUTPUTTYPE_PSP,
    FMOD_OUTPUTTYPE_WII,
    FMOD_OUTPUTTYPE_MAX
} FMOD_OUTPUTTYPE;

typedef enum {
    FMOD_SOUND_TYPE_UNKNOWN,
    FMOD_SOUND_TYPE_AAC,
    FMOD_SOUND_TYPE_AIFF,
    FMOD_SOUND_TYPE_ASF,
    FMOD_SOUND_TYPE_AT3,
    FMOD_SOUND_TYPE_CDDA,
    FMOD_SOUND_TYPE_DLS,
    FMOD_SOUND_TYPE_FLAC,
    FMOD_SOUND_TYPE_FSB,
    FMOD_SOUND_TYPE_GCADPCM,
    FMOD_SOUND_TYPE_IT,
    FMOD_SOUND_TYPE_MIDI,
    FMOD_SOUND_TYPE_MOD,
    FMOD_SOUND_TYPE_MPEG,
    FMOD_SOUND_TYPE_OGGVORBIS,
    FMOD_SOUND_TYPE_PLAYLIST,
    FMOD_SOUND_TYPE_RAW,
    FMOD_SOUND_TYPE_S3M,
    FMOD_SOUND_TYPE_SF2,
    FMOD_SOUND_TYPE_USER,
    FMOD_SOUND_TYPE_WAV,
    FMOD_SOUND_TYPE_XM,
    FMOD_SOUND_TYPE_XMA,
    FMOD_SOUND_TYPE_VAG,
    FMOD_SOUND_TYPE_MAX
} FMOD_SOUND_TYPE;

typedef enum {
    FMOD_OPENSTATE_READY,
    FMOD_OPENSTATE_LOADING,
    FMOD_OPENSTATE_ERROR,
    FMOD_OPENSTATE_CONNECTING,
    FMOD_OPENSTATE_BUFFERING,
    FMOD_OPENSTATE_SEEKING,
    FMOD_OPENSTATE_STREAMING,
    FMOD_OPENSTATE_SETPOSITION,
    FMOD_OPENSTATE_MAX
} FMOD_OPENSTATE;

/* ---- added for fmod_eventimpl_simple (FMOD Ex public API) ---- */
typedef enum {
    FMOD_DELAYTYPE_END_MS,
    FMOD_DELAYTYPE_DSPCLOCK_START,
    FMOD_DELAYTYPE_DSPCLOCK_END,
    FMOD_DELAYTYPE_DSPCLOCK_PAUSE,
    FMOD_DELAYTYPE_MAX
} FMOD_DELAYTYPE;

/* FMOD Ex speaker enum (enumerators as in the Gormiti FMOD build's DWARF: sibling.exe types FMOD_SPEAKER). */
typedef enum FMOD_SPEAKER {
    FMOD_SPEAKER_FRONT_LEFT,
    FMOD_SPEAKER_FRONT_RIGHT,
    FMOD_SPEAKER_FRONT_CENTER,
    FMOD_SPEAKER_LOW_FREQUENCY,
    FMOD_SPEAKER_BACK_LEFT,
    FMOD_SPEAKER_BACK_RIGHT,
    FMOD_SPEAKER_SIDE_LEFT,
    FMOD_SPEAKER_SIDE_RIGHT,
    FMOD_SPEAKER_MAX,
    FMOD_SPEAKER_MONO = FMOD_SPEAKER_FRONT_LEFT,
    FMOD_SPEAKER_NULL = FMOD_SPEAKER_MAX,
    FMOD_SPEAKER_SBL = FMOD_SPEAKER_SIDE_LEFT,
    FMOD_SPEAKER_SBR = FMOD_SPEAKER_SIDE_RIGHT,
    FMOD_SPEAKER_FORCEINT = 65536
} FMOD_SPEAKER;

/* FMOD Ex channel callback type (enumerators as in the Gormiti FMOD build's DWARF). */
typedef enum FMOD_CHANNEL_CALLBACKTYPE {
    FMOD_CHANNEL_CALLBACKTYPE_END,
    FMOD_CHANNEL_CALLBACKTYPE_VIRTUALVOICE,
    FMOD_CHANNEL_CALLBACKTYPE_SYNCPOINT,
    FMOD_CHANNEL_CALLBACKTYPE_OCCLUSION,
    FMOD_CHANNEL_CALLBACKTYPE_MAX,
    FMOD_CHANNEL_CALLBACKTYPE_FORCEINT = 65536
} FMOD_CHANNEL_CALLBACKTYPE;

typedef enum {
    FMOD_SPEAKERMODE_RAW,
    FMOD_SPEAKERMODE_MONO,
    FMOD_SPEAKERMODE_STEREO,
    FMOD_SPEAKERMODE_QUAD,
    FMOD_SPEAKERMODE_SURROUND,
    FMOD_SPEAKERMODE_5POINT1,
    FMOD_SPEAKERMODE_7POINT1,
    FMOD_SPEAKERMODE_PROLOGIC,
    FMOD_SPEAKERMODE_MAX
} FMOD_SPEAKERMODE;

typedef enum {
    FMOD_PLUGINTYPE_OUTPUT,
    FMOD_PLUGINTYPE_CODEC,
    FMOD_PLUGINTYPE_DSP,
    FMOD_PLUGINTYPE_MAX,
    FMOD_PLUGINTYPE_FORCEINT = 65536
} FMOD_PLUGINTYPE;

typedef enum {
    FMOD_DSP_FFT_WINDOW_RECT,
    FMOD_DSP_FFT_WINDOW_TRIANGLE,
    FMOD_DSP_FFT_WINDOW_HAMMING,
    FMOD_DSP_FFT_WINDOW_HANNING,
    FMOD_DSP_FFT_WINDOW_BLACKMAN,
    FMOD_DSP_FFT_WINDOW_BLACKMANHARRIS,
    FMOD_DSP_FFT_WINDOW_MAX
} FMOD_DSP_FFT_WINDOW;

typedef struct FMOD_REVERB_CHANNELPROPERTIES FMOD_REVERB_CHANNELPROPERTIES;
typedef struct FMOD_REVERB_PROPERTIES        FMOD_REVERB_PROPERTIES;
typedef struct FMOD_ADVANCEDSETTINGS         FMOD_ADVANCEDSETTINGS;

/*
 * Per-channel reverb properties (FMOD Ex 4.x FMOD_REVERB_CHANNELPROPERTIES, the 19-field form: 0x4C
 * bytes in retail). Field names follow the public header.
 */
struct FMOD_REVERB_CHANNELPROPERTIES {
    int          Direct;
    int          DirectHF;
    int          Room;
    int          RoomHF;
    int          Obstruction;
    float        ObstructionLFRatio;
    int          Occlusion;
    float        OcclusionLFRatio;
    float        OcclusionRoomRatio;
    float        OcclusionDirectRatio;
    int          Exclusion;
    float        ExclusionLFRatio;
    int          OutsideVolumeHF;
    float        DopplerFactor;
    float        RolloffFactor;
    float        RoomRolloffFactor;
    float        AirAbsorptionFactor;
    unsigned int Flags;
    struct FMOD_DSP *ConnectionPoint;
};

#define FMOD_REVERB_CHANNELFLAGS_DIRECTHFAUTO 0x00000001
#define FMOD_REVERB_CHANNELFLAGS_ROOMAUTO     0x00000002
#define FMOD_REVERB_CHANNELFLAGS_ROOMHFAUTO   0x00000004
#define FMOD_REVERB_CHANNELFLAGS_INSTANCE0    0x00000010
#define FMOD_REVERB_CHANNELFLAGS_INSTANCE1    0x00000020
#define FMOD_REVERB_CHANNELFLAGS_INSTANCE2    0x00000040
#define FMOD_REVERB_CHANNELFLAGS_INSTANCE3    0x00000080
typedef unsigned int FMOD_MEMORY_TYPE;
#define FMOD_MEMORY_NORMAL         0x00000000
#define FMOD_MEMORY_STREAM_FILE    0x00000001
#define FMOD_MEMORY_STREAM_DECODE  0x00000002
#define FMOD_MEMORY_XBOX360_PHYSICAL 0x00100000
#define FMOD_MEMORY_SECONDARY      0x00400000
#define FMOD_MEMORY_ALL            0xFFFFFFFF

typedef void * (F_CALLBACK *FMOD_MEMORY_ALLOCCALLBACK)  ( unsigned int size, FMOD_MEMORY_TYPE type );
typedef void * (F_CALLBACK *FMOD_MEMORY_REALLOCCALLBACK)( void *ptr, unsigned int size, FMOD_MEMORY_TYPE type );
typedef void   (F_CALLBACK *FMOD_MEMORY_FREECALLBACK)   ( void *ptr, FMOD_MEMORY_TYPE type );

/* File system callbacks (FMOD_System_SetFileSystem; used by FMOD::UserFile). */
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_OPENCALLBACK) ( const char *name, int unicode, unsigned int *filesize, void **handle, void **userdata );
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_CLOSECALLBACK)( void *handle, void *userdata );
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_READCALLBACK) ( void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata );
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_SEEKCALLBACK) ( void *handle, unsigned int pos, void *userdata );

/* System callback (FMOD_System_SetCallback); the values are Gormiti's DWARF (FMOD_SYSTEM_CALLBACKTYPE). */
typedef enum {
    FMOD_SYSTEM_CALLBACKTYPE_DEVICELISTCHANGED,
    FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED,
    FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED,
    FMOD_SYSTEM_CALLBACKTYPE_BADDSPCONNECTION,
    FMOD_SYSTEM_CALLBACKTYPE_BADDSPLEVEL,
    FMOD_SYSTEM_CALLBACKTYPE_MAX,
    FMOD_SYSTEM_CALLBACKTYPE_FORCEINT = 65536
} FMOD_SYSTEM_CALLBACKTYPE;

typedef FMOD_RESULT (F_CALLBACK *FMOD_SYSTEM_CALLBACK)( FMOD_SYSTEM *system, FMOD_SYSTEM_CALLBACKTYPE type, void *commanddata1, void *commanddata2 );

/*
 * System::setAdvancedSettings' parameters (FMOD Ex 4.x FMOD_ADVANCEDSETTINGS; 0x48 bytes in Gormiti's
 * DWARF). Field names follow the public header and Gormiti's DWARF.
 */
struct FMOD_ADVANCEDSETTINGS {
    int                 cbsize;                     /* 0x00 */
    int                 maxMPEGcodecs;              /* 0x04 */
    int                 maxADPCMcodecs;             /* 0x08 */
    int                 maxXMAcodecs;               /* 0x0C */
    int                 maxPCMcodecs;               /* 0x10 */
    int                 ASIONumChannels;            /* 0x14 */
    char              **ASIOChannelList;            /* 0x18 */
    FMOD_SPEAKER       *ASIOSpeakerList;            /* 0x1C */
    int                 max3DReverbDSPs;            /* 0x20 */
    float               HRTFMinAngle;               /* 0x24 */
    float               HRTFMaxAngle;               /* 0x28 */
    float               HRTFFreq;                   /* 0x2C */
    float               vol0virtualvol;             /* 0x30 */
    int                 eventqueuesize;             /* 0x34 */
    unsigned int        defaultDecodeBufferSize;    /* 0x38 */
    char               *debugLogFilename;           /* 0x3C */
    unsigned short      profileport;                /* 0x40 */
    unsigned int        geometryThreadPeriod;       /* 0x44 */
};

/*
 * Reverb properties (FMOD_REVERB_PROPERTIES of the public API, completing the typedef above).
 * Layout: Gormiti DWARF (sibling.exe types FMOD_REVERB_PROPERTIES), size 0x7C.
 */
struct FMOD_REVERB_PROPERTIES {
    int          Instance;
    int          Environment;
    float        EnvSize;
    float        EnvDiffusion;
    int          Room;
    int          RoomHF;
    int          RoomLF;
    float        DecayTime;
    float        DecayHFRatio;
    float        DecayLFRatio;
    int          Reflections;
    float        ReflectionsDelay;
    float        ReflectionsPan[3];
    int          Reverb;
    float        ReverbDelay;
    float        ReverbPan[3];
    float        EchoTime;
    float        EchoDepth;
    float        ModulationTime;
    float        ModulationDepth;
    float        AirAbsorptionHF;
    float        HFReference;
    float        LFReference;
    float        RoomRolloffFactor;
    float        Diffusion;
    float        Density;
    unsigned int Flags;
};

#define FMOD_REVERB_FLAGS_HIGHQUALITYREVERB   0x00000400

/* The "off" preset (public FMOD_PRESET_OFF). */
#define FMOD_PRESET_OFF { 0, -1, 7.5f, 1.00f, -10000, -10000, 0, 1.00f, 1.00f, 1.0f, -2602, 0.007f, { 0.0f, 0.0f, 0.0f }, 200, 0.011f, { 0.0f, 0.0f, 0.0f }, 0.250f, 0.000f, 0.250f, 0.000f, -5.0f, 5000.0f, 250.0f, 0.0f, 0.0f, 0.0f, 0x33F }

/* ---- Sound creation (fmod_async) ---- */

typedef struct FMOD_SOUNDGROUP FMOD_SOUNDGROUP;

/* FMOD_MODE bits: the name argument of createSound points at the data in memory. */
#define FMOD_OPENMEMORY            0x00000800
#define FMOD_OPENMEMORY_POINT      0x10000000

/* FMOD_TIMEUNIT flags. */
#define FMOD_TIMEUNIT_MS                0x00000001
#define FMOD_TIMEUNIT_PCM               0x00000002
#define FMOD_TIMEUNIT_PCMBYTES          0x00000004
#define FMOD_TIMEUNIT_RAWBYTES          0x00000008
#define FMOD_TIMEUNIT_MODORDER          0x00000100
#define FMOD_TIMEUNIT_MODROW            0x00000200
#define FMOD_TIMEUNIT_MODPATTERN        0x00000400
#define FMOD_TIMEUNIT_SENTENCE_MS       0x00010000
#define FMOD_TIMEUNIT_SENTENCE_PCM      0x00020000
#define FMOD_TIMEUNIT_SENTENCE_PCMBYTES 0x00040000
#define FMOD_TIMEUNIT_SENTENCE_SOUND    0x00080000
#define FMOD_TIMEUNIT_SENTENCE_SUBSOUND 0x00100000
#define FMOD_TIMEUNIT_BUFFERED          0x10000000

typedef enum {
    FMOD_SPEAKERMAPTYPE_DEFAULT,
    FMOD_SPEAKERMAPTYPE_ALLMONO,
    FMOD_SPEAKERMAPTYPE_ALLSTEREO,
    FMOD_SPEAKERMAPTYPE_51_PROTOOLS
} FMOD_SPEAKERMAPTYPE;

typedef FMOD_RESULT (F_CALLBACK *FMOD_SOUND_NONBLOCKCALLBACK)( FMOD_SOUND *sound, FMOD_RESULT result );
typedef FMOD_RESULT (F_CALLBACK *FMOD_SOUND_PCMREADCALLBACK)( FMOD_SOUND *sound, void *data, unsigned int datalen );
typedef FMOD_RESULT (F_CALLBACK *FMOD_SOUND_PCMSETPOSCALLBACK)( FMOD_SOUND *sound, int subsound, unsigned int position,
                                                               FMOD_TIMEUNIT postype );

/* Extra information for creating a sound (size 0x70, Gormiti DWARF). */
typedef struct FMOD_CREATESOUNDEXINFO {
    int                          cbsize;               /* 0x00 */
    unsigned int                 length;               /* 0x04 */
    unsigned int                 fileoffset;           /* 0x08 */
    int                          numchannels;          /* 0x0C */
    int                          defaultfrequency;     /* 0x10 */
    FMOD_SOUND_FORMAT            format;               /* 0x14 */
    unsigned int                 decodebuffersize;     /* 0x18 */
    int                          initialsubsound;      /* 0x1C */
    int                          numsubsounds;         /* 0x20 */
    int                         *inclusionlist;        /* 0x24 */
    int                          inclusionlistnum;     /* 0x28 */
    FMOD_SOUND_PCMREADCALLBACK   pcmreadcallback;      /* 0x2C */
    FMOD_SOUND_PCMSETPOSCALLBACK pcmsetposcallback;    /* 0x30 */
    FMOD_SOUND_NONBLOCKCALLBACK  nonblockcallback;     /* 0x34, called when a non-blocking open ends */
    const char                  *dlsname;              /* 0x38 */
    const char                  *encryptionkey;        /* 0x3C */
    int                          maxpolyphony;         /* 0x40 */
    void                        *userdata;             /* 0x44 */
    FMOD_SOUND_TYPE              suggestedsoundtype;   /* 0x48 */
    FMOD_FILE_OPENCALLBACK       useropen;             /* 0x4C */
    FMOD_FILE_CLOSECALLBACK      userclose;            /* 0x50 */
    FMOD_FILE_READCALLBACK       userread;             /* 0x54 */
    FMOD_FILE_SEEKCALLBACK       userseek;             /* 0x58 */
    FMOD_SPEAKERMAPTYPE          speakermap;           /* 0x5C */
    FMOD_SOUNDGROUP             *initialsoundgroup;    /* 0x60 */
    unsigned int                 initialseekposition;  /* 0x64 */
    FMOD_TIMEUNIT                initialseekpostype;   /* 0x68 */
    int                          ignoresetfilesystem;  /* 0x6C */
} FMOD_CREATESOUNDEXINFO;

/* ---- end of the sound creation block ---- */

#ifdef __cplusplus
extern "C" {
#endif

FMOD_RESULT F_API FMOD_Memory_Initialize( void *poolmem, int poollen, FMOD_MEMORY_ALLOCCALLBACK useralloc,
                                          FMOD_MEMORY_REALLOCCALLBACK userrealloc, FMOD_MEMORY_FREECALLBACK userfree,
                                          FMOD_MEMORY_TYPE memtypeflags );
FMOD_RESULT F_API FMOD_System_Create( FMOD_SYSTEM **system );

FMOD_RESULT F_API FMOD_Sound_GetSyncPoint( FMOD_SOUND *sound, int index, FMOD_SYNCPOINT **point );
FMOD_RESULT F_API FMOD_Sound_GetSyncPointInfo( FMOD_SOUND *sound, FMOD_SYNCPOINT *point, char *name, int namelen,
                                               unsigned int *offset, FMOD_TIMEUNIT offsettype );

FMOD_RESULT F_API FMOD_Channel_GetCurrentSound( FMOD_CHANNEL *channel, FMOD_SOUND **sound );
FMOD_RESULT F_API FMOD_Channel_GetUserData( FMOD_CHANNEL *channel, void **userdata );

FMOD_RESULT F_API FMOD_File_SetDiskBusy( int busy );

#ifdef __cplusplus
}
#endif

#endif /* _FMOD_H */
