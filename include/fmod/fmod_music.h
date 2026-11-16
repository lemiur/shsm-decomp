/*
 * FMOD::MusicSong and the tracker-module state it plays (fmod_music.cpp): the song, its channels, the
 * voices ("virtual channels") they spawn, the instruments and their samples, and ChannelMusic, the
 * ChannelReal that stands for the whole song in a ChannelGroup.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_music.obj; sibling.exe types MusicSong,
 * MusicChannel, MusicVirtualChannel, MusicInstrument, MusicSample, MusicPattern, MusicNote,
 * MusicEnvelopeState, _SNDMIXPLUGIN, _SNDMIXPLUGININFO, ChannelMusic). ChannelMusic's vtable
 * (retail lbl_8044C9D0) has the 44 slots of ChannelReal; the six it overrides are declared below.
 */
#ifndef _FMOD_MUSIC_H
#define _FMOD_MUSIC_H

#include <fmod/fmod.h>
#include <fmod/fmod_channel_real.h>
#include <fmod/fmod_channelgroupi.h>
#include <fmod/fmod_channeli.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class ChannelPool;
class ChannelSoftware;
class DSPI;
class MusicChannel;
class MusicInstrument;
class MusicSong;
class MusicVirtualChannel;
class SoundI;

// One cell of a pattern.
class MusicNote {
public:
    unsigned char mNote;                            // 0x00
    unsigned char mNumber;                          // 0x01
    unsigned char mVolume;                          // 0x02
    unsigned char mEffect;                          // 0x03
    unsigned char mEffectParam;                     // 0x04
}; // size 0x5

// A pattern: its rows (mRows of the song's channel count of notes each).
class MusicPattern {
public:
    int        mRows;                               // 0x00
    MusicNote *mData;                               // 0x04
}; // size 0x8

// One sample of an instrument. The inline constructor gives the default sample every instrument starts
// with (the tracker default middle C, 8363 Hz, and full volume).
class MusicSample {
public:
    SoundI           *mSound;                       // 0x00
    unsigned int      mMiddleC;                     // 0x04
    unsigned char     mDefaultVolume;               // 0x08
    unsigned char     mDefaultPan;                  // 0x09
    unsigned int      mLoopStart;                   // 0x0C
    unsigned int      mLoopLength;                  // 0x10
    int               mRelative;                    // 0x14
    int               mFineTune;                    // 0x18
    unsigned int      mRawLength;                   // 0x1C
    unsigned char     mFlags;                       // 0x20
    unsigned char     mGlobalVolume;                // 0x21
    unsigned int      mSusLoopBegin;                // 0x24
    unsigned int      mSusLoopEnd;                  // 0x28
    unsigned char     mVibSpeed;                    // 0x2C
    unsigned char     mVibDepth;                    // 0x2D
    unsigned char     mVibType;                     // 0x2E
    unsigned char     mVibRate;                     // 0x2F
    FMOD_SOUND_FORMAT mOriginalFormat;              // 0x30

    MusicSample() {
        mSound = 0;
        mMiddleC = 8363;
        mDefaultVolume = 255;
        mLoopStart = 0;
        mLoopLength = 0;
        mRelative = 0;
    }
}; // size 0x34

