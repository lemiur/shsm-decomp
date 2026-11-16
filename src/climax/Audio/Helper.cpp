// Unit name and boundary: Gormiti link map (Helper.obj)
// Names and layout: Gormiti link map and DWARF (Helper.obj, game/System/Audio/FMOD/Helper.cpp)
//
// Data owned by this unit: .sdata 0x80528890 size 0x8, lbl_80528890 (the string MemFileOpen copies).
//
// DiskFileRead (0x801FD7FC) and AsyncDiskFileRead (0x801FDA28) are unpaired with Gormiti's functions of
// the same name: retail is another revision.

#include <string.h>
#include <climax/Audio/Helper.h>
#include <climax/Audio/System.h>

#include <climax/Memory.h>
#include <climax/ThreadSync.h>

namespace Audio {

/*
====================
DiskSoundProject::ReleaseRef

Gives up one reference and frees the project (and its disk streams) once none are left.
====================
*/
// 0x801FD268
void DiskSoundProject::ReleaseRef() {
    m_refCount--;
    if ( m_refCount <= 0 ) {
        System::GetInstance()->FreeDiskSoundProject( this, true );
    }
}

/*
====================
MemFileOpen

Opens a sound project file for FMOD: either the memory FMOD was handed (fileSize is not zero, name is the
data) or the memory stream a loaded sound project registered under name. Hands FMOD a private copy.
====================
*/
// 0x801FD28C
int MemFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle ) {
    MemoryStream* stream = NULL;
    if ( *fileSize != 0 ) {
        stream = new( GAME_HEAP, AUDIO_HEAP_FLAGS ) MemoryStream( "", name, *fileSize );
    } else {
        MemoryStream* loadedStream = System::GetInstance()->FindSoundProjectFile( name );
        if ( loadedStream != NULL ) {
            stream = new( GAME_HEAP, AUDIO_HEAP_FLAGS ) MemoryStream( loadedStream->m_name, loadedStream->m_data, loadedStream->m_length );
            *fileSize = stream->m_length;
        }
    }
    if ( stream == NULL ) {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    handle->fsHandle = stream;
    handle->fread = MemFileRead;
    handle->fclose = MemFileClose;
    handle->fseek = MemFileSeek;
    handle->ftell = MemFileGetPos;
    return FMOD_OK;
}

/*
====================
MemFileClose

Frees the memory stream FMOD was handed by MemFileOpen.
====================
*/
// 0x801FD3AC
int MemFileClose( void* handle ) {
    MemoryStream* stream = static_cast< MemoryStream* >( handle );
    if ( stream != NULL ) {
        delete stream;
    }
    return 0;
}

/*
====================
MemFileRead

Copies up to size bytes from the stream's position, no more than are left, and moves the position on.
====================
*/
// 0x801FD3D8
int MemFileRead( void* handle, void* buffer, unsigned int size, unsigned int* read ) {
    MemoryStream* stream = static_cast< MemoryStream* >( handle );
    int count = size;
    if ( count + stream->m_fp >= stream->m_length ) {
        count = stream->m_length - stream->m_fp;
    }
    memcpy( buffer, stream->m_data + stream->m_fp, count );
    *read = count;
    stream->m_fp += count;
    return 0;
}

/*
====================
MemFileSeek

====================
*/
// 0x801FD458
int MemFileSeek( void* handle, unsigned int pos ) {
    static_cast< MemoryStream* >( handle )->m_fp = pos;
    return 0;
}

/*
====================
MemFileGetPos

====================
*/
// 0x801FD464
int MemFileGetPos( void* handle, unsigned int& pos ) {
    pos = static_cast< MemoryStream* >( handle )->m_fp;
    return 0;
}

/*
====================
DiskSoundProject::~DiskSoundProject

Closes the file handle the project was opened with; the two streams free their own buffers.
====================
*/
// 0x801FD474
DiskSoundProject::~DiskSoundProject() {
    if ( handle.fsHandle != NULL ) {
        handle.fclose( handle.fsHandle );
    }
}

/*
====================
DiskFileOpen

Opens a sound file of a loaded disk sound project for FMOD. The stream it hands out is a copy of the
project's own stream: an async stream opens its own file handle when the project's is already in use.
====================
*/
// 0x801FD540
int DiskFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle ) {
    DiskStream* stream = NULL;
    DiskSoundProject* dproject;
    DiskStream* loadedStream = System::GetInstance()->FindDiskSoundProjectFile( name, &dproject );
    if ( loadedStream != NULL ) {
        if ( loadedStream->m_isAsync && System::GetInstance()->IsAsyncStreamingDisabled() ) {
            return FMOD_ERR_FILE_NOTFOUND;
        }
        stream = new( GAME_HEAP, AUDIO_HEAP_FLAGS ) DiskStream( loadedStream->m_name, loadedStream->m_startFP, loadedStream->m_length );
        stream->m_isAsync = loadedStream->m_isAsync;
        stream->orgStream = loadedStream;
        if ( loadedStream->m_locked && stream->m_isAsync ) {
            FileHandle handle;                      // shadows the parameter, as in the original
            int retVal = System::GetInstance()->m_asdFopen( dproject->name, NULL, &handle, true );
            if ( retVal != 0 || handle.fsHandle == NULL ) {
                delete stream;
                *fileSize = 0;
                return retVal;
            }
            stream->m_fsHandle = handle.fsHandle;
            stream->m_didDuplicate = true;
        } else {
            stream->m_fsHandle = loadedStream->m_fsHandle;
            stream->orgStream->m_locked = true;
        }
        if ( stream->m_isAsync ) {
            System::GetInstance()->AddAsyncStream( stream );
        }
        *fileSize = stream->m_length;
    }
    if ( stream == NULL ) {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    handle->fsHandle = stream;
    if ( !stream->m_isAsync ) {
        handle->fread = DiskFileRead;
        handle->fclose = DiskFileClose;
        handle->fseek = DiskFileSeek;
        handle->ftell = DiskFileGetPos;
    } else {
        handle->fread = AsyncDiskFileRead;
        handle->fclose = AsyncDiskFileClose;
        handle->fseek = AsyncDiskFileSeek;
        handle->ftell = AsyncDiskFileGetPos;
    }
    return FMOD_OK;
}

/*
====================
DiskFileClose

Frees the stream FMOD was handed by DiskFileOpen.
====================
*/
// 0x801FD798
int DiskFileClose( void* handle ) {
    DiskStream* diskStream = static_cast< DiskStream* >( handle );
    // Debug build: the result is kept in a local `loadedStream` (Gormiti Debug)
    System::GetInstance()->FindDiskSoundProjectFile( diskStream->m_name, NULL );
    delete diskStream;
    return 0;
}

/*
====================
DiskFileRead

Reads from the project's file at the stream's position, seeking there first when the file is elsewhere.
====================
*/
// 0x801FD7FC
int DiskFileRead( void* handle, void* buffer, unsigned int bytesToRead, unsigned int* bytesRead ) {
    DiskStream* diskStream = static_cast< DiskStream* >( handle );
    unsigned int currentPos;
    System::GetInstance()->m_sdFgetpos( diskStream->m_fsHandle, currentPos );
    if ( currentPos != diskStream->m_startFP + diskStream->m_fp ) {
        System::GetInstance()->m_sdFseek( diskStream->m_fsHandle, diskStream->m_startFP + diskStream->m_fp );
    }
    System::GetInstance()->m_sdFread( diskStream->m_fsHandle, buffer, bytesToRead, bytesRead );
    diskStream->m_fp += *bytesRead;
    return 0;
}

/*
====================
DiskFileSeek

====================
*/
// 0x801FD8C0
int DiskFileSeek( void* handle, unsigned int pos ) {
    static_cast< DiskStream* >( handle )->m_fp = pos;
    return 0;
}

/*
====================
DiskFileGetPos

====================
*/
// 0x801FD8CC
int DiskFileGetPos( void* handle, unsigned int& pos ) {
    pos = static_cast< DiskStream* >( handle )->m_fp;
    return 0;
}

/*
====================
AsyncDiskFileClose

Frees an async stream. Off the main thread the system closes it later (AddPendingCloseAsyncStream).
====================
*/
// 0x801FD8DC
int AsyncDiskFileClose( void* handle ) {
    DiskStream* diskStream = static_cast< DiskStream* >( handle );
    if ( !ThreadSync::IsMainThread() ) {
        System::GetInstance()->AddPendingCloseAsyncStream( diskStream );
        return 0;
    }
    if ( diskStream->m_isAsync ) {
        System::GetInstance()->RemoveAsyncStream( diskStream );
        if ( diskStream->m_didDuplicate ) {
            System::GetInstance()->m_asdFclose( diskStream->m_fsHandle );
        } else if ( diskStream->orgStream != NULL ) {
            diskStream->orgStream->m_locked = false;
        }
    }
    delete diskStream;
    return 0;
}

/*
====================
AsyncCallback

Called when an async read finishes: records how much was read (nothing when the read failed) and
lets the stream read again.
====================
*/
// 0x801FD9AC
int AsyncCallback( void* userData, char* buffer, int readSize, int status ) {
    DiskStream* diskStream = static_cast< DiskStream* >( userData );
    if ( !System::GetInstance()->CheckAsyncStream( diskStream ) ) {
        return 0;
    }
    diskStream->m_dataRead = readSize;
    if ( status == 2 ) {
        diskStream->m_dataRead = 0;
    }
    diskStream->m_isReading = false;
    return 0;
}

/*
====================
AsyncDiskFileRead

Starts an async read of the project's file; on failure FMOD gets an empty (zeroed) buffer.
====================
*/
// 0x801FDA28
int AsyncDiskFileRead( void* handle, void* buffer, unsigned int bytesToRead, unsigned int* bytesRead ) {
    DiskStream* diskStream = static_cast< DiskStream* >( handle );
    unsigned int currentPos;
    System::GetInstance()->m_asdFgetpos( diskStream->m_fsHandle, currentPos );
    int ret = System::GetInstance()->m_asdFread( diskStream->m_fsHandle, buffer, bytesToRead, bytesRead, AsyncCallback, diskStream, true );
    if ( ret == 0 ) {
        diskStream->m_fp += *bytesRead;
    } else {
        *bytesRead = 0;
        memset( buffer, 0, bytesToRead );
    }
    return ret;
}

/*
====================
AsyncDiskFileSeek

Seeks the async file handle to pos within the stream's data and remembers the position.
====================
*/
// 0x801FDAE0
int AsyncDiskFileSeek( void* handle, unsigned int pos ) {
    DiskStream* stream = static_cast< DiskStream* >( handle );
    int newPos = pos + stream->m_startFP;
    System::GetInstance()->m_asdFseek( stream->m_fsHandle, newPos );
    stream->m_fp = newPos;
    return 0;
}

/*
====================
AsyncDiskFileGetPos

====================
*/
// 0x801FDB38
int AsyncDiskFileGetPos( void* handle, unsigned int& pos ) {
    pos = static_cast< DiskStream* >( handle )->m_fp;
    return 0;
}

}
