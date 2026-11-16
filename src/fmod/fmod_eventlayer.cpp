/*
 * fmod_eventlayer.cpp: FMOD::EventLayer, a layer of a complex event: its sounds, its envelopes and the
 * DSP network and channel group they play through. Allocation tags carry the original file's line
 * numbers (docs/exceptions.md, "FMOD rulings (fmod-1)"): the #line directives restore them.
 */
// Names and layout: Gormiti link map and DWARF (fmod_event.a fmod_eventlayer.obj)

#include <math.h>

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_eventenvelope.h>
#include <fmod/fmod_eventi.h>
#include <fmod/fmod_eventlayer.h>
#include <fmod/fmod_eventparameteri.h>
#include <fmod/fmod_eventsound.h>
#include <fmod/fmod_eventsystemi.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

// Parameters of the SFX reverb DSP unit that an envelope maps onto a logarithmic scale (names invented).
#define SFXREVERB_HFREFERENCE   12
#define SFXREVERB_LFREFERENCE   14
#define SFXREVERB_LASTPARAM     12  // the envelopes of the higher parameters are not applied on start

// The natural logarithm as a float, as MSL's C++ math overload returns it (name invented). Inlined as a
// call whose result is rounded to float before the envelope scaling uses it, as retail schedules it.
static inline float LogF( float x ) {
    return (float)log( x );
}

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
EventLayer::EventLayer
====================
*/
EventLayer::EventLayer() {
    init();
} // 0x8039C898

/*
====================
EventLayer::init
====================
*/
FMOD_RESULT EventLayer::init() {
    m_eventi            = NULL;
    m_flags             = 0;
    m_priority          = -1;
    m_playcount         = 0;
    m_controlparameter  = NULL;
    m_channelgroup      = NULL;
    m_channelgroup_mem  = NULL;
    m_pendingsound      = NULL;
    m_envelopememory    = NULL;
    m_envelopedefmemory = NULL;

    return FMOD_OK;
} // 0x8039C8F8

