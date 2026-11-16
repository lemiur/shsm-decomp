/*
 * fmod_eventimpl_simple.cpp: FMOD::EventImplSimple, the body of a simple event (one sound
 * definition, played through one EventSound). Allocation tags carry the original file's line
 * numbers (docs/exceptions.md, "FMOD rulings (fmod-1)"): the #line directives restore them.
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventimpl_simple.obj)
// Unit parked (NonMatching). Open diffs: update, updateSound, createStreams, buildSoundBankList (walls in
// their headers).

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_eventgroupi.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventimpl.h>
#include <fmod/fmod_eventsound.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_musicsystemi.h>
#include <fmod/fmod_simplemempool.h>
#include <fmod/fmod_soundbank.h>
#include <fmod/fmod_string.h>
#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <new>

namespace FMOD {

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

/*
====================
EventImplSimple::EventImplSimple
====================
*/
EventImplSimple::EventImplSimple() {
    m_sound = NULL;
    m_flags = 0;
} // 0x80399C30

/*
====================
EventImplSimple::release

Frees the body unless it came from the instance pool.
====================
*/
FMOD_RESULT EventImplSimple::release() {
    if ( !m_eventi->getMemPool() ) {
#line 48
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x80399C50

/*
====================
EventImplSimple::releaseData
====================
*/
FMOD_RESULT EventImplSimple::releaseData() {
    if ( m_sound ) {
        m_sound->release( !( m_eventi->m_data_base.m_exflags & EVENTI_EXFLAG_POOLCOPY ) );
    }

    return FMOD_OK;
} // 0x80399CB4

/*
====================
EventImplSimple::unloadAllSounds
====================
*/
FMOD_RESULT EventImplSimple::unloadAllSounds() {
    if ( m_sound ) {
        if ( ( m_sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( m_sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) {
            CHECK_RESULT( m_sound->unload() );
        } else {
            for ( LinkedListNode *node = m_sound->m_oneshothead.getNext(); node != &m_sound->m_oneshothead; node = node->getNext() ) {
                CHECK_RESULT( static_cast<EventSound *>( node )->unload() );
            }
        }
    }

    return FMOD_OK;
} // 0x80399CFC

/*
====================
EventImplSimple::updateTimeParameters
====================
*/
FMOD_RESULT EventImplSimple::updateTimeParameters( unsigned int delta ) {
    if ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_TIMEUPDATE ) {
        return m_eventi->update();
    }

    return FMOD_OK;
} // 0x80399D94

/*
====================
EventImplSimple::update

The per-frame update of the event's sound: starts it (or, for a oneshot, spawns the next sound
when the spawn timer runs out), updates the playing channel, unpauses it on the DSP clock, and
retires spawned sounds that have finished. Stops the event when it is done or ran out of channels.

99.95% (wall: register/schedule): in the EVENTI_FLAG_0x40000 update retail numbers the
oneshotsplaying mask before the m_eventi load of the clearing statement (mask in r4, event in r5,
`or mask, flags`); here the order is the reverse. `flags = mask | flags` moves the or but not the
load (worse). Gormiti's line table keeps the clear and the set as two statements on consecutive lines
(no inline helper or macro), the set's code first; the ternary without parentheses, with 0U, with a cast
flag, and with an inverted or `== true` test leave it or lose more.
====================
*/
FMOD_RESULT EventImplSimple::update( unsigned int elapsed_time, unsigned int updateflags ) {
    FMOD_RESULT result;
    bool        oneshotsplaying = false, oneshotevent_stillplaying = false, allsoundsready = true;
    EventSound *oneshotsound;

    unsigned long long sound_end    = 0;
    EventSound        *latest_sound = NULL;

    if ( m_eventi->isSampleAccurate() ) {
        CHECK_RESULT( m_sound->getLatestEndTime( &sound_end, &latest_sound ) );
    }

    EventSound *sound = m_sound;

    if ( ( sound->m_flags & ( EVENTSOUND_FLAG_0x10 | EVENTSOUND_FLAG_0x08 ) ) == ( EVENTSOUND_FLAG_0x10 | EVENTSOUND_FLAG_0x08 ) ) {
        bool actuallyplaying = false;

        if ( sound->m_channel ) {
            result = sound->m_channel->isPlaying( &actuallyplaying );
            if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_CHANNEL_STOLEN ) {
                return result;
            }
        }

        if ( actuallyplaying ) {
            oneshotevent_stillplaying = true;
        } else {
            sound->m_flags &= ~EVENTSOUND_FLAG_0x08;
        }
    }

    if ( sound->m_notreadysound ) {
        allsoundsready = false;
    }

    bool isplaying = false;

    if ( sound->m_channel ) {
        result = sound->m_channel->isPlaying( &isplaying );
        if ( result == FMOD_ERR_CHANNEL_STOLEN || result == FMOD_ERR_INVALID_HANDLE ) {
            result = sound->stopSound( true );
            if ( result != FMOD_OK ) {
                return result;
            }

            if ( sound->m_def->m_loopcount != -1 && ( ( sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) ) {
                sound->m_flags &= ~EVENTSOUND_FLAG_0x80;
                sound->m_flags |= EVENTSOUND_FLAG_0x100;
            }
        } else if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( sound->m_flags & EVENTSOUND_FLAG_0x80 ) {
        isplaying = true;
    }

    bool software = false;
    if ( sound ) {
        CHECK_RESULT( useSoftware( sound, &software ) );
    }

    if ( !isplaying ) {
        if ( software || !sound->isWaitPrevious() || !sound->isSoundPlaying() ) {
            if ( ( sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) {
                if ( !( sound->m_flags & EVENTSOUND_FLAG_0x100 ) ) {
                    oneshotevent_stillplaying = true;

                    result = m_eventi->playSound( sound, elapsed_time, NULL );
                    if ( result == FMOD_ERR_NOTREADY || result == FMOD_ERR_FILE_DISKEJECTED ) {
                        result = FMOD_OK;
                    }

                    if ( result == FMOD_ERR_TOOMANYCHANNELS ) {
                        sound->m_stream_time += elapsed_time;

                        if ( g_eventsystemi->m_initflags & FMOD_EVENT_INIT_FAIL_ON_MAXSTREAMS ) {
                            m_eventi->m_data_base.m_flags |= EVENTI_FLAG_MAXSTREAMS;
                        }
                    } else if ( result != FMOD_OK ) {
                        return result;
                    }
                }
            } else {
                int spawnmin = sound->m_def->m_sounddef->m_def->m_spawntime_min;
                int spawnmax = sound->m_def->m_sounddef->m_def->m_spawntime_max;

                if ( spawnmax > 0 && spawnmin <= spawnmax && sound->m_spawn_timer > 0.0f ) {
                    float timer_delta = (float)elapsed_time;

                    sound->m_spawn_timer -= timer_delta;
                }

                if ( sound->m_spawn_current < sound->m_def->m_sounddef->m_def->m_spawn_max ) {
                    oneshotsound  = NULL;
                    bool do_spawn = false;

                    if ( spawnmax <= 0 || spawnmin > spawnmax ) {
                        if ( !( sound->m_flags & EVENTSOUND_FLAG_0x100 ) ) {
                            do_spawn = true;
                            sound->m_flags |= EVENTSOUND_FLAG_0x100;
                        } else {
                            do_spawn = false;
                        }
                    } else if ( sound->m_spawn_timer <= 0.0f ) {
                        do_spawn = true;

                        if ( spawnmin != spawnmax ) {
                            sound->m_spawn_timer += (float)( spawnmin + rand() % ( spawnmax - spawnmin ) );
                        } else {
                            sound->m_spawn_timer += (float)spawnmin;
                        }

                        if ( sound->m_spawn_timer <= 0.0f ) {
                            if ( spawnmin != spawnmax ) {
                                sound->m_spawn_timer = (float)( spawnmin + rand() % ( spawnmax - spawnmin ) );
                            } else {
                                sound->m_spawn_timer = (float)spawnmin;
                            }
                        }
                    }

                    if ( do_spawn ) {
                        bool findoneshot = true;

                        if ( sound->m_nextentryindex > -1 ) {
                            SoundDefWaveform *waveform = &sound->m_def->m_sounddef->m_entry[sound->m_nextentryindex];

                            if ( waveform->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                                if ( sound->m_stream_time >= (int)waveform->m_lengthms ) {
                                    findoneshot = false;
                                }
                            } else if ( waveform->m_entrytype == SOUNDDEF_ENTRYTYPE_NULL ) {
                                findoneshot = false;
                            }
                        }

                        if ( findoneshot ) {
                            for ( LinkedListNode *node = sound->m_oneshothead.getNext(); node != &sound->m_oneshothead; node = node->getNext() ) {
                                if ( !( ( (EventSound *)node )->m_flags & EVENTSOUND_FLAG_0x01 ) ) {
                                    oneshotsound = (EventSound *)node;
                                    break;
                                }
                            }
                        }
                    }

                    if ( oneshotsound ) {
                        oneshotsound->m_stream_time = sound->m_stream_time;
                        oneshotevent_stillplaying   = true;

                        bool dontplay = false;
                        result        = m_eventi->playSound( oneshotsound, elapsed_time, &dontplay );
                        if ( result == FMOD_OK ) {
                            if ( !dontplay ) {
                                sound = oneshotsound;
                                sound->m_flags |= EVENTSOUND_FLAG_0x01;
                                sound->m_parentsound->m_spawn_current++;
                            }
                        } else if ( result == FMOD_ERR_NOTREADY || result == FMOD_ERR_FILE_DISKEJECTED ) {
                            sound->m_flags &= ~EVENTSOUND_FLAG_0x100;
                            sound->m_spawn_timer = 0.0f;
                        } else if ( result == FMOD_ERR_TOOMANYCHANNELS ) {
                            sound->m_nextentryindex = oneshotsound->m_nextentryindex;
                            sound->m_stream_time += elapsed_time;
                            sound->m_flags &= ~EVENTSOUND_FLAG_0x100;
                            sound->m_spawn_timer = 0.0f;

                            if ( g_eventsystemi->m_initflags & FMOD_EVENT_INIT_FAIL_ON_MAXSTREAMS ) {
                                m_eventi->m_data_base.m_flags |= EVENTI_FLAG_MAXSTREAMS;
                            }
                        } else {
                            return result;
                        }
                    } else {
                        sound = NULL;
                    }
                } else {
                    sound = NULL;
                }
            }
        }
    } else {
        sound->m_rampdowntime     = -1;
        sound->m_last_volume      = 666.0f;
        oneshotevent_stillplaying = true;
    }

    if ( sound && sound->m_channel ) {
        result = updateSound( sound, updateflags );
        if ( result != FMOD_OK ) {
            return result;
        }

        bool paused;

        result = sound->m_channel->getPaused( &paused );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
            return result;
        }

        if ( paused ) {
            if ( software ) {
                if ( sound->isWaitPrevious() && sound_end ) {
                    FMOD_UINT64P tmp;

                    tmp.mValue = sound_end;
                    CHECK_RESULT( sound->m_channel->setDelay( FMOD_DELAYTYPE_DSPCLOCK_START, tmp.mHi, tmp.mLo ) );

                    sound->m_previous_sound = latest_sound;
                } else if ( m_eventi->isSampleAccurate() ) {
                    FMOD_UINT64P tmp;

                    CHECK_RESULT( g_eventsystemi->m_system->getDSPClock( &tmp.mHi, &tmp.mLo ) );

                    unsigned int blocksize;

                    CHECK_RESULT( g_eventsystemi->m_system->getDSPBufferSize( &blocksize, NULL ) );

                    tmp.mValue += blocksize;

                    CHECK_RESULT( sound->m_channel->setDelay( FMOD_DELAYTYPE_DSPCLOCK_START, tmp.mHi, tmp.mLo ) );
                }
            }

            result = sound->m_channel->setPaused( false );
            if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
                return result;
            }
        }
    }

    /*
        Spawned oneshot sounds: start the ones still waiting, update the playing ones and retire
        those that have finished.
    */
    oneshotsound = m_sound;

    if ( oneshotsound->m_flags & EVENTSOUND_FLAG_ONESHOT ) {
        for ( LinkedListNode *oneshotsoundnode = oneshotsound->m_oneshothead.getNext(); oneshotsoundnode != &oneshotsound->m_oneshothead;
              oneshotsoundnode = oneshotsoundnode->getNext() ) {
            if ( ( (EventSound *)oneshotsoundnode )->m_flags & EVENTSOUND_FLAG_0x01 ) {
                bool deletesound = false;
                bool isplaying;

                if ( !( (EventSound *)oneshotsoundnode )->m_channel ) {
                    result = m_eventi->playSound( (EventSound *)oneshotsoundnode, elapsed_time, NULL );
                    if ( result == FMOD_ERR_NOTREADY || result == FMOD_ERR_FILE_DISKEJECTED ) {
                        result = FMOD_OK;
                    }
                    if ( result != FMOD_OK && result != FMOD_ERR_TOOMANYCHANNELS ) {
                        return result;
                    }

                    if ( ( (EventSound *)oneshotsoundnode )->m_channel ) {
                        bool paused;

                        result = oneshotsound->m_channel->getPaused( &paused );
                        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
                            return result;
                        }

                        if ( paused ) {
                            result = ( (EventSound *)oneshotsoundnode )->m_channel->setPaused( false );
                            if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
                                return result;
                            }
                        }
                    }
                } else {
                    result = updateSound( (EventSound *)oneshotsoundnode, updateflags );
                    if ( result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN || result == FMOD_ERR_INVALID_PARAM ) {
                        deletesound = true;
                    } else if ( result != FMOD_OK ) {
                        return result;
                    }

                    if ( !deletesound ) {
                        result = ( (EventSound *)oneshotsoundnode )->m_channel->isPlaying( &isplaying );
                        if ( result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN ) {
                            isplaying = false;
                        } else if ( result != FMOD_OK ) {
                            return result;
                        }

                        deletesound = !isplaying;
                    }

                    if ( deletesound ) {
                        if ( m_eventi->m_data_base.m_eventcallback ) {
                            EventSound *callbacksound = ( (EventSound *)oneshotsoundnode )->m_parentsound ? ( (EventSound *)oneshotsoundnode )->m_parentsound
                                                                                                          : (EventSound *)oneshotsoundnode;

                            m_eventi->callEventCallback( FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_END, callbacksound->m_def->m_sounddef->m_name,
                                                         (void *)callbacksound->m_lastentryindex );
                        }

                        ( (EventSound *)oneshotsoundnode )->m_flags &= ~EVENTSOUND_FLAG_0x01;
                        ( (EventSound *)oneshotsoundnode )->unload();
                        oneshotsound->m_spawn_current--;
                    } else {
                        oneshotsplaying = true;
                    }
                }

                oneshotevent_stillplaying = true;
            }
        }
    }

    m_eventi->m_data_base.m_flags &= ~EVENTI_FLAG_0x40000;
    m_eventi->m_data_base.m_flags |= ( oneshotsplaying ? EVENTI_FLAG_0x40000 : 0 );

    m_eventi->m_data_base.m_flags &= ~updateflags;

    if ( ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_WILLTERMINATE ) && !oneshotevent_stillplaying && !( m_eventi->m_data_base.m_flags & EVENTI_FLAG_0x20 ) &&
         allsoundsready ) {
        m_eventi->stop( true );
    } else if ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_MAXSTREAMS ) {
        m_eventi->stop( false );
        m_eventi->m_data_base.m_flags &= ~EVENTI_FLAG_MAXSTREAMS;
        return FMOD_ERR_EVENT_MAXSTREAMS;
    }

    return FMOD_OK;
} // 0x80399DB0
/*
====================
EventImplSimple::updateSound

Applies the sound's volume and frequency to its channel (each only when it changed), and the
event's 3D spread and pan level to a software 3D channel.
99.93% (wall: register/schedule): the first volume product lands in f1 where retail uses f0; every
association and statement split of the product, and a ternary for the mute test, leave it. The 1.0f of
envelope_volume and envelope_pitch is one register (f27) in retail too.
====================
*/
FMOD_RESULT EventImplSimple::updateSound( EventSound *sound, unsigned int delta ) {
    FMOD_RESULT result;
    float       pan, speaker_spread, panlevel;
    float       final_freq, envelope_pitch, sounddef_pitch, sounddefault_freq;
    float       final_volume, sound_volume, envelope_volume, sounddef_volume, sounddefault_volume;

    EventSoundDef *def = sound->m_def;

    sound_volume    = def->m_volume;
    sounddef_volume = sound->m_sounddefvolume;
    envelope_volume = 1.0f;
    sounddef_pitch  = sound->m_sounddefpitch;
    envelope_pitch  = 1.0f;

    if ( sound->m_sound ) {
        result = sound->m_sound->getDefaults( &sounddefault_freq, NULL, NULL, NULL );

        pan                 = 0.0f;
        sounddefault_volume = 1.0f;
    } else if ( sound->m_dsp ) {
        result = sound->m_dsp->getDefaults( &sounddefault_freq, &sounddefault_volume, &pan, NULL );
    } else {
        return FMOD_OK;
    }

    if ( result != FMOD_OK ) {
        return result;
    }

    speaker_spread = m_eventi->m_data_3d.m_speakerspread;
    panlevel       = m_eventi->m_data_3d.m_3dpanlevel;

    final_volume = sound_volume * envelope_volume * sounddef_volume * sounddefault_volume;

    if ( g_eventsystemi->getMute() ) {
        final_volume = 0.0f;
    }

    final_freq = sounddefault_freq * sounddef_pitch * envelope_pitch;
    final_freq += def->m_finetune;

    if ( sound->m_channel != sound->m_last_channel ) {
        sound->m_last_volume  = -666.0f;
        sound->m_last_freq    = -666.0f;
        sound->m_last_pan     = -666.0f;
        sound->m_last_channel = sound->m_channel;
    }

    if ( final_volume != sound->m_last_volume ) {
        result = sound->m_channel->setVolume( final_volume );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
            return result;
        }

        sound->m_last_volume = final_volume;
    }

    if ( final_freq != sound->m_last_freq ) {
        result = sound->m_channel->setFrequency( final_freq );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
            return result;
        }

        sound->m_last_freq = final_freq;
    }

    unsigned int channelmode;

    result = sound->m_channel->getMode( &channelmode );
    if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
        return result;
    }

    if ( ( m_eventi->m_data_base.m_mode & FMOD_3D ) && ( channelmode & FMOD_SOFTWARE ) ) {
        result = sound->m_channel->set3DSpread( speaker_spread );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
            return result;
        }

        result = sound->m_channel->set3DPanLevel( panlevel );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x8039A744

