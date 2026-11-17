/*
 * FMOD::Codec and FMOD_CODEC_DESCRIPTION_EX (fmod_codec.cpp).
 * Layout: Gormiti DWARF (sibling.exe types Codec, FMOD_CODEC_DESCRIPTION_EX).
 */
#ifndef _FMOD_CODECI_H
#define _FMOD_CODECI_H

#include "fmod.h"
#include "fmod_codec.h"
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_linkedlist.h>
#include <string.h>

namespace FMOD {

class File;
class Metadata;

// Codec::mFlags bit (name invented from use).
#define CODEC_FLAG_KNOWNLENGTH  0x00000001
// Codec::mFlags bit set when the user gave a length in FMOD_CREATESOUNDEXINFO (name invented from use).
#define CODEC_FLAG_USERLENGTH   0x00000010

class FMOD_CODEC_DESCRIPTION_EX : public FMOD_CODEC_DESCRIPTION, public SortedLinkedListNode {
public:
    FMOD_SOUND_TYPE mType;                          // 0x40
    int             mSize;                          // 0x44
    void           *mModule;                        // 0x48
    unsigned int    mHandle;                        // 0x4C
    FMOD_RESULT   (*reset)( FMOD_CODEC_STATE * );   // 0x50
    void           *canpoint;                       // 0x54
    void           *getmusicnumchannels;            // 0x58
    void           *setmusicchannelvolume;          // 0x5C
    void           *getmusicchannelvolume;          // 0x60
    void           *gethardwaremusicchannel;        // 0x64
    FMOD_RESULT   (*update)( FMOD_CODEC_STATE * );  // 0x68
    void           *getmemoryused;                  // 0x6C
};

class Codec : public Plugin, public FMOD_CODEC_STATE {
public:
    FMOD_CODEC_WAVEFORMAT     *mWaveFormatMemory;       // 0x3C
    FMOD_SOUND_TYPE            mType;                   // 0x40
    FMOD_CODEC_DESCRIPTION_EX  mDescription;            // 0x44
    unsigned int               mSrcDataOffset;          // 0xB4
    unsigned int               mLoopPoints[2];          // 0xB8
    int                        mSubSoundIndex;          // 0xC0
    unsigned int               mFlags;                  // 0xC4
    unsigned int               mBlockAlign;             // 0xC8
    unsigned char             *mReadBuffer;             // 0xCC
    unsigned int               mReadBufferLength;       // 0xD0
    unsigned char             *mPCMBuffer;              // 0xD4
    unsigned char             *mPCMBufferMemory;        // 0xD8
    unsigned int               mPCMBufferLength;        // 0xDC
    unsigned int               mPCMBufferLengthBytes;   // 0xE0
    unsigned int               mPCMBufferOffsetBytes;   // 0xE4
    unsigned int               mPCMBufferFilledBytes;   // 0xE8
    unsigned int               mMode;                   // 0xEC
    unsigned int               mOriginalMode;           // 0xF0
    Metadata                  *mMetadata;               // 0xF4
    File                      *mFile;                   // 0xF8
    bool                       mNonInterleaved;         // 0xFC

    Codec() {
        mType     = FMOD_SOUND_TYPE_UNKNOWN;
        mMetadata = NULL;
        fileread  = defaultFileRead;
        fileseek  = defaultFileSeek;
        metadata  = defaultMetaData;
    }

    FMOD_RESULT reset() {
        mPCMBufferOffsetBytes = 0;
        if ( mPCMBuffer ) {
            memset( mPCMBuffer, 0, mPCMBufferLengthBytes );
        }
        if ( mDescription.reset ) {
            return mDescription.reset( this );
        }
        return FMOD_OK;
    }

    virtual FMOD_RESULT release();

    FMOD_RESULT read( void *buffer, unsigned int sizebytes, unsigned int *bytesread );
    FMOD_RESULT getMetadataFromFile();
    FMOD_RESULT getLength( unsigned int *length, FMOD_TIMEUNIT lengthtype );
    FMOD_RESULT setPosition( int subsound, unsigned int position, FMOD_TIMEUNIT postype );
    FMOD_RESULT getPosition( unsigned int *position, FMOD_TIMEUNIT postype );
    FMOD_RESULT metaData( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen,
                          FMOD_TAGDATATYPE datatype, bool unique );

    // The callbacks a codec's state gets when its description does not supply them (fmod_pluginfactory.cpp).
    static FMOD_RESULT F_CALLBACK defaultFileRead( void *handle, void *buffer, unsigned int sizebytes,
                                                   unsigned int *bytesread, void *userdata );
    static FMOD_RESULT F_CALLBACK defaultFileSeek( void *handle, unsigned int pos, void *userdata );
    static FMOD_RESULT F_CALLBACK defaultMetaData( FMOD_CODEC_STATE *codec_state, FMOD_TAGTYPE tagtype, char *name,
                                                   void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype,
                                                   int unique );
    static FMOD_RESULT F_CALLBACK defaultGetWaveFormat( FMOD_CODEC_STATE *codec_state, int index,
                                                        FMOD_CODEC_WAVEFORMAT *waveformat );
};

} // namespace FMOD

#endif /* _FMOD_CODECI_H */
