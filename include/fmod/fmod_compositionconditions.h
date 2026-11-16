/*
 * The Event system's music composition conditions: a condition is evaluated against the music state and
 * says whether a transition or segment applies. AbstractCondition is the interface, RefCountCondition
 * adds a reference count, and the three kinds test a parameter, the music state, or are a constant.
 * Virtuals: retail vtables __vt__Q24FMOD17ConstantCondition, __vt__Q24FMOD19MusicStateCondition and
 * __vt__Q24FMOD18ParameterCondition + Gormiti link map; layout: Gormiti DWARF
 * (sibling.exe types RefCountCondition, ParameterCondition, MusicStateCondition).
 */
#ifndef _FMOD_COMPOSITIONCONDITIONS_H
#define _FMOD_COMPOSITIONCONDITIONS_H

#include <fmod/fmod.h>

namespace FMOD {

class MusicState;

class AbstractCondition {
public:
    // vtable pointer at 0x00: the first virtual below
    virtual ~AbstractCondition() {}                                                 // slot 0x08
    virtual int type() const = 0;                                                   // slot 0x0C
    virtual FMOD_RESULT evaluate( const MusicState &state, bool &result ) const = 0; // slot 0x10
    virtual AbstractCondition *clone() const = 0;                                   // slot 0x14
    virtual void destroy() = 0;                                                     // slot 0x18
};

class RefCountCondition : public AbstractCondition {
public:
    unsigned int m_ref;                             // 0x04

    RefCountCondition() : m_ref( 1 ) {}

    virtual AbstractCondition *clone() const;       // slot 0x14, override: takes a reference and returns this
    virtual void destroy();                         // slot 0x18, override: drops a reference
    virtual void release();                         // slot 0x1C, new: frees the condition
};

class ParameterCondition : public RefCountCondition {
public:
    enum Test {
        IsEqual,
        IsGreater,
        IsGreaterInclusive,
        IsLess,
        IsLessInclusive,
        IsBetween,
        IsBetweenInclusive
    };

    float        m_first;       // 0x08, the value compared to, or the lower bound
    float        m_second;      // 0x0C, the upper bound of the Is-Between tests
    unsigned int m_parameter;   // 0x10, id of the parameter tested
    Test         m_test;        // 0x14
    bool         m_negated;     // 0x18, the result is inverted

    ParameterCondition();
    virtual int type() const { return 1; }          // slot 0x0C, override
    virtual FMOD_RESULT evaluate( const MusicState &state, bool &result ) const; // slot 0x10, override

    FMOD_RESULT init( unsigned int parameter );
};

class MusicStateCondition : public RefCountCondition {
public:
    enum Test {
        IsTargetTheme,
        IsCueActive,
        IsStopping
    };

    Test         m_test;            // 0x08
    unsigned int m_target_theme;    // 0x0C
    unsigned int m_target_cue;      // 0x10

    MusicStateCondition();
    virtual int type() const { return 2; }          // slot 0x0C, override
    virtual FMOD_RESULT evaluate( const MusicState &state, bool &result ) const; // slot 0x10, override
};

class ConstantCondition : public RefCountCondition {
public:
    bool m_value;                   // 0x08, name invented (no Gormiti layout)

    ConstantCondition();
    virtual int type() const { return 3; }          // slot 0x0C, override
    virtual FMOD_RESULT evaluate( const MusicState &state, bool &result ) const; // slot 0x10, override
};

} // namespace FMOD

#endif /* _FMOD_COMPOSITIONCONDITIONS_H */