// An instrument: its samples, the note-to-sample map and the volume, pan and pitch envelopes.
class MusicInstrument {
public:
    signed char    mName[28];                       // 0x00
    int            mNumSamples;                     // 0x1C
    MusicSample    mSample[16];                     // 0x20
    unsigned char  mKeyMap[96];                     // 0x360
    unsigned char  mVolumeType;                     // 0x3C0
    unsigned char  mVolumeNumPoints;                // 0x3C1
    unsigned short mVolumePoints[40];               // 0x3C2
    unsigned char  mVolumeSustain;                  // 0x412
    unsigned char  mVolumeLoopStart;                // 0x413
    unsigned char  mVolumeLoopEnd;                  // 0x414
    unsigned char  mVolumeSustainLoopStart;         // 0x415
    unsigned char  mVolumeSustainLoopEnd;           // 0x416
    unsigned char  mPanType;                        // 0x417
    unsigned char  mPanNumPoints;                   // 0x418
    unsigned short mPanPoints[40];                  // 0x41A
    unsigned char  mPanSustain;                     // 0x46A
    unsigned char  mPanLoopStart;                   // 0x46B
    unsigned char  mPanLoopEnd;                     // 0x46C
    unsigned char  mPanSustainLoopStart;            // 0x46D
    unsigned char  mPanSustainLoopEnd;              // 0x46E
    unsigned char  mPitchType;                      // 0x46F
    unsigned char  mPitchNumpoints;                 // 0x470
    unsigned short mPitchPoints[40];                // 0x472
    unsigned char  mPitchSustain;                   // 0x4C2
    unsigned char  mPitchLoopStart;                 // 0x4C3
    unsigned char  mPitchLoopEnd;                   // 0x4C4
    unsigned char  mPitchSustainLoopStart;          // 0x4C5
    unsigned char  mPitchSustainLoopEnd;            // 0x4C6
    unsigned char  mVibratoType;                    // 0x4C7
    unsigned char  mVibratoSweep;                   // 0x4C8
    unsigned char  mVibratoDepth;                   // 0x4C9
    unsigned char  mVibratoRate;                    // 0x4CA
    unsigned short mVolumeFade;                     // 0x4CC
    unsigned char  mGlobalVolume;                   // 0x4CE
    unsigned char  mDefaultPan;                     // 0x4CF
    unsigned char  mNNA;                            // 0x4D0
    unsigned char  mDupCheckType;                   // 0x4D1
    unsigned char  mDupCheckAction;                 // 0x4D2
    unsigned char  mPitchPanSep;                    // 0x4D3
    unsigned char  mPitchPanCenter;                 // 0x4D4
    unsigned char  mVolumeVariation;                // 0x4D5
    unsigned char  mPanVariation;                   // 0x4D6
    unsigned char  mNoteTable[240];                 // 0x4D7
    unsigned int   mFilterCutOff;                   // 0x5C8
    unsigned int   mFilterResonance;                // 0x5CC
    unsigned char  mMIDIOutput;                     // 0x5D0
}; // size 0x5D4

// The state of one envelope of a voice.
class MusicEnvelopeState {
public:
    int  mTick;                                     // 0x00
    int  mPosition;                                 // 0x04
    int  mFraction;                                 // 0x08
    int  mValue;                                    // 0x0C
    int  mDelta;                                    // 0x10
    bool mStopped;                                  // 0x14
}; // size 0x18

// A voice of the song: a ChannelI playing one sample for a MusicChannel. The voices of a channel hang
// in a ring headed by the channel's own first voice.
class MusicVirtualChannel : public LinkedListNode {
public:
    int                mIndex;                      // 0x0C
    bool               mAllocated;                  // 0x10
    bool               mFlip;                       // 0x11
    ChannelI           mChannel;                    // 0x18
    MusicSample       *mSample;                     // 0x1C8
    MusicSong         *mSong;                       // 0x1CC
    unsigned char      mLastInstrument;             // 0x1D0
    unsigned char      mLastNote;                   // 0x1D1
    unsigned char      mLastSample;                 // 0x1D2
    bool               mBackground;                 // 0x1D3
    unsigned char      mNoteControl;                // 0x1D4
    unsigned char      mNNA;                        // 0x1D5
    unsigned char      mVolType;                    // 0x1D6
    int                mFrequency;                  // 0x1D8
    int                mVolume;                     // 0x1DC
    int                mPan;                        // 0x1E0
    int                mVolumeDelta;                // 0x1E4
    int                mFrequencyDelta;             // 0x1E8
    int                mPanDelta;                   // 0x1EC
    unsigned int       mSampleOffset;               // 0x1F0
    int                mDirection;                  // 0x1F4
    int                mSampGlobalVol;              // 0x1F8
    MusicEnvelopeState mEnvVolume;                  // 0x1FC
    MusicEnvelopeState mEnvPan;                     // 0x214
    int                mEnvPitchTick;               // 0x22C
    int                mEnvPitchPos;                // 0x230
    int                mEnvPitchFrac;               // 0x234
    int                mEnvPitch;                   // 0x238
    int                mEnvPitchDelta;              // 0x23C
    bool               mEnvPitchStopped;            // 0x240
    bool               mFade;                       // 0x241
    int                mFadeOutVolume;              // 0x244
    int                mIVibPos;                    // 0x248
    int                mIVibSweepPos;               // 0x24C
    bool               mKeyOff;                     // 0x250
    bool               mRamping;                    // 0x251
    int                mTicksToDie;                 // 0x254