/*
====================
EventLayer::release

Releases the layer's sounds and envelopes and frees the layer's memory unless its event
came from the instance pool.
====================
*/
FMOD_RESULT EventLayer::release() {
    releaseDSPNetwork();

    LinkedListNode *node;
    LinkedListNode *next;

    for ( node = m_soundhead.getNext(); node != &m_soundhead; node = next ) {
        EventSound *eventsound = static_cast<EventSound *>( node );

        next = node->getNext();
        eventsound->removeNode();
        eventsound->release( true );
    }

    for ( node = m_envelopehead.getNext(); node != &m_envelopehead; node = next ) {
        EventEnvelope *eventenvelope = static_cast<EventEnvelope *>( node );

        next = node->getNext();
        eventenvelope->removeNode();
        eventenvelope->release();
    }

    if ( !m_eventi->getMemPool() ) {
        if ( m_envelopememory ) {
#line 108
            FMOD_Memory_Free( m_envelopememory );
            m_envelopememory = NULL;
        }
        if ( m_envelopedefmemory ) {
#line 113
            FMOD_Memory_Free( m_envelopedefmemory );
            m_envelopedefmemory = NULL;
        }
    }

    if ( !m_eventi->getMemPool() ) {
#line 123
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x8039C930

/*
====================
EventLayer::dereferenceParentEnvelopes

Points each envelope at its parent envelope, found by the index its definition carries.
====================
*/
FMOD_RESULT EventLayer::dereferenceParentEnvelopes() {
    for ( LinkedListNode *node = m_envelopehead.getNext(); node != &m_envelopehead; node = node->getNext() ) {
        EventEnvelope *envelope = static_cast<EventEnvelope *>( node );

        if ( envelope->m_def->m_parentindex == -1 ) {
            envelope->m_parent           = NULL;
            envelope->m_def->m_parentdef = NULL;
        } else {
            LinkedListNode *parentnode = m_envelopehead.getNext();
            int             i          = 0;
            while ( parentnode != &m_envelopehead && i < envelope->m_def->m_parentindex ) {
                parentnode = parentnode->getNext();
                i++;
            }

            if ( parentnode != &m_envelopehead ) {
                EventEnvelope *parentenvelope = static_cast<EventEnvelope *>( parentnode );
                envelope->m_parent            = parentenvelope;
                envelope->m_def->m_parentdef  = parentenvelope->m_def;
            } else {
                return FMOD_ERR_FILE_BAD;
            }
        }
    }

    return FMOD_OK;
} // 0x8039CAB8

/*
====================
EventLayer::update

Applies the envelopes whose parameter changed to their DSP units: bypasses or restores a unit as its
definition asks, then sets the DSP parameter from the envelope's value.
====================
*/
FMOD_RESULT EventLayer::update() {
    FMOD_RESULT result;

    for ( LinkedListNode *envelopenode = m_envelopehead.getNext(); envelopenode != &m_envelopehead;
          envelopenode = envelopenode->getNext() ) {
        EventEnvelope *envelope = static_cast<EventEnvelope *>( envelopenode );
        float          v;

        if ( !envelope->m_dsp_unit || !( m_flags & EVENTLAYER_FLAG_0x02 ) ) {
            continue;
        }

        result = envelope->m_def->getValue( envelope->m_parameteri->m_value, &v );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( !envelope->m_parent ) {
            if ( envelope->m_def->m_flags & ENVELOPEDEF_BYPASS ) {
                if ( !( envelope->m_def->m_flags & ENVELOPEDEF_BYPASSED ) ) {
                    result = envelope->m_dsp_unit->setBypass( true );
                    if ( result != FMOD_OK ) {
                        return result;
                    }
                    if ( envelope->m_def->m_flags & ENVELOPEDEF_BYPASS ) {
                        envelope->m_def->m_flags |= ENVELOPEDEF_BYPASSED;
                    } else {
                        envelope->m_def->m_flags &= ~ENVELOPEDEF_BYPASSED;
                    }
                }
            } else if ( envelope->m_def->m_flags & ENVELOPEDEF_BYPASSED ) {
                result = envelope->m_dsp_unit->setBypass( false );
                if ( result != FMOD_OK ) {
                    return result;
                }
                if ( envelope->m_def->m_flags & ENVELOPEDEF_BYPASS ) {
                    envelope->m_def->m_flags |= ENVELOPEDEF_BYPASSED;
                } else {
                    envelope->m_def->m_flags &= ~ENVELOPEDEF_BYPASSED;
                }
            }
        }

        if ( ( envelope->m_flags & ENVELOPE_FLAG_UPDATE ) && ( envelope->m_def->m_flags & ENVELOPEDEF_0x04 ) ) {
            float         parammin, parammax;
            FMOD_DSP_TYPE dsptype;

            envelope->m_dsp_unit->getType( &dsptype );
            if ( dsptype == FMOD_DSP_TYPE_SFXREVERB && envelope->m_def->m_dsp_paramindex > SFXREVERB_LASTPARAM ) {
                envelope->m_flags &= ~ENVELOPE_FLAG_UPDATE;
                continue;
            }

            result = envelope->m_dsp_unit->getParameterInfo( envelope->m_def->m_dsp_paramindex, NULL, NULL, NULL, 0,
                                                             &parammin, &parammax );
            if ( result != FMOD_OK ) {
                return result;
            }

            EventEnvelopeDef *def = envelope->m_def;
            float sival   = 0.0f;
            bool  si_done = false;

            if ( def->m_mappingmethod == MAPPING_SICENTRIC ) {
                if ( dsptype == FMOD_DSP_TYPE_HIGHPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_ITLOWPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS_SIMPLE ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_PARAMEQ ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_SFXREVERB ) {
                    if ( def->m_dsp_paramindex == SFXREVERB_HFREFERENCE ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    } else if ( def->m_dsp_paramindex == SFXREVERB_LFREFERENCE ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( def->m_flags & ENVELOPEDEF_0x40 ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( 5.020597f * v );
                        sival = 0.39866656f * ( sival - 1.0f );
                        si_done = true;
                    }
                }
            }
            if ( !si_done ) {
                sival = parammin + v * ( parammax - parammin );
            }

            result = envelope->m_dsp_unit->setParameter( def->m_dsp_paramindex, sival );
            if ( result != FMOD_OK ) {
                return result;
            }
            envelope->m_flags &= ~ENVELOPE_FLAG_UPDATE;
        }
    }

    return FMOD_OK;
} // 0x8039CB4C

/*
====================
EventLayer::getLatestEndTime

The latest end time of the layer's sounds and the sound that ends last; a looping sound has none
(both are cleared).
====================
*/
FMOD_RESULT EventLayer::getLatestEndTime( unsigned long long *end_time, EventSound **sound ) {
    unsigned long long max_delay = 0;
    EventSound        *max_sound = NULL;

    for ( LinkedListNode *soundnode = m_soundhead.getNext(); soundnode != &m_soundhead;
          soundnode = soundnode->getNext() ) {
        EventSound *eventsound = static_cast<EventSound *>( soundnode );
        bool        looping;

        CHECK_RESULT( eventsound->isCurrentlyLooping( &looping ) );
        if ( looping ) {
            *end_time = 0;
            *sound    = NULL;
            return FMOD_OK;
        }

        unsigned long long temp_end;
        EventSound        *temp_sound;

        CHECK_RESULT( eventsound->getLatestEndTime( &temp_end, &temp_sound ) );
        if ( temp_end > max_delay ) {
            max_sound = temp_sound;
            max_delay = temp_end;
        }
    }

    *end_time = max_delay;
    *sound    = max_sound;

    return FMOD_OK;
} // 0x8039D094

/*
====================
EventLayer::rescheduleSounds
====================
*/
FMOD_RESULT EventLayer::rescheduleSounds() {
    if ( !( m_flags & EVENTLAYER_FLAG_0x02 ) ) {
        return FMOD_OK;
    }

    LinkedListNode *soundnode;

    for ( soundnode = m_soundhead.getNext(); soundnode != &m_soundhead; soundnode = soundnode->getNext() ) {
        CHECK_RESULT( static_cast<EventSound *>( soundnode )->setReschedulePending() );
    }

    for ( soundnode = m_soundhead.getNext(); soundnode != &m_soundhead; soundnode = soundnode->getNext() ) {
        CHECK_RESULT( static_cast<EventSound *>( soundnode )->rescheduleAll() );
    }

    return FMOD_OK;
} // 0x8039D174

/*
====================
EventLayer::clearPreviousPointers
====================
*/
FMOD_RESULT EventLayer::clearPreviousPointers( EventSound *sound ) {
    for ( LinkedListNode *soundnode = m_soundhead.getNext(); soundnode != &m_soundhead;
          soundnode = soundnode->getNext() ) {
        CHECK_RESULT( static_cast<EventSound *>( soundnode )->clearPreviousPointers( sound ) );
    }

    return FMOD_OK;
} // 0x8039D21C

/*
====================
EventLayer::otherSoundsPlaying
====================
*/
bool EventLayer::otherSoundsPlaying( EventSound *sound ) {
    if ( m_playcount == 0 ) {
        return false;
    }
    if ( m_playcount == 1 ) {
        return !sound->isSoundPlaying();
    }

    return true;
} // 0x8039D288

/*
====================
EventLayer::willTerminate
====================
*/
bool EventLayer::willTerminate() const {
    bool all_sounds_terminate = true;

    for ( LinkedListNode *node = m_soundhead.getNext(); node != &m_soundhead; node = node->getNext() ) {
        if ( !static_cast<EventSound *>( node )->willTerminate() ) {
            all_sounds_terminate = false;
            break;
        }
    }

    return all_sounds_terminate;
} // 0x8039D2D8

/*
====================
EventLayer::getEnvelope

The first envelope whose definition has any of the given flags set.
====================
*/
EventEnvelope *EventLayer::getEnvelope( unsigned int flags ) {
    for ( LinkedListNode *envelopenode = m_envelopehead.getNext(); envelopenode != &m_envelopehead;
          envelopenode = envelopenode->getNext() ) {
        EventEnvelope *eventenvelope = static_cast<EventEnvelope *>( envelopenode );

        if ( eventenvelope->m_def->m_flags & flags ) {
            return eventenvelope;
        }
    }

    return NULL;
} // 0x8039D344

/*
====================
EventLayer::createDSPNetwork

Gives the layer its own channel group (under the parent's) when it has the flag for one, and
creates the DSP unit of each of its envelopes: a unit of its own for an envelope on parameter 0,
the parent envelope's unit otherwise. Then sets each unit's parameter from the envelope's value.
====================
*/
FMOD_RESULT EventLayer::createDSPNetwork( ChannelGroup *parentchannelgroup ) {
    FMOD_RESULT     result;
    LinkedListNode *envelopenode;

    if ( m_flags & EVENTLAYER_FLAG_0x08 ) {
        return FMOD_OK;
    }

    if ( m_channelgroup ) {
        return FMOD_OK;
    }

    if ( !( m_flags & EVENTLAYER_FLAG_0x10 ) ) {
        m_channelgroup = parentchannelgroup;
        return FMOD_OK;
    }

    result = g_eventsystemi->m_system->createChannelGroup( NULL, &m_channelgroup_mem );
    if ( result != FMOD_OK ) {
        return result;
    }
    m_channelgroup = m_channelgroup_mem;

    result = parentchannelgroup->addGroup( m_channelgroup );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = m_channelgroup->setVolume( 1.0f );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = m_channelgroup->setPitch( 1.0f );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( envelopenode = m_envelopehead.getNext(); envelopenode != &m_envelopehead;
          envelopenode = envelopenode->getNext() ) {
        EventEnvelope *envelope = static_cast<EventEnvelope *>( envelopenode );

        bool parent_is_0x80  = getEnvelope( ENVELOPEDEF_0x80 ) ? ( envelope->m_parent == getEnvelope( ENVELOPEDEF_0x80 ) ) : false;
        bool parent_is_0x200 = getEnvelope( ENVELOPEDEF_0x200 ) ? ( envelope->m_parent == getEnvelope( ENVELOPEDEF_0x200 ) ) : false;
        bool parent_is_0x800 = getEnvelope( ENVELOPEDEF_0x800 ) ? ( envelope->m_parent == getEnvelope( ENVELOPEDEF_0x800 ) ) : false;

        if ( ( envelope->m_def->m_exflags & 1 ) && !parent_is_0x80 && !parent_is_0x200 && !parent_is_0x800
             && !envelope->m_dsp_unit ) {
            if ( envelope->m_def->m_dsp_paramindex == 0 ) {
                DSP *dsphead;

                result = m_channelgroup->getDSPHead( &dsphead );
                if ( result != FMOD_OK ) {
                    return result;
                }

                if ( dsphead ) {
                    result = g_eventsystemi->createDSPByName( envelope->m_def->m_dsp_name, &envelope->m_dsp_unit );
                    if ( result != FMOD_OK ) {
                        envelope->m_dsp_unit = NULL;
                        continue;
                    }

                    result = m_channelgroup->addDSP( envelope->m_dsp_unit, NULL );
                    if ( result != FMOD_OK ) {
                        return result;
                    }

                    result = envelope->m_dsp_unit->setActive( false );
                    if ( result != FMOD_OK ) {
                        return result;
                    }

                    if ( envelope->m_def->m_flags & ENVELOPEDEF_BYPASS ) {
                        envelope->m_def->m_flags &= ~ENVELOPEDEF_BYPASSED;
                    } else {
                        envelope->m_def->m_flags |= ENVELOPEDEF_BYPASSED;
                    }
                } else {
                    return FMOD_ERR_NEEDSSOFTWARE;
                }
            } else {
                envelope->m_dsp_unit = envelope->m_parent->m_dsp_unit;
            }

            if ( envelope->m_dsp_unit ) {
                float         parammin, parammax, v;
                FMOD_DSP_TYPE dsptype;

                envelope->m_dsp_unit->getType( &dsptype );
                if ( dsptype == FMOD_DSP_TYPE_SFXREVERB && envelope->m_def->m_dsp_paramindex > SFXREVERB_LASTPARAM ) {
                    continue;
                }

                result = envelope->m_def->getValue( envelope->m_parameteri->m_value, &v );
                if ( result != FMOD_OK ) {
                    return result;
                }

                result = envelope->m_dsp_unit->getParameterInfo( envelope->m_def->m_dsp_paramindex, NULL, NULL, NULL, 0,
                                                                 &parammin, &parammax );
                if ( result != FMOD_OK ) {
                    return result;
                }

                EventEnvelopeDef *def = envelope->m_def;
                float sival   = 0.0f;
                bool  si_done = false;

                if ( def->m_mappingmethod == MAPPING_SICENTRIC ) {
                    if ( dsptype == FMOD_DSP_TYPE_HIGHPASS ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( dsptype == FMOD_DSP_TYPE_ITLOWPASS ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS_SIMPLE ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( dsptype == FMOD_DSP_TYPE_PARAMEQ ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( dsptype == FMOD_DSP_TYPE_SFXREVERB ) {
                        if ( def->m_dsp_paramindex == SFXREVERB_HFREFERENCE ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        } else if ( def->m_dsp_paramindex == SFXREVERB_LFREFERENCE ) {
                            sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                            si_done = true;
                        }
                    } else if ( def->m_flags & ENVELOPEDEF_0x40 ) {
                        if ( def->m_dsp_paramindex == 0 ) {
                            sival = (float)exp( 5.020597f * v );
                            sival = 0.39866656f * ( sival - 1.0f );
                            si_done = true;
                        }
                    }
                }
                if ( !si_done ) {
                    sival = parammin + v * ( parammax - parammin );
                }

                result = envelope->m_dsp_unit->setParameter( def->m_dsp_paramindex, sival );
                if ( result != FMOD_OK ) {
                    return result;
                }
            }
        }
    }

    return FMOD_OK;
} // 0x8039D37C

/*
====================
EventLayer::releaseDSPNetwork

Releases the DSP units of the layer's own envelopes (not those shared with a parent) and its channel group.
====================
*/
FMOD_RESULT EventLayer::releaseDSPNetwork() {
    FMOD_RESULT result;

    if ( !m_channelgroup ) {
        return FMOD_OK;
    }

    for ( LinkedListNode *envelopenode = m_envelopehead.getNext(); envelopenode != &m_envelopehead;
          envelopenode = envelopenode->getNext() ) {
        EventEnvelope *envelope = static_cast<EventEnvelope *>( envelopenode );

        if ( envelope->m_dsp_unit && !envelope->m_parent ) {
            result = envelope->m_dsp_unit->release();
            if ( result != FMOD_OK ) {
                return result;
            }
        }
        envelope->m_dsp_unit = NULL;
    }

    if ( m_channelgroup_mem ) {
        result = m_channelgroup_mem->release();
        if ( result != FMOD_OK ) {
            return result;
        }
        m_channelgroup_mem = NULL;
    }
    m_channelgroup = NULL;

    return FMOD_OK;
} // 0x8039DAEC

/*
====================
EventLayer::startEffects

Sets every envelope's DSP parameter from its current value (on start) and activates or
deactivates the DSP units that an envelope on parameter 0 drives.
====================
*/
FMOD_RESULT EventLayer::startEffects( bool start ) {
    FMOD_RESULT     result;
    LinkedListNode *node;

    if ( m_flags & EVENTLAYER_FLAG_0x08 ) {
        return FMOD_OK;
    }

    for ( node = m_envelopehead.getNext(); node != &m_envelopehead; node = node->getNext() ) {
        EventEnvelope *envelope = static_cast<EventEnvelope *>( node );

        if ( start && envelope->m_dsp_unit ) {
            float         parammin, parammax, v;
            FMOD_DSP_TYPE dsptype;

            envelope->m_dsp_unit->getType( &dsptype );
            if ( dsptype == FMOD_DSP_TYPE_SFXREVERB && envelope->m_def->m_dsp_paramindex > SFXREVERB_LASTPARAM ) {
                continue;
            }

            result = envelope->m_def->getValue( envelope->m_parameteri->m_value, &v );
            if ( result != FMOD_OK ) {
                return result;
            }

            result = envelope->m_dsp_unit->getParameterInfo( envelope->m_def->m_dsp_paramindex, NULL, NULL, NULL, 0,
                                                             &parammin, &parammax );
            if ( result != FMOD_OK ) {
                return result;
            }

            EventEnvelopeDef *def = envelope->m_def;

            float sival    = 0.0f;
            bool  si_done  = false;

            if ( def->m_mappingmethod == MAPPING_SICENTRIC ) {
                if ( dsptype == FMOD_DSP_TYPE_HIGHPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_ITLOWPASS ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_LOWPASS_SIMPLE ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_PARAMEQ ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( dsptype == FMOD_DSP_TYPE_SFXREVERB ) {
                    if ( def->m_dsp_paramindex == SFXREVERB_HFREFERENCE ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    } else if ( def->m_dsp_paramindex == SFXREVERB_LFREFERENCE ) {
                        sival = (float)exp( LogF( parammin ) + v * ( LogF( parammax ) - LogF( parammin ) ) );
                        si_done = true;
                    }
                } else if ( def->m_flags & ENVELOPEDEF_0x40 ) {
                    if ( def->m_dsp_paramindex == 0 ) {
                        sival = (float)exp( 5.020597f * v );
                        sival = 0.39866656f * ( sival - 1.0f );
                        si_done = true;
                    }
                }
            }

            if ( !si_done ) {
                sival = parammin + v * ( parammax - parammin );
            }

            result = envelope->m_dsp_unit->setParameter( def->m_dsp_paramindex, sival );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        if ( envelope->m_def->m_dsp_paramindex == 0 && envelope->m_dsp_unit ) {
            result = envelope->m_dsp_unit->setActive( start );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    return FMOD_OK;
} // 0x8039DBB8

} // namespace FMOD
