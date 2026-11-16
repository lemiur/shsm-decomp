/*
 * FMOD::Output, the base of the sound outputs (fmod_output.cpp), and the descriptions an output
 * registers itself with. Callbacks of the descriptions are left opaque, with their signature in the
 * comment.
 * Virtuals: retail vtable lbl_8044FB80 + Gormiti link map (__vt__Q24FMOD6Output)
 * Layout: Gormiti DWARF (sibling.exe types Output, FMOD_OUTPUT_DESCRIPTION_EX), size 0xF0
 */
#ifndef _FMOD_OUTPUT_H
#define _FMOD_OUTPUT_H

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_linkedlist.h>

// The state an output's callbacks receive.
typedef struct FMOD_OUTPUT_STATE FMOD_OUTPUT_STATE;

// The callbacks of an output (FMOD Ex public API signatures).
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_READFROMMIXERCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                                      void *buffer, unsigned int length );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_CLOSECALLBACK)( FMOD_OUTPUT_STATE *output_state );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_GETPOSITIONCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                                    unsigned int *pcm );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_LOCKCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                            unsigned int offset, unsigned int length,
                                                            void **ptr1, void **ptr2,
                                                            unsigned int *len1, unsigned int *len2 );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_UNLOCKCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                              void *ptr1, void *ptr2,
                                                              unsigned int len1, unsigned int len2 );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_POSTMIXCALLBACK)( FMOD_OUTPUT_STATE *output_state );
typedef FMOD_RESULT (F_CALLBACK *FMOD_OUTPUT_REVERB_SETPROPERTIESCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                                          const FMOD_REVERB_PROPERTIES *prop );

struct FMOD_OUTPUT_STATE {
    void *plugindata;
    FMOD_OUTPUT_READFROMMIXERCALLBACK readfrommixer;
};

// The callback of an output that tells how many channels a sample of this mode and format may have.
typedef int (F_CALLBACK *FMOD_OUTPUT_GETSAMPLEMAXCHANNELSCALLBACK)( FMOD_OUTPUT_STATE *output_state,
                                                                  FMOD_MODE mode,
                                                                  FMOD_SOUND_FORMAT format );

// The public description of an output plugin.
typedef struct FMOD_OUTPUT_DESCRIPTION {
    const char  *name;
    unsigned int version;
    int          polling;
    void        *getnumdrivers;         // 0x0C, FMOD_RESULT (*)( FMOD_OUTPUT_STATE *, int * )
    void        *getdrivername;         // 0x10
    void        *getdrivercaps;         // 0x14
    void        *init;                  // 0x18
    FMOD_OUTPUT_CLOSECALLBACK       close;          // 0x1C
    void        *update;                // 0x20
    void        *gethandle;             // 0x24
    FMOD_OUTPUT_GETPOSITIONCALLBACK getposition;    // 0x28
    FMOD_OUTPUT_LOCKCALLBACK        lock;           // 0x2C
    FMOD_OUTPUT_UNLOCKCALLBACK      unlock;         // 0x30
} FMOD_OUTPUT_DESCRIPTION;

namespace FMOD {

class ChannelGroupI;
class ChannelPool;
class ChannelReal;
class SystemI;

// The description FMOD builds an output from: the public description plus the output's list node.
class FMOD_OUTPUT_DESCRIPTION_EX : public FMOD_OUTPUT_DESCRIPTION, public LinkedListNode {
public:
    FMOD_OUTPUTTYPE mType;                      // 0x40
    int             mSize;                      // 0x44
    void           *mModule;                    // 0x48, FMOD_OS_LIBRARY *
    unsigned int    mHandle;                    // 0x4C
    FMOD_OUTPUT_GETSAMPLEMAXCHANNELSCALLBACK getsamplemaxchannels; // 0x50
    void           *getdriverinfo;              // 0x54
    void           *getdrivercapsex;            // 0x58
    void           *getdrivercapsex2;           // 0x5C
    void           *initex;                     // 0x60
    void           *start;                      // 0x64
    void           *stop;                       // 0x68
    void           *updatefinished;             // 0x6C
    void           *createsample;               // 0x70
    void           *getsoundram;                // 0x74
    FMOD_OUTPUT_POSTMIXCALLBACK postmixcallback; // 0x78
    void           *record_getnumdrivers;       // 0x7C
    void           *record_getdriverinfo;       // 0x80
    void           *record_getdrivercaps;       // 0x84
    void           *record_start;               // 0x88
    void           *record_stop;                // 0x8C
    void           *record_getposition;         // 0x90
    void           *record_lock;                // 0x94
    void           *record_unlock;              // 0x98
    FMOD_OUTPUT_REVERB_SETPROPERTIESCALLBACK reverb_setproperties; // 0x9C
    void           *getmemoryused;              // 0xA0
};

class Output : public Plugin, public FMOD_OUTPUT_STATE {
public:
    ChannelPool                *mChannelPool;           // 0x24
    bool                        mEnumerated;            // 0x28
    bool                        mPolling;               // 0x29
    SystemI                    *mSystem;                // 0x2C
    ChannelPool                *mChannelPool3D;         // 0x30
    int                         mMixAheadBlocks;        // 0x34
    ChannelGroupI              *mMusicChannelGroup;     // 0x38
    int                         mNum2DChannelsFromCaps; // 0x3C
    int                         mNum3DChannelsFromCaps; // 0x40
    int                         mTotalChannelsFromCaps; // 0x44
    int                         mDSPTick;               // 0x48
    FMOD_OUTPUT_DESCRIPTION_EX  mDescription;           // 0x4C

    Output();                                           // 0x80379E10

    // Mixes numsamples samples into buffer (name and signature: Gormiti DWARF, Output::mix; the retail
    // body differs from Gormiti's).
    FMOD_RESULT mix( void *buffer, unsigned int numsamples );   // 0x80379F0C

    virtual FMOD_RESULT release();                      // override of Plugin
    virtual FMOD_RESULT getFreeChannel( unsigned int, ChannelReal **, int, int, int *, bool );

    static FMOD_RESULT F_CALLBACK mixCallback( FMOD_OUTPUT_STATE *output_state, void *buffer,
                                               unsigned int numsamples );   // 0x8037A418
};

} // namespace FMOD

#endif /* _FMOD_OUTPUT_H */
