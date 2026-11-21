#ifndef CLIMAX_AUDIO_MUSIC_H
#define CLIMAX_AUDIO_MUSIC_H

// Names and layout: Gormiti link map and DWARF (Audio::MusicHeader and Audio::MusicData, game/Objects/Audio/Music.cpp)

#include <revolution/types.h>
#include <climax/Audio/Defs.h>

namespace Audio {

/*
 * The header at the start of a music file: the file name, the length of the XM data and the format version.
 */
class MusicHeader {
public:
    char                filename[64];           // 0x00
    int                 xmLength;               // 0x40
    int                 version;                // 0x44
};

/*
 * A loaded music file: its header, the XM data and whether the loader allocated that data.
 */
class MusicData {
public:
    MusicHeader         hdr;                    // 0x00
    char*               data;                   // 0x48
    bool                allocated;              // 0x4C
};

// The music file the game has loaded (retail lbl_80529318; the symbol's second word is not read by these functions).
extern MusicData*       lbl_80529318;

int MusicFileOpen( const char* name, unsigned int* fileSize, FileHandle* handle );

// Gormiti: LoadMusicData(char*, int, bool, bool, MusicData*) (another revision: retail takes its arguments in r3, r5, r6
// and r7, and never reads the int).
int LoadMusicData( char* data, int, bool allocateNew, bool correctEndian, MusicData* musicData );

}

#endif
