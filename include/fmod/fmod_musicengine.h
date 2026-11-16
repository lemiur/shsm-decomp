/*
 * FMOD::MusicEngine: the Event system's music player, and the music state it keeps. Header named after
 * the Gormiti unit that defines the class (fmod_musicengine.cpp). Layouts: Gormiti DWARF
 * (sibling.exe types MusicEngine, MusicStatePrimary, ThemeStack, CueStateTable, MusicSync);
 * MusicObserver's slots: Gormiti __vt__Q24FMOD13MusicObserver + retail vtable of MusicEngine.
 */
#ifndef _FMOD_MUSICENGINE_H
#define _FMOD_MUSICENGINE_H

#include <fmod/fmod.h>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class System;
class ChannelGroup;
class MusicSettings;

// A theme running in a music state: the state it was started in and its stack id there.
class ThemeStackEntry {
public:
    MusicState *m_state;                            // 0x00
    unsigned int m_stack_id;                        // 0x04
}; // size 0x8

// The stack of the themes a music state is playing.
class ThemeStack {
public:
    class StackItem;

    unsigned int m_max_length;                      // 0x00
    unsigned int m_length;                          // 0x04
    unsigned int m_next_stack_id;                   // 0x08
    StackItem *m_stack;                             // 0x0C
}; // size 0x10

// The cues a music state has active.
class CueStateTable {
public:
    LinkedListNode m_cuestates;                     // 0x00
}; // size 0xC

// The state of the primary player.
class MusicStatePrimary : public MusicState {
public:
    MusicStatePrimary();
    FMOD_RESULT init( unsigned int maxthemes );

    // Slots of the retail MusicStatePrimary vtable, overriding MusicState's.
    virtual FMOD_RESULT addTheme( unsigned int theme, unsigned int *handle );
    virtual FMOD_RESULT removeTheme( unsigned int handle );
    virtual bool isCueActive( unsigned int cue ) const;
    virtual unsigned int getThemeStackTop() const;
    virtual unsigned int getSceneStackTop() const;

    ThemeStack m_themestack;                        // 0x04
    CueStateTable m_cuestates;                      // 0x14
}; // size 0x20

// The time signature and tempo of the music, and where it started.
class MusicSync {
public:
    struct TimeSignature {
        unsigned char upper;                        // 0x00
        unsigned char lower;                        // 0x01
    };

    TimeSignature m_time_signature;                 // 0x00
    float m_beatsperminute;                         // 0x04
    unsigned long long m_start_time;                // 0x08
    unsigned long long m_fadestart;                 // 0x10
    unsigned long long m_fadelength;                // 0x18
    unsigned int m_outputrate;                      // 0x20
    unsigned int m_stepsequence;                    // 0x24
}; // size 0x28

// What a segment player reports its progress to.
class MusicObserver {
public:
    // vtable pointer at 0x00: the first virtual below
    virtual FMOD_RESULT updatePosition( unsigned int, unsigned int ) = 0;   // slot 0x08
    virtual FMOD_RESULT startSegment( unsigned int ) = 0;                   // slot 0x0C
    virtual FMOD_RESULT endSegment( unsigned int ) = 0;                     // slot 0x10
}; // size 0x4

class MusicEngine : public MusicObserver {
public:
    class PlayerEntry;

    MusicEngine();

    FMOD_RESULT init( System *system, unsigned int numplayers, MusicSettings *settings );
    FMOD_RESULT release();
    FMOD_RESULT start();
    FMOD_RESULT stop();
    FMOD_RESULT reset();
    FMOD_RESULT setVolume( float volume );
    FMOD_RESULT getVolume( float *volume );
    FMOD_RESULT setPaused( bool paused );
    FMOD_RESULT getPaused( bool *paused );
    FMOD_RESULT setMute( bool mute );
    FMOD_RESULT getMute( bool *mute );
    FMOD_RESULT getInfo( FMOD_MUSIC_INFO *info );
    FMOD_RESULT setCallback( FMOD_MUSIC_CALLBACK callback, void *userdata );
    FMOD_RESULT update();
    FMOD_RESULT beginTheme( unsigned int theme, ThemeStackEntry *entry );
    FMOD_RESULT endTheme( const ThemeStackEntry &entry );
    FMOD_RESULT beginCue( unsigned int cue );
    FMOD_RESULT endCue( unsigned int cue );

    // Slots of the retail MusicEngine vtable, overriding MusicObserver's.
    virtual FMOD_RESULT updatePosition( unsigned int, unsigned int );
    virtual FMOD_RESULT startSegment( unsigned int );
    virtual FMOD_RESULT endSegment( unsigned int );

    System *m_system;                               // 0x04
    int m_numplayers;                               // 0x08
    PlayerEntry *m_players;                         // 0x0C
    PlayerEntry *m_primary_player;                  // 0x10
    PlayerEntry *m_master_player;                   // 0x14
    bool m_paused;                                  // 0x18
    bool m_playing;                                 // 0x19
    MusicStatePrimary m_primary_state;              // 0x1C
    MusicSync m_musicsync;                          // 0x40
    ChannelGroup *m_channelgroup;                   // 0x68
    unsigned long long m_timer_end;                 // 0x70
    int m_current_bar;                              // 0x78
    int m_current_beat;                             // 0x7C
    FMOD_MUSIC_CALLBACK m_callback;                 // 0x80
    void *m_callbackdata;                           // 0x84
}; // size 0x88

} // namespace FMOD

#endif /* _FMOD_MUSICENGINE_H */
