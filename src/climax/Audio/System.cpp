
// Unit name: Gormiti link map (System.obj); boundary: begins at the last climax/Audio/Source.cpp function, ends before Time.obj's first function Open (0x802020D0, Time.obj starts with the 4-byte Open); the AsyncDiskSoundProject weak virtuals 0x80202024-0x802020D0 are emitted here
// Names and layout: Gormiti link map and DWARF (Audio::System, System.obj, game/System/Audio/FMOD/System.cpp)
//
// Retail's System.obj holds 64 functions of the 190 in Gormiti's: the event-source updates, the source
// loading and most of the lists' members are inlined or absent. Functions of another revision than
// Gormiti's (UpdatePendingDeleteEventGroups, AsyncCallback, UpdateAsync, FlushPendingAsyncGroups, Update,
// LoadAsyncDiskSoundProject, LoadSoundProject, FreeEventGroup, LoadDiskSoundProject( FileHandle*, const char* ))
// are named by the same-named or same-sized function of Gormiti in the same position.

#include <string.h>
#include <climax/Audio/System.h>
#include <climax/Audio/Helper.h>     // after System.h: System.obj emits these classes' vtables ahead of the lists System declares
#include <climax/Audio/EventGroup.h>
#include <climax/Audio/Listener.h>
#include <fmod/fmodwii.h>
#include <climax/Audio/EventSource.h>
#include <climax/Audio/AsyncEventSource.h>
#include <extras.h>
#include <climax/Memory.h>
#include <climax/Time.h>
#include <climax/Utils.h>
#include <climax/Debug.h>

namespace Audio {

// A sound project pack starts with 0x14 bytes of preamble; the SPHeader follows (name by invention).
enum { SP_PREAMBLE_SIZE = 0x14 };
const unsigned int SP_HEADER_READ_SIZE = SP_PREAMBLE_SIZE + sizeof( SPHeader );

/*
====================
SPFileEntry::CorrectEndian / SPHeader::CorrectEndian

The fields of a pack header read from the file are byte-swapped in place
(Gormiti: CorrectEndian, out of line there; retail inlines both; the description is invented).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021E564
inline void SPFileEntry::CorrectEndian() {
    size = fn_801B6C78( size );
    offset = fn_801B6C78( offset );
}

// no copy in retail (inlined); Gormiti 0x8021E4C0
inline void SPHeader::CorrectEndian() {
    entries[0].CorrectEndian();
    entries[1].CorrectEndian();
}

/*
====================
AsyncQueueItem::~AsyncQueueItem
====================
*/
// 0x801FE034
AsyncQueueItem::~AsyncQueueItem() {
    if ( System::GetInstance()->m_currAsyncQueueItem == this ) {
        System::GetInstance()->m_currAsyncQueueItem = NULL;
    }
}

/*
====================
GetPlatformPostfix

The suffix of the platform's file names: none on the Wii.
====================
*/
// 0x801FE090
const char* GetPlatformPostfix() {
    return "";
}

/*
====================
System::SFileOpen

FMOD's file open callback for the sound files: opens name through the file callbacks.
====================
*/
// 0x801FE098
FMOD_RESULT System::SFileOpen( const char* name, int unicode, unsigned int* filesize, void** handle, void** userdata ) {
    FileHandle* fileHandle = new( GAME_HEAP, AUDIO_HEAP_FLAGS ) FileHandle;
    FMOD_RESULT result = static_cast< FMOD_RESULT >( MemFileOpen( name, filesize, fileHandle ) );
    if ( result != FMOD_OK ) {
        result = static_cast< FMOD_RESULT >( DiskFileOpen( name, filesize, fileHandle ) );
        if ( result != FMOD_OK ) {
            result = static_cast< FMOD_RESULT >( GetInstance()->m_fopen( name, filesize, fileHandle ) );
        }
    }
    if ( result != FMOD_OK ) {
        delete fileHandle;
        fileHandle = NULL;
    }
    *handle = fileHandle;
    return result;
}

/*
====================
System::SFileClose
====================
*/
// 0x801FE174
FMOD_RESULT System::SFileClose( void* handle, void* userdata ) {
    FileHandle* fileHandle = static_cast< FileHandle* >( handle );
    FMOD_RESULT result = static_cast< FMOD_RESULT >( fileHandle->fclose( fileHandle->fsHandle ) );
    delete fileHandle;
    return result;
}

/*
====================
System::SFileRead
====================
*/
// 0x801FE1C4
FMOD_RESULT System::SFileRead( void* handle, void* buffer, unsigned int sizebytes, unsigned int* bytesread, void* userdata ) {
    // Gormiti's DWARF keeps the result in a local, time, that neither of its builds reads.
    RWS::Time::GetTime();
    FileHandle* fileHandle = static_cast< FileHandle* >( handle );
    FMOD_RESULT result = static_cast< FMOD_RESULT >( fileHandle->fread( fileHandle->fsHandle, buffer, sizebytes, bytesread ) );
    return result;
}

/*
====================
System::SFileSeek
====================
*/
// 0x801FE230
FMOD_RESULT System::SFileSeek( void* handle, unsigned int pos, void* userdata ) {
    // Gormiti's DWARF keeps the result in a local, time, that neither of its builds reads.
    RWS::Time::GetTime();
    FileHandle* fileHandle = static_cast< FileHandle* >( handle );
    FMOD_RESULT result = static_cast< FMOD_RESULT >( fileHandle->fseek( fileHandle->fsHandle, pos ) );
    return result;
}

/*
====================
System::System

Sets every list, stream table and callback to its empty state (the list members' constructors install
the six STLList vtables 0x801FE414-0x801FE84C emit).
====================
*/
// 0x801FE27C
System::System( int maxChannels, int initFlags, MemoryParams* memParams ) :
    m_numActiveAsyncStreams( 0 ),
    m_numPendingCloseAsyncStreams( 0 ),
    m_streamingSoundCount( 0 ),
    m_currAsyncQueueItem( NULL ),
    m_printIOStatus( false ),
    m_callback( NULL ),
    m_asyncStreamsDisabled( false ) {
    memset( m_activeAsyncStreams, 0, sizeof( m_activeAsyncStreams ) );
    memset( m_pendingCloseAsyncStreams, 0, sizeof( m_pendingCloseAsyncStreams ) );
    LocalInit( maxChannels, initFlags, memParams );
}

// TODO: the six STLList destructors are emitted by the constructor and the unit's other users once
// those are written; until then they are instantiated here.
template ListUtils::STLList< SoundProject* >::~STLList();                   // 0x801FE414
template ListUtils::STLList< DiskSoundProject* >::~STLList();               // 0x801FE4EC
template ListUtils::STLList< Source* >::~STLList();                         // 0x801FE5C4
template ListUtils::STLList< EventGroup* >::~STLList();                     // 0x801FE69C
template ListUtils::STLList< ReverbPreset* >::~STLList();                   // 0x801FE774
template ListUtils::STLList< AsyncDiskSoundProject* >::~STLList();          // 0x801FE84C

/*
====================
System::Init

Creates the one audio system and initialises it.
====================
*/
// 0x801FE924
System* System::Init( int maxChannels, int initFlags, MemoryParams* memParams ) {
    System* system = new( GAME_HEAP, GAME_HEAP_FLAGS ) System( maxChannels, initFlags, memParams );
    lbl_8052A748 = system;
    return system;
}

/*
====================
System::LocalInit

Brings up FMOD and the event system with the memory the caller gave.
====================
*/
// 0x801FE990
int System::LocalInit( int maxChannels, int initFlags, MemoryParams* memParams ) {
    if ( memParams != NULL ) {
        FMOD_Memory_Initialize( memParams->memPool, memParams->memPoolLength, memParams->memAlloc, memParams->memRealloc,
                                memParams->memFree, FMOD_MEMORY_ALL );
    }
    FMOD::EventSystem_Create( &m_fmodEventSystem );
    m_fmodEventSystem->getSystemObject( &m_fmodSystem );
    m_fmodSystem->setSpeakerMode( FMOD_SPEAKERMODE_PROLOGIC );

    FMOD_WII_INFO wiiInfo;
    memset( &wiiInfo, 0, sizeof( wiiInfo ) );
    wiiInfo.dvdreadpriority = 3;
    wiiInfo.muteremotespeakerifnosound = 1;
    m_fmodEventSystem->init( maxChannels, initFlags, &wiiInfo, 0 );

    FMOD_ADVANCEDSETTINGS settings;
    memset( &settings, 0, sizeof( settings ) );
    settings.cbsize = sizeof( settings );
    settings.eventqueuesize = 256;
    m_fmodSystem->setAdvancedSettings( &settings );

    unsigned int bsize = 0;
    unsigned int tunit;
    m_fmodSystem->getStreamBufferSize( &bsize, &tunit );
    unsigned int fmodVersion;
    m_fmodSystem->getVersion( &fmodVersion );
    m_fmodSystem->setUserData( this );
    m_fmodSystem->set3DSettings( 1.0f, 1.0f, 1.0f );

    m_lastFrameInterval = 0.0f;
    m_fopen = NULL;
    m_fclose = NULL;
    m_fread = NULL;
    m_fseek = NULL;
    m_allSounds = NULL;
    m_masterChannel = NULL;
    m_currReverb = NULL;
    m_targetReverb = NULL;
    m_totalReverbInterpTime = 0.5f;
    memset( m_listeners, 0, sizeof( m_listeners ) );
    m_listeners[0] = new( GAME_HEAP, GAME_HEAP_FLAGS ) Listener;
    m_listeners[0]->Create( 0 );
    m_fmodSystem->getMasterSoundGroup( &m_allSounds );
    m_fmodSystem->getMasterChannelGroup( &m_masterChannel );

    // Gormiti's Debug build calls SetReverb( NULL ) here; retail inlines it, sharing its "off" preset initialiser.
    SetReverb( NULL );
    return 0;
}

/*
====================
System::SetFileOpenCallback
====================
*/
// 0x801FEB9C
int System::SetFileOpenCallback( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ) ) {
    return SetFileSystemCallbacks( fopen_, m_fclose, m_fread, m_fseek, -1 );
}

