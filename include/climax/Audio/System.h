#ifndef CLIMAX_AUDIO_SYSTEM_H
#define CLIMAX_AUDIO_SYSTEM_H

// Names and layout: Gormiti link map and DWARF (Audio::System, game/System/Audio/FMOD/System.h).
// The element types of the STLList members follow the vtable each constructor store installs
// (System constructor 0x801FE27C). Members of the class are declared as the units reconstructed
// so far use them; its own unit (System.obj, 0x801FE034) is not reconstructed yet.

#include <revolution/types.h>
#include <climax/STLList.h>
#include <climax/ThreadSync.h>
#include <climax/Audio/Defs.h>
#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_event.h>

namespace Audio {
class System;
}

// Gormiti: the static behind System::GetInstance (System.cpp); its name is not recorded.
extern Audio::System* lbl_8052A748;

namespace Audio {

class AsyncDiskSoundProject;
class DiskSoundProject;
class AsyncEventSource;
class EventGroup;
class EventSource;
class Listener;
class Source;
class SoundProject;

// The async file thread's state (another unit, not reconstructed yet: 0x801F8E58).
bool IsAsyncThreadActive();

/* The memory FMOD and the audio layer allocate from (Gormiti DWARF, audio.cpp). */
struct MemoryParams {
    void*           memPool;                                    // 0x00
    int             memPoolLength;                              // 0x04
    void*           ( *memAlloc )( unsigned int, unsigned int );    // 0x08
    void*           ( *memRealloc )( void*, unsigned int, unsigned int );   // 0x0C
    void            ( *memFree )( void*, unsigned int );        // 0x10
};

/* One request for the async thread: read data, then call back (Gormiti DWARF). */
class AsyncQueueItem {
public:
                    ~AsyncQueueItem();

    void*           data;                       // 0x00
    void*           retVal;                     // 0x04
    bool            complete;                   // 0x08
    bool            cancelled;                  // 0x09
    char*           filename;                   // 0x0C
    int             ( *callback )( AsyncQueueItem* );   // 0x10
};

/* A reverb setting of the sound design (Gormiti DWARF). */
class ReverbPreset {
public:
    char*           name;                       // 0x00
    char*           ps2PresetName;              // 0x04
    char*           pspPresetName;              // 0x08
    int             room;                       // 0x0C
    int             roomHF;                     // 0x10
    float           roomRollOff;                // 0x14
    float           decayTime;                  // 0x18
    float           decayHFRatio;               // 0x1C
    int             reflections;                // 0x20
    float           reflectDelay;               // 0x24
    int             reverb;                     // 0x28
    float           reverbDelay;                // 0x2C
    float           diffusion;                  // 0x30
    float           density;                    // 0x34
    float           hfReference;                // 0x38
    int             roomLF;                     // 0x3C
    float           lfReference;                // 0x40
};

/* One file of a sound project's pack: where it lies and how long it is (Gormiti DWARF, System.cpp). */
class SPFileEntry {
public:
    void            CorrectEndian();

    int             size;                       // 0x00
    int             offset;                     // 0x04
    char            name[64];                   // 0x08
};

/* The header of a sound project pack: the project file's and the sound file's entries (Gormiti DWARF, System.cpp). */
class SPHeader {
public:
    void            CorrectEndian();

    char            name[64];                   // 0x00
    SPFileEntry     entries[2];                 // 0x40
};

/*
 * The audio system: the FMOD system and event system, the listeners, the lists of loaded sound
 * projects and sources, the reverb state and the file callbacks FMOD reads through.
 */
class System {
public:
    /* The one audio system (Gormiti: s_instance of System.cpp, inline in System.h:429). */
    static System*  GetInstance();

    float           GetLastFrameInterval() const;
    bool            IsPrintIOStatusEnabled() const;
    bool            IsAsyncStreamingDisabled() const;
    int             TriggerCallback( CallbackType type, void* info );
    void            ChannelStart( FMOD::Channel* channel );
    MemoryStream*   FindSoundProjectFile( const char* filename );
    DiskStream*     FindDiskSoundProjectFile( const char* filename, DiskSoundProject** refProject );
    void            FreeDiskSoundProject( DiskSoundProject* project, bool deleteProject );
    void            AddAsyncStream( DiskStream* asyncStream );
    void            RemoveAsyncStream( DiskStream* asyncStream );
    bool            CheckAsyncStream( DiskStream* asyncStream );
    bool            AddPendingCloseAsyncStream( DiskStream* asyncStream );

                    System( int maxChannels, int initFlags, MemoryParams* memParams );

    static System*  Init( int maxChannels, int initFlags, MemoryParams* memParams );
    int             LocalInit( int maxChannels, int initFlags, MemoryParams* memParams );