    void cleanUp();                                 // no copy in retail (inlined)
}; // size 0x258

// A channel of the song. Its first member is the head of the ring of the channel's voices.
class MusicChannel {
public:
    MusicVirtualChannel mVirtualChannelHead;        // 0x00
    unsigned char       mInstrument;                // 0x258
    unsigned char       mNote;                      // 0x259
    unsigned char       mSample;                    // 0x25A
    unsigned char       mRealNote;                  // 0x25B
    int                 mPeriod;                    // 0x25C
    unsigned char       mRecentEffect;              // 0x260
    int                 mVolume;                    // 0x264
    int                 mPan;                       // 0x268
    int                 mVolumeDelta;               // 0x26C
    unsigned int        mSampleOffset;              // 0x270
    int                 mGlobalVolume;              // 0x274
    float               mMasterVolume;              // 0x278
    unsigned char       mPortaUpDown;               // 0x27C
    unsigned char       mPortaDown;                 // 0x27D
    unsigned char       mPortaUp;                   // 0x27E
    unsigned char       mXtraPortaDown;             // 0x27F
    unsigned char       mXtraPortaUp;               // 0x280
    unsigned char       mVolumeSlide;               // 0x281
    unsigned char       mPanSlide;                  // 0x282
    unsigned char       mRetrigX;                   // 0x283
    unsigned char       mRetrigY;                   // 0x284
    unsigned char       mRetrigCount;               // 0x285
    int                 mPortaTarget;               // 0x288
    unsigned char       mPortaSpeed;                // 0x28C
    unsigned char       mPortaReached;              // 0x28D
    signed char         mVibPos;                    // 0x28E
    unsigned char       mVibSpeed;                  // 0x28F
    unsigned char       mVibDepth;                  // 0x290
    unsigned char       mVibType;                   // 0x291
    signed char         mTremoloPosition;           // 0x292
    unsigned char       mTremoloSpeed;              // 0x293
    unsigned char       mTremoloDepth;              // 0x294
    int                 mPanbrelloPos;              // 0x298
    unsigned char       mPanbrelloSpeed;            // 0x29C
    unsigned char       mPanbrelloDepth;            // 0x29D
    unsigned char       mTremorPosition;            // 0x29E
    unsigned char       mTremorOn;                  // 0x29F
    unsigned char       mTremorOff;                 // 0x2A0
    unsigned char       mArpeggio;                  // 0x2A1
    int                 mPatternLoopRow;            // 0x2A4
    int                 mPatternLoopNumber;         // 0x2A8
    unsigned char       mChannelVolumeSlide;        // 0x2AC
    unsigned char       mSpecialParam;              // 0x2AD
    unsigned char       mWaveControl;               // 0x2AE
    unsigned char       mWaveControlVibrato;        // 0x2AF
    unsigned char       mWaveControlTremolo;        // 0x2B0
    unsigned char       mWaveControlPan;            // 0x2B1
    unsigned char       mFineVolumeSlideDown;       // 0x2B2
    unsigned char       mFineVolumeSlideUp;         // 0x2B3
    unsigned char       mFinePortaUp;               // 0x2B4
    unsigned char       mFinePortaDown;             // 0x2B5
    unsigned char       mHighOffset;                // 0x2B6
    unsigned char       mVolumeColumnVolumeSlide;   // 0x2B7
}; // size 0x2B8