/*
====================
System::SetSynchronousDiskCallbacks
====================
*/
// 0x801FEC38
int System::SetSynchronousDiskCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ), int ( *fclose_ )( void* ),
                                         int ( *fread_ )( void*, void*, unsigned int, unsigned int* ), int ( *fseek_ )( void*, unsigned int ),
                                         int ( *fgetpos_ )( void*, unsigned int& ) ) {
    m_sdFopen = fopen_;
    m_sdFclose = fclose_;
    m_sdFread = fread_;
    m_sdFseek = fseek_;
    m_sdFgetpos = fgetpos_;
    return 0;
}

/*
====================
System::SetAsynchronousDiskCallbacks
====================
*/
// 0x801FEC54
int System::SetAsynchronousDiskCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle*, bool ), int ( *fclose_ )( void* ),
                                          int ( *fread_ )( void*, void*, unsigned int, unsigned int*, int ( * )( void*, char*, int, int ), void*, bool ),
                                          int ( *fseek_ )( void*, unsigned int ), int ( *fgetpos_ )( void*, unsigned int& ),
                                          int ( *poll_ )( void* ), int ( *cancel_ )( void* ) ) {
    m_asdFopen = fopen_;
    m_asdFclose = fclose_;
    m_asdFread = fread_;
    m_asdFseek = fseek_;
    m_asdFgetpos = fgetpos_;
    m_asdPoll = poll_;
    m_asdCancel = cancel_;
    return 0;
}