    // FMOD's file callbacks (FMOD_FILE_OPENCALLBACK and the like) for the sound files.
    static FMOD_RESULT  SFileOpen( const char* name, int unicode, unsigned int* filesize, void** handle, void** userdata );
    static FMOD_RESULT  SFileClose( void* handle, void* userdata );
    static FMOD_RESULT  SFileRead( void* handle, void* buffer, unsigned int sizebytes, unsigned int* bytesread, void* userdata );
    static FMOD_RESULT  SFileSeek( void* handle, unsigned int pos, void* userdata );

    int             SetFileOpenCallback( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ) );
    int             SetSynchronousDiskCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ), int ( *fclose_ )( void* ),
                                                 int ( *fread_ )( void*, void*, unsigned int, unsigned int* ), int ( *fseek_ )( void*, unsigned int ),
                                                 int ( *fgetpos_ )( void*, unsigned int& ) );
    int             SetAsynchronousDiskCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle*, bool ), int ( *fclose_ )( void* ),
                                                  int ( *fread_ )( void*, void*, unsigned int, unsigned int*, int ( * )( void*, char*, int, int ), void*, bool ),
                                                  int ( *fseek_ )( void*, unsigned int ), int ( *fgetpos_ )( void*, unsigned int& ),
                                                  int ( *poll_ )( void* ), int ( *cancel_ )( void* ) );
    int             SetFileSystemCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ), int ( *fclose_ )( void* ),
                                            int ( *fread_ )( void*, void*, unsigned int, unsigned int* ), int ( *fseek_ )( void*, unsigned int ),
                                            int blockAlignment );

    bool            RemoveSource( Source* source, bool deleteSource );
    Source*         LoadStreamSource( const char* filename, int flags, SourceParams* params );

    int             InterpolateReverb( ReverbPreset* startPreset, ReverbPreset* endPreset, float percentage );
    ReverbPreset*    FindReverbPreset( const char* name );
    int             SetReverbPreset( ReverbPreset* preset, bool force );
    int             SetReverb( const char* presetName );
    void            AddReverbPreset( ReverbPreset* preset );

    void            UpdatePendingDeleteEventGroups();
    static int      SAsyncCallback( void* userData, char* buffer, int readSize, int status );
    int             AsyncCallback( void* userData, char* buffer, int readSize, int status );
    void            CancelAllAsyncStreams();
    void            CancelAsyncItemForEventSource( AsyncEventSource* esource );
    int             QueueAsyncItem( AsyncQueueItem* item );
    void            RemoveQueuedAsyncItem( AsyncQueueItem* item );
    void            UpdateAsyncQueueItem();
    void            UpdateAsync();
    void            FlushPendingAsyncGroups();
    void            RemovePendingEventGroup( EventGroup* egroup );
    void            RemoveAsyncPlayback( AsyncEventSource* esource );

    void            AddUpdateSound( EventSource* sound );
    void            RemoveUpdateSound( EventSource* sound );
    int             Update( float frameInterval );
    void            UpdateReverb( float frameInterval );
    void            UpdateSounds();
    void            UpdatePendingCloseDiskStreams();

    Listener*       GetDefaultListener();

    EventSource*            LoadFirstEventSourceFromAnyGroup( const char* eventSourceName );
    EventGroup*             IsEventGroupLoaded( const char* friendlyName );
    EventGroup*             LoadEventGroup( const char* nameOrData, const char* friendlyName, int lengthOfData, bool loadedFromDisk, bool autoDelete );
    EventGroup*             LoadEventGroup( SoundProject* project );
    bool                    ForceDeleteEventGroup( EventGroup* egroup, bool deletePointerAsWell );
    EventGroup*             LoadAsyncEventGroup( const char* friendlyName, DiskSoundProject* project, bool autoDelete );
    bool                    CanDeleteEventSource( EventSource* esource, bool force );
    bool                    FreeEventGroup( EventGroup* egroup, bool deleteGroup );

    DiskSoundProject*       LoadDiskSoundProject( const char* filename );
    DiskSoundProject*       LoadDiskSoundProject( FileHandle* fileHandle, const char* filename );
    DiskSoundProject*       FindDiskSoundProject( const char* filename );
    AsyncDiskSoundProject*  LoadAsyncDiskSoundProject( const char* filename );
    SoundProject*           LoadSoundProject( void* data, int length );
    void                    FreeSoundProject( SoundProject* project, bool deleteProject );

