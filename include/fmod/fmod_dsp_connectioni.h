/*
 * FMOD::DSPConnectionI: one link of the DSP graph, from an input unit to an output unit, with the
 * level matrix that mixes the input's channels into the output's. Connections are handed out by
 * DSPConnectionPool (fmod_dsp_connectionpool.cpp) and carry the list nodes that file them in the
 * input unit's output list and the output unit's input list.
 * Layout and member names: Gormiti DWARF (sibling.exe types DSPConnectionI), size 0x7C. No vtable.
 */
#ifndef _FMOD_DSP_CONNECTIONI_H
#define _FMOD_DSP_CONNECTIONI_H

#include "fmod.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class DSPI;

class DSPConnectionI {
public:
    LinkedListNode  mInputNode;             // 0x00, filed in the output unit's input list
    LinkedListNode  mOutputNode;            // 0x0C, filed in the input unit's output list
    LinkedListNode *mNode;                  // 0x18, the pool's list node for this connection
    short           mMaxOutputLevels;       // 0x1C
    short           mMaxInputLevels;        // 0x1E
    float          *mLevel[6];              // 0x20, one row of levels per input channel
    float          *mLevelCurrent[6];       // 0x38
    float          *mLevelDelta[6];         // 0x50
    DSPI           *mInputUnit;             // 0x68
    DSPI           *mOutputUnit;            // 0x6C
    short           mRampCount;             // 0x70
    short           mSetLevelsUsed;         // 0x72
    float           mVolume;                // 0x74
    void           *mUserData;              // 0x78

    // Carves this connection's three level matrices out of leveldata, which advances past them.
    // Parameter names invented (the DWARF records none).
    FMOD_RESULT init( float *&leveldata, int numoutputlevels, int numinputlevels );  // 0x8035B0AC
    FMOD_RESULT reset();                                                             // 0x8035B1C4
    FMOD_RESULT setUnity();                                                          // 0x8035B244
    FMOD_RESULT mix( float *outbuffer, float *inbuffer, int outchannels, int inchannels,
                     unsigned int length );                                          // 0x8035B2D0
    FMOD_RESULT mixAndRamp( float *outbuffer, float *inbuffer, int outchannels, int inchannels,
                            unsigned int length );                                   // 0x8035C468
    FMOD_RESULT rampTo();                                                            // 0x8035C75C
    FMOD_RESULT checkUnity( int outchannels, int inchannels );                       // 0x8035C948
    inline FMOD_RESULT checkMono( int outchannels, int inchannels ) {
        int count;
        int count2;

        if ( inchannels == 1 ) {
            return FMOD_OK;
        }

        for ( count = 0; count < outchannels; count++ ) {
            for ( count2 = 1; count2 < inchannels; count2++ ) {
                if ( 0.0f != mLevel[count][count2] ) {
                    return FMOD_ERR_TOOMANYCHANNELS;
                }
            }
        }

        return FMOD_OK;
    }
    FMOD_RESULT setPan( float pan, int outchannels, int inchannels,
                        FMOD_SPEAKERMODE speakermode );                              // 0x8035CA08
    FMOD_RESULT setLevels( float *levels, int numinputchannels );                    // 0x8035D168
    FMOD_RESULT getLevels( float *levels, int numinputchannels );                    // 0x8035D30C
    FMOD_RESULT setMix( float volume );                                              // 0x8035D39C
    FMOD_RESULT copy( DSPConnectionI *source );                                      // 0x8035D3DC
};

} // namespace FMOD

extern "C" {
void FMOD_DSP_Connection_MixMonoToStereo_SIMD( float *inbuffer, float *outbuffer, unsigned int length, double volumel, double volumer );
void FMOD_DSP_Connection_MixStereoToStereo_SIMD( float *inbuffer, float *outbuffer, unsigned int length, double volumel, double volumer );
void FMOD_DSP_Connection_MixMonoTo5_1_SIMD( float *inbuffer, float *outbuffer, unsigned int length, float *volume0to3, float *volume4to1, float *volume2to5 );
void FMOD_DSP_Connection_Mix5_1To5_1_SIMD( float *inbuffer, float *outbuffer, unsigned int length, float *volume0to3, float *volume4to1, float *volume2to5 );
}

#endif /* _FMOD_DSP_CONNECTIONI_H */
