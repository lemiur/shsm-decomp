/*
 * FMOD::Plugin, FMOD::DSPI and FMOD::DSPFilter: the DSP unit classes (fmod_dspi.cpp). Only what the
 * emulated channel uses is declared with names: the layouts and the DSPI slot list follow the
 * retail vtable lbl_8044EA08 (DSPFilter) and the sibling build.
 * Layout: Gormiti DWARF (sibling.exe types DSPI) for Plugin, DSPI, DSPFilter and
 * FMOD_DSP_DESCRIPTION_EX.
 */
#ifndef _FMOD_DSPI_H
#define _FMOD_DSPI_H

#include "fmod.h"
#include "fmod_dsp.h"
#include "fmod_linkedlist.h"
#include "fmod_memory.h"

namespace FMOD {

class SystemI;
class DSPConnectionI;
class DSP;
class DSPI;
class System;
struct Global;

// The DSP description FMOD builds a unit from: the public description plus the unit's list node.
class FMOD_DSP_DESCRIPTION_EX : public FMOD_DSP_DESCRIPTION, public LinkedListNode {
public:
    FMOD_SOUND_FORMAT   mFormat;                // 0x68
    FMOD_DSP_TYPE       mType;                  // 0x6C
    int                 mSize;                  // 0x70
    FMOD_DSP_CATEGORY   mCategory;              // 0x74
    int                 mResamplerBlockLength;  // 0x78
    unsigned int        mHandle;                // 0x7C
    DSPI               *mDSPSoundCard;          // 0x80
    void               *getmemoryused;          // 0x84, FMOD_RESULT (*)( FMOD_DSP_STATE *, MemoryTracker * )
    void               *update;                 // 0x88, FMOD_RESULT (*)( FMOD_DSP_STATE * )
};

// Base of the systems' loadable units; it owns the object's vtable pointer (0x18).
class Plugin : public SortedLinkedListNode {
public:
    SystemI *mSystem;                           // 0x10
    Global  *mGlobal;                           // 0x14

    Plugin() {
        mGlobal = lbl_80529020;
    }

    virtual FMOD_RESULT release();              // slot 0
};

// DSPI::mFlags bits (FINISHED named after DSPWaveTable::getFinished and setFinished; ACTIVE/BYPASS
// named after DSP::setActive and DSP::setBypass; 0x10 and 0x100 are unnamed).
#define DSPI_FLAG_ACTIVE   0x00000002
#define DSPI_FLAG_BYPASS   0x00000004
#define DSPI_FLAG_FINISHED 0x00000008
#define DSPI_FLAG_0x10     0x00000010
#define DSPI_FLAG_0x100    0x00000100

class DSPI : public Plugin, public FMOD_DSP_STATE {
public:
    LinkedListNode          mInputHead;         // 0x28
    LinkedListNode          mOutputHead;        // 0x34
    int                     mNumInputs;         // 0x40
    int                     mNumOutputs;        // 0x44
    float                  *mOutputBuffer;      // 0x48
    int                     mTargetFrequency;   // 0x4C
    short                   mTreeLevel;         // 0x50
    short                   mLastChannels;      // 0x52
    unsigned short          mCPUUsage;          // 0x54
    unsigned short          mCPUUsageTemp;      // 0x56
    FMOD_DSP_DESCRIPTION_EX mDescription;       // 0x58
    float                   mDefaultVolume;     // 0xE4
    float                   mDefaultFrequency;  // 0xE8
    float                   mDefaultPan;        // 0xEC
    int                     mDefaultPriority;   // 0xF0
    float                  *mBuffer;            // 0xF4
    int                     mBufferChannels;    // 0xF8
    unsigned int            mDSPTick;           // 0xFC
    unsigned int            mFlags;             // 0x100
    char                    mWantsToFinishMem;  // 0x104

    DSPI();                                     // 0x803678F8

