// Unit name and boundary: Gormiti link map (Music.obj, game/Objects/Audio/Music.cpp); the MusicFileOpen body is paired with it by identical bytes

#include <string.h>
#include <climax/Audio/Music.h>
#include <climax/Audio/Helper.h>
#include <climax/Memory.h>
#include <climax/Utils.h>

namespace Audio {

// The music file the game has loaded; read by MusicFileOpen and written by the loader.
MusicData* lbl_80529318;

/*
====================
MusicFileOpen

Opens the loaded music file for FMOD: hands FMOD a memory stream over its XM data, under the name FMOD asked for.

The local nameLength is the Debug build's (Gormiti Debug: it feeds an extension check). Gormiti's Final DWARF drops
it, the way MWCC's DWARF drops locals the optimiser removes, and the strlen call stays. R17 reads the local as dead:
the function is MATCH-PENDING until the source is settled.
====================
*/
// 0x8003BCA4
int MusicFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle ) {
    if ( lbl_80529318 == NULL ) {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    // Debug build: the name length feeds an extension check (Gormiti Debug); the release build keeps only this call.
    int nameLength = strlen( name );
    MemoryStream* stream = new( GAME_HEAP, GAME_HEAP_FLAGS ) MemoryStream( name, lbl_80529318->data, lbl_80529318->hdr.xmLength );
    if ( stream == NULL ) {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    handle->fsHandle = stream;
    *fileSize = stream->m_length;
    handle->fread = MemFileRead;
    handle->fclose = MemFileClose;
    handle->fseek = MemFileSeek;
    return FMOD_OK;
}

/*
====================
LoadMusicData

Lays a music file's header and XM data into musicData. When correctEndian is set the header is copied and its
lengths are byte-swapped. When allocateNew is set the data is copied into a new buffer, otherwise it is read in
place from data. Returns the size of the header and data, or 0 when the buffer could not be allocated.
====================
*/
// 0x8003BD70
int LoadMusicData( char* data, int, bool allocateNew, bool correctEndian, MusicData* musicData ) {
    if ( correctEndian == true ) {
        memcpy( musicData, data, sizeof( MusicHeader ) );
        musicData->hdr.xmLength = fn_801B6C78( musicData->hdr.xmLength );
        musicData->hdr.version = fn_801B6C78( musicData->hdr.version );
    }
    int musicLength = musicData->hdr.xmLength;
    if ( allocateNew == true ) {
        musicData->allocated = true;
        musicData->data = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[ musicLength ];
        if ( musicData->data == NULL ) {
            return 0;
        }
        memcpy( musicData->data, data + sizeof( MusicHeader ), musicLength );
    } else {
        musicData->data = data + sizeof( MusicHeader );
        musicData->allocated = false;
    }
    return musicLength + sizeof( MusicHeader );
}

}