public:
    float                                   m_lastFrameInterval;                // 0x00
    FMOD::System*                           m_fmodSystem;                       // 0x04
    FMOD::EventSystem*                      m_fmodEventSystem;                  // 0x08
    FMOD::SoundGroup*                       m_allSounds;                        // 0x0C
    FMOD::ChannelGroup*                     m_masterChannel;                    // 0x10
    Listener*                               m_listeners[4];                     // 0x14
    ListUtils::STLList< SoundProject* >     m_soundProjects;                    // 0x24
    ListUtils::STLList< DiskSoundProject* > m_diskSoundProjects;                // 0x34
    ListUtils::STLList< Source* >           m_sourceList;                       // 0x44
    ListUtils::STLList< EventGroup* >       m_eventGroups;                      // 0x54
    DiskStream*                             m_activeAsyncStreams[32];           // 0x64
    int                                     m_numActiveAsyncStreams;            // 0xE4
    DiskStream*                             m_pendingCloseAsyncStreams[32];     // 0xE8
    int                                     m_numPendingCloseAsyncStreams;      // 0x168
    int                                     m_streamingSoundCount;              // 0x16C
    ListUtils::STLList< EventGroup* >       m_pendingDeleteEventGroups;         // 0x170
    ListUtils::STLList< EventSource* >      m_pendingDeleteEventSources;        // 0x180
    ReverbPreset*                           m_currReverb;                       // 0x190
    ReverbPreset*                           m_targetReverb;                     // 0x194
    float                                   m_currReverbInterpTime;             // 0x198
    float                                   m_totalReverbInterpTime;            // 0x19C
    ListUtils::STLList< ReverbPreset* >     m_reverbPresets;                    // 0x1A0
    ListUtils::STLList< AsyncDiskSoundProject* > m_pendingAsyncProjects;        // 0x1B0
    ListUtils::STLList< EventSource* >      m_pendingAsyncPlaybacks;            // 0x1C0
    ListUtils::STLList< EventGroup* >       m_pendingEventGroups;               // 0x1D0
    ListUtils::STLList< EventSource* >      m_updateSounds;                     // 0x1E0
    AsyncQueueItem*                         m_currAsyncQueueItem;               // 0x1F0
    bool                                    m_printIOStatus;                    // 0x1F4
    ICallbackSink*                          m_callback;                         // 0x1F8
    bool                                    m_asyncStreamsDisabled;             // 0x1FC
    ThreadSync::cLockObject                 m_diskAccessMutex;                  // 0x200
    int                                     ( *m_fopen )( const char*, unsigned int*, FileHandle* );  // 0x20C
    int                                     ( *m_fclose )( void* );             // 0x210
    int                                     ( *m_fread )( void*, void*, unsigned int, unsigned int* );  // 0x214
    int                                     ( *m_fseek )( void*, unsigned int );    // 0x218
    int                                     ( *m_sdFopen )( const char*, unsigned int*, FileHandle* );    // 0x21C
    int                                     ( *m_sdFclose )( void* );           // 0x220
    int                                     ( *m_sdFread )( void*, void*, unsigned int, unsigned int* );    // 0x224
    int                                     ( *m_sdFseek )( void*, unsigned int );  // 0x228
    int                                     ( *m_sdFgetpos )( void*, unsigned int& );   // 0x22C
    int                                     ( *m_asdFopen )( const char*, unsigned int*, FileHandle*, bool ); // 0x230
    int                                     ( *m_asdFclose )( void* );          // 0x234
    int                                     ( *m_asdFread )( void*, void*, unsigned int, unsigned int*, int ( * )( void*, char*, int, int ), void*, bool ); // 0x238
    int                                     ( *m_asdFseek )( void*, unsigned int ); // 0x23C
    int                                     ( *m_asdFgetpos )( void*, unsigned int& );  // 0x240
    int                                     ( *m_asdPoll )( void* );            // 0x244
    int                                     ( *m_asdCancel )( void* );          // 0x248
};

inline System* System::GetInstance() {
    return ::lbl_8052A748;
}

// Gormiti: System.h:461 (out of line in AsyncEventSource.obj there; retail inlines it into ~AsyncEventGroup).
inline void System::RemovePendingEventGroup( EventGroup* egroup ) {
    m_pendingEventGroups.Remove( egroup );
}

inline float System::GetLastFrameInterval() const {
    return m_lastFrameInterval;
}

inline bool System::IsPrintIOStatusEnabled() const {
    return m_printIOStatus;
}

inline bool System::IsAsyncStreamingDisabled() const {
    return m_asyncStreamsDisabled;
}

/* Tells the sink, if there is one, that a sound or event was created or deleted (Gormiti: System.h:439). */
inline int System::TriggerCallback( CallbackType type, void* info ) {
    if ( m_callback != NULL ) {
        return m_callback->Callback( type, info );
    }
    return 0;
}

}

#endif
