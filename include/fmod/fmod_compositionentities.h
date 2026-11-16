/*
 * FMOD::MusicState: the interface of the Event system's music state (themes, scenes and cues), which a
 * composition condition is evaluated against. Header named after the Gormiti unit that declares it
 * (fmod_compositionentities.cpp). Virtuals: slots of the retail MusicStatePrimary vtable
 * (the base part of the derived table); layout: Gormiti DWARF (sibling.exe types MusicState).
 */
#ifndef _FMOD_COMPOSITIONENTITIES_H
#define _FMOD_COMPOSITIONENTITIES_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

// Sound definition enums, outside the namespace as the mangled setters show (F10RandMethod, F15RecalculateMode).
// When a sound definition randomises a pitch or volume (names and values invented from the two arms of
// getPitch/getVolume; Gormiti's DWARF has no entry for the type).
enum RandMethod {
    RANDMETHOD_MINMAX        = 0,   // between the random minimum and maximum
    RANDMETHOD_RANDOMIZATION = 1    // by a fraction of the base value
};

// Gormiti DWARF: enum RecalculateMode.
enum RecalculateMode {
    RECALCULATE_ALWAYS               = 0,
    RECALCULATE_ON_PARAMETER_TRIGGER = 1,
    RECALCULATE_ON_EVENT_START       = 2,
    NUM_RECALCULATEMODE              = 3
};

namespace FMOD {

class MusicState {
public:
    // vtable pointer at 0x00: the first virtual below
    virtual FMOD_RESULT addTheme( unsigned int theme, unsigned int *handle ) = 0;   // slot 0x08
    virtual FMOD_RESULT removeTheme( unsigned int handle ) = 0;                     // slot 0x0C
    virtual bool isCueActive( unsigned int cue ) const = 0;                         // slot 0x10
    virtual unsigned int getThemeStackTop() const = 0;                              // slot 0x14
    virtual unsigned int getSceneStackTop() const = 0;                              // slot 0x18
}; // size 0x4

// A music scene: the themes its cues play. Slot list of the retail CoreScene vtable (the base part of the
// derived table); layout: Gormiti DWARF (sibling.exe types Scene).
class Scene {
public:
    // vtable pointer at 0x00: the first virtual below
    virtual ~Scene();                               // slot 0x08
    virtual unsigned int getThemeForCue( unsigned int cue ) const;  // slot 0x0C

    unsigned int m_id;                              // 0x04
};

// ---- Sound definitions (added for fmod_eventimpl_simple; only the members used so far) ----
// Layout: Gormiti DWARF (sibling.exe types SoundDef, SoundDefDef, SoundDefEntry, SoundDefWaveform).

class EventProjectI;
class EventSound;
class SoundBank;

// SoundDefEntry::m_entrytype (names invented: FMOD Designer's entry kinds).
#define SOUNDDEF_ENTRYTYPE_NONE      -1
#define SOUNDDEF_ENTRYTYPE_WAVEFORM   0
#define SOUNDDEF_ENTRYTYPE_OSCILLATOR 1
#define SOUNDDEF_ENTRYTYPE_NULL       2
#define SOUNDDEF_ENTRYTYPE_PROGRAMMER 3

// SoundDefDef::getPlayMode() value (names invented: how the next entry of a sound definition is chosen).
#define SOUNDDEF_PLAYMODE_SEQUENTIAL       0
#define SOUNDDEF_PLAYMODE_RANDOM           1
#define SOUNDDEF_PLAYMODE_RANDOM_NOREPEAT  2
#define SOUNDDEF_PLAYMODE_PROGRAMMER       3
#define SOUNDDEF_PLAYMODE_SHUFFLE          4
#define SOUNDDEF_PLAYMODE_SHUFFLE_GLOBAL   6

// SoundDefDef::m_flags layout (names invented): pitch recalculation mode in bits 0-1, the pitch and volume
// randomisation methods in bits 2 and 3, the play mode in bits 4-6.
#define SOUNDDEF_FLAGS_RECALCULATE_MASK  0x03
#define SOUNDDEF_FLAGS_PITCHRAND_SHIFT   2
#define SOUNDDEF_FLAGS_VOLUMERAND_SHIFT  3
#define SOUNDDEF_FLAGS_PLAYMODE_SHIFT    4

// What every entry of a sound definition starts with. size 0x8
struct SoundDefEntry {
    int m_entrytype;    // 0x00, SOUNDDEF_ENTRYTYPE_*
    int m_weight;       // 0x04