/*
====================
System::SetFileSystemCallbacks
====================
*/
// 0x801FEC78
int System::SetFileSystemCallbacks( int ( *fopen_ )( const char*, unsigned int*, FileHandle* ), int ( *fclose_ )( void* ),
                                    int ( *fread_ )( void*, void*, unsigned int, unsigned int* ), int ( *fseek_ )( void*, unsigned int ),
                                    int blockAlignment ) {
    m_fopen = fopen_;
    m_fclose = fclose_;
    m_fread = fread_;
    m_fseek = fseek_;
    FMOD_FILE_OPENCALLBACK openCallback = fopen_ == NULL ? NULL : SFileOpen;
    FMOD_FILE_CLOSECALLBACK closeCallback = m_fclose == NULL ? NULL : SFileClose;
    FMOD_FILE_READCALLBACK readCallback = m_fread == NULL ? NULL : SFileRead;
    FMOD_FILE_SEEKCALLBACK seekCallback = m_fseek == NULL ? NULL : SFileSeek;
    m_fmodSystem->setFileSystem( openCallback, closeCallback, readCallback, seekCallback, blockAlignment );
    return 0;
}

/*
====================
System::ChannelStart
====================
*/
// 0x801FED10
void System::ChannelStart( FMOD::Channel* channel ) {
    channel->setChannelGroup( m_masterChannel );
}

/*
====================
System::RemoveSource
====================
*/
// 0x801FED20
bool System::RemoveSource( Source* source, bool deleteSource ) {
    m_sourceList.Remove( source );
    if ( deleteSource == true ) {
        delete source;
    }
    return true;
}

/*
====================
System::LoadStreamSource
====================
*/
// 0x801FEE10
Source* System::LoadStreamSource( const char* filename, int flags, SourceParams* params ) {
    Source* source = new( GAME_HEAP, GAME_HEAP_FLAGS ) Source;
    if ( source->LoadStream( filename, flags, params ) != 0 ) {
        delete source;
        return NULL;
    }
    m_sourceList.AddBack( source );
    return source;
}

/*
====================
ReverbInterpolate

The value percentage of the way from startValue to endValue (Gormiti: ReverbInterpolate<f> and <i>;
retail inlines both).
====================
*/
template< class T >
static inline T ReverbInterpolate( T startValue, T endValue, float percentage ) {
    return static_cast< T >( startValue * ( 1.0f - percentage ) + endValue * percentage );
}

/*
====================
System::InterpolateReverb

Blends two reverb presets by percentage into FMOD's reverb properties. A NULL start preset blends
from the current one.
====================
*/
// 0x801FEF18
int System::InterpolateReverb( ReverbPreset* startPreset, ReverbPreset* endPreset, float percentage ) {
    if ( startPreset == NULL ) {
        startPreset = m_currReverb;
    }
    FMOD_REVERB_PROPERTIES fmodReverb;
    memset( &fmodReverb, 0, sizeof( fmodReverb ) );
    fmodReverb.Instance = 0;
    fmodReverb.Room = ReverbInterpolate( startPreset->room, endPreset->room, percentage );
    fmodReverb.RoomHF = ReverbInterpolate( startPreset->roomHF, endPreset->roomHF, percentage );
    fmodReverb.RoomRolloffFactor = ReverbInterpolate( startPreset->roomRollOff, endPreset->roomRollOff, percentage );
    fmodReverb.DecayTime = ReverbInterpolate( startPreset->decayTime, endPreset->decayTime, percentage );
    fmodReverb.DecayHFRatio = ReverbInterpolate( startPreset->decayHFRatio, endPreset->decayHFRatio, percentage );
    fmodReverb.Reflections = ReverbInterpolate( startPreset->reflections, endPreset->reflections, percentage );
    fmodReverb.ReflectionsDelay = ReverbInterpolate( startPreset->reflectDelay, endPreset->reflectDelay, percentage );
    fmodReverb.Reverb = ReverbInterpolate( startPreset->reverb, endPreset->reverb, percentage );
    fmodReverb.ReverbDelay = ReverbInterpolate( startPreset->reverbDelay, endPreset->reverbDelay, percentage );
    fmodReverb.Diffusion = ReverbInterpolate( startPreset->diffusion, endPreset->diffusion, percentage );
    fmodReverb.Density = ReverbInterpolate( startPreset->density, endPreset->density, percentage );
    fmodReverb.HFReference = ReverbInterpolate( startPreset->hfReference, endPreset->hfReference, percentage );
    fmodReverb.RoomLF = ReverbInterpolate( startPreset->roomLF, endPreset->roomLF, percentage );
    fmodReverb.LFReference = ReverbInterpolate( startPreset->lfReference, endPreset->lfReference, percentage );
    fmodReverb.Flags = FMOD_REVERB_FLAGS_HIGHQUALITYREVERB;
    return m_fmodSystem->setReverbProperties( &fmodReverb );
}

/*
====================
System::UpdatePendingDeleteEventGroups

Once no sound is streaming and the async thread is idle, frees the event sources and then the event
groups that were parked for deletion.
====================
*/
// 0x801FF1E8
void System::UpdatePendingDeleteEventGroups() {
    if ( m_streamingSoundCount > 0 || IsAsyncThreadActive() ) {
        return;
    }
    ListUtils::STLList< EventSource* > toRemove;
    for ( ListUtils::STLList< EventSource* >::Iterator i = m_pendingDeleteEventSources.GetHead(); !i.End(); ++i ) {
        EventSource* esource = *i;
        m_pendingAsyncPlaybacks.Remove( esource );
        toRemove.AddBack( esource );
    }
    for ( ListUtils::STLList< EventSource* >::Iterator i = toRemove.GetHead(); !i.End(); ++i ) {
        EventSource* esource = *i;
        m_pendingDeleteEventSources.Remove( esource );
        char* name;   // Debug build: the name is logged with the delete (Gormiti Debug)
        esource->GetName( &name );
        esource->fn_801FCDE4();
    }
    for ( ListUtils::STLList< EventGroup* >::Iterator i = m_pendingDeleteEventGroups.GetHead(); !i.End(); ++i ) {
        EventGroup* egroup = *i;
        delete egroup;
    }
    m_pendingDeleteEventGroups.Clear();
}

