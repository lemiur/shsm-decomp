#ifndef CLIMAX_AUDIO_DEFS_H
#define CLIMAX_AUDIO_DEFS_H

// Names and layout: Gormiti link map and DWARF (game/System/Audio/FMOD/Defs.h; the types the audio
// units declare there: Stats, CPUUsage, StatsType, SoundType, CallbackType, Channel::Type,
// SourceParams, ICallbackSink, GetMemoryStats).

#include <string.h>
#include <revolution/types.h>
#include <fmod/fmod.h>
#include <fmod/fmod.hpp>

namespace Audio {

class Source;

/* Channel::Type is the channel index System::playSound takes (FMOD's FMOD_CHANNEL_FREE and _REUSE come first). */
class Channel {
public:
    enum Type {
        Free            = -1,
        Reuse           = -2,
        Previous        = 0x10000000,
        Dual            = 0x00100000,
        WiiController0  = 0x01000000,
        WiiController1  = 0x01000001,
        WiiController2  = 0x01000002,
        WiiController3  = 0x01000003
    };
};

/* What an audio event reports to the system's ICallbackSink. */
enum CallbackType {
    CBT_AUDIO_SYSTEM,
    CBT_EVENT_SOURCE,
    CBT_EVENT_SOURCE_CREATING,
    CBT_EVENT_SOURCE_DELETING,
    CBT_EVENT_GROUP_CREATING,
    CBT_EVENT_GROUP_DELETING,
    CBT_SOUND_CREATING,
    CBT_SOUND_DELETING
};

/* Which statistics Source::GetStats and its kin gather (a bit set). */
enum StatsType {
    StatsActiveChannels     = 1,
    StatsMemory             = 2,
    StatsDetailedMemory     = 4,
    StatsCPUUsage           = 8,
    StatsNormal             = 11,
    StatsDetailed           = 15
};

/* The result codes of the event sources and groups (Gormiti DWARF, AudioManager.cpp). */
enum Error {
    Ok                  = 0,
    FOpen               = 23,
    UnSupported         = 82,
    AppSpecific         = 1048575,
    GenericError        = 1048575,
    EventLoading        = 1048576,
    EventCannotLoad     = 1048577,
    EventError          = 1048578,
    EventStopped        = 1048579,
    EventNeedsUpdate    = 1048580,
    EventNoUpdateNeeded = 1048581,
    InvalidParam        = 1048582,
    InvalidHandle       = 1048583,
    Fail                = -1
};

enum SoundType {
    SoundTypeDefault,
    SoundTypeEvent
};

class CPUUsage {
public:
    float               dsp;                    // 0x00
    float               stream;                 // 0x04
    float               update;                 // 0x08
    float               total;                  // 0x0C
};

class Stats {
public:
    CPUUsage            cpuUsage;               // 0x00
    unsigned int        numActiveChannels;      // 0x10
    unsigned int        totalMemoryUsed;        // 0x14
    unsigned int        RAMUsage;               // 0x18
    unsigned int        maxRAMAllocated;        // 0x1C
    unsigned int        SRAMUsage;              // 0x20
    unsigned int        maxSRAMAllocated;       // 0x24
    unsigned int        availableSRAM;          // 0x28
    unsigned int        detailedUsage[48];      // 0x2C
};

class SProject;

class SourceParams {
public:
    void*               customData;             // 0x00
    int                 ( *callback )( Source*, SourceParams*, int );   // 0x04
};

/*
 * A sound project file read from memory: the name, the data and the read position (Gormiti DWARF).
 */
class MemoryStream {
public:
                        MemoryStream( const char* name, const char* data, int length );

    char                m_name[64];             // 0x00
    const char*         m_data;                 // 0x40
    int                 m_length;               // 0x44
    int                 m_fp;                   // 0x48
};

// Gormiti: Helper.h:62.
inline MemoryStream::MemoryStream( const char* name, const char* data, int length ) {
    strcpy( m_name, name );
    m_data = data;
    m_length = length;
    m_fp = 0;
}

/*
 * A sound project file read from disk, synchronously or through the async file system (Gormiti DWARF).
 */
class DiskStream {
public:
                        DiskStream( const char* name, int startFP, int length );
                        ~DiskStream();