// A mixer plug-in slot of an Impulse Tracker module.
class _SNDMIXPLUGININFO {
public:
    unsigned int dwPluginId1;                       // 0x00
    unsigned int dwPluginId2;                       // 0x04
    unsigned int dwInputRouting;                    // 0x08
    unsigned int dwOutputRouting;                   // 0x0C
    unsigned int dwReserved[4];                     // 0x10
    char         szName[32];                        // 0x20
    char         szLibraryName[64];                 // 0x40
}; // size 0x80

class _SNDMIXPLUGIN {
public:
    ChannelGroupI     mChannelGroup;                // 0x00
    void             *pMixPlugin;                   // 0x5C
    _SNDMIXPLUGININFO Info;                         // 0x60
}; // size 0xE0

// ChannelReal that stands for the whole song in its ChannelGroup: the channel calls forward to the song.
class ChannelMusic : public ChannelReal {
public:
    MusicSong *mMusic;                              // 0x64

    virtual ~ChannelMusic() {}                      // slot 0x14, override; weak, defined in the class // 0x8033E500
    virtual FMOD_RESULT start();                    // slot 0x28, override
    virtual FMOD_RESULT updateStream();             // slot 0x30, override
    virtual FMOD_RESULT stop();                     // slot 0x34, override
    virtual FMOD_RESULT setPaused( bool paused );   // slot 0x38, override
    virtual FMOD_RESULT setVolume( float volume );  // slot 0x40, override
}; // size 0x68

// A tracker-module song, the Codec of the music formats (its module codecs fill in the pattern data).
class MusicSong : public Codec {
public:
    char                 mSongName[256];            // 0xFD
    MusicPattern        *mPattern;                  // 0x200
    DSPI                *mDSPHead;                  // 0x204
    bool                *mVisited;                  // 0x208
    unsigned char        mOrderList[256];           // 0x20C
    int                  mNumChannels;              // 0x30C
    MusicChannel        *mMusicChannel[64];         // 0x310
    int                  mNumVirtualChannels;       // 0x410
    MusicVirtualChannel *mVirtualChannel;           // 0x414
    ChannelPool         *mChannelPool;              // 0x418
    ChannelSoftware     *mChannelSoftware;          // 0x41C
    DSPI               **mLowPass;                  // 0x420
    ChannelGroupI        mChannelGroup;             // 0x424
    ChannelMusic         mHardwareMusicChannel;     // 0x480
    int                  mMixerSamplesLeft;         // 0x4E8
    int                  mMixerSamplesPerTick;      // 0x4EC
    unsigned int         mPCMOffset;                // 0x4F0
    unsigned int         mDSPTick;                  // 0x4F4
    int                  mDefaultSpeed;             // 0x4F8
    unsigned int         mDefaultBPM;               // 0x4FC
    unsigned char        mDefaultPan[64];           // 0x500
    unsigned char        mDefaultVolume[64];        // 0x540
    unsigned char        mDefaultGlobalVolume;      // 0x580
    int                  mNumOrders;                // 0x584
    int                  mNumPatterns;              // 0x588
    int                  mNumPatternsMem;           // 0x58C
    int                  mNumInstruments;           // 0x590
    int                  mNumSamples;               // 0x594
    MusicInstrument     *mInstrument;               // 0x598
    signed char         *mPatternPtr;               // 0x59C
    unsigned char        mLastNote[64];             // 0x5A0
    unsigned char        mLastNumber[64];           // 0x5E0
    unsigned char        mLastVolume[64];           // 0x620
    unsigned char        mLastEffect[64];           // 0x660
    unsigned char        mLastEffectParam[64];      // 0x6A0
    unsigned char        mPreviousMaskVariable[64]; // 0x6E0
    MusicNote            mNote[64];                 // 0x720
    int                  mRestart;                  // 0x860
    float                mMasterSpeed;              // 0x864
    float                mPanSeparation;            // 0x868
    int                  mMasterVolume;             // 0x86C
    int                  mGlobalVolume;             // 0x870
    unsigned char        mGlobalVolumeSlide;        // 0x874
    unsigned short       mMusicFlags;               // 0x876
    bool                 mPlaying;                  // 0x878
    bool                 mFinished;                 // 0x879
    bool                 mLooping;                  // 0x87A
    int                  mTick;                     // 0x87C
    int                  mSpeed;                    // 0x880
    int                  mBPM;                      // 0x884
    int                  mRow;                      // 0x888
    int                  mOrder;                    // 0x88C
    int                  mPatternDelay;             // 0x890
    int                  mPatternDelayTicks;        // 0x894
    int                  mNextRow;                  // 0x898
    int                  mNextOrder;                // 0x89C