/*
====================
System::SAsyncCallback
====================
*/
// 0x801FF554
int System::SAsyncCallback( void* userData, char* buffer, int readSize, int status ) {
    return GetInstance()->AsyncCallback( userData, buffer, readSize, status );
}

/*
====================
System::AsyncCallback

The async file system's completion callback (name by position: Gormiti's same-sized function).
====================
*/
// 0x801FF578
int System::AsyncCallback( void* userData, char* buffer, int readSize, int status ) {
    AsyncDiskSoundProject* project = static_cast< AsyncDiskSoundProject* >( userData );
    unsigned int bytesRead = 0;
    if ( readSize == SP_HEADER_READ_SIZE ) {
        // The header came in: set up the project's two disk streams and its memory stream, then read the project file.
        SPHeader hdr;
        memcpy( &hdr, buffer + SP_PREAMBLE_SIZE, sizeof( SPHeader ) );
        hdr.CorrectEndian();
        project->projectFile.m_length = hdr.entries[0].size;
        strcpy( project->projectFile.m_name, hdr.entries[0].name );
        project->projectFile.m_startFP = hdr.entries[0].offset + SP_PREAMBLE_SIZE;
        project->projectFile.m_fsHandle = project->handle.fsHandle;
        project->soundFile.m_length = hdr.entries[1].size;
        strcpy( project->soundFile.m_name, hdr.entries[1].name );
        project->soundFile.m_startFP = hdr.entries[1].offset + SP_PREAMBLE_SIZE;
        project->soundFile.m_fsHandle = project->handle.fsHandle;
        project->soundFile.m_fp = project->soundFile.m_startFP;
        project->memProjectFile.m_length = hdr.entries[0].size;
        strcpy( project->memProjectFile.m_name, hdr.entries[0].name );
        project->m_data = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[ project->memProjectFile.m_length ];
        project->memProjectFile.m_data = project->m_data;
        delete[] buffer;
        GetInstance()->m_asdFread( project->handle.fsHandle, project->m_data, project->memProjectFile.m_length, &bytesRead, SAsyncCallback, project, false );
        return 0;
    }
    project->memStreamRead = project->memProjectFile.m_length;
    return 0;
}

/*
====================
System::CancelAllAsyncStreams
====================
*/
// 0x801FF6C8
void System::CancelAllAsyncStreams() {
    for ( int i = 0; i < m_numActiveAsyncStreams; i++ ) {
        m_asdCancel( m_activeAsyncStreams[i]->m_fsHandle );
    }
}

/*
====================
System::AddAsyncStream
====================
*/
// 0x801FF734
void System::AddAsyncStream( DiskStream* asyncStream ) {
    if ( m_numActiveAsyncStreams < 32 ) {
        m_activeAsyncStreams[ m_numActiveAsyncStreams++ ] = asyncStream;
    }
}

/*
====================
System::RemoveAsyncStream
====================
*/
// 0x801FF758
void System::RemoveAsyncStream( DiskStream* asyncStream ) {
    int index = -1;
    for ( int i = 0; i < m_numActiveAsyncStreams; i++ ) {
        if ( m_activeAsyncStreams[i] == asyncStream ) {
            index = i;
            break;
        }
    }
    if ( index < 0 ) {
        return;
    }
    if ( index == m_numActiveAsyncStreams - 1 ) {
        m_activeAsyncStreams[index] = NULL;
        m_numActiveAsyncStreams--;
    } else {
        DiskStream* last = m_activeAsyncStreams[ m_numActiveAsyncStreams - 1 ];
        m_activeAsyncStreams[ m_numActiveAsyncStreams - 1 ] = m_activeAsyncStreams[index];
        m_activeAsyncStreams[index] = last;
        m_activeAsyncStreams[ m_numActiveAsyncStreams - 1 ] = NULL;
        m_numActiveAsyncStreams--;
    }
}

/*
====================
System::CheckAsyncStream
====================
*/
// 0x801FF818
bool System::CheckAsyncStream( DiskStream* asyncStream ) {
    for ( int i = 0; i < m_numActiveAsyncStreams; i++ ) {
        if ( m_activeAsyncStreams[i] == asyncStream ) {
            return true;
        }
    }
    return false;
}

/*
====================
System::CancelAsyncItemForEventSource
====================
*/
// 0x801FF84C
void System::CancelAsyncItemForEventSource( AsyncEventSource* esource ) {
    if ( esource == NULL ) {
        CancelAllAsyncStreams();
    } else {
        // Cancels the open async read of the project this source's group is waiting for.
        const char* name = esource->m_parent->GetName();
        AsyncDiskSoundProject* dproject = static_cast< AsyncDiskSoundProject* >( FindDiskSoundProject( name ) );
        for ( int i = 0; i < m_numActiveAsyncStreams; i++ ) {
            DiskStream* stream = m_activeAsyncStreams[i];
            if ( stricmp( stream->m_name, dproject->projectFile.m_name ) == 0 || stricmp( stream->m_name, dproject->soundFile.m_name ) == 0 ) {
                m_asdCancel( stream->m_fsHandle );
                break;
            }
        }
    }
    // The original does not check esource here (the NULL case above falls through to it).
    if ( m_currAsyncQueueItem != NULL && esource->m_parent != NULL && stricmp( esource->m_parent->GetName(), m_currAsyncQueueItem->filename ) == 0 ) {
        m_currAsyncQueueItem->cancelled = true;
        m_currAsyncQueueItem->complete = true;
        m_currAsyncQueueItem = NULL;
    }
}

/*
====================
System::QueueAsyncItem
====================
*/
// 0x801FF99C
int System::QueueAsyncItem( AsyncQueueItem* item ) {
    m_currAsyncQueueItem = item;
    return 0;
}

