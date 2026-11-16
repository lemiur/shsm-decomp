#ifndef CLIMAX_AUDIO_SOURCE_H
#define CLIMAX_AUDIO_SOURCE_H

// Names and layout: Gormiti link map and DWARF (Source.obj, game/System/Audio/FMOD/Source.h)

#include <revolution/types.h>
#include <climax/Audio/Defs.h>

namespace Audio {

/*
 * A playable sound: the interface the audio manager and the event sources share. The vtable is
 * retail's: slot 0x38 is the inline IsAsync, slot 0x44 is null (a pure slot Source does not define).
 */
class ISource {
public:
                        ISource() {}
    virtual             ~ISource() {}

    virtual int         Play( Channel::Type channelType, bool startPaused ) = 0;
    virtual int         Stop( bool ) = 0;
    virtual int         Pause( bool pause ) = 0;
    virtual bool        IsPlaying() const = 0;
    virtual bool        IsPaused() const = 0;
    virtual void        SetPosition( float* pos ) = 0;
    virtual void        SetVelocity( float* vel ) = 0;
    virtual void        GetPosition( float* pos ) = 0;
    virtual void        GetVelocity( float* vel ) = 0;
    virtual int         GetStats( StatsType type, Stats* stats ) = 0;
    virtual SoundType   GetSoundType() = 0;
    virtual bool        IsAsync() { return false; }
    virtual ISource*    Clone( bool ) = 0;
};

/*
 * A sound played through one FMOD channel: the FMOD sound it was loaded or streamed into and the
 * channel it plays on.
 */
class Source : public ISource {
public:
                        Source();
    virtual             ~Source();

    int                 LoadStream( const char* filename, int, SourceParams* );

    virtual int         Play( Channel::Type channelType, bool startPaused );
    virtual int         Stop( bool );
    virtual int         Pause( bool pause );
    virtual bool        IsPlaying() const;
    virtual bool        IsPaused() const;
    virtual void        SetPosition( float* pos );
    virtual void        SetVelocity( float* vel );
    virtual void        GetPosition( float* pos );
    virtual void        GetVelocity( float* vel );
    virtual int         GetStats( StatsType type, Stats* stats );
    virtual SoundType   GetSoundType();
    virtual ISource*    Clone( bool );
    virtual bool        SetVolume( float volume );

private:
    FMOD::Sound*        m_fmodSound;    // 0x04
    FMOD::Channel*      m_fmodChannel;  // 0x08
    short               m_priority;     // 0x0C
};

}

#endif
