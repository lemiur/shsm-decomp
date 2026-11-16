/*
 * FMOD::ReverbI, the internal reverb object (fmod_reverbi.cpp): one reverb with up to four instances,
 * each with its own reverb DSP, its properties and the per-channel send data.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_reverbi.obj; sibling.exe types ReverbI,
 * FMOD_REVERB_INSTANCE, FMOD_REVERB_CHANNELDATA), size 0x248.
 */
#ifndef _FMOD_REVERBI_H
#define _FMOD_REVERBI_H

#include "fmod.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class DSPI;
class DSPConnectionI;
class Reverb;
class SystemI;

// Whether the reverb takes its properties from the system's channels (physical) or only follows them
// (virtual) (Gormiti DWARF).
enum FMOD_REVERB_MODE {
    FMOD_REVERB_PHYSICAL = 1,
    FMOD_REVERB_VIRTUAL  = 2
};

// A reverb has FMOD_REVERB_MAXINSTANCES instances (the public API's instance numbers 0 to 3; the
// name is invented).
#define FMOD_REVERB_MAXINSTANCES 4

// What one channel sends into one instance of a reverb.
struct FMOD_REVERB_CHANNELDATA {
    FMOD_REVERB_CHANNELPROPERTIES mChanProps;       // 0x00
    DSPConnectionI               *mDSPConnection;   // 0x4C
    float                         mPresenceGain;    // 0x50
};

// One of a reverb's instances.
struct FMOD_REVERB_INSTANCE {
    DSPI                    *mDSP;          // 0x00, the reverb unit
    FMOD_REVERB_CHANNELDATA *mChannelData;  // 0x04, one entry per real channel of the system
    FMOD_REVERB_PROPERTIES   mProps;        // 0x08
};

class ReverbI : public LinkedListNode {
public:
    FMOD_REVERB_INSTANCE mInstance[FMOD_REVERB_MAXINSTANCES];   // 0x0C
    SystemI             *mSystem;                               // 0x21C
    void                *mUserData;                             // 0x220
    float                mGain;                                 // 0x224
    bool                 mDisableIfNoEnvironment;               // 0x228
    bool                 m3D;                                   // 0x229
    bool                 mActive;                               // 0x22A
    FMOD_REVERB_MODE     mMode;                                 // 0x22C
    FMOD_VECTOR          mPosition;                             // 0x230
    float                mMinRadius;                            // 0x23C
    float                mMaxRadius;                            // 0x240
    float                mMinMaxDistance;                       // 0x244

    ReverbI();

    // Sets the reverb up on system and gives its four instances their defaults.
    FMOD_RESULT init( SystemI *system, bool is_3d, FMOD_REVERB_MODE mode );
    FMOD_RESULT createDSP( int instance );
    FMOD_RESULT getChanProperties( int instance, int index, FMOD_REVERB_CHANNELPROPERTIES *props,
                                   DSPConnectionI **connection );
    FMOD_RESULT setChanProperties( int instance, int index, const FMOD_REVERB_CHANNELPROPERTIES *props,
                                   DSPConnectionI *connection );
    FMOD_RESULT resetChanProperties( int instance, int index );
    FMOD_RESULT resetConnectionPointer( int instance, int index );
    FMOD_RESULT setPresenceGain( int instance, int index, float gain );

    // Called by the public FMOD::Reverb methods (fmod_reverb.cpp).
    static FMOD_RESULT validate( Reverb *reverb, ReverbI **reverbi );
    FMOD_RESULT release( bool freethis );
    FMOD_RESULT set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance );
    FMOD_RESULT get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance );
    FMOD_RESULT setProperties( const FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT getProperties( FMOD_REVERB_PROPERTIES *prop );
    FMOD_RESULT setActive( bool active );
    FMOD_RESULT getActive( bool *active );

    // Unlinks and releases the reverb unit of one instance. No copy in retail: its body is inlined into
    // release (name and use: Gormiti link map, UNUSED).
    FMOD_RESULT releaseDSP( int instance );
};

} // namespace FMOD

#endif /* _FMOD_REVERBI_H */
