/*
 * FMOD::Parameter and FMOD::ParameterRepository: a composition parameter and the registry that finds it.
 * Only what composition conditions use is declared; the slot lists are those of the retail
 * CoreParameter and CoreParameterRepository vtables (the base part of the derived tables).
 * Header named after the Gormiti unit (fmod_compositioncore.cpp); layout: Gormiti DWARF
 * (sibling.exe types Parameter, ParameterRepository).
 */
#ifndef _FMOD_COMPOSITIONCORE_H
#define _FMOD_COMPOSITIONCORE_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>

namespace FMOD {

class Scene;
class Segment;

// A list of music entities (cues or parameters). Layout: Gormiti DWARF (sibling.exe types MusicEntityListing).
class MusicEntityListing {
public:
    // Whether entity is one of this listing's.
    bool validate( const FMOD_MUSIC_ENTITY *entity ) const;
    // The first entity from start on whose name matches filter (all of them when filter is NULL).
    FMOD_MUSIC_ENTITY *find( const FMOD_MUSIC_ENTITY *start, const char *filter ) const;

    int m_count;                                    // 0x00
    FMOD_MUSIC_ENTITY *m_listing;                   // 0x04
};

class Parameter {
public:
    // vtable pointer at 0x00: the first virtual below
    virtual ~Parameter();                           // slot 0x08
    virtual void setValue( float value );           // slot 0x0C
    virtual float getValue() const;                 // slot 0x10

    unsigned int m_id;                              // 0x04
};

class ParameterRepository {
public:
    // The repository of the running Event system.
    static ParameterRepository *repository();

    // vtable pointer at 0x00: the first virtual below
    virtual ~ParameterRepository();                 // slot 0x08
    virtual Parameter *getParameter( unsigned int id ) const;   // slot 0x0C
    virtual MusicEntityListing getMusicEntityListing() const;   // slot 0x10
};

// The repository of the music cues. Only the slots the music system calls are declared; the slot list is
// that of the retail CoreCueRepository vtable (the base part of the derived table).
class CueRepository {
public:
    static CueRepository *repository();

    // vtable pointer at 0x00: the first virtual below
    virtual ~CueRepository();                       // slot 0x08
    virtual MusicEntityListing getMusicEntityListing() const;   // slot 0x0C
};

// The repository of the music scenes; slot list as for CueRepository (retail CoreSceneRepository vtable).
class SceneRepository {
public:
    static SceneRepository *repository();

    // vtable pointer at 0x00: the first virtual below
    virtual ~SceneRepository();                     // slot 0x08
    virtual Scene *getScene( unsigned int id ) const;           // slot 0x0C
};

// The repository of the music segments; slot list of the retail CoreSegmentRepository vtable (the base part
// of the derived table).
class SegmentRepository {
public:
    static SegmentRepository *repository();

    // vtable pointer at 0x00: the first virtual below
    virtual ~SegmentRepository();                   // slot 0x08
    virtual Segment *getSegment( unsigned int id ) const;       // slot 0x0C
    virtual FMOD_RESULT loadSoundData( FMOD_EVENT_RESOURCE resource, unsigned int mode );   // slot 0x10
    virtual FMOD_RESULT freeSoundData( bool waituntilready );   // slot 0x14
};

// The core music repositories (fmod_compositioncore.cpp, asm-only): only the static each one releases its
// contents with is declared, which EventProjectI::release calls. Names from the retail symbols.
class CoreLinkRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreCueRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreParameterRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreSceneRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreThemeRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreTimelineRepository {
public:
    static FMOD_RESULT releaseRepository();
};

class CoreSegmentRepository {
public:
    static FMOD_RESULT releaseRepository();
};

} // namespace FMOD

#endif /* _FMOD_COMPOSITIONCORE_H */