/*
====================
System::RemoveQueuedAsyncItem

Marks the queued request done and empties the queue (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021E87C
inline void System::RemoveQueuedAsyncItem( AsyncQueueItem* item ) {
    item->complete = true;
    m_currAsyncQueueItem = NULL;
}

/*
====================
System::UpdateAsyncQueueItem

The current request is done when its callback returns 0 (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021E8A0
inline void System::UpdateAsyncQueueItem() {
    if ( m_currAsyncQueueItem != NULL ) {
        if ( m_currAsyncQueueItem->callback( m_currAsyncQueueItem ) == 0 ) {
            RemoveQueuedAsyncItem( m_currAsyncQueueItem );
        }
    }
}

/*
====================
System::UpdateAsync

Runs the async queue and the pending sound projects.
====================
*/
// 0x801FF9A8
void System::UpdateAsync() {
    UpdateAsyncQueueItem();

    // The pending projects whose project file has arrived leave the pending list; the others are polled.
    ListUtils::STLList< AsyncDiskSoundProject* > toRemove;
    for ( ListUtils::STLList< AsyncDiskSoundProject* >::Iterator it = m_pendingAsyncProjects.GetHead(); !it.End(); ++it ) {
        AsyncDiskSoundProject* project = *it;
        if ( project->handle.fsHandle != NULL ) {
            if ( project->memStreamRead == project->projectFile.m_length && project->projectFile.m_length > 0 ) {
                toRemove.AddBack( project );
            } else {
                GetInstance()->m_asdPoll( project->handle.fsHandle );
            }
        }
    }
    for ( ListUtils::STLList< AsyncDiskSoundProject* >::Iterator it = toRemove.GetHead(); !it.End(); ++it ) {
        m_pendingAsyncProjects.Remove( *it );
    }
    toRemove.Clear();
    FlushPendingAsyncGroups();
}

/*
====================
System::FlushPendingAsyncGroups

Loads the pending event groups whose async project has arrived, and takes the async playbacks of
those groups off the pending list.
====================
*/
// 0x801FFC88
void System::FlushPendingAsyncGroups() {
    if ( m_pendingAsyncProjects.GetSize() != 0 ) {
        return;
    }
    ListUtils::STLList< EventGroup* > toRemove;
    ListUtils::STLList< EventSource* > toRemoveES;
    for ( ListUtils::STLList< EventGroup* >::Iterator i = m_pendingEventGroups.GetHead(); !i.End(); ++i ) {
        EventGroup* egroup = *i;
        DiskSoundProject* dproject = FindDiskSoundProject( egroup->GetName() );
        if ( dproject != NULL && dproject->IsAsync() ) {
            AsyncDiskSoundProject* project = static_cast< AsyncDiskSoundProject* >( dproject );
            if ( project->m_data != NULL && project->memStreamRead >= project->projectFile.m_length ) {
                egroup->Init( m_fmodEventSystem, project->memProjectFile.m_data, project->name, project->memProjectFile.m_length, false, egroup->IsAutoDelete() );
                toRemove.AddBack( egroup );
            }
        }
    }
    for ( ListUtils::STLList< EventGroup* >::Iterator i = toRemove.GetHead(); !i.End(); ++i ) {
        EventGroup* egroup = *i;
        for ( ListUtils::STLList< EventSource* >::Iterator j = m_pendingAsyncPlaybacks.GetHead(); !j.End(); ++j ) {
            EventSource* esource = *j;
            if ( esource->m_parent == egroup ) {
                toRemoveES.AddBack( esource );
            }
        }
        for ( ListUtils::STLList< EventSource* >::Iterator j = toRemoveES.GetHead(); !j.End(); ++j ) {
            m_pendingAsyncPlaybacks.Remove( *j );
        }
        toRemoveES.Clear();
        m_pendingEventGroups.Remove( egroup );
        m_eventGroups.AddBack( egroup );
    }
    toRemove.Clear();
}

/*
====================
System::AddUpdateSound
====================
*/
// 0x80200210
void System::AddUpdateSound( EventSource* sound ) {
    if ( m_updateSounds.Find( sound ) == NULL ) {
        m_updateSounds.AddBack( sound );
    }
}

/*
====================
System::RemoveUpdateSound
====================
*/
// 0x802002DC
void System::RemoveUpdateSound( EventSource* sound ) {
    m_updateSounds.Remove( sound );
}

/*
====================
System::AddPendingCloseAsyncStream
====================
*/
// 0x80200370
bool System::AddPendingCloseAsyncStream( DiskStream* asyncStream ) {
    for ( int i = 0; i < m_numPendingCloseAsyncStreams; i++ ) {
        if ( m_pendingCloseAsyncStreams[i] == asyncStream ) {
            return false;
        }
    }
    m_pendingCloseAsyncStreams[ m_numPendingCloseAsyncStreams++ ] = asyncStream;
    return true;
}

/*
====================
System::UpdateReverb

Moves the reverb on towards the target preset (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021DF40
inline void System::UpdateReverb( float frameInterval ) {
    if ( m_targetReverb != NULL && m_currReverb != NULL ) {
        m_currReverbInterpTime += frameInterval;
        float percentage = m_currReverbInterpTime / m_totalReverbInterpTime;
        bool done = false;
        if ( percentage > 1.0f ) {
            percentage = 1.0f;
            done = true;
        }
        InterpolateReverb( m_currReverb, m_targetReverb, percentage );
        if ( done == true ) {
            m_currReverb = m_targetReverb;
            m_targetReverb = NULL;
        }
    }
}

/*
====================
System::UpdateSounds

The sounds that need no more updates leave the update list (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021F644
inline void System::UpdateSounds() {
    for ( ListUtils::STLList< EventSource* >::Iterator it = m_updateSounds.GetTail(); !it.End(); ) {
        EventSource* sound = *it;
        --it;
        int errCode = sound->Update();
        if ( errCode == EventNoUpdateNeeded ) {
            RemoveUpdateSound( sound );
        }
    }
}

/*
====================
System::UpdatePendingCloseDiskStreams

Closes the async streams that were queued for closing (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x8021F7B0
inline void System::UpdatePendingCloseDiskStreams() {
    for ( int i = 0; i < m_numPendingCloseAsyncStreams; i++ ) {
        AsyncDiskFileClose( m_pendingCloseAsyncStreams[i] );
    }
    memset( m_pendingCloseAsyncStreams, 0, sizeof( m_pendingCloseAsyncStreams ) );
    m_numPendingCloseAsyncStreams = 0;
}

/*
====================
System::Update

The per-frame update (name by position: Gormiti's same-sized function).
====================
*/
// 0x802003C0
int System::Update( float frameInterval ) {
    m_lastFrameInterval = frameInterval;
    UpdateAsync();
    UpdateReverb( frameInterval );
    UpdateSounds();
    UpdatePendingCloseDiskStreams();
    UpdatePendingDeleteEventGroups();
    m_fmodEventSystem->update();
    return 0;
}
/*
====================
System::GetDefaultListener
====================
*/
// 0x802005AC
Listener* System::GetDefaultListener() {
    return m_listeners[0];
}

