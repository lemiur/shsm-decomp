/*
 * fmod_sounddef.cpp: FMOD::SoundDef and SoundDefDef, the sound definitions of an event project (what a
 * sound of an event plays: a list of weighted entries, and how an entry, a volume, a pitch and an offset
 * are picked each time), and the entry classes SoundDefWaveform, SoundDefOscillator, SoundDefNull and
 * SoundDefProgrammer. Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD
 * rulings (fmod-1)"): the #line directives restore them.
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_sounddef.obj)
// Names and layout: Gormiti link map and DWARF (fmod_event.a fmod_sounddef.obj)

#include <math.h>
#include <stdlib.h>

#include <fmod/fmod.h>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_eventprojecti.h>
#include <fmod/fmod_eventsound.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_simplemempool.h>

namespace FMOD {

/*
====================
SoundDefEntry::SoundDefEntry

Inlined into the constructors of the entry kinds. Retail's constructors share the -1 with the oscillator's
m_osctype, so the kind starts as "none" and each constructor sets its own.
====================
*/
SoundDefEntry::SoundDefEntry() {
    m_entrytype = SOUNDDEF_ENTRYTYPE_NONE;
    m_weight    = 100;
} // no copy in retail (inlined)

/*
====================
SoundDefWaveform::SoundDefWaveform
====================
*/
SoundDefWaveform::SoundDefWaveform() {
    m_entrytype      = SOUNDDEF_ENTRYTYPE_WAVEFORM;
    m_soundbank      = NULL;
    m_subsoundindex  = 0;
    m_lengthms       = 0;
    m_sourcefilename = NULL;
} // 0x803B3EC0

/*
====================
SoundDefWaveform::release

Frees the source file name. Inlined into SoundDef::release.
====================
*/
FMOD_RESULT SoundDefWaveform::release() {
    if ( m_sourcefilename ) {
#line 75
        FMOD_Memory_Free( m_sourcefilename );
    }

    return FMOD_OK;
} // no copy in retail (inlined)

/*
====================
SoundDefOscillator::SoundDefOscillator
====================
*/
SoundDefOscillator::SoundDefOscillator() {
    m_entrytype = SOUNDDEF_ENTRYTYPE_OSCILLATOR;
    m_osctype   = -1;
    m_frequency = 0.0f;
} // 0x803B3EE4

/*
====================
SoundDefNull::SoundDefNull
====================
*/
SoundDefNull::SoundDefNull() {
    m_entrytype = SOUNDDEF_ENTRYTYPE_NULL;
} // 0x803B3F08

/*
====================
SoundDefProgrammer::SoundDefProgrammer
====================
*/
SoundDefProgrammer::SoundDefProgrammer() {
    m_entrytype = SOUNDDEF_ENTRYTYPE_PROGRAMMER;
} // 0x803B3F1C

/*
====================
SoundDefDef::SoundDefDef
====================
*/
SoundDefDef::SoundDefDef() {
    init( NULL );
} // 0x803B3F30

/*
====================
SoundDefDef::init

Sets the defaults: no spawning, full volume, no pitch shift, the programmer's play mode and the
randomised methods for the pitch and the volume.
====================
*/
FMOD_RESULT SoundDefDef::init( EventProjectI *project ) {
    m_spawntime_min          = 0;
    m_spawntime_max          = 0;
    m_spawn_max              = 0;
    m_volume                 = 1.0f;
    m_volume_random_min      = 0.0f;
    m_volume_random_max      = 0.0f;
    m_volume_randomization   = 0.0f;
    m_pitch                  = 0.0f;
    m_pitch_random_min       = 0.0f;
    m_pitch_random_max       = 0.0f;
    m_pitch_randomization    = 0.0f;
    m_flags                  = 0;
    setPlayMode( SOUNDDEF_PLAYMODE_PROGRAMMER );
    setVolumeRandMethod( RANDMETHOD_RANDOMIZATION );
    setPitchRandMethod( RANDMETHOD_RANDOMIZATION );
    setPitchRecalculateMode( RECALCULATE_ALWAYS );
    m_position_randomization = 0.0f;
    m_project                = project;

    return FMOD_OK;
} // 0x803B3F64

