/*
 * fmod_compositionconditions.cpp: the Event system's music composition conditions
 * (FMOD::RefCountCondition, ParameterCondition, MusicStateCondition, ConstantCondition). Allocation tags
 * carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_compositionconditions.obj)

#include <math.h>

#include <fmod/fmod_compositionconditions.h>
#include <fmod/fmod_compositioncore.h>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

// Two parameter values are equal when they differ by less than this.
#define FLOAT_EQUAL( _a, _b ) ( fabsf( ( _a ) - ( _b ) ) < 0.00001 )

/*
====================
RefCountCondition::release

Frees the condition.
====================
*/
void RefCountCondition::release() {
#line 28
    FMOD_Memory_Free( this );
} // 0x8037BF14

/*
====================
RefCountCondition::clone

A condition is shared, not copied: the clone is this with another reference.
====================
*/
AbstractCondition *RefCountCondition::clone() const {
    RefCountCondition *p = const_cast<RefCountCondition *>( this );

    p->m_ref++;

    return p;
} // 0x8037BF34

/*
====================
RefCountCondition::destroy

Drops a reference; the last one releases the condition.
====================
*/
void RefCountCondition::destroy() {
    m_ref--;
    if ( m_ref == 0 ) {
        release();
    }
} // 0x8037BF44

/*
====================
ParameterCondition::ParameterCondition
====================
*/
ParameterCondition::ParameterCondition() {
    m_first     = 0.0f;
    m_second    = 0.0f;
    m_parameter = 0;
    m_negated   = false;
} // 0x8037BF68

/*
====================
ParameterCondition::init

Selects the parameter to test.
====================
*/
FMOD_RESULT ParameterCondition::init( unsigned int parameter ) {
    m_parameter = parameter;

    return FMOD_OK;
} // 0x8037BF98

/*
====================
ParameterCondition::evaluate

Tests the parameter's value against m_first (and m_second); fails when the parameter does not exist.
====================
*/
FMOD_RESULT ParameterCondition::evaluate( const MusicState &state, bool &result ) const {
    Parameter *parameter = ParameterRepository::repository()->getParameter( m_parameter );
    if ( !parameter ) {
        return FMOD_ERR_INTERNAL;
    }

    float value = parameter->getValue();

    switch ( m_test ) {
        case IsEqual:
            result = FLOAT_EQUAL( value, m_first );
            break;
        case IsLess:
            result = value < m_first;
            break;
        case IsLessInclusive:
            result = value < m_first || FLOAT_EQUAL( value, m_first );
            break;
        case IsGreater:
            result = value > m_first;
            break;
        case IsGreaterInclusive:
            result = value > m_first || FLOAT_EQUAL( value, m_first );
            break;
        case IsBetween:
            result = value > m_first && value < m_second;
            break;
        case IsBetweenInclusive:
            result = ( value > m_first && value < m_second ) || FLOAT_EQUAL( value, m_first ) || FLOAT_EQUAL( value, m_second );
            break;
    }

    if ( m_negated ) {
        result = !result;
    }

    return FMOD_OK;
} // 0x8037BFA4

/*
====================
MusicStateCondition::MusicStateCondition
====================
*/
MusicStateCondition::MusicStateCondition() {
    m_target_theme = 0;
    m_target_cue   = 0;
} // 0x8037C1F4

/*
====================
MusicStateCondition::evaluate

Tests the music state: its top theme, a cue, or that no theme is playing.
====================
*/
FMOD_RESULT MusicStateCondition::evaluate( const MusicState &state, bool &result ) const {
    switch ( m_test ) {
        case IsTargetTheme:
            result = state.getThemeStackTop() == m_target_theme;
            break;
        case IsCueActive:
            result = state.isCueActive( m_target_cue );
            break;
        case IsStopping:
            result = state.getThemeStackTop() == 0;
            break;
    }

    return FMOD_OK;
} // 0x8037C218

/*
====================
ConstantCondition::ConstantCondition

A condition that is true.
====================
*/
ConstantCondition::ConstantCondition() {
    m_value = true;
} // 0x8037C2DC

/*
====================
ConstantCondition::evaluate
====================
*/
FMOD_RESULT ConstantCondition::evaluate( const MusicState &state, bool &result ) const {
    result = m_value;

    return FMOD_OK;
} // 0x8037C2F8

} // namespace FMOD