/*
====================
System::LoadFirstEventSourceFromAnyGroup
====================
*/
// 0x802005B4
EventSource* System::LoadFirstEventSourceFromAnyGroup( const char* eventSourceName ) {
    for ( ListUtils::STLList< EventGroup* >::Iterator it = m_eventGroups.GetTail(); !it.End(); --it ) {
        EventGroup* egroup = *it;
        if ( !egroup->IsAsync() ) {
            EventSource* esource = egroup->LoadEventSource( eventSourceName, true, NULL );
            if ( esource != NULL ) {
                return esource;
            }
        }
    }
    return NULL;
}

/*
====================
System::IsEventGroupLoaded
====================
*/
// 0x80200650
EventGroup* System::IsEventGroupLoaded( const char* friendlyName ) {
    for ( ListUtils::STLList< EventGroup* >::Iterator it = m_eventGroups.GetHead(); !it.End(); ++it ) {
        EventGroup* iter = *it;
        if ( stricmp( iter->GetName(), friendlyName ) == 0 ) {
            return iter;
        }
        if ( iter->HasDifferentNameFromFMOD() == true ) {
            char* fmodName = NULL;
            iter->GetFMODName( &fmodName );
            if ( fmodName != NULL && stricmp( fmodName, friendlyName ) == 0 ) {
                return iter;
            }
        }
    }
    return NULL;
}

/*
====================
System::LoadEventGroup
====================
*/
// 0x8020072C
EventGroup* System::LoadEventGroup( const char* nameOrData, const char* friendlyName, int lengthOfData, bool loadedFromDisk, bool autoDelete ) {
    if ( lengthOfData > 0 && friendlyName == NULL ) {
        // Debug build: assertion at this point (Gormiti Debug)
        RWS_ASSERT_FAIL( "Invalid parameters" );
    }
    if ( friendlyName == NULL ) {
        friendlyName = nameOrData;
    }
    EventGroup* egroup = IsEventGroupLoaded( friendlyName );
    if ( egroup != NULL ) {
        egroup->AddRef();
        return egroup;
    }
    egroup = new( GAME_HEAP, GAME_HEAP_FLAGS ) EventGroup;
    int result = egroup->Init( m_fmodEventSystem, nameOrData, friendlyName, lengthOfData, loadedFromDisk, autoDelete );
    if ( result != 0 ) {
        delete egroup;
        return NULL;
    }
    m_eventGroups.AddBack( egroup );
    egroup->AddRef();
    return egroup;
}

/*
====================
System::ForceDeleteEventGroup
====================
*/
// 0x8020092C
bool System::ForceDeleteEventGroup( EventGroup* egroup, bool deletePointerAsWell ) {
    if ( !deletePointerAsWell ) {
        egroup->Free();
    } else {
        egroup->SetCustomData( NULL );
        m_eventGroups.Remove( egroup );
        delete egroup;
    }
    return true;
}

/*
====================
System::FreeEventGroup

Gives up one reference to the group; the last reference takes it out of the lists and deletes it,
or parks it until the streams and the async thread are idle.
====================
*/
// 0x80200A30
bool System::FreeEventGroup( EventGroup* egroup, bool deleteGroup ) {
    if ( deleteGroup == true ) {
        egroup->ReleaseRef();
        int refCount = egroup->GetRefCount();
        if ( refCount == 0 ) {
            egroup->SetCustomData( NULL );
            m_eventGroups.Remove( egroup );
            m_pendingEventGroups.Remove( egroup );
            if ( m_streamingSoundCount <= 0 && !IsAsyncThreadActive() ) {
                delete egroup;
            } else {
                m_pendingDeleteEventGroups.AddBack( egroup );
            }
            return true;
        }
        return false;
    }
    return false;
}

/*
====================
System::LoadDiskSoundProject

Reads the pack header of an opened sound project through the synchronous disk callbacks and sets up
the project's disk streams from it (name by position: Gormiti's overload of the same signature).
====================
*/
// 0x80200C64
DiskSoundProject* System::LoadDiskSoundProject( FileHandle* fileHandle, const char* filename ) {
    DiskSoundProject* project = new( GAME_HEAP, GAME_HEAP_FLAGS ) DiskSoundProject;
    project->handle = *fileHandle;
    unsigned int startPos = 0;
    GetInstance()->m_sdFgetpos( project->handle.fsHandle, startPos );
    GetInstance()->m_sdFseek( project->handle.fsHandle, startPos + SP_PREAMBLE_SIZE );
    SPHeader hdr;
    unsigned int bytesRead = 0;
    strcpy( project->name, filename );
    GetInstance()->m_sdFread( project->handle.fsHandle, &hdr, sizeof( SPHeader ), &bytesRead );
    hdr.CorrectEndian();
    project->projectFile.m_length = hdr.entries[0].size;
    strcpy( project->projectFile.m_name, hdr.entries[0].name );
    project->projectFile.m_startFP = startPos + SP_PREAMBLE_SIZE + hdr.entries[0].offset;
    project->projectFile.m_fsHandle = project->handle.fsHandle;
    project->soundFile.m_length = hdr.entries[1].size;
    strcpy( project->soundFile.m_name, hdr.entries[1].name );
    project->soundFile.m_startFP = startPos + SP_PREAMBLE_SIZE + hdr.entries[1].offset;
    project->soundFile.m_fsHandle = project->handle.fsHandle;
    m_diskSoundProjects.AddBack( project );
    return project;
}

