/*
 * fmod_eventenvelope.cpp: FMOD::EventEnvelope, EventEnvelopeDef and EventEnvelopePoint, the envelopes of
 * an event project. Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD
 * rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_eventenvelope.obj)

#include <math.h>

#include <fmod/fmod.hpp>
#include <fmod/fmod_eventenvelope.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

/*
====================
EventEnvelopePoint::EventEnvelopePoint
====================
*/
EventEnvelopePoint::EventEnvelopePoint() {
    m_x     = 0.0f;
    m_y     = 0.0f;
    m_flags = ENVELOPEPOINT_BEZIER;
} // 0x803861D8

/*
====================
EventEnvelope::EventEnvelope
====================
*/
EventEnvelope::EventEnvelope() {
    m_def        = NULL;
    m_parent     = NULL;
    m_dsp_unit   = NULL;
    m_parameteri = NULL;
    m_flags      = 0;
} // 0x80386200

/*
====================
EventEnvelope::init

Binds the envelope to its definition.
====================
*/
FMOD_RESULT EventEnvelope::init( EventEnvelopeDef *def ) {
    m_def        = def;
    m_parent     = NULL;
    m_dsp_unit   = NULL;
    m_parameteri = NULL;
    m_flags      = 0;

    return FMOD_OK;
} // 0x80386228

