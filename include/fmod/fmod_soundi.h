/*
 * FMOD::SoundI and FMOD::Stream: the internal sound object and its streaming form (fmod_soundi.cpp,
 * fmod_async.cpp). Only what the channels use is named among the data members. The vtable pointer
 * sits at 0x130 after the data members.
 * Layout: Gormiti DWARF (sibling.exe types SoundI, Stream). Slot order: retail vtable lbl_8044D6E0.
 */
#ifndef _FMOD_SOUNDI_H
#define _FMOD_SOUNDI_H

#include "fmod.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class ChannelStream;
class Codec;
class Sound;
class SoundGroupI;
class System;

class AsyncData;
class SystemI;

// SoundI::mFlags bits (names invented except SOUNDI_FLAG_ASYNCFINISHING; PRELOADED and PROGRAMMERSOUND
// are named for the errors that release returns when they are set).
#define SOUNDI_FLAG_ASYNCFINISHING  0x00000001 // the async thread is finishing the sound's open
#define SOUNDI_FLAG_EOF             0x00000008
#define SOUNDI_FLAG_0x10            0x00000010
#define SOUNDI_FLAG_0x40            0x00000040
#define SOUNDI_FLAG_PRELOADED       0x00000100 // release returns FMOD_ERR_PRELOADED
#define SOUNDI_FLAG_PROGRAMMERSOUND 0x00000200 // release returns FMOD_ERR_PROGRAMMERSOUND

// SoundI::mDefaultChannelMask bits set from FMOD_SPEAKERMAPTYPE (names invented).
#define SOUNDI_CHANNELMASK_ALLMONO   0x10000000
#define SOUNDI_CHANNELMASK_ALLSTEREO 0x20000000

struct SoundSentenceEntry {
    int          mIndex;        // 0x00
    unsigned int mLength;       // 0x04
};

class SoundI : public LinkedListNode {
public:
    FMOD_SOUND_TYPE            mType;                       // 0x0C
    FMOD_SOUND_FORMAT          mFormat;                     // 0x10
    FMOD_MODE                  mMode;                       // 0x14
    char                      *mName;                       // 0x18
    unsigned int               mPosition;                   // 0x1C
    unsigned int               mLength;                     // 0x20
    unsigned int               mLengthBytes;                // 0x24
    unsigned int               mLoopStart;                  // 0x28
    unsigned int               mLoopLength;                 // 0x2C
    int                        mLoopCount;                  // 0x30
    unsigned int               mFlags;                      // 0x34, SOUNDI_FLAG_ bits
    Codec                     *mCodec;                      // 0x38
    int                        mChannels;                   // 0x3C
    float                      mDefaultVolume;              // 0x40
    float                      mDefaultFrequency;           // 0x44
    float                      mDefaultPan;                 // 0x48
    int                        mDefaultPriority;            // 0x4C
    unsigned int               mDefaultChannelMask;         // 0x50
    unsigned char              field_0x54[0x7C - 0x54];
    SoundI                   **mSubSound;                   // 0x7C
    SoundI                    *mSubSoundShared;             // 0x80
    int                        mNumSubSounds;               // 0x84
    int                        mNumActiveSubSounds;         // 0x88
    SoundI                    *mSubSoundParent;             // 0x8C
    int                        mSubSoundIndex;              // 0x90
    SoundSentenceEntry        *mSubSoundList;               // 0x94
    int                        mSubSoundListNum;            // 0x98
    SoundI                    *mSubSampleParent;            // 0x9C
    int                        mNumSubSamples;              // 0xA0
    SoundI                    *mSubSample[16];              // 0xA4
    bool                       mLockCanRead;                // 0xE4
    unsigned char              field_0xE5[0xE8 - 0xE5];
    void                      *mUserData;                   // 0xE8
    void                      *mUserDataInternal;           // 0xEC
    SystemI                   *mSystem;                     // 0xF0
    unsigned char              field_0xF4[0x108 - 0xF4];
    AsyncData                 *mAsyncData;                  // 0x108, the parameters of a non-blocking open
    FMOD_OPENSTATE             mOpenState;                  // 0x10C
    unsigned char              field_0x110[0x120 - 0x110];
    int                        mNumAudible;                 // 0x120, number of channels of this sound playing
    FMOD_SOUND_PCMREADCALLBACK   mPostReadCallback;         // 0x124
    FMOD_SOUND_PCMSETPOSCALLBACK mPostSetPositionCallback;  // 0x128
    FMOD_SOUND                *mPostCallbackSound;          // 0x12C