/*
====================
System::RemoveAsyncPlayback
====================
*/
// 0x80200EEC
void System::RemoveAsyncPlayback( AsyncEventSource* esource ) {
    m_pendingAsyncPlaybacks.Remove( esource );
}

/*
====================
System::LoadAsyncDiskSoundProject

The async disk sound project of that file: the loaded one, or a new one whose header is read by the
async file system (the callback reads the project file when the header has arrived).
====================
*/
// 0x80200F80
AsyncDiskSoundProject* System::LoadAsyncDiskSoundProject( const char* filename ) {
    DiskSoundProject* found = FindDiskSoundProject( filename );
    if ( found != NULL ) {
        if ( found->IsAsync() ) {
            return static_cast< AsyncDiskSoundProject* >( found );
        }
        return NULL;
    }
    FileHandle handle;
    unsigned int fileSize = 0;
    if ( GetInstance()->m_asdFopen( filename, &fileSize, &handle, false ) != 0 ) {
        return NULL;
    }
    AsyncDiskSoundProject* project = new( GAME_HEAP, GAME_HEAP_FLAGS ) AsyncDiskSoundProject;
    project->handle = handle;
    strcpy( project->name, filename );
    m_pendingAsyncProjects.AddBack( project );
    m_diskSoundProjects.AddBack( project );
    char* header = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[ SP_HEADER_READ_SIZE ];
    unsigned int bytesRead = 0;
    GetInstance()->m_asdFread( project->handle.fsHandle, header, SP_HEADER_READ_SIZE, &bytesRead, SAsyncCallback, project, false );
    return project;
}

/*
====================
System::LoadAsyncEventGroup
====================
*/
// 0x802012D0
EventGroup* System::LoadAsyncEventGroup( const char* friendlyName, DiskSoundProject* project, bool autoDelete ) {
    EventGroup* egroup = IsEventGroupLoaded( friendlyName );
    if ( egroup != NULL ) {
        return egroup;
    }
    egroup = new( GAME_HEAP, GAME_HEAP_FLAGS ) AsyncEventGroup( friendlyName );
    egroup->SetAutoDelete( autoDelete );
    egroup->SetParentProject( project );
    egroup->AddRef();
    m_pendingEventGroups.AddBack( egroup );
    return egroup;
}

/*
====================
System::FindDiskSoundProject

The loaded disk sound project of that name, if there is one (no copy in retail: inlined).
====================
*/
// no copy in retail (inlined); Gormiti 0x80220624
inline DiskSoundProject* System::FindDiskSoundProject( const char* filename ) {
    for ( ListUtils::STLList< DiskSoundProject* >::Iterator it = m_diskSoundProjects.GetHead(); !it.End(); ++it ) {
        DiskSoundProject* project = *it;
        if ( stricmp( project->name, filename ) == 0 ) {
            return project;
        }
    }
    return NULL;
}

/*
====================
System::LoadDiskSoundProject

The disk sound project of that file: the loaded one, or one read through the synchronous disk callbacks.
====================
*/
// 0x80201460
DiskSoundProject* System::LoadDiskSoundProject( const char* filename ) {
    DiskSoundProject* project = FindDiskSoundProject( filename );
    if ( project != NULL ) {
        return project;
    }
    FileHandle handle;
    unsigned int fileSize = 0;
    if ( GetInstance()->m_sdFopen( filename, &fileSize, &handle ) != 0 || fileSize == 0 ) {
        return NULL;
    }
    return LoadDiskSoundProject( &handle, filename );
}

/*
====================
System::LoadSoundProject

Name by position: Gormiti's function of the same signature.
====================
*/
// 0x80201550
SoundProject* System::LoadSoundProject( void* buffer, int size ) {
    MemoryStream stream( "temp", static_cast< const char* >( buffer ), size );
    SPHeader hdr;
    unsigned int read = 0;
    MemFileRead( &stream, &hdr, sizeof( SPHeader ), &read );
    hdr.CorrectEndian();
    SoundProject* project = new( GAME_HEAP, GAME_HEAP_FLAGS ) SoundProject;
    strcpy( project->name, hdr.name );
    project->data = static_cast< char* >( buffer );
    project->projectFile.m_length = hdr.entries[0].size;
    strcpy( project->projectFile.m_name, hdr.entries[0].name );
    project->projectFile.m_data = project->data + hdr.entries[0].offset;
    project->soundFile.m_length = hdr.entries[1].size;
    strcpy( project->soundFile.m_name, hdr.entries[1].name );
    project->soundFile.m_data = stream.m_data + hdr.entries[1].offset;
    m_soundProjects.AddBack( project );
    return project;
}

/*
====================
System::FindSoundProjectFile
====================
*/
// 0x80201720
MemoryStream* System::FindSoundProjectFile( const char* filename ) {
    for ( ListUtils::STLList< SoundProject* >::Iterator it = m_soundProjects.GetHead(); !it.End(); ++it ) {
        SoundProject* project = *it;
        if ( stricmp( project->projectFile.m_name, filename ) == 0 ) {
            return &project->projectFile;
        }
        if ( stricmp( project->soundFile.m_name, filename ) == 0 ) {
            return &project->soundFile;
        }
    }
    for ( ListUtils::STLList< DiskSoundProject* >::Iterator it = m_diskSoundProjects.GetHead(); !it.End(); ++it ) {
        DiskSoundProject* project = *it;
        if ( project->IsAsync() && stricmp( project->projectFile.m_name, filename ) == 0 ) {
            return &static_cast< AsyncDiskSoundProject* >( project )->memProjectFile;
        }
    }
    return NULL;
}