/*
====================
SoundDefDef::release

Frees the definition. Gormiti's map has an out-of-line copy of 0x40 bytes that retail dropped; only its
allocation tag string is needed, as the first of the file's strings it owns in .data (the layout puts
three before SoundDef::release's). The body is an unconfirmed reconstruction from that size: one call
to the allocator's free.
====================
*/
FMOD_RESULT SoundDefDef::release() {
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // no copy in retail (dead-stripped)

/*
====================
SoundDefDef::validateSpawntimes

Keeps the spawn times non-negative and the minimum no later than the maximum.
====================
*/
void SoundDefDef::validateSpawntimes() {
    m_spawntime_max = m_spawntime_max < 0 ? 0 : m_spawntime_max;
    m_spawntime_min = m_spawntime_min > m_spawntime_max ? m_spawntime_max : m_spawntime_min;
} // 0x803B3FB4

/*
====================
SoundDefDef::setSpawnIntensity

Sets the spawn times from an intensity: the louder, the shorter (500 to 1500 milliseconds at an
intensity of 1).
====================
*/
void SoundDefDef::setSpawnIntensity( float intensity ) {
    if ( intensity > 0.0f ) {
        m_spawntime_min = (int)( 500.0f / intensity );
        m_spawntime_max = (int)( 1500.0f / intensity );
    } else {
        m_spawntime_min = 0;
        m_spawntime_max = 0;
    }
} // 0x803B3FDC

/*
====================
SoundDefDef::getPlayMode
====================
*/
int SoundDefDef::getPlayMode() {
    return ( m_flags >> SOUNDDEF_FLAGS_PLAYMODE_SHIFT ) & 7;
} // 0x803B4034

/*
====================
SoundDefDef::getVolumeRandMethod

Inlined into SoundDef::getVolume.
====================
*/
RandMethod SoundDefDef::getVolumeRandMethod() {
    return (RandMethod)( ( m_flags >> SOUNDDEF_FLAGS_VOLUMERAND_SHIFT ) & 1 );
} // no copy in retail (inlined)

/*
====================
SoundDefDef::getPitchRandMethod

Inlined into SoundDef::getPitch.
====================
*/
RandMethod SoundDefDef::getPitchRandMethod() {
    return (RandMethod)( ( m_flags >> SOUNDDEF_FLAGS_PITCHRAND_SHIFT ) & 1 );
} // no copy in retail (inlined)

/*
====================
SoundDefDef::getPitchRecalculateMode
====================
*/
int SoundDefDef::getPitchRecalculateMode() {
    return m_flags & SOUNDDEF_FLAGS_RECALCULATE_MASK;
} // 0x803B4040

/*
====================
SoundDefDef::setPlayMode
====================
*/
void SoundDefDef::setPlayMode( int mode ) {
    m_flags = ( m_flags & ~( 7 << SOUNDDEF_FLAGS_PLAYMODE_SHIFT ) ) | ( mode << SOUNDDEF_FLAGS_PLAYMODE_SHIFT );
} // 0x803B404C

/*
====================
SoundDefDef::setVolumeRandMethod
====================
*/
void SoundDefDef::setVolumeRandMethod( RandMethod method ) {
    m_flags = ( m_flags & ~( 1 << SOUNDDEF_FLAGS_VOLUMERAND_SHIFT ) ) | ( method << SOUNDDEF_FLAGS_VOLUMERAND_SHIFT );
} // 0x803B4064

/*
====================
SoundDefDef::setPitchRandMethod
====================
*/
void SoundDefDef::setPitchRandMethod( RandMethod method ) {
    m_flags = ( m_flags & ~( 1 << SOUNDDEF_FLAGS_PITCHRAND_SHIFT ) ) | ( method << SOUNDDEF_FLAGS_PITCHRAND_SHIFT );
} // 0x803B407C

/*
====================
SoundDefDef::setPitchRecalculateMode
====================
*/
void SoundDefDef::setPitchRecalculateMode( RecalculateMode mode ) {
    m_flags = ( m_flags & ~SOUNDDEF_FLAGS_RECALCULATE_MASK ) | mode;
} // 0x803B4094

/*
====================
SoundDef::SoundDef
====================
*/
SoundDef::SoundDef() {
    m_def            = NULL;
    m_name           = NULL;
    m_numentries     = 0;
    m_lastentryindex = -1;
    m_shuffleindex   = 0;
    m_shuffletable   = NULL;
    m_entry          = NULL;
} // 0x803B40A8

/*
====================
SoundDef::init

Makes this definition a copy of src, with an entries array of its own. Gormiti's map has an out-of-line
copy of 0xD4 bytes that retail dropped; only its allocation tag string is needed (the third string
before SoundDef::release's in .data). The body is an unconfirmed reconstruction from that evidence.
====================
*/
FMOD_RESULT SoundDef::init( SoundDef *src ) {
    m_def        = src->m_def;
    m_numentries = src->m_numentries;
    m_entry      = (SoundDefWaveform *)FMOD_Memory_Calloc( m_numentries * sizeof( SoundDefWaveform ) );
    if ( !m_entry ) {
        return FMOD_ERR_MEMORY;
    }

    for ( int i = 0; i < m_numentries; i++ ) {
        m_entry[i] = src->m_entry[i];
    }

    return FMOD_OK;
} // no copy in retail (dead-stripped)

/*
====================
SoundDef::release

Frees the name, the entries, the shuffle table and, when asked, the definition itself. The name and
the entries come from the project's sound definition pool; the rest from the global pool.
====================
*/
FMOD_RESULT SoundDef::release( bool freethis ) {
    SimpleMemPool *pool = m_def->m_project ? m_def->m_project->m_sounddef_pool : NULL;

    if ( m_name ) {
#line 380
        pool->free( m_name, __FILE__, __LINE__ );
    }

    if ( m_entry ) {
        for ( int i = 0; i < m_numentries; i++ ) {
            SoundDefEntry *entry = &m_entry[i];

            if ( entry->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                static_cast<SoundDefWaveform *>( entry )->release();
            }
        }
#line 395
        pool->free( m_entry, __FILE__, __LINE__ );
    }

    if ( m_shuffletable ) {
#line 400
        FMOD_Memory_Free( m_shuffletable );
    }

    if ( freethis ) {
#line 412
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x803B40DC

/*
====================
SoundDef::shuffle

Shuffles the sound's table of entry indices, and keeps the first entry from repeating the last one
of the previous order. Inlined into getEntry.
====================
*/
void SoundDef::shuffle( EventSound *sound ) {
    int last = sound->m_shuffletable[m_numentries - 1];

    for ( int i = 0; i < m_numentries; i++ ) {
        int j = i + rand() % ( m_numentries - i );
        int t = sound->m_shuffletable[j];

        sound->m_shuffletable[j] = sound->m_shuffletable[i];
        sound->m_shuffletable[i] = t;
    }

    if ( sound->m_shuffletable[0] == last ) {
        int j = 1 + rand() % ( m_numentries - 1 );
        int t = sound->m_shuffletable[j];

        sound->m_shuffletable[j] = sound->m_shuffletable[0];
        sound->m_shuffletable[0] = t;
    }
} // no copy in retail (inlined)

/*
====================
shuffleTable

Shuffles the definition-wide table of entry indices, as SoundDef::shuffle does a sound's. Name
invented: Gormiti's line table puts the whole shuffle of getEntry's global mode on one line, with the
lexical blocks of an inlined body, and its loads stay in registers across rand(), so the table and
count arrive as values. Inlined into getEntry.
====================
*/
static void shuffleTable( int *table, int numentries ) {
    int last = table[numentries - 1];

    for ( int i = 0; i < numentries; i++ ) {
        int j = i + rand() % ( numentries - i );
        int t = table[j];

        table[j] = table[i];
        table[i] = t;
    }

    if ( table[0] == last ) {
        int j = 1 + rand() % ( numentries - 1 );
        int t = table[j];

        table[j] = table[0];
        table[0] = t;
    }
} // no copy in retail (inlined)

/*
====================
SoundDef::getEntry

Picks the entry a sound plays next, by the play mode: in order, by weight (with or without allowing
the last entry again), or from a shuffled order kept per sound or for the definition as a whole.
Returns the index through entry and records it as the last one played.
====================
*/
FMOD_RESULT SoundDef::getEntry( EventSound *sound, int *entry ) {
    int entryindex = 0;

    if ( !sound || !entry || !m_numentries ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( m_def->getPlayMode() == SOUNDDEF_PLAYMODE_SEQUENTIAL || m_def->getPlayMode() == SOUNDDEF_PLAYMODE_PROGRAMMER ) {
        entryindex = sound->m_entryindex;
        entryindex++;
        if ( entryindex >= m_numentries ) {
            entryindex = 0;
        }
        sound->m_entryindex = entryindex;
    } else if ( ( m_def->getPlayMode() == SOUNDDEF_PLAYMODE_RANDOM || m_def->getPlayMode() == SOUNDDEF_PLAYMODE_RANDOM_NOREPEAT )
                && m_numentries > 1 ) {
        int i, r, totalweight;

        for ( i = 0, totalweight = 0; i < m_numentries; i++ ) {
            totalweight += m_entry[i].m_weight;
        }

        if ( totalweight ) {
            entryindex = rand() % totalweight;
            for ( i = 0, r = 0; i < m_numentries; i++ ) {
                r += m_entry[i].m_weight;
                if ( entryindex < r ) {
                    entryindex = i;
                    break;
                }
            }
            entryindex %= m_numentries;

            if ( m_def->getPlayMode() == SOUNDDEF_PLAYMODE_RANDOM_NOREPEAT && entryindex == m_lastentryindex ) {
                entryindex = ( entryindex + 1 ) % m_numentries;
            }
        }
    } else if ( m_def->getPlayMode() == SOUNDDEF_PLAYMODE_SHUFFLE && m_numentries > 1 ) {
        if ( !sound->m_shuffletable || sound->m_shufflesize != m_numentries ) {
            if ( sound->m_shuffletable ) {
#line 507
                FMOD_Memory_Free( sound->m_shuffletable );
            }

            sound->m_shuffleindex = 0;
            sound->m_shufflesize  = m_numentries;
#line 512
            sound->m_shuffletable = (int *)FMOD_Memory_Calloc( m_numentries * sizeof( int ) );
            if ( !sound->m_shuffletable ) {
                return FMOD_ERR_MEMORY;
            }

            for ( int i = 0; i < m_numentries; i++ ) {
                sound->m_shuffletable[i] = i;
            }
            shuffle( sound );
        }

        sound->m_shuffleindex++;
        if ( sound->m_shuffleindex >= m_numentries ) {
            sound->m_shuffleindex = 0;
            shuffle( sound );
        }

        entryindex          = sound->m_shuffletable[sound->m_shuffleindex];
        sound->m_entryindex = entryindex;
    } else if ( m_def->getPlayMode() == SOUNDDEF_PLAYMODE_SHUFFLE_GLOBAL && m_numentries > 1 ) {
        if ( ++m_shuffleindex >= m_numentries ) {
            m_shuffleindex = 0;
            shuffleTable( m_shuffletable, m_numentries );
        }

        entryindex          = m_shuffletable[m_shuffleindex];
        sound->m_entryindex = entryindex;
    }

    *entry                  = entryindex;
    m_lastentryindex        = entryindex;
    sound->m_lastentryindex = entryindex;

    return FMOD_OK;
} // 0x803B420C

/*
====================
SoundDef::getVolume

The volume of a sound: the definition's volume, randomised by its method, within 0 to 1.
====================
*/
float SoundDef::getVolume() {
    float v = m_def->m_volume;

    if ( m_def->getVolumeRandMethod() == RANDMETHOD_RANDOMIZATION ) {
        if ( 1.0f != m_def->m_volume_randomization ) {
            v *= m_def->m_volume_randomization
               + ( 1.0f - m_def->m_volume_randomization ) * ( (float)rand() / RAND_MAX );
        }
    } else if ( m_def->getVolumeRandMethod() == RANDMETHOD_MINMAX ) {
        if ( !( 1.0f == m_def->m_volume_random_min && 1.0f == m_def->m_volume_random_max ) ) {
            float t = m_def->m_volume_random_max - m_def->m_volume_random_min;

            if ( 0.0f == t ) {
                v *= m_def->m_volume_random_max;
            } else {
                v *= m_def->m_volume_random_min + (float)( rand() % (int)( 10000.0f * t ) + 1 ) / 10000.0f;
            }
        }
    }

    if ( v < 0.0f ) {
        v = 0.0f;
    } else if ( v > 1.0f ) {
        v = 1.0f;
    }

    return v;
} // 0x803B4774

/*
====================
SoundDef::getPitch

The pitch of a sound as a frequency multiplier: the definition's pitch, in octaves over a range of four,
randomised by its method.

The powers go through MSL's inline powf: randomized_pitch coalesces with its parameter, made at
expansion, which gives retail's colouring (f30). The second branch's `def` and `pitch` locals are not
in Gormiti's DWARF: `def` is what keeps retail's m_def in a register across the powf calls (a
function-wide `def` also reuses it in the first branch, where retail reloads m_def after rand).
====================
*/
float SoundDef::getPitch() {
    float v = 0.0f;

    if ( m_def->getPitchRandMethod() == RANDMETHOD_RANDOMIZATION ) {
        float randomized_pitch = 4.0f * m_def->m_pitch;

        if ( 0.0f != m_def->m_pitch_randomization ) {
            randomized_pitch += 4.0f * m_def->m_pitch_randomization * ( 2.0f * (float)rand() / RAND_MAX - 1.0f );
        }
        v = powf( 2.0f, randomized_pitch );
    } else if ( m_def->getPitchRandMethod() == RANDMETHOD_MINMAX ) {
        SoundDefDef *def = m_def;

        float pitch = powf( 2.0f, 4.0f * def->m_pitch );
        float min   = powf( 2.0f, 4.0f * def->m_pitch_random_min );
        float max   = powf( 2.0f, 4.0f * def->m_pitch_random_max );
        float t     = max - min;
        int   pp    = (int)( 10000.0f * t );

        if ( 0.0f == t || pp == 0 ) {
            v = pitch * max;
        } else {
            v = pitch * ( min + ( 1.0f + (float)( rand() % pp ) ) / 10000.0f );
        }

        if ( v < -16.0f ) {
            v = -16.0f;
        } else if ( v > 16.0f ) {
            v = 16.0f;
        }
    }

    return v;
} // 0x803B48F8

/*
====================
SoundDef::getOffset

A random offset from the sound's position, within a cube whose side is twice the definition's position
randomisation.
====================
*/
FMOD_VECTOR SoundDef::getOffset() {
    FMOD_VECTOR v = { 0.0f, 0.0f, 0.0f };

    if ( m_def->m_position_randomization > 0.0f ) {
        float diameter = 2.0f * m_def->m_position_randomization;

        v.x = m_def->m_position_randomization - diameter * ( (float)rand() / RAND_MAX );
        v.y = m_def->m_position_randomization - diameter * ( (float)rand() / RAND_MAX );
        v.z = m_def->m_position_randomization - diameter * ( (float)rand() / RAND_MAX );
    }

    return v;
} // 0x803B4AC8

} // namespace FMOD