    FMOD_RESULT play( bool fromopen );
    FMOD_RESULT stop();
    FMOD_RESULT setBPM( int bpm );
    FMOD_RESULT spawnNewVirtualChannel( MusicChannel *cptr, MusicSample *sptr, MusicVirtualChannel **newvcptr );
    FMOD_RESULT playSound( MusicSample *sample, MusicVirtualChannel *vcptr, bool addfilter, _SNDMIXPLUGIN *plugin );

    // Bodies of the Codec description callbacks below (names from the Gormiti link map, where each is a
    // dead-stripped copy; parameter names invented).
    FMOD_RESULT getLengthInternal( unsigned int *length, unsigned int lengthtype );                  // no copy in retail (inlined)
    FMOD_RESULT getPositionInternal( unsigned int *position, unsigned int postype );                 // no copy in retail (inlined)
    FMOD_RESULT getMusicNumChannelsInternal( int *numchannels );                                     // no copy in retail (inlined)
    FMOD_RESULT setMusicChannelVolumeInternal( int channel, float volume );                          // no copy in retail (inlined)
    FMOD_RESULT getMusicChannelVolumeInternal( int channel, float *volume );                         // no copy in retail (inlined)
    FMOD_RESULT getHardwareMusicChannel( ChannelReal **realchannel );                                // no copy in retail (inlined)

    // The Codec description callbacks of the music codecs (parameter names invented).
    static FMOD_RESULT F_CALLBACK getLengthCallback( FMOD_CODEC_STATE *codec_state, unsigned int *length, unsigned int lengthtype );
    static FMOD_RESULT F_CALLBACK getPositionCallback( FMOD_CODEC_STATE *codec_state, unsigned int *position, unsigned int postype );
    static FMOD_RESULT F_CALLBACK getMusicNumChannelsCallback( FMOD_CODEC_STATE *codec_state, int *numchannels );
    static FMOD_RESULT F_CALLBACK setMusicChannelVolumeCallback( FMOD_CODEC_STATE *codec_state, int channel, float volume );
    static FMOD_RESULT F_CALLBACK getMusicChannelVolumeCallback( FMOD_CODEC_STATE *codec_state, int channel, float *volume );
    static FMOD_RESULT F_CALLBACK getHardwareMusicChannelCallback( FMOD_CODEC_STATE *codec_state, ChannelReal **realchannel );
}; // size 0x8A0

// Placeholders of the music codecs (fmod_music.cpp).
extern MusicSample gDummySample;
extern MusicVirtualChannel gDummyVirtualChannel;
extern MusicChannel gDummyChannel;
extern MusicInstrument gDummyInstrument;

} // namespace FMOD

#endif /* _FMOD_MUSIC_H */