    SoundI();

    static FMOD_RESULT validate( Sound *sound, SoundI **soundi );

    // The 49 slots of the vtable, in retail order.
    virtual bool        isStream();     // false unless the sound is a stream
    virtual FMOD_RESULT setPosition( unsigned int position );
    virtual FMOD_RESULT getPosition( unsigned int *position );
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT getSystemObject( System **system );
    virtual FMOD_RESULT lock( unsigned int offset, unsigned int length, void **ptr1, void **ptr2,
                              unsigned int *len1, unsigned int *len2 );
    virtual FMOD_RESULT unlock( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 );
    virtual FMOD_RESULT setDefaults( float frequency, float volume, float pan, int priority );
    virtual FMOD_RESULT getDefaults( float *frequency, float *volume, float *pan, int *priority );
    virtual FMOD_RESULT setVariations( float frequencyvar, float volumevar, float panvar );
    virtual FMOD_RESULT getVariations( float *frequencyvar, float *volumevar, float *panvar );
    virtual FMOD_RESULT set3DMinMaxDistance( float min, float max );
    virtual FMOD_RESULT get3DMinMaxDistance( float *min, float *max );
    virtual FMOD_RESULT set3DConeSettings( float insideconeangle, float outsideconeangle,
                                           float outsidevolume );
    virtual FMOD_RESULT get3DConeSettings( float *insideconeangle, float *outsideconeangle,
                                           float *outsidevolume );
    virtual FMOD_RESULT set3DCustomRolloff( FMOD_VECTOR *points, int numpoints );
    virtual FMOD_RESULT get3DCustomRolloff( FMOD_VECTOR **points, int *numpoints );
    virtual FMOD_RESULT setSubSound( int index, SoundI *subsound );
    virtual FMOD_RESULT getSubSound( int index, SoundI **subsound );
    virtual FMOD_RESULT setSubSoundSentence( int *subsoundlist, int numsubsounds );
    virtual FMOD_RESULT getName( char *name, int namelen );
    virtual FMOD_RESULT getLength( unsigned int *length, unsigned int lengthtype );
    virtual FMOD_RESULT getFormat( FMOD_SOUND_TYPE *type, FMOD_SOUND_FORMAT *format, int *channels,
                                   int *bits );
    virtual FMOD_RESULT getNumSubSounds( int *numsubsounds );
    virtual FMOD_RESULT getNumTags( int *numtags, int *numtagsupdated );
    virtual FMOD_RESULT getTag( const char *name, int index, FMOD_TAG *tag );
    virtual FMOD_RESULT getOpenState( FMOD_OPENSTATE *openstate, unsigned int *percentbuffered,
                                      bool *starving );
    virtual FMOD_RESULT readData( void *buffer, unsigned int lenbytes, unsigned int *read );
    virtual FMOD_RESULT seekData( unsigned int pcm );
    virtual FMOD_RESULT setSoundGroup( SoundGroupI *soundgroup );
    virtual FMOD_RESULT getSoundGroup( SoundGroupI **soundgroup );
    virtual FMOD_RESULT getNumSyncPoints( int *numsyncpoints );
    virtual FMOD_RESULT getSyncPoint( int index, FMOD_SYNCPOINT **point );
    virtual FMOD_RESULT getSyncPointInfo( FMOD_SYNCPOINT *point, char *name, int namelen,
                                          unsigned int *offset, unsigned int offsettype );
    virtual FMOD_RESULT addSyncPoint( unsigned int offset, unsigned int offsettype, const char *name,
                                      FMOD_SYNCPOINT **point, int index, bool createpoint );
    virtual FMOD_RESULT addSyncPointInternal( unsigned int offset, unsigned int offsettype,
                                              const char *name, FMOD_SYNCPOINT **point, int index,
                                              bool createpoint );
    virtual FMOD_RESULT deleteSyncPoint( FMOD_SYNCPOINT *point );
    virtual FMOD_RESULT setMode( unsigned int mode );
    virtual FMOD_RESULT getMode( unsigned int *mode );
    virtual FMOD_RESULT setLoopCount( int loopcount );
    virtual FMOD_RESULT getLoopCount( int *loopcount );
    virtual FMOD_RESULT setLoopPoints( unsigned int loopstart, unsigned int loopstarttype,
                                       unsigned int loopend, unsigned int loopendtype );
    virtual FMOD_RESULT getLoopPoints( unsigned int *loopstart, unsigned int loopstarttype,
                                       unsigned int *loopend, unsigned int loopendtype );
    virtual FMOD_RESULT getMusicNumChannels( int *numchannels );
    virtual FMOD_RESULT setMusicChannelVolume( int channel, float volume );
    virtual FMOD_RESULT getMusicChannelVolume( int channel, float *volume );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits,
                                       unsigned int *memoryused, unsigned int *memoryused_details );

    // Name from Gormiti's DWARF (SoundI::updateSubSound, fmod_soundi.cpp; another revision there).
    FMOD_RESULT read( unsigned int position, unsigned int length, unsigned int *read_out );
    FMOD_RESULT seek( int subsound, unsigned int position );
    FMOD_RESULT clear( unsigned int position, unsigned int length );
    FMOD_RESULT updateSubSound( int index, bool updateinfoonly );

    static FMOD_RESULT getBitsFromFormat( FMOD_SOUND_FORMAT format, int *bits ) {
        switch ( format ) {
            case FMOD_SOUND_FORMAT_PCM8:     *bits = 8;  break;
            case FMOD_SOUND_FORMAT_PCM16:    *bits = 16; break;
            case FMOD_SOUND_FORMAT_PCM24:    *bits = 24; break;
            case FMOD_SOUND_FORMAT_PCM32:    *bits = 32; break;
            case FMOD_SOUND_FORMAT_PCMFLOAT: *bits = 32; break;
            case FMOD_SOUND_FORMAT_GCADPCM:  *bits = 0;  break;
            case FMOD_SOUND_FORMAT_IMAADPCM: *bits = 0;  break;
            case FMOD_SOUND_FORMAT_XMA:      *bits = 0;  break;
            case FMOD_SOUND_FORMAT_VAG:      *bits = 0;  break;
            case FMOD_SOUND_FORMAT_MPEG:     *bits = 0;  break;
            case FMOD_SOUND_FORMAT_NONE:     *bits = 0;  break;
            default:                         return FMOD_ERR_FORMAT;
        }
        return FMOD_OK;
    }

    // Byte length of a sample count. Compressed formats count whole frames: rounded up to the frame that
    // holds the last sample (the default, a lock's or a buffer's view), or down to the last whole frame
    // (a stream's read). XMA and MPEG have no frame size here and report the sample count unchanged.
    // The roundup parameter's name is invented.
    FMOD_RESULT getBytesFromSamples( unsigned int samples, unsigned int *bytes, bool roundup = true ) {
        return getBytesFromSamples( samples, bytes, mChannels, mFormat, roundup );
    }

    static FMOD_RESULT getBytesFromSamples( unsigned int samples, unsigned int *bytes, int channels,
                                            FMOD_SOUND_FORMAT format, bool roundup = true ) {
        int bits = 0;

        getBitsFromFormat( format, &bits );

        if ( bits ) {
            *bytes = (unsigned int)( ( (long long)bits * (unsigned long long)samples ) >> 3 );
        } else {
            switch ( format ) {
                case FMOD_SOUND_FORMAT_GCADPCM: {
                    if ( roundup ) {
                        *bytes = ( ( samples + 13 ) / 14 ) * 8;
                    } else {
                        *bytes = ( samples / 14 ) * 8;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_IMAADPCM: {
                    if ( roundup ) {
                        *bytes = ( ( samples + 63 ) / 64 ) * 36;
                    } else {
                        *bytes = ( samples / 64 ) * 36;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_VAG: {
                    if ( roundup ) {
                        *bytes = ( ( samples + 27 ) / 28 ) * 16;
                    } else {
                        *bytes = ( samples / 28 ) * 16;
                    }
                    break;
                }
                case FMOD_SOUND_FORMAT_XMA: {
                    *bytes = samples;
                    return FMOD_OK;
                }
                case FMOD_SOUND_FORMAT_MPEG: {
                    *bytes = samples;
                    return FMOD_OK;
                }
                case FMOD_SOUND_FORMAT_NONE: {
                    *bytes = 0;
                    break;
                }
                default: {
                    return FMOD_ERR_FORMAT;
                }
            }
        }

        *bytes *= channels;

        return FMOD_OK;
    }

    FMOD_RESULT getSamplesFromBytes( unsigned int bytes, unsigned int *samples ) {
        return getSamplesFromBytes( bytes, samples, mChannels, mFormat );
    }

    static FMOD_RESULT getSamplesFromBytes( unsigned int bytes, unsigned int *samples, int channels,
                                            FMOD_SOUND_FORMAT format ) {
        int bits = 0;

        if ( channels == 0 ) {
            return FMOD_ERR_INVALID_PARAM;
        }

        getBitsFromFormat( format, &bits );

        if ( bits ) {
            *samples = (unsigned int)( ( (unsigned long long)bytes * 8 ) / bits );
        } else {
            switch ( format ) {
                case FMOD_SOUND_FORMAT_GCADPCM: {
                    *samples = ( bytes * 14 ) >> 3;
                    break;
                }
                case FMOD_SOUND_FORMAT_IMAADPCM: {
                    *samples = ( bytes * 64 ) / 36;
                    break;
                }
                case FMOD_SOUND_FORMAT_VAG: {
                    *samples = ( bytes * 28 ) >> 4;
                    break;
                }
                case FMOD_SOUND_FORMAT_XMA: {
                    *samples = bytes;
                    return FMOD_OK;
                }
                case FMOD_SOUND_FORMAT_MPEG: {
                    *samples = bytes;
                    return FMOD_OK;
                }
                case FMOD_SOUND_FORMAT_NONE: {
                    *samples = 0;
                    break;
                }
                default: {
                    return FMOD_ERR_FORMAT;
                }
            }
        }

        *samples /= channels;

        return FMOD_OK;
    }
};

class Sample : public SoundI {
public:
    void         *mLockBuffer;      // 0x134
    unsigned int  mLockLength;      // 0x138
    unsigned int  mLockOffset;      // 0x13C

    Sample();

    // Overrides of SoundI and own slots (retail vtable lbl_8044D138).
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT lock( unsigned int offset, unsigned int length, void **ptr1, void **ptr2,
                              unsigned int *len1, unsigned int *len2 );
    virtual FMOD_RESULT unlock( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 );
    virtual FMOD_RESULT setDefaults( float frequency, float volume, float pan, int priority );
    virtual FMOD_RESULT setVariations( float frequencyvar, float volumevar, float panvar );
    virtual FMOD_RESULT set3DMinMaxDistance( float min, float max );
    virtual FMOD_RESULT set3DConeSettings( float insideconeangle, float outsideconeangle,
                                           float outsidevolume );
    virtual FMOD_RESULT setMode( unsigned int mode );
    virtual FMOD_RESULT setLoopCount( int loopcount );
    virtual FMOD_RESULT setLoopPoints( unsigned int loopstart, unsigned int loopstarttype,
                                       unsigned int loopend, unsigned int loopendtype );

    virtual FMOD_RESULT lockInternal( unsigned int offset, unsigned int length, void **ptr1,
                                      void **ptr2, unsigned int *len1, unsigned int *len2 ) {
        return FMOD_ERR_SUBSOUNDS;
    }
    virtual FMOD_RESULT unlockInternal( void *ptr1, void *ptr2, unsigned int len1, unsigned int len2 ) {
        return FMOD_OK;
    }
    virtual FMOD_RESULT setBufferData( void *data ) {
        return FMOD_ERR_NEEDSSOFTWARE;
    }
};

class Stream : public SoundI {
public:
    LinkedListNode  mStreamNode;        // 0x134
    ChannelStream  *mChannel;           // 0x140
    Sample         *mSample;            // 0x144
    unsigned int    mLastPos;           // 0x148
    int             mBlockSize;         // 0x14C
    int             mLoopCountCurrent;  // 0x150
    int             mInitialPosition;   // 0x154

    Stream();

    // Overrides of SoundI (retail vtable lbl_8044D23C).
    virtual bool        isStream();
    virtual FMOD_RESULT setLoopCount( int loopcount );

    FMOD_RESULT fill( unsigned int offset, unsigned int length, unsigned int *read_out,
                      bool calledfromsentence );
    FMOD_RESULT flush();
    FMOD_RESULT setPosition( unsigned int position, FMOD_TIMEUNIT postype );
    FMOD_RESULT getPosition( unsigned int *position, FMOD_TIMEUNIT postype );
};

} // namespace FMOD

#endif /* _FMOD_SOUNDI_H */