/*
====================
System::FindDiskSoundProjectFile
====================
*/
// 0x80201808
DiskStream* System::FindDiskSoundProjectFile( const char* filename, DiskSoundProject** refProject ) {
    for ( ListUtils::STLList< DiskSoundProject* >::Iterator it = m_diskSoundProjects.GetHead(); !it.End(); ++it ) {
        DiskSoundProject* iter = *it;
        if ( !iter->IsAsync() || iter->IsLoaded() ) {
            if ( stricmp( iter->projectFile.m_name, filename ) == 0 && !iter->IsAsync() ) {
                if ( refProject != NULL ) {
                    *refProject = iter;
                }
                return &iter->projectFile;
            }
            if ( stricmp( iter->soundFile.m_name, filename ) == 0 ) {
                if ( refProject != NULL ) {
                    *refProject = iter;
                }
                return &iter->soundFile;
            }
        }
    }
    return NULL;
}

/*
====================
System::FreeDiskSoundProject
====================
*/
// 0x8020190C
void System::FreeDiskSoundProject( DiskSoundProject* project, bool deleteProject ) {
    m_diskSoundProjects.Remove( project );
    if ( project->IsAsync() ) {
        m_pendingAsyncProjects.Remove( static_cast< AsyncDiskSoundProject* >( project ) );
    }
    if ( deleteProject == true ) {
        delete project;
    }
}

/*
====================
System::FreeSoundProject
====================
*/
// 0x80201AAC
void System::FreeSoundProject( SoundProject* project, bool deleteProject ) {
    m_soundProjects.Remove( project );
    if ( deleteProject == true ) {
        delete project;
    }
}

/*
====================
SoundProject::~SoundProject
====================
*/
// 0x80201B98
SoundProject::~SoundProject() {
    delete[] data;
}

/*
====================
System::LoadEventGroup
====================
*/
// 0x80201C10
EventGroup* System::LoadEventGroup( SoundProject* project ) {
    return LoadEventGroup( project->projectFile.m_data, project->name, project->projectFile.m_length, false, false );
}

/*
====================
System::FindReverbPreset
====================
*/
// 0x80201C2C
ReverbPreset* System::FindReverbPreset( const char* name ) {
    for ( ListUtils::STLList< ReverbPreset* >::Iterator it = m_reverbPresets.GetHead(); !it.End(); ++it ) {
        ReverbPreset* preset = *it;
        if ( stricmp( preset->name, name ) == 0 ) {
            return preset;
        }
    }
    return NULL;
}

/*
====================
System::SetReverbPreset
====================
*/
// 0x80201C9C
int System::SetReverbPreset( ReverbPreset* preset, bool force ) {
    if ( m_currReverb != NULL && force == true ) {
        m_targetReverb = preset;
        m_currReverbInterpTime = 0.0f;
        return 0;
    }
    m_currReverb = preset;
    FMOD_REVERB_PROPERTIES fmodReverb;
    memset( &fmodReverb, 0, sizeof( fmodReverb ) );
    fmodReverb.Instance = 0;
    fmodReverb.Room = preset->room;
    fmodReverb.RoomHF = preset->roomHF;
    fmodReverb.RoomRolloffFactor = preset->roomRollOff;
    fmodReverb.DecayTime = preset->decayTime;
    fmodReverb.DecayHFRatio = preset->decayHFRatio;
    fmodReverb.Reflections = preset->reflections;
    fmodReverb.ReflectionsDelay = preset->reflectDelay;
    fmodReverb.Reverb = preset->reverb;
    fmodReverb.ReverbDelay = preset->reverbDelay;
    fmodReverb.Diffusion = preset->diffusion;
    fmodReverb.Density = preset->density;
    fmodReverb.HFReference = preset->hfReference;
    fmodReverb.RoomLF = preset->roomLF;
    fmodReverb.LFReference = preset->lfReference;
    fmodReverb.Flags = FMOD_REVERB_FLAGS_HIGHQUALITYREVERB;
    FMOD_RESULT result = m_fmodSystem->setReverbProperties( &fmodReverb );
    return result;
}

/*
====================
System::SetReverb
====================
*/
// 0x80201D98
int System::SetReverb( const char* presetName ) {
    if ( presetName == NULL || stricmp( presetName, "off" ) == 0 || stricmp( presetName, "none" ) == 0 ) {
        FMOD_REVERB_PROPERTIES off = FMOD_PRESET_OFF;
        return m_fmodSystem->setReverbProperties( &off );
    }
    ReverbPreset* preset = FindReverbPreset( presetName );
    if ( preset == NULL ) {
        return -1;
    }
    return SetReverbPreset( preset, true );
}

/*
====================
System::AddReverbPreset
====================
*/
// 0x80201EA4
void System::AddReverbPreset( ReverbPreset* preset ) {
    m_reverbPresets.AddBack( preset );
}

/*
====================
System::CanDeleteEventSource
====================
*/
// 0x80201F40
bool System::CanDeleteEventSource( EventSource* esource, bool force ) {
    if ( m_streamingSoundCount > 0 ) {
        if ( force ) {
            m_pendingDeleteEventSources.AddBack( esource );
        }
        return false;
    }
    for ( ListUtils::STLList< EventSource* >::Iterator it = m_pendingDeleteEventSources.GetHead(); !it.End(); ++it ) {
        if ( *it == esource ) {
            return false;
        }
    }
    return true;
}

/*
====================
AsyncDiskSoundProject::IsLoaded
====================
*/
// 0x80202024
bool AsyncDiskSoundProject::IsLoaded() const {
    if ( memStreamRead == memProjectFile.m_length && memProjectFile.m_length > 0 ) {
        return true;
    }
    return false;
}

/*
====================
AsyncDiskSoundProject::IsAsync
====================
*/
// 0x8020204C
bool AsyncDiskSoundProject::IsAsync() const {
    return true;
}

/*
====================
AsyncDiskSoundProject::~AsyncDiskSoundProject
====================
*/
// 0x80202054
AsyncDiskSoundProject::~AsyncDiskSoundProject() {
    if ( m_data != NULL ) {
        delete[] m_data;
    }
}

}