    // The slots after Plugin::release, in retail order (vtable lbl_8044ED18, slots 1 to 21).
    virtual FMOD_RESULT execute( void *inbuffer, void **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick );
    virtual FMOD_RESULT execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick );
    virtual FMOD_RESULT alloc( FMOD_DSP_DESCRIPTION_EX *description );
    virtual FMOD_RESULT setPosition( unsigned int position, bool processinputs );
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT getSystemObject( System **system );
    virtual FMOD_RESULT addInput( DSPI *target, DSPConnectionI **connection );
    virtual FMOD_RESULT remove();
    virtual FMOD_RESULT reset();
    virtual FMOD_RESULT setParameter( int index, float value );
    virtual FMOD_RESULT getParameter( int index, float *value, char *valuestr, int valuestrlen );
    virtual FMOD_RESULT getNumParameters( int *numparams );
    virtual FMOD_RESULT getParameterInfo( int index, char *name, char *label, char *description,
                                          int descriptionlen, float *min, float *max );
    virtual FMOD_RESULT showConfigDialog( void *hwnd, bool show );
    virtual FMOD_RESULT getInfo( char *name, unsigned int *version, int *channels, int *configwidth,
                                 int *configheight );
    virtual FMOD_RESULT getType( FMOD_DSP_TYPE *type );
    virtual FMOD_RESULT setDefaults( float frequency, float volume, float pan, int priority );
    virtual FMOD_RESULT getDefaults( float *frequency, float *volume, float *pan, int *priority );
    virtual FMOD_RESULT stopBuffering();
    virtual FMOD_RESULT setTargetFrequency( int frequency );
    virtual FMOD_RESULT getTargetFrequency( int *frequency );

    FMOD_RESULT addInputQueued( DSPI *target, bool checkcircular, DSPConnectionI *connection,
                                DSPConnectionI **connection_out );
    FMOD_RESULT disconnectFrom( DSPI *target, DSPConnectionI *connection );
    FMOD_RESULT disconnectAll( bool inputs, bool outputs );
    FMOD_RESULT getOutput( int index, DSPI **outputdsp, DSPConnectionI **output, bool protect );
    FMOD_RESULT insertInputBetween( DSPI *dsptoinsert, int inputindex, DSPConnectionI **connection_out );
    FMOD_RESULT updateTreeLevel( int level );
    static FMOD_RESULT validate( DSP *dsp, DSPI **dspi );  // 0x80365EA4

    // Inline helpers behind DSP::setActive and DSP::setBypass (mFlags bits 0x2 and 0x4). Retail has
    // no out-of-line copy; the names are invented after the public setters they serve.
    void setActive( bool active ) {
        if ( active ) {
            mFlags |= 0x2;
        } else {
            mFlags &= ~0x2;
        }
    }
    void setBypass( bool bypass ) {
        if ( bypass ) {
            mFlags |= 0x4;
        } else {
            mFlags &= ~0x4;
        }
    }

    // Converts length samples between sample formats (fmod_dsp_convert.cpp).
    static FMOD_RESULT convert( void *outbuffer, void *inbuffer, FMOD_SOUND_FORMAT outformat,
                                FMOD_SOUND_FORMAT informat, unsigned int length, int outchannels,
                                int inchannels, float volume );
};

class DSPFilter : public DSPI {
public:
    float        *mHistoryBuffer;               // 0x108
    unsigned int  mHistoryPosition;             // 0x10C
    unsigned int  mHistoryLength;               // 0x110
    int           mBufferChannels;              // 0x114

    DSPFilter() {}                              // sets the vtable pointer (lbl_8044EA08)

    // Overrides of the slots above (vtable lbl_8044EA08).
    virtual FMOD_RESULT execute( float *inbuffer, float **outbuffer, unsigned int *length, int inchannels,
                                 int *outchannels, FMOD_SPEAKERMODE speakermode, unsigned int tick );
    virtual FMOD_RESULT release( bool freethis );
    virtual FMOD_RESULT stopBuffering();

    FMOD_RESULT startBuffering( unsigned int length );
    FMOD_RESULT getHistoryBuffer( float **buffer, unsigned int *position, unsigned int *length );
};

} // namespace FMOD

#endif /* _FMOD_DSPI_H */