FMOD_RESULT EventImplSimple::createDSPNetwork() {
    return FMOD_OK;
} // 0x8039A9A8

FMOD_RESULT EventImplSimple::releaseDSPNetwork() {
    return FMOD_OK;
} // 0x8039A9B0

/*
====================
EventImplSimple::createStreams

Opens a stream instance for each reference the event makes to each stream bank not yet loaded:
the software references first, then the hardware ones.

99.87% (wall: control flow): retail materialises the software test as 1, then 0 when both tests fail;
this spelling sets 0 first (as do if/else assignments of the bool). Gormiti records no local for the test
(line 819, mode on 820), but its DWARF also omits duplicateEvent's sound local; and every
spelling without the bool falls to 93% (the getMode() accessor and the bool fix the registers).
`bool software = true; if ( j >= sw && !flag ) software = false;` gives retail's 1-then-0 order but
renumbers every callee-saved register (97.7%): the named, twice-assigned bool is the wrong census.
The materialisation order is fixed by the compiler for any expression (||, !(&&), != 0, !!, ?:,
int); every compiler version in tools/compilers gives the same.
====================
*/
FMOD_RESULT EventImplSimple::createStreams( unsigned int eventmode ) {
    EventI *parentevent = m_eventi->getOriginal() ? m_eventi->getOriginal() : m_eventi;

    for ( int i = 0; i < parentevent->m_data_parent->m_numsoundbanks; i++ ) {
        SoundBank *soundbank = parentevent->m_data_parent->m_soundbank[i];

        if ( ( soundbank->m_mode & FMOD_CREATESTREAM ) && !m_eventi->getBankLoaded( i ) ) {
            int numsoundbankrefs_sw = parentevent->m_data_parent->m_numsoundbankrefs[i] & 0xFFFF;
            int numsoundbankrefs_hw = (unsigned int)parentevent->m_data_parent->m_numsoundbankrefs[i] >> 16;

            for ( int j = 0; j < numsoundbankrefs_sw + numsoundbankrefs_hw; j++ ) {
                bool software = j < numsoundbankrefs_sw || ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_0x8000 );
                unsigned int mode = ( software ? FMOD_SOFTWARE : FMOD_HARDWARE ) | m_eventi->getMode();

                if ( eventmode & FMOD_EVENT_ERROR_ON_DISKACCESS ) {
                    return FMOD_ERR_FILE_UNWANTED;
                }

                CHECK_RESULT( soundbank->createStreamInstance( mode, eventmode, m_eventi, m_eventi->m_data_base.m_eventgroupi ) );
            }

            m_eventi->setBankLoaded( i, true );
        }
    }

    return FMOD_OK;
} // 0x8039A9B8
/*
====================
EventImplSimple::stop

Unloads the sound, or each sound a oneshot spawned.
====================
*/
FMOD_RESULT EventImplSimple::stop() {
    FMOD_RESULT result;
    EventSound *sound = m_sound;

    if ( ( sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) {
        if ( sound->m_channel && m_eventi->m_data_base.m_eventcallback ) {
            m_eventi->callEventCallback( FMOD_EVENT_CALLBACKTYPE_SOUNDDEF_END, sound->m_def->m_sounddef->m_name,
                                         (void *)sound->m_lastentryindex );
        }

        result = sound->unload();
        if ( result != FMOD_OK ) {
            return result;
        }
    } else {
        for ( LinkedListNode *oneshotsoundnode = sound->m_oneshothead.getNext(); oneshotsoundnode != &sound->m_oneshothead;
              oneshotsoundnode = oneshotsoundnode->getNext() ) {
            EventSound *oneshotsound = static_cast<EventSound *>( oneshotsoundnode );

            result = oneshotsound->unload();
            if ( result != FMOD_OK ) {
                return result;
            }

            oneshotsound->m_flags &= ~EVENTSOUND_FLAG_0x01;
        }

        sound->m_spawn_current = 0;
    }

    return FMOD_OK;
} // 0x8039AB1C

/*
====================
EventImplSimple::buildSoundBankList

Builds the parent event's bank information: the banks its sound definition's waveforms come from,
for each bank the software and hardware reference counts (low and high 16 bits) and the list of
subsound indices it uses (terminated by -1), and with the user asset manager the list of waveform
file names. Everything is gathered on the stack, then copied into one allocation.

98.7% (wall: register/schedule, no CFG or call difference): (1) the two NULL terminator stores
(m_waveformarray, m_soundbankname) take the constant in r3/r4 before the address chain in retail;
(2) the reference-count merge loads sounddefrefcnt_array[0] before bankrefcnt_array[j] (operand
order of the sums does not move it); (3) the unrolled bank-name loop colours the m_soundbank and
m_soundbankname bases differently. The inner subsound loop uses entryindex (Gormiti's DWARF lists
it; with j the two loop counters swap registers).
====================
*/
FMOD_RESULT EventImplSimple::buildSoundBankList() {
    int i, j, bankindex, entryindex, num_unique_banks = 0;
    int numbanks = g_eventsystemi->m_soundbankhead.count();
    SoundBank **bank_array           = (SoundBank **)alloca( numbanks * sizeof( SoundBank * ) );
    int        *bankrefcnt_array     = (int *)alloca( numbanks * sizeof( int ) );
    SoundDef  **sounddef_array;
    int        *sounddefrefcnt_array;
    int       **bankentry_array;

    memset( bankrefcnt_array, 0, numbanks * sizeof( int ) );
    memset( bank_array, 0, numbanks * sizeof( SoundBank * ) );

    sounddef_array = (SoundDef **)alloca( sizeof( SoundDef * ) );
    memset( sounddef_array, 0, sizeof( SoundDef * ) );

    sounddefrefcnt_array = (int *)alloca( sizeof( int ) );
    memset( sounddefrefcnt_array, 0, sizeof( int ) );

    int sounddefref_sw = 0;
    int sounddefref_hw = 0;

    sounddef_array[0] = m_sound->m_def->m_sounddef;

    if ( m_flags & 1 ) {
        sounddefref_sw = 1;
    }
    if ( !( m_flags & 1 ) ) {
        sounddefref_hw = 1;
    }

    sounddefrefcnt_array[0] = ( sounddefref_hw << 16 ) | ( sounddefref_sw & 0xFFFF );

    /*
        With the user asset manager, the event also lists the file names of its waveforms.
    */
    if ( g_eventsystemi->m_initflags & FMOD_EVENT_INIT_USER_ASSETMANAGER ) {
        int numwaveforms = 0;

        {
            SoundDef *sounddef = sounddef_array[0];

            for ( j = 0; j < sounddef->m_numentries; j++ ) {
                if ( sounddef->m_entry[j].m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                    numwaveforms++;
                }
            }
        }

        int    uniquewaveformcount = 0;
        char **waveformarray       = (char **)alloca( numwaveforms * sizeof( char * ) );
        memset( waveformarray, 0, numwaveforms * sizeof( char * ) );

        SoundDef *sounddef = sounddef_array[0];

        for ( i = 0; i < sounddef->m_numentries; i++ ) {
            SoundDefEntry *entry = &sounddef->m_entry[i];

            if ( entry->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                SoundDefWaveform *waveform = static_cast<SoundDefWaveform *>( entry );

                for ( int j = 0; j < numwaveforms; j++ ) {
                    if ( !waveformarray[j] ) {
                        waveformarray[j] = waveform->m_sourcefilename;
                        uniquewaveformcount++;
                        break;
                    } else if ( !FMOD_strcmp( waveformarray[j], waveform->m_sourcefilename ) ) {
                        break;
                    }
                }
            }
        }

        m_eventi->m_data_parent->m_waveformcount = uniquewaveformcount;
#line 988
        m_eventi->m_data_parent->m_waveformarray = (char **)FMOD_Memory_Alloc( ( uniquewaveformcount + 1 ) * sizeof( char * ) );
        if ( !m_eventi->m_data_parent->m_waveformarray ) {
            return FMOD_ERR_MEMORY;
        }

        memcpy( m_eventi->m_data_parent->m_waveformarray, waveformarray, uniquewaveformcount * sizeof( char * ) );
        m_eventi->m_data_parent->m_waveformarray[uniquewaveformcount] = NULL;
    }

    /*
        The banks the waveforms come from, with the reference counts they add up to.
    */
    {
        SoundDef *sounddef = sounddef_array[0];

        for ( i = 0; i < sounddef->m_numentries; i++ ) {
            SoundDefEntry *entry = &sounddef->m_entry[i];

            if ( entry->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                SoundDefWaveform *waveform = static_cast<SoundDefWaveform *>( entry );

                for ( j = 0; j < numbanks; j++ ) {
                    if ( bank_array[j] == waveform->m_soundbank ) {
                        bankrefcnt_array[j] = ( ( ( (unsigned int)bankrefcnt_array[j] >> 16 ) + ( (unsigned int)sounddefrefcnt_array[0] >> 16 ) ) << 16 ) |
                                              ( ( ( bankrefcnt_array[j] & 0xFFFF ) + ( sounddefrefcnt_array[0] & 0xFFFF ) ) & 0xFFFF );
                        break;
                    } else if ( !bank_array[j] ) {
                        bank_array[j]       = waveform->m_soundbank;
                        bankrefcnt_array[j] = sounddefrefcnt_array[0];
                        num_unique_banks++;
                        break;
                    }
                }
            }
        }
    }

    /*
        The subsound indices each bank is used for.
    */
    bankentry_array = (int **)alloca( num_unique_banks * sizeof( int * ) );
    memset( bankentry_array, 0, num_unique_banks * sizeof( int * ) );

    for ( bankindex = 0; bankindex < num_unique_banks; bankindex++ ) {
        int numsoundbankrefs = ( (unsigned int)bankrefcnt_array[bankindex] >> 16 ) + ( bankrefcnt_array[bankindex] & 0xFFFF );

        bankentry_array[bankindex] = (int *)alloca( ( numsoundbankrefs + 1 ) * sizeof( int ) );
        memset( bankentry_array[bankindex], -1, ( numsoundbankrefs + 1 ) * sizeof( int ) );

        int *ref_array = bankentry_array[bankindex];

        SoundDef *sounddef = sounddef_array[0];

        for ( i = 0; i < sounddef->m_numentries; i++ ) {
            SoundDefEntry *entry = &sounddef->m_entry[i];

            if ( entry->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
                SoundDefWaveform *waveform = static_cast<SoundDefWaveform *>( entry );

                if ( bank_array[bankindex] == waveform->m_soundbank ) {
                    for ( entryindex = 0; entryindex < numsoundbankrefs; entryindex++ ) {
                        if ( ref_array[entryindex] == (int)waveform->m_subsoundindex ) {
                            break;
                        }

                        if ( ref_array[entryindex] == -1 ) {
                            ref_array[entryindex] = waveform->m_subsoundindex;
                            break;
                        }
                    }
                }
            }
        }
    }

    /*
        One allocation holds the bank list, the reference counts, the bank names, the entry list
        pointers and the entry lists.
    */
    int bank_array_size       = num_unique_banks * sizeof( SoundBank * );
    int bankrefcnt_array_size = num_unique_banks * sizeof( int );
    int bankname_array_size   = ( num_unique_banks + 1 ) * sizeof( char * );
    int bankentry_array_size  = num_unique_banks * sizeof( int * );
    int bigbufsize            = bank_array_size + bankrefcnt_array_size + bankname_array_size + bankentry_array_size;

    for ( i = 0; i < num_unique_banks; i++ ) {
        int *ref_array = bankentry_array[i];

        for ( j = 0; ref_array[j] != -1; j++ ) {
        }
        bigbufsize += ( j + 1 ) * sizeof( int );
    }

    if ( m_eventi->m_data_parent->m_soundbank ) {
#line 1105
        FMOD_Memory_Free( m_eventi->m_data_parent->m_soundbank );
    }

#line 1108
    char *bigbuf = (char *)FMOD_Memory_Alloc( bigbufsize );
    if ( !bigbuf ) {
        return FMOD_ERR_MEMORY;
    }

    m_eventi->m_data_parent->m_soundbank = (SoundBank **)bigbuf;
    memcpy( m_eventi->m_data_parent->m_soundbank, bank_array, bank_array_size );
    bigbuf += bank_array_size;

    m_eventi->m_data_parent->m_numsoundbankrefs = (int *)bigbuf;
    memcpy( m_eventi->m_data_parent->m_numsoundbankrefs, bankrefcnt_array, bankrefcnt_array_size );
    bigbuf += bankrefcnt_array_size;

    m_eventi->m_data_parent->m_soundbankname = (char **)bigbuf;
    for ( i = 0; i < num_unique_banks; i++ ) {
        m_eventi->m_data_parent->m_soundbankname[i] = m_eventi->m_data_parent->m_soundbank[i]->m_name;
    }
    m_eventi->m_data_parent->m_soundbankname[i] = NULL;
    bigbuf += bankname_array_size;

    m_eventi->m_data_parent->m_bankentry_array = (int **)bigbuf;
    bigbuf += bankentry_array_size;

    int *entry_array = (int *)bigbuf;

    for ( i = 0; i < num_unique_banks; i++ ) {
        int *ref_array = bankentry_array[i];

        m_eventi->m_data_parent->m_bankentry_array[i] = entry_array;

        while ( *ref_array != -1 ) {
            *entry_array++ = *ref_array++;
        }
        *entry_array++ = -1;
    }

    m_eventi->m_data_parent->m_numsoundbanks = num_unique_banks;
    m_eventi->m_data_parent->m_bankinfosize  = bigbufsize;

    return FMOD_OK;
} // 0x8039ABF4

FMOD_RESULT EventImplSimple::updateEnvelopes( void *data ) {
    return FMOD_OK;
} // 0x8039B3C8

/*
====================
EventImplSimple::getLengthMs

The length of the longest waveform, or -1 when the sound has no fixed length.
====================
*/
FMOD_RESULT EventImplSimple::getLengthMs( int *length ) {
    EventSound *sound = m_sound;
    int numentries    = sound->m_def->m_sounddef->m_numentries;

    if ( ( sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) {
        *length = -1;
        return FMOD_OK;
    }

    for ( int i = 0; i < numentries; i++ ) {
        SoundDefEntry *entry = &sound->m_def->m_sounddef->m_entry[i];

        if ( entry->m_entrytype == SOUNDDEF_ENTRYTYPE_WAVEFORM ) {
            SoundDefWaveform *waveform = static_cast<SoundDefWaveform *>( entry );

            if ( (int)waveform->m_lengthms > *length ) {
                *length = waveform->m_lengthms;
            }
        } else if ( entry->m_entrytype != SOUNDDEF_ENTRYTYPE_NULL ) {
            *length = -1;
            return FMOD_OK;
        }
    }

    return FMOD_OK;
} // 0x8039B3D0

/*
====================
EventImplSimple::getPositionMs

The time the event has played, plus the time since the mixer last advanced while it is running,
plus its time offset.
====================
*/
FMOD_RESULT EventImplSimple::getPositionMs( unsigned int *position ) {
    FMOD_RESULT  result;
    unsigned int currenttime;

    result = FMOD_OS_Time_GetMs( &currenttime );
    if ( result != FMOD_OK ) {
        return result;
    }

    *position = m_eventi->getDSPClockElapsed().mHi;

    if ( ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_TIMEUPDATE ) && !( m_eventi->m_data_base.m_flags & EVENTI_FLAG_PAUSED ) ) {
        if ( lbl_80529020->mDSPClockTimeStamp ) {
            *position = *position + ( currenttime - lbl_80529020->mDSPClockTimeStamp ) +
                        ( lbl_80529020->mDSPClock.mHi - m_eventi->getLastDSPClock().mHi );
        } else {
            *position = 0;
        }
    }

    float timeoffset = m_eventi->m_data_base.m_timeoffset;

    *position += (int)( timeoffset * 1000.0f + 0.5f );

    return FMOD_OK;
} // 0x8039B470

/*
====================
EventImplSimple::getChannelsPlaying
====================
*/
FMOD_RESULT EventImplSimple::getChannelsPlaying( int *channelsplaying ) {
    if ( !channelsplaying ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channelsplaying = 0;

    if ( m_sound ) {
        int playing = ( m_sound->m_channel != NULL );

        for ( LinkedListNode *node = m_sound->m_oneshothead.getNext(); node != &m_sound->m_oneshothead; node = node->getNext() ) {
            playing += ( static_cast<EventSound *>( node )->m_channel != NULL );
        }

        *channelsplaying = playing;
    }

    return FMOD_OK;
} // 0x8039B574

/*
====================
EventImplSimple::getAudibility

The summed audibility of the channels in the instance's channel group.
====================
*/
FMOD_RESULT EventImplSimple::getAudibility( float *volume ) {
    if ( !volume ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    float         total        = 0.0f;
    ChannelGroup *channelgroup = m_eventi->getInstanceChannelGroup();

    if ( channelgroup ) {
        int numchannels;

        CHECK_RESULT( channelgroup->getNumChannels( &numchannels ) );

        for ( int i = 0; i < numchannels; i++ ) {
            Channel *channel;
            float    audibility;

            CHECK_RESULT( channelgroup->getChannel( i, &channel ) );
            CHECK_RESULT( channel->getAudibility( &audibility ) );
            total += audibility;
        }
    }

    *volume = total;

    return FMOD_OK;
} // 0x8039B5E0

/*
====================
EventImplSimple::estimateAudibility
====================
*/
FMOD_RESULT EventImplSimple::estimateAudibility( float *audibility ) {
    if ( !audibility ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *audibility = m_sound->m_def->m_volume;

    return FMOD_OK;
} // 0x8039B6D4

/*
====================
EventImplSimple::start
====================
*/
FMOD_RESULT EventImplSimple::start() {
    EventSound *sound = m_sound;

    sound->m_spawn_timer = 0.0f;
    sound->m_stream_time = 0;
    sound->m_flags &= ~EVENTSOUND_FLAG_START_CLEAR;
    sound->m_flags |= EVENTSOUND_FLAG_0x400;

    if ( sound->m_def->m_sounddef->m_def->getPlayMode() == SOUNDDEF_PLAYMODE_PROGRAMMER ) {
        sound->m_entryindex = -1;
    }

    return FMOD_OK;
} // 0x8039B6FC

// A simple event has no parameters and no properties of its own.

FMOD_RESULT EventImplSimple::getParameter( const char *name, EventParameter **parameter ) {
    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x8039B76C

FMOD_RESULT EventImplSimple::getParameterByIndex( int index, EventParameter **parameter ) {
    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x8039B774

FMOD_RESULT EventImplSimple::getParameterWithDef( EventParameterDef *def, EventParameterI **parameter ) {
    return FMOD_ERR_INTERNAL;
} // 0x8039B77C

FMOD_RESULT EventImplSimple::getNumParameters( int *numparameters ) {
    *numparameters = 0;
    return FMOD_OK;
} // 0x8039B784

FMOD_RESULT EventImplSimple::setMode() {
    return m_sound->setMode( m_eventi->getMode() );
} // 0x8039B794

FMOD_RESULT EventImplSimple::setMinMaxDistance() {
    return m_sound->setMinMaxDistance( m_eventi->getMinDistance(), m_eventi->getMaxDistance() );
} // 0x8039B7A4

FMOD_RESULT EventImplSimple::setReverbWetLevelDB() {
    return m_sound->setReverbWetLevelDB( m_eventi->getReverbWetLevelDB() );
} // 0x8039B7B8

FMOD_RESULT EventImplSimple::setReverbDryLevelDB() {
    return m_sound->setReverbDryLevelDB( m_eventi->getReverbDryLevelDB() );
} // 0x8039B7C8

FMOD_RESULT EventImplSimple::getProperty( const char *propertyname, void *value ) {
    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x8039B7D8

FMOD_RESULT EventImplSimple::getPropertyByIndex( int propertyindex, void *value ) {
    return FMOD_ERR_EVENT_NOTFOUND;
} // 0x8039B7E0

FMOD_RESULT EventImplSimple::setProperty( const char *propertyname, void *value ) {
    return FMOD_ERR_INVALID_PARAM;
} // 0x8039B7E8

FMOD_RESULT EventImplSimple::setPropertyByIndex( int propertyindex, void *value ) {
    return FMOD_ERR_INVALID_PARAM;
} // 0x8039B7F0

FMOD_RESULT EventImplSimple::getNumProperties( int *numproperties ) {
    *numproperties = 0;
    return FMOD_OK;
} // 0x8039B7F8

FMOD_RESULT EventImplSimple::getPropertyInfo( int *propertyindex, char **propertyname, FMOD_EVENTPROPERTY_TYPE *type ) {
    return FMOD_ERR_INVALID_PARAM;
} // 0x8039B808

/*
====================
EventImplSimple::set3DAttributes

The sound sits at the event's position plus the sound definition's offset.
====================
*/
FMOD_RESULT EventImplSimple::set3DAttributes() {
    FMOD_VECTOR pos;

    pos.x = m_sound->m_sounddefoffset.x + m_eventi->m_data_3d.m_pos.x;
    pos.y = m_sound->m_sounddefoffset.y + m_eventi->m_data_3d.m_pos.y;
    pos.z = m_sound->m_sounddefoffset.z + m_eventi->m_data_3d.m_pos.z;

    return m_sound->set3DAttributes( &pos, m_eventi->getVelocity(), m_eventi->getOrientation() );
} // 0x8039B810

/*
====================
EventImplSimple::setReverbProperties

Applies the event's reverb levels to the sound's channel and to those of the sounds it spawned. A
channel that is gone, unsupported or has no reverb instance is not an error.
====================
*/
FMOD_RESULT EventImplSimple::setReverbProperties() {
    int                           wetlevel = dBToReverbLevel( m_eventi->m_data_base.m_reverbwetlevel_db );
    int                           drylevel = dBToReverbLevel( m_eventi->m_data_base.m_reverbdrylevel_db );
    FMOD_REVERB_CHANNELPROPERTIES prop;
    FMOD_RESULT                   result;

    memset( &prop, 0, sizeof( prop ) );

    if ( m_eventi->m_data_base.m_reverbproperties ) {
        for ( int i = 0; i < 4; i++ ) {
            prop.Room   = wetlevel;
            prop.Direct = drylevel;
            prop.Room   += m_eventi->m_data_base.m_reverbproperties[i].Room;
            prop.Direct += m_eventi->m_data_base.m_reverbproperties[i].Direct;
            prop.Flags           = m_eventi->m_data_base.m_reverbproperties[i].Flags;
            prop.ConnectionPoint = NULL;

            result = m_sound->m_channel->setReverbProperties( &prop );
            if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_UNSUPPORTED &&
                 result != FMOD_ERR_REVERB_INSTANCE ) {
                return result;
            }

            for ( LinkedListNode *node = m_sound->m_oneshothead.getNext(); node != &m_sound->m_oneshothead; node = node->getNext() ) {
                result = static_cast<EventSound *>( node )->m_channel->setReverbProperties( &prop );
                if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_UNSUPPORTED &&
                     result != FMOD_ERR_REVERB_INSTANCE ) {
                    return result;
                }
            }
        }
    } else {
        m_sound->m_channel->getReverbProperties( &prop );

        prop.Room   = wetlevel;
        prop.Direct = drylevel;
        prop.Flags |= FMOD_REVERB_CHANNELFLAGS_INSTANCE0 | FMOD_REVERB_CHANNELFLAGS_INSTANCE1 |
                      FMOD_REVERB_CHANNELFLAGS_INSTANCE2 | FMOD_REVERB_CHANNELFLAGS_INSTANCE3;

        result = m_sound->m_channel->setReverbProperties( &prop );
        if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_UNSUPPORTED ) {
            return result;
        }

        for ( LinkedListNode *node = m_sound->m_oneshothead.getNext(); node != &m_sound->m_oneshothead; node = node->getNext() ) {
            result = static_cast<EventSound *>( node )->m_channel->setReverbProperties( &prop );
            if ( result != FMOD_OK && result != FMOD_ERR_INVALID_HANDLE && result != FMOD_ERR_UNSUPPORTED &&
                 result != FMOD_ERR_REVERB_INSTANCE ) {
                return result;
            }
        }
    }

    return FMOD_OK;
} // 0x8039B874

/*
====================
EventImplSimple::getState

Adds the playing, loading and starving state of the sound's channel, or of the first of the
spawned sounds that is playing, to *state.
====================
*/
FMOD_RESULT EventImplSimple::getState( unsigned int *state ) {
    FMOD_RESULT result;
    EventSound *sound = m_sound;
    bool        isplaying;

    if ( ( sound->m_flags & EVENTSOUND_FLAG_0x02 ) || ( sound->m_flags & EVENTSOUND_FLAG_0x10 ) ) {
        result = sound->m_channel->isPlaying( &isplaying );
        if ( result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN ) {
            isplaying = false;
        } else if ( result != FMOD_OK ) {
            return result;
        }

        if ( sound->m_sound ) {
            bool           starving;
            FMOD_OPENSTATE openstate;

            result = sound->m_sound->getOpenState( &openstate, NULL, &starving );
            if ( result == FMOD_OK ) {
                if ( starving ) {
                    *state |= FMOD_EVENT_STATE_STARVING;
                }

                if ( openstate != FMOD_OPENSTATE_READY && openstate != FMOD_OPENSTATE_STREAMING ) {
                    *state |= FMOD_EVENT_STATE_LOADING;
                }
            } else {
                return result;
            }
        }

        if ( isplaying ) {
            *state |= FMOD_EVENT_STATE_CHANNELSACTIVE;
        }
    } else {
        for ( LinkedListNode *oneshotsoundnode = sound->m_oneshothead.getNext(); oneshotsoundnode != &sound->m_oneshothead;
              oneshotsoundnode = oneshotsoundnode->getNext() ) {
            EventSound *oneshotsound = static_cast<EventSound *>( oneshotsoundnode );

            result = oneshotsound->m_channel->isPlaying( &isplaying );
            if ( result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN ) {
                isplaying = false;
            } else if ( result != FMOD_OK ) {
                return result;
            }

            if ( oneshotsound->m_sound ) {
                bool           starving;
                FMOD_OPENSTATE openstate;

                result = oneshotsound->m_sound->getOpenState( &openstate, NULL, &starving );
                if ( result == FMOD_OK ) {
                    if ( starving ) {
                        *state |= FMOD_EVENT_STATE_STARVING;
                    }

                    if ( openstate != FMOD_OPENSTATE_READY && openstate != FMOD_OPENSTATE_STREAMING ) {
                        *state |= FMOD_EVENT_STATE_LOADING;
                    }
                } else {
                    return result;
                }
            }

            if ( isplaying ) {
                *state |= FMOD_EVENT_STATE_CHANNELSACTIVE;
                break;
            }
        }
    }

    return FMOD_OK;
} // 0x8039BA70

/*
====================
EventImplSimple::willTerminate
====================
*/
FMOD_RESULT EventImplSimple::willTerminate( int *terminate ) const {
    if ( !terminate ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *terminate = ( m_eventi->m_data_base.m_flags & EVENTI_FLAG_WILLTERMINATE ) != 0;

    return FMOD_OK;
} // 0x8039BC4C

FMOD_RESULT EventImplSimple::getParameterByIndexInternal( int index, EventParameterI **parameter ) {
    *parameter = NULL;
    return FMOD_ERR_INVALID_PARAM;
} // 0x8039BC74

/*
====================
EventImplSimple::duplicateEvent

Gives dstevent a body of its own: the same sound definition, its own sound.
The new sound is held in a local (r31) that Gormiti's DWARF does not list; it is stored last.
====================
*/
FMOD_RESULT EventImplSimple::duplicateEvent( EventI *dstevent, SimpleMemPool *mempool ) {
    EventImplSimple *dstevent_impl = static_cast<EventImplSimple *>( dstevent->m_impl );
    EventSound      *srcsound      = m_sound;

    dstevent_impl->m_flags = m_flags;

#line 1704
    EventSound *sound = new ( mempool->alloc( sizeof( EventSound ), __FILE__, __LINE__ ) ) EventSound;
    if ( !sound ) {
        return FMOD_ERR_MEMORY;
    }

    sound->init( srcsound->m_def );
    sound->m_def->m_refcnt++;

    if ( srcsound->m_def->m_sounddef->m_def->getPlayMode() == 0 ) {
        sound->m_entryindex = srcsound->m_def->m_sounddef->m_lastentryindex;
    }

    sound->m_flags  = srcsound->m_flags;
    sound->m_layer  = NULL;
    sound->m_eventi = dstevent;

    if ( sound->m_flags & EVENTSOUND_FLAG_ONESHOT ) {
        CHECK_RESULT( sound->createOneshotSounds( mempool ) );
    }

    dstevent_impl->m_sound = sound;

    return FMOD_OK;
} // 0x8039BC84

/*
====================
EventImplSimple::copyProperties

Makes dstevent's sound, and the sounds it spawned, play this event's sound definition.
====================
*/
FMOD_RESULT EventImplSimple::copyProperties( EventI *dstevent ) {
    EventSound      *srcsound = m_sound;
    EventImplSimple *dstimpl  = static_cast<EventImplSimple *>( dstevent->m_impl );
    EventSound      *dstsound = dstimpl->m_sound;

    dstimpl->m_flags = m_flags;
    dstsound->m_def  = srcsound->m_def;

    for ( LinkedListNode *node = dstsound->m_oneshothead.getNext(); node != &dstsound->m_oneshothead; node = node->getNext() ) {
        static_cast<EventSound *>( node )->m_def = srcsound->m_def;
    }

    if ( srcsound->m_def->m_sounddef->m_def->getPlayMode() == 0 ) {
        dstsound->m_entryindex = srcsound->m_def->m_sounddef->m_lastentryindex;
    }

    dstsound->m_flags  = srcsound->m_flags;
    dstsound->m_layer  = NULL;
    dstsound->m_eventi = dstevent;

    return FMOD_OK;
} // 0x8039BD7C

FMOD_RESULT EventImplSimple::setupInstance() {
    return FMOD_OK;
} // 0x8039BE30

/*
====================
EventImplSimple::load

Reads the event's flags and its one sound from the project file.
====================
*/
FMOD_RESULT EventImplSimple::load( File *file, unsigned int fileversion ) {
    CHECK_RESULT( file->read( &m_flags, 4, 1, NULL ) );

#line 1810
    EventSoundDef *sounddef = new ( FMOD_Memory_Alloc( sizeof( EventSoundDef ) ) ) EventSoundDef;
    if ( !sounddef ) {
        return FMOD_ERR_MEMORY;
    }

#line 1816
    EventSound *sound = new ( FMOD_Memory_Alloc( sizeof( EventSound ) ) ) EventSound;
    if ( !sound ) {
        return FMOD_ERR_MEMORY;
    }

    sounddef->init();
    sound->init( sounddef );
    sounddef->m_refcnt++;

    // A parent counts the memory its instances' sounds take.
    m_eventi->m_data_parent->m_mempoolsize = m_eventi->getMemPoolSize() + sizeof( EventSound );

    CHECK_RESULT( sound->loadFromFEV( file, fileversion ) );

    if ( sound->isWaitPrevious() ) {
        m_eventi->setSampleAccurate( true );
    }

    sound->m_eventi = m_eventi;
    m_sound         = sound;

    return FMOD_OK;
} // 0x8039BE38

/*
====================
EventImplSimple::dereferenceSoundnames

Resolves the sound's sound definition name and, for a oneshot, counts the memory of the sounds it
can spawn.
====================
*/
FMOD_RESULT EventImplSimple::dereferenceSoundnames( SoundDef *sounddeflisthead, int fileversion ) {
    CHECK_RESULT( g_eventsystemi->dereferenceSoundname( sounddeflisthead, fileversion, m_sound ) );

    if ( m_sound->m_flags & EVENTSOUND_FLAG_ONESHOT ) {
        m_eventi->m_data_parent->m_mempoolsize = m_eventi->getMemPoolSize() + m_sound->m_def->m_sounddef->m_def->m_spawn_max * sizeof( EventSound );
    }

    if ( m_sound->willTerminate() ) {
        m_eventi->m_data_base.m_flags |= EVENTI_FLAG_WILLTERMINATE;
    } else {
        m_eventi->m_data_base.m_flags &= ~EVENTI_FLAG_WILLTERMINATE;
    }

    return FMOD_OK;
} // 0x8039BFDC

FMOD_RESULT EventImplSimple::soundJustPlayed( EventSound *sound ) {
    return FMOD_OK;
} // 0x8039C0DC

FMOD_RESULT EventImplSimple::soundJustStopped( EventSound *sound ) {
    return FMOD_OK;
} // 0x8039C0E4

FMOD_RESULT EventImplSimple::useSoftware( EventSound *sound, bool *software ) {
    *software = m_flags & 1;
    return FMOD_OK;
} // 0x8039C0EC

FMOD_RESULT EventImplSimple::getPriority( EventSound *sound, int *priority ) {
    *priority = m_eventi->m_data_base.m_priority;
    return FMOD_OK;
} // 0x8039C100

FMOD_RESULT EventImplSimple::getTimeOffset( EventSound *sound, float *timeoffset ) {
    *timeoffset = m_eventi->m_data_base.m_timeoffset;
    return FMOD_OK;
} // 0x8039C114

FMOD_RESULT EventImplSimple::getChannelGroup( EventSound *sound, ChannelGroup **channelgroup ) {
    *channelgroup = m_eventi->getInstanceChannelGroup();
    return FMOD_OK;
} // 0x8039C128

FMOD_RESULT EventImplSimple::hasPanEnvelope( EventSound *sound, bool *has ) {
    *has = false;
    return FMOD_OK;
} // 0x8039C150

FMOD_RESULT EventImplSimple::hasSpeakerLevelEnvelope( EventSound *sound, bool *has ) {
    *has = false;
    return FMOD_OK;
} // 0x8039C160

FMOD_RESULT EventImplSimple::getSound( int layerindex, int soundindex, EventSound **sound ) {
    *sound = m_sound;
    return FMOD_OK;
} // 0x8039C170

FMOD_RESULT EventImplSimple::rescheduleSounds() {
    CHECK_RESULT( m_sound->setReschedulePending() );
    CHECK_RESULT( m_sound->rescheduleAll() );

    return FMOD_OK;
} // 0x8039C180

FMOD_RESULT EventImplSimple::clearPreviousSoundPointers( EventSound *sound ) {
    CHECK_RESULT( sound->clearPreviousPointers( sound ) );

    return FMOD_OK;
} // 0x8039C1D8

} // namespace FMOD