    char                m_name[64];             // 0x00
    int                 m_length;               // 0x40
    int                 m_fp;                   // 0x44
    int                 m_startFP;              // 0x48
    int                 m_alignOffset;          // 0x4C
    bool                m_isAsync;              // 0x50
    bool                m_locked;               // 0x51
    bool                m_didDuplicate;         // 0x52
    DiskStream*         orgStream;              // 0x54
    void*               m_fsHandle;             // 0x58
    SProject*           m_parentProject;        // 0x5C
    char*               m_asyncBuffer;          // 0x60
    bool                m_isReading;            // 0x64
    int                 m_dataRead;             // 0x68
};

// Gormiti: Helper.h:154.
inline DiskStream::DiskStream( const char* name, int startFP, int length ) {
    strcpy( m_name, name );
    m_startFP = startFP;
    m_length = length;
    m_fp = 0;
    m_alignOffset = 0;
    m_isAsync = false;
    m_fsHandle = NULL;
    m_parentProject = NULL;
    m_asyncBuffer = NULL;
    m_isReading = false;
    m_dataRead = 0;
    m_locked = false;
    m_didDuplicate = false;
    orgStream = NULL;
}

// Gormiti: Helper.h:172. The test is inverted (it frees only when there is no buffer); retail keeps it so.
inline DiskStream::~DiskStream() {
    if ( m_asyncBuffer == NULL ) {
        delete[] m_asyncBuffer;
        m_asyncBuffer = NULL;
    }
}

/*
 * What FMOD is handed by a file open callback: the stream behind the file and the callbacks that read it
 * (Gormiti DWARF, Defs.h:497).
 */
class FileHandle {
public:
                        FileHandle();
    FileHandle&         operator=( const FileHandle& other );

    void*               fsHandle;               // 0x00
    bool                isAsync;                // 0x04
    int                 ( *fread )( void*, void*, unsigned int, unsigned int* );    // 0x08
    int                 ( *fclose )( void* );   // 0x0C
    int                 ( *fseek )( void*, unsigned int );  // 0x10
    int                 ( *ftell )( void*, unsigned int& ); // 0x14
};

inline FileHandle::FileHandle() {
    isAsync = false;
    fsHandle = NULL;
    fread = NULL;
    fclose = NULL;
    fseek = NULL;
    ftell = NULL;
}

// Gormiti: Defs.h (an out-of-line __as__ in Gormiti's System.obj; retail inlines it). It copies the
// stream and its callbacks but not isAsync.
inline FileHandle& FileHandle::operator=( const FileHandle& other ) {
    fsHandle = other.fsHandle;
    fread = other.fread;
    fclose = other.fclose;
    fseek = other.fseek;
    ftell = other.ftell;
    return *this;
}

/*
 * The receiver of the audio system's callbacks (System::m_callback). Slot 0x0C is pure.
 */
class ICallbackSink {
public:
    virtual             ~ICallbackSink() {}
    virtual int         Callback( CallbackType type, void* info ) = 0;
};

/*
 * Memory statistics of one FMOD object (a channel): retail inlines it into Source::GetStats and has
 * no out-of-line copy (Gormiti's Final build keeps one, 0x8021B0E8).
 */
template< class T >
inline int GetMemoryStats( T* p, StatsType type, Stats* stats ) {
    if ( !( type & StatsMemory ) ) {
        return FMOD_ERR_UNSUPPORTED;
    }
    if ( p != NULL ) {
        FMOD_RESULT result = p->getMemoryInfo( (unsigned int)-1, (unsigned int)-1, &stats->totalMemoryUsed, &stats->detailedUsage[0] );
        if ( result != FMOD_OK ) {
            return result;
        }
        return FMOD_OK;
    }
    return -1;
}

}

#endif
