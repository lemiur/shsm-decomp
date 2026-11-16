/*
 * FMOD::EventEnvelope, EventEnvelopeDef and EventEnvelopePoint: an event project's envelope (a curve
 * that maps an event parameter to a DSP parameter), its definition shared between instances, and the
 * points of the curve. Layout: Gormiti DWARF (sibling.exe types EventEnvelope, EventEnvelopeDef,
 * EventEnvelopePoint, FMOD_POINT). Flag, enum and constant names are invented.
 */
#ifndef _FMOD_EVENTENVELOPE_H
#define _FMOD_EVENTENVELOPE_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

// A point of a 2D curve (global scope, as the mangled names of FMOD::FMOD_Bezier show).
class FMOD_POINT {
public:
    float x;    // 0x00
    float y;    // 0x04
};

namespace FMOD {

class DSP;
class EventParameterDef;
class EventParameterI;
class System;

// Name invented from the enumerators: how a definition maps its parameter onto the curve.
enum MappingMethod {
    MAPPING_VANILLA,
    MAPPING_SICENTRIC
};

// EventEnvelopePoint::m_flags: how the curve runs from the previous point to this one.
#define ENVELOPEPOINT_BEZIER    0x00000001
#define ENVELOPEPOINT_LINEAR    0x00000002
#define ENVELOPEPOINT_LOG       0x00000004
#define ENVELOPEPOINT_CUBIC     0x00000008
// The point is inside the definition's point memory block rather than allocated on its own.
#define ENVELOPEPOINT_POOLED    0x00000010

// EventEnvelopeDef::m_flags: the last value computed is cached in m_last_inval and m_last_outval.
#define ENVELOPEDEF_CACHED      0x00004000
// ---- added for fmod_eventlayer (names invented from use) ----
// The envelope's value is mapped onto the DSP parameter by an exponential, volume-like curve (parameter 0).
// EventEnvelopeDef::m_flags bits 0x01 and 0x02: the definition asks for its DSP unit to be bypassed / the unit is bypassed now.
#define ENVELOPEDEF_BYPASS      0x00000001
#define ENVELOPEDEF_BYPASSED    0x00000002
#define ENVELOPEDEF_0x04        0x00000004  // update applies the envelope to its DSP parameter only with it
#define ENVELOPEDEF_0x40        0x00000040
#define ENVELOPEDEF_0x80        0x00000080  // createDSPNetwork: the envelopes whose parent is the layer's one with it get no DSP unit
#define ENVELOPEDEF_0x200       0x00000200  // as 0x80
#define ENVELOPEDEF_0x800       0x00000800  // as 0x80

// A point of an envelope curve; linked into its definition's list in order of x.
class EventEnvelopePoint : public LinkedListNode {
public:
    float        m_x;       // 0x0C, the parameter value
    float        m_y;       // 0x10, the DSP parameter value, 0 to 1
    unsigned int m_flags;   // 0x14, ENVELOPEPOINT_*

    EventEnvelopePoint();
};

class EventEnvelopeDef {
public:
    float              m_last_inval;        // 0x00
    float              m_last_outval;       // 0x04
    unsigned int       m_flags;             // 0x08, ENVELOPEDEF_*
    unsigned int       m_exflags;           // 0x0C
    int                m_refcnt;            // 0x10, number of envelopes using this definition
    char              *m_dsp_name;          // 0x14
    int                m_dsp_paramindex;    // 0x18
    int                m_parentindex;       // 0x1C
    EventEnvelopeDef  *m_parentdef;         // 0x20
    EventParameterDef *m_parameterdef;      // 0x24
    LinkedListNode     m_pointhead;         // 0x28, head of the list of points
    EventEnvelopePoint *m_pointmemory;      // 0x34, block holding the points read from the project
    MappingMethod      m_mappingmethod;     // 0x38

    EventEnvelopeDef();
    FMOD_RESULT init( System *system );
    // Frees the DSP name, the points and the point memory.
    FMOD_RESULT release();
    // The curve's value at inval; the last value computed is cached.
    FMOD_RESULT getValue( float inval, float *outval );
};

// An envelope of an event instance; drives one parameter of a DSP unit.
// EventEnvelope::m_flags: the envelope's value changed and its DSP parameter is to be set again (name invented).
#define ENVELOPE_FLAG_UPDATE    0x0001

class EventEnvelope : public LinkedListNode {
public:
    EventEnvelopeDef *m_def;            // 0x0C
    EventEnvelope    *m_parent;         // 0x10
    DSP              *m_dsp_unit;       // 0x14
    EventParameterI  *m_parameteri;     // 0x18
    unsigned short    m_flags;          // 0x1C

    EventEnvelope();
    FMOD_RESULT init( EventEnvelopeDef *def );
    // Drops a reference to the definition (freeing it with the last one) and releases the DSP unit.
    FMOD_RESULT release();
};

// Curve helpers of the Event system (their unit is not split yet).
// The point of the cubic Bezier curve through points (4 control points) at t, 0 to 1.
void FMOD_Bezier( FMOD_POINT *result, FMOD_POINT *points, float t );
// A power-law shaping of x; name and parameter names invented.
float envelopePowerLaw( float a, float b, float c, float x );

} // namespace FMOD

#endif /* _FMOD_EVENTENVELOPE_H */
