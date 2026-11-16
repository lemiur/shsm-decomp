/*
 * FMOD::ChannelI, the internal channel behind an FMOD::Channel handle (fmod.cpp). Only the members
 * the real channels read are named.
 * Layout: Gormiti DWARF (sibling.exe types ChannelI), size 0x1B0.
 */
#ifndef _FMOD_CHANNELI_H
#define _FMOD_CHANNELI_H

#include "fmod.h"
#include "fmod_linkedlist.h"
#include "fmod_types.h"

// FMOD::ChannelI::mFlags bits (names invented from use in fmod_channel_emulated.cpp and fmod_channeli).
// 0x1 is the channel's own paused flag (setPaused/getPaused); 0x2 is its own mute flag (setMute).
#define CHANNELI_FLAG_0x1           0x00000001
#define CHANNELI_FLAG_0x2           0x00000002
#define CHANNELI_FLAG_PAUSED        0x00000004
#define CHANNELI_FLAG_PLAYINGPAUSED 0x00000040
// Set on the music player's voices when MusicSong::play prepares them (meaning from use).
#define CHANNELI_FLAG_0x800         0x00000800
// ChannelI::stopEx flag the music player passes when it releases a voice (meaning from use).
#define CHANNELI_STOPFLAG_0x4       0x00000004

namespace FMOD {

class Channel;
class ChannelGroupI;
class SoundI;
class SystemI;
class ChannelReal;
class DSPI;
class DSPConnectionI;

class ChannelI : public LinkedListNode {
public:
    unsigned char   field_0xC[0x1C - 0xC];
    int             mIndex;             // 0x1C
    void           *mUserData;          // 0x20
    unsigned int    mHandleOriginal;    // 0x24
    SystemI        *mSystem;            // 0x28
    int             mNumRealChannels;   // 0x2C
    ChannelReal    *mRealChannel[16];   // 0x30
    unsigned int    mHandleCurrent;     // 0x70
    unsigned int    mFlags;             // 0x74
    unsigned char   field_0x78[0x8C - 0x78];
    ChannelGroupI  *mChannelGroup;      // 0x8C
    unsigned char   field_0x90[0xA8 - 0x90];
    FMOD_UINT64P    mDSPClockDelay;     // 0xA8
    unsigned char   field_0xB0[0xC4 - 0xB0];
    float           mVolume;            // 0xC4
    float           mFrequency;         // 0xC8
    float           mPan;               // 0xCC
    float           mSpeakerFL;         // 0xD0
    float           mSpeakerFR;         // 0xD4
    float           mSpeakerC;          // 0xD8
    float           mSpeakerLFE;        // 0xDC
    float           mSpeakerBL;         // 0xE0
    float           mSpeakerBR;         // 0xE4
    float           mSpeakerSL;         // 0xE8
    float           mSpeakerSR;         // 0xEC
    float           mInputMix[16];      // 0xF0
    float          *mLevels;            // 0x130
    float           mReverbDryVolume;   // 0x134
    float           mVolume3D;          // 0x138
    float           mPitch3D;           // 0x13C
    unsigned char   field_0x140[0x164 - 0x140];
    float           mConeVolume3D;      // 0x164
    unsigned char   field_0x168[0x180 - 0x168];
    float           mDirectOcclusion;   // 0x180
    unsigned char   field_0x184[0x1B0 - 0x184];

    // Called by the public FMOD::Channel methods (fmod_channel.cpp).
    static FMOD_RESULT validate( Channel *channel, ChannelI **channeli );

    ChannelI();                         // 0x80337D14
    FMOD_RESULT init();
    // Trailing flags (names from the Gormiti DWARF): reset, startmuted.
    FMOD_RESULT play( SoundI *sound, bool paused, bool reset, bool startmuted ); // 0x80337F80
    FMOD_RESULT addDSP( DSPI *dsp, DSPConnectionI **dspconnection );
    FMOD_RESULT stop();
    FMOD_RESULT stopEx( unsigned int stopflag );
    FMOD_RESULT setPaused( bool paused );
    FMOD_RESULT getPaused( bool *paused );
    // Trailing bool flag of setVolume, setPan, setSpeakerMix and setSpeakerLevels (name invented): the retail
    // call sites pass false from Channel::setVolume and true from the other three.
    FMOD_RESULT setVolume( float volume, bool flag );
    FMOD_RESULT getVolume( float *volume );
    FMOD_RESULT setFrequency( float frequency );
    FMOD_RESULT getFrequency( float *frequency );
    FMOD_RESULT setPan( float pan, bool flag );
    FMOD_RESULT setDelay( FMOD_DELAYTYPE delaytype, unsigned int delayhi, unsigned int delaylo );
    FMOD_RESULT getDelay( FMOD_DELAYTYPE delaytype, unsigned int *delayhi, unsigned int *delaylo );
    FMOD_RESULT setSpeakerMix( float frontleft, float frontright, float center, float lfe, float backleft,
                               float backright, float sideleft, float sideright, bool flag );
    FMOD_RESULT setSpeakerLevels( FMOD_SPEAKER speaker, float *levels, int numlevels, bool flag );
    FMOD_RESULT setInputChannelMix( float *levels, int numlevels );
    FMOD_RESULT setMute( bool mute );
    FMOD_RESULT setPriority( int priority );
    FMOD_RESULT setPosition( unsigned int position, FMOD_TIMEUNIT postype );
    FMOD_RESULT getPosition( unsigned int *position, FMOD_TIMEUNIT postype );
    FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );
    FMOD_RESULT setChannelGroupInternal( ChannelGroupI *channelgroup, bool forcedspreconnect, bool resetvolume );
    FMOD_RESULT setChannelGroup( ChannelGroupI *channelgroup );
    FMOD_RESULT getDSPHead( DSPI **dsp );
    FMOD_RESULT setCallback( FMOD_RESULT ( F_CALLBACK *callback )( FMOD_CHANNEL *channel,
                                                                  FMOD_CHANNEL_CALLBACKTYPE type,
                                                                  void *data1, void *data2 ) );
    FMOD_RESULT set3DAttributes( const FMOD_VECTOR *pos, const FMOD_VECTOR *vel );
    FMOD_RESULT get3DAttributes( FMOD_VECTOR *pos, FMOD_VECTOR *vel );
    FMOD_RESULT set3DMinMaxDistance( float mindistance, float maxdistance );
    FMOD_RESULT set3DConeOrientation( FMOD_VECTOR *orientation );
    FMOD_RESULT set3DOcclusion( float directocclusion, float reverbocclusion );
    FMOD_RESULT set3DSpread( float angle );
    FMOD_RESULT set3DPanLevel( float level );
    FMOD_RESULT set3DDopplerLevel( float level );
    FMOD_RESULT isPlaying( bool *isplaying );
    FMOD_RESULT getAudibility( float *audibility );
    FMOD_RESULT getCurrentSound( SoundI **sound );
    FMOD_RESULT setMode( FMOD_MODE mode );
    FMOD_RESULT getMode( FMOD_MODE *mode );
    FMOD_RESULT setUserData( void *userdata );
    FMOD_RESULT getUserData( void **userdata );
    FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                               unsigned int *memoryused_array );
};

} // namespace FMOD

#endif /* _FMOD_CHANNELI_H */