/*
====================
EventEnvelope::release

Drops the reference to the definition, freeing it with the last one, and releases the DSP unit unless it
belongs to a parent envelope.

The out-of-line EventEnvelopeDef::release is expected: Gormiti's map has it UNUSED.
====================
*/
FMOD_RESULT EventEnvelope::release() {
    FMOD_RESULT result;

    if ( m_def ) {
        if ( --m_def->m_refcnt == 0 ) {
            result = m_def->release();
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    if ( m_parent == NULL && m_dsp_unit ) {
        m_dsp_unit->release();
    }
    m_dsp_unit = NULL;

    return FMOD_OK;
} // 0x80386248

/*
====================
EventEnvelopeDef::release

Frees the DSP name, the points that were allocated one by one and the point memory. Defined further on
in the original file and inlined into EventEnvelope::release; no copy in retail.
The next link is read through the const point: a const-qualified read is its own expression to the
common-subexpression pass, so removeNode loads the link again, as in retail (Gormiti's revision of
this loop shares the load).
====================
*/
FMOD_RESULT EventEnvelopeDef::release() {
    if ( m_dsp_name ) {
#line 187
        FMOD_Memory_Free( m_dsp_name );
    }

    LinkedListNode *next;
    for ( LinkedListNode *node = m_pointhead.getNext(); node != &m_pointhead; node = next ) {
        const EventEnvelopePoint *point = static_cast<const EventEnvelopePoint *>( node );

        next = point->getNext();

        node->removeNode();
        if ( !( point->m_flags & ENVELOPEPOINT_POOLED ) ) {
#line 202
            FMOD_Memory_Free( node );
        }
    }

    if ( m_pointmemory ) {
#line 208
        FMOD_Memory_Free( m_pointmemory );
    }

    return FMOD_OK;
} // no copy in retail

/*
====================
EventEnvelopeDef::EventEnvelopeDef
====================
*/
EventEnvelopeDef::EventEnvelopeDef() {
    m_flags          = 4;
    m_exflags        = 0;
    m_refcnt         = 0;
    m_dsp_name       = NULL;
    m_dsp_paramindex = -1;
    m_parentindex    = -1;
    m_parentdef      = NULL;
    m_parameterdef   = NULL;
    m_pointmemory    = NULL;
    m_mappingmethod  = MAPPING_VANILLA;
    m_last_inval     = 0.0f;
    m_last_outval    = 0.0f;
} // 0x80386378

/*
====================
EventEnvelopeDef::init

Resets the definition to its defaults.
====================
*/
FMOD_RESULT EventEnvelopeDef::init( System *system ) {
    m_flags          = 4;
    m_exflags        = 0;
    m_refcnt         = 0;
    m_dsp_name       = NULL;
    m_dsp_paramindex = -1;
    m_parentindex    = -1;
    m_parentdef      = NULL;
    m_parameterdef   = NULL;
    m_pointmemory    = NULL;
    m_mappingmethod  = MAPPING_VANILLA;
    m_last_inval     = 0.0f;
    m_last_outval    = 0.0f;

    return FMOD_OK;
} // 0x803863CC

/*
====================
EventEnvelopeDef::getValue

Evaluates the curve at inval: between the two points around it, along the curve the later point asks
for. Before the first point the value is the first point's, after the last point the last one's.
====================
*/
FMOD_RESULT EventEnvelopeDef::getValue( float inval, float *outval ) {
    LinkedListNode     *pointnode;
    int                 numpoints;
    EventEnvelopePoint *lastpoint = NULL;

    if ( ( m_flags & ENVELOPEDEF_CACHED ) && inval == m_last_inval ) {
        *outval = m_last_outval;
        return FMOD_OK;
    }

    numpoints = 0;
    for ( pointnode = m_pointhead.getNext(); pointnode != &m_pointhead; pointnode = pointnode->getNext() ) {
        numpoints++;
    }
    if ( numpoints == 1 ) {
        *outval = static_cast<EventEnvelopePoint *>( m_pointhead.getNext() )->m_y;
        return FMOD_OK;
    }

    for ( pointnode = m_pointhead.getNext(); pointnode != &m_pointhead; pointnode = pointnode->getNext() ) {
        EventEnvelopePoint *point = static_cast<EventEnvelopePoint *>( pointnode );

        if ( point->m_x > inval && lastpoint ) {
            if ( point->m_flags & ENVELOPEPOINT_BEZIER ) {
                FMOD_POINT bez[4];
                FMOD_POINT p;
                float      midx;

                bez[0].x = lastpoint->m_x;
                bez[0].y = lastpoint->m_y;
                bez[3].x = point->m_x;
                bez[3].y = point->m_y;
                midx     = lastpoint->m_x + 0.5f * ( point->m_x - lastpoint->m_x );
                bez[1].x = midx;
                bez[1].y = lastpoint->m_y;
                bez[2].x = midx;
                bez[2].y = point->m_y;
                FMOD_Bezier( &p, bez, ( inval - lastpoint->m_x ) / ( point->m_x - lastpoint->m_x ) );
                *outval = p.y;
            } else if ( point->m_flags & ENVELOPEPOINT_LINEAR ) {
                float x = ( inval - lastpoint->m_x ) / ( point->m_x - lastpoint->m_x );
                float y = lastpoint->m_y + x * ( point->m_y - lastpoint->m_y );

                *outval = y;
            } else if ( point->m_flags & ENVELOPEPOINT_LOG ) {
                float x = ( inval - lastpoint->m_x ) / ( point->m_x - lastpoint->m_x );
                float ymin;
                float yrange;

                if ( point->m_y > lastpoint->m_y ) {
                    ymin   = lastpoint->m_y;
                    yrange = point->m_y - lastpoint->m_y;
                } else {
                    ymin   = point->m_y;
                    yrange = lastpoint->m_y - point->m_y;
                    x      = 1.0f - x;
                }

                float scalefactor = 1024.0f;
                float c           = (float)log( 1.0f + scalefactor ) / (float)log( 2.0 );
                float d           = -1.0f;
                float y           = ymin + yrange * envelopePowerLaw( 2.0f, c, d, x ) / scalefactor;

                *outval = y;
            } else if ( point->m_flags & ENVELOPEPOINT_CUBIC ) {
                float x = ( inval - lastpoint->m_x ) / ( point->m_x - lastpoint->m_x );
                float ymin;
                float yrange;

                if ( point->m_y > lastpoint->m_y ) {
                    ymin   = lastpoint->m_y;
                    yrange = point->m_y - lastpoint->m_y;
                    x      = 1.0f - x;
                } else {
                    ymin   = point->m_y;
                    yrange = lastpoint->m_y - point->m_y;
                }

                float xt = x - 0.5f;
                float y  = 0.5f - 4.0f * ( xt * xt * xt );

                *outval = ymin + yrange * y;
            } else {
                *outval = 0.0f;
            }

            m_last_inval  = inval;
            m_last_outval = *outval;
            m_flags |= ENVELOPEDEF_CACHED;
            return FMOD_OK;
        }

        lastpoint = point;
    }

    *outval = lastpoint ? lastpoint->m_y : 0.0f;

    m_last_inval  = inval;
    m_last_outval = *outval;
    m_flags |= ENVELOPEDEF_CACHED;
    return FMOD_OK;
} // 0x80386414

} // namespace FMOD