    SoundDefEntry();    // no copy in retail (inlined)
};

// A waveform entry. size 0x18
struct SoundDefWaveform : public SoundDefEntry {
    SoundBank    *m_soundbank;      // 0x08
    unsigned int  m_subsoundindex;  // 0x0C
    unsigned int  m_lengthms;       // 0x10
    char         *m_sourcefilename; // 0x14

    SoundDefWaveform();             // 0x803B3EC0
    FMOD_RESULT release();          // no copy in retail (inlined)
};

// An oscillator entry. size 0x10
struct SoundDefOscillator : public SoundDefEntry {
    int   m_osctype;                // 0x08
    float m_frequency;              // 0x0C

    SoundDefOscillator();           // 0x803B3EE4
};

// An entry that plays nothing. size 0x8
struct SoundDefNull : public SoundDefEntry {
    SoundDefNull();                 // 0x803B3F08
};

// An entry whose sound the application supplies. size 0x8
struct SoundDefProgrammer : public SoundDefEntry {
    SoundDefProgrammer();           // 0x803B3F1C
};

// size 0x38
class SoundDefDef {
public:
    unsigned int   m_flags;                 // 0x00
    int            m_spawn_max;             // 0x04
    int            m_spawntime_min;         // 0x08
    int            m_spawntime_max;         // 0x0C
    float          m_volume;                // 0x10
    float          m_volume_random_min;     // 0x14
    float          m_volume_random_max;     // 0x18
    float          m_volume_randomization;  // 0x1C
    float          m_pitch;                 // 0x20
    float          m_pitch_random_min;      // 0x24
    float          m_pitch_random_max;      // 0x28
    float          m_pitch_randomization;   // 0x2C
    float          m_position_randomization;// 0x30
    EventProjectI *m_project;               // 0x34

    SoundDefDef();                                  // 0x803B3F30
    FMOD_RESULT init( EventProjectI *project );     // 0x803B3F64
    FMOD_RESULT release();                          // no copy in retail (dead-stripped)
    void validateSpawntimes();                      // 0x803B3FB4
    void setSpawnIntensity( float intensity );      // 0x803B3FDC
    int getPlayMode();                              // 0x803B4034
    RandMethod getVolumeRandMethod();               // no copy in retail (inlined)
    RandMethod getPitchRandMethod();                // no copy in retail (inlined)
    int getPitchRecalculateMode();                  // 0x803B4040
    void setPlayMode( int mode );                   // 0x803B404C
    void setVolumeRandMethod( RandMethod method );  // 0x803B4064
    void setPitchRandMethod( RandMethod method );   // 0x803B407C
    void setPitchRecalculateMode( RecalculateMode mode ); // 0x803B4094
};

// size 0x28
class SoundDef : public LinkedListNode {
public:
    SoundDefDef      *m_def;            // 0x0C
    char             *m_name;           // 0x10
    int               m_numentries;     // 0x14
    int               m_lastentryindex; // 0x18
    int               m_shuffleindex;   // 0x1C
    int              *m_shuffletable;   // 0x20
    SoundDefWaveform *m_entry;          // 0x24, m_numentries records

    SoundDef();                                             // 0x803B40A8
    FMOD_RESULT init( SoundDef *src );                      // no copy in retail (dead-stripped)
    FMOD_RESULT release( bool freethis );                 // 0x803B40DC
    FMOD_RESULT getEntry( EventSound *sound, int *entry );  // 0x803B420C
    void shuffle( EventSound *sound );                      // no copy in retail (inlined)
    float getVolume();                                      // 0x803B4774
    float getPitch();                                       // 0x803B48F8
    FMOD_VECTOR getOffset();                                // 0x803B4AC8
};

} // namespace FMOD

#endif /* _FMOD_COMPOSITIONENTITIES_H */
