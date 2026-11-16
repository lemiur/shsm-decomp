/*
 * fmod_dsp_connectioni.cpp: FMOD::DSPConnectionI, one link in the DSP graph with its mixing
 * matrix and volume ramping.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_connectioni.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_connectioni.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_dsp_connectioni.h>
#include <fmod/fmod_dspi.h>
#include <math.h>

#define FMOD_DSP_MAX_CHANNELS             6
#define FMOD_DSP_CONNECTION_RAMPSAMPLES   64
// Name invented: the value 1000 is retail's own case label in setPan (cmpwi 0x3E8, sharing the STEREO arm);
// Gormiti's FMOD_SPEAKERMODE DWARF does not list it.
#define FMOD_SPEAKERMODE_STEREO_LINEAR    ( (FMOD_SPEAKERMODE)1000 )

namespace FMOD {

/*
====================
DSPConnectionI::init

Carves the target, current and delta level matrices out of leveldata, advancing leveldata past them.
====================
*/
FMOD_RESULT DSPConnectionI::init( float *&leveldata, int numoutputlevels, int numinputlevels ) {
    int count;

    mMaxOutputLevels = numoutputlevels;
    mMaxInputLevels  = numinputlevels;
    if ( mMaxInputLevels < mMaxOutputLevels ) {
        mMaxInputLevels = mMaxOutputLevels;
    }

    for ( count = 0; count < FMOD_DSP_MAX_CHANNELS; count++ ) {
        if ( count < mMaxOutputLevels ) {
            mLevel[count]        = leveldata;
            leveldata           += mMaxInputLevels;
            mLevelCurrent[count] = leveldata;
            leveldata           += mMaxInputLevels;
            mLevelDelta[count]   = leveldata;
            leveldata           += mMaxInputLevels;
        } else {
            mLevel[count]        = NULL;
            mLevelCurrent[count] = NULL;
            mLevelDelta[count]   = NULL;
        }
    }

    return FMOD_OK;
} // 0x8035B0AC

/*
====================
DSPConnectionI::reset

Resets volume, user data, ramp count and all level matrices to their initial state.
====================
*/
FMOD_RESULT DSPConnectionI::reset() {
    int count;
    int count2;

    mVolume    = 1.0f;
    mUserData  = NULL;
    mRampCount = 0;

    for ( count = 0; count < mMaxOutputLevels; count++ ) {
        for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
            mLevel[count][count2]        = 0.0f;
            mLevelCurrent[count][count2] = 0.0f;
            mLevelDelta[count][count2]   = 0.0f;
        }
    }

    mSetLevelsUsed = 0;

    return FMOD_OK;
} // 0x8035B1C4

/*
====================
DSPConnectionI::setUnity

Sets the level matrix and current levels to the identity matrix with unit volume and no ramp.
====================
*/
FMOD_RESULT DSPConnectionI::setUnity() {
    int count;
    int count2;

    for ( count = 0; count < mMaxOutputLevels; count++ ) {
        for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
            if ( count == count2 ) {
                mLevelCurrent[count][count2] = 1.0f;
                mLevel[count][count2]        = 1.0f;
            } else {
                mLevel[count][count2]        = 0.0f;
                mLevelCurrent[count][count2] = 0.0f;
            }
        }
    }

    mVolume    = 1.0f;
    mRampCount = 0;

    return FMOD_OK;
} // 0x8035B244

/*
====================
DSPConnectionI::mix

Mixes length samples from inbuffer into outbuffer, ramping first if a ramp is in progress and
dispatching to channel-count-specific or SIMD paths.
====================
*/
FMOD_RESULT DSPConnectionI::mix( float *outbuffer, float *inbuffer, int outchannels, int inchannels, unsigned int length ) {
    unsigned int count;
    float        l00, l10;
    float        l01, l11;
    float        l20, l30, l40, l50;
    float        l21, l31, l41, l51;
    float        l22, l33, l44, l55;
    float        levelcurrent[6][16] = { 0 };

    if ( !inbuffer ) {
        return FMOD_OK;
    }

    if ( mRampCount ) {
        unsigned int len = mRampCount;
        if ( len > length ) {
            len = length;
        }

        mixAndRamp( outbuffer, inbuffer, outchannels, inchannels, len );

        length    -= len;
        outbuffer += len * outchannels;
        inbuffer  += len * inchannels;
    }

    if ( !length ) {
        return FMOD_OK;
    }

    if ( 0.0f == mVolume ) {
        return FMOD_OK;
    }

    for ( count = 0; count < (unsigned int)mMaxOutputLevels; count++ ) {
        if ( mLevelCurrent[count] ) {
            int count2;
            for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
                levelcurrent[count][count2] = mLevelCurrent[count][count2];
            }
        }
    }

    l00 = levelcurrent[0][0];
    l10 = levelcurrent[1][0];
    l20 = levelcurrent[2][0];
    l30 = levelcurrent[3][0];
    l40 = levelcurrent[4][0];
    l50 = levelcurrent[5][0];
    l01 = levelcurrent[0][1];
    l11 = levelcurrent[1][1];
    l21 = levelcurrent[2][1];
    l31 = levelcurrent[3][1];
    l41 = levelcurrent[4][1];
    l51 = levelcurrent[5][1];
    l22 = levelcurrent[2][2];
    l33 = levelcurrent[3][3];
    l44 = levelcurrent[4][4];
    l55 = levelcurrent[5][5];

    if ( outchannels == 2 && ( inchannels == 1 || inchannels == 2 ) ) {
        if ( inchannels == 1 ) {
            while ( ( (unsigned int)outbuffer & 15 ) && length ) {
                outbuffer[0] += inbuffer[0] * l00;
                outbuffer[1] += inbuffer[0] * l10;
                inbuffer++;
                outbuffer += 2;
                length--;
            }

            FMOD_DSP_Connection_MixMonoToStereo_SIMD( inbuffer, outbuffer, length, l00, l10 );
        } else if ( inchannels == 2 ) {
            if ( fabsf( l01 ) < 0.0001f && fabsf( l10 ) < 0.0001f ) {
                while ( ( (unsigned int)outbuffer & 15 ) && length ) {
                    outbuffer[0] += inbuffer[0] * l00;
                    outbuffer[1] += inbuffer[1] * l11;
                    inbuffer  += 2;
                    outbuffer += 2;
                    length--;
                }

                FMOD_DSP_Connection_MixStereoToStereo_SIMD( inbuffer, outbuffer, length, l00, l11 );
            } else {
                unsigned int len;

                len = length >> 2;
                while ( len ) {
                    outbuffer[0] += inbuffer[0] * l00;
                    outbuffer[0] += inbuffer[1] * l01;
                    outbuffer[1] += inbuffer[0] * l10;
                    outbuffer[1] += inbuffer[1] * l11;
                    outbuffer[2] += inbuffer[2] * l00;
                    outbuffer[2] += inbuffer[3] * l01;
                    outbuffer[3] += inbuffer[2] * l10;
                    outbuffer[3] += inbuffer[3] * l11;
                    outbuffer[4] += inbuffer[4] * l00;
                    outbuffer[4] += inbuffer[5] * l01;
                    outbuffer[5] += inbuffer[4] * l10;
                    outbuffer[5] += inbuffer[5] * l11;
                    outbuffer[6] += inbuffer[6] * l00;
                    outbuffer[6] += inbuffer[7] * l01;
                    outbuffer[7] += inbuffer[6] * l10;
                    outbuffer[7] += inbuffer[7] * l11;

                    inbuffer  += 8;
                    outbuffer += 8;
                    len--;
                }

                len = length & 3;
                while ( len ) {
                    outbuffer[0] += inbuffer[0] * l00;
                    outbuffer[0] += inbuffer[1] * l01;
                    outbuffer[1] += inbuffer[0] * l10;
                    outbuffer[1] += inbuffer[1] * l11;

                    inbuffer  += 2;
                    outbuffer += 2;
                    len--;
                }
            }
        }
    } else if ( outchannels == 6 && ( inchannels == 1 || inchannels == 2 || inchannels == 6 ) ) {
        if ( inchannels == 1 ) {
            float volume0to3[4] = { l00, l10, l20, l30 };
            float volume4to1[4] = { l40, l50, l00, l10 };
            float volume2to5[4] = { l20, l30, l40, l50 };

            while ( ( (unsigned int)outbuffer & 15 ) && length ) {
                outbuffer[0] += inbuffer[0] * l00;
                outbuffer[1] += inbuffer[0] * l10;
                outbuffer[2] += inbuffer[0] * l20;
                outbuffer[3] += inbuffer[0] * l30;
                outbuffer[4] += inbuffer[0] * l40;
                outbuffer[5] += inbuffer[0] * l50;
                inbuffer++;
                outbuffer += 6;
                length--;
            }

            FMOD_DSP_Connection_MixMonoTo5_1_SIMD( inbuffer, outbuffer, length, volume0to3, volume4to1, volume2to5 );
        } else if ( inchannels == 2 ) {
            if ( fabsf( l01 ) < 0.0001f && fabsf( l10 ) < 0.0001f &&
                 fabsf( l20 ) < 0.0001f && fabsf( l21 ) < 0.0001f &&
                 fabsf( l30 ) < 0.0001f && fabsf( l31 ) < 0.0001f &&
                 fabsf( l40 ) < 0.0001f && fabsf( l41 ) < 0.0001f &&
                 fabsf( l50 ) < 0.0001f && fabsf( l51 ) < 0.0001f ) {
                unsigned int len;

                len = length >> 2;
                while ( len ) {
                    outbuffer[0]  += inbuffer[0] * l00;
                    outbuffer[1]  += inbuffer[1] * l11;
                    outbuffer[6]  += inbuffer[2] * l00;
                    outbuffer[7]  += inbuffer[3] * l11;
                    outbuffer[12] += inbuffer[4] * l00;
                    outbuffer[13] += inbuffer[5] * l11;
                    outbuffer[18] += inbuffer[6] * l00;
                    outbuffer[19] += inbuffer[7] * l11;

                    inbuffer  += 8;
                    outbuffer += 24;
                    len--;
                }

                len = length & 3;
                if ( len ) {
                    while ( len ) {
                        outbuffer[0] += inbuffer[0] * l00;
                        outbuffer[1] += inbuffer[1] * l11;

                        inbuffer  += 2;
                        outbuffer += 6;
                        len--;
                    }
                }
            } else {
                unsigned int len;

                len = length >> 1;
                while ( len ) {
                    outbuffer[0]  += inbuffer[0] * l00;
                    outbuffer[0]  += inbuffer[1] * l01;
                    outbuffer[1]  += inbuffer[0] * l10;
                    outbuffer[1]  += inbuffer[1] * l11;
                    outbuffer[2]  += inbuffer[0] * l20;
                    outbuffer[2]  += inbuffer[1] * l21;
                    outbuffer[3]  += inbuffer[0] * l30;
                    outbuffer[3]  += inbuffer[1] * l31;
                    outbuffer[4]  += inbuffer[0] * l40;
                    outbuffer[4]  += inbuffer[1] * l41;
                    outbuffer[5]  += inbuffer[0] * l50;
                    outbuffer[5]  += inbuffer[1] * l51;

                    outbuffer[6]  += inbuffer[2] * l00;
                    outbuffer[6]  += inbuffer[3] * l01;
                    outbuffer[7]  += inbuffer[2] * l10;
                    outbuffer[7]  += inbuffer[3] * l11;
                    outbuffer[8]  += inbuffer[2] * l20;
                    outbuffer[8]  += inbuffer[3] * l21;
                    outbuffer[9]  += inbuffer[2] * l30;
                    outbuffer[9]  += inbuffer[3] * l31;
                    outbuffer[10] += inbuffer[2] * l40;
                    outbuffer[10] += inbuffer[3] * l41;
                    outbuffer[11] += inbuffer[2] * l50;
                    outbuffer[11] += inbuffer[3] * l51;

                    inbuffer  += 4;
                    outbuffer += 12;
                    len--;
                }

                len = length & 1;
                while ( len ) {
                    outbuffer[0] += inbuffer[0] * l00;
                    outbuffer[0] += inbuffer[1] * l01;
                    outbuffer[1] += inbuffer[0] * l10;
                    outbuffer[1] += inbuffer[1] * l11;
                    outbuffer[2] += inbuffer[0] * l20;
                    outbuffer[2] += inbuffer[1] * l21;
                    outbuffer[3] += inbuffer[0] * l30;
                    outbuffer[3] += inbuffer[1] * l31;
                    outbuffer[4] += inbuffer[0] * l40;
                    outbuffer[4] += inbuffer[1] * l41;
                    outbuffer[5] += inbuffer[0] * l50;
                    outbuffer[5] += inbuffer[1] * l51;

                    inbuffer  += 2;
                    outbuffer += 6;
                    len--;
                }
            }
        } else if ( inchannels == 6 ) {
            FMOD_RESULT unity = checkUnity( outchannels, inchannels );
            FMOD_RESULT mono  = FMOD_ERR_TOOMANYCHANNELS;

            if ( unity != FMOD_OK ) {
                mono = checkMono( outchannels, inchannels );
            }

            if ( unity == FMOD_OK || mono == FMOD_OK ) {
                float l[6];
                if ( unity == FMOD_OK ) {
                    l[0] = l00;
                    l[1] = l11;
                    l[2] = l22;
                    l[3] = l33;
                    l[4] = l44;
                    l[5] = l55;
                } else {
                    l[0] = l00;
                    l[1] = l10;
                    l[2] = l20;
                    l[3] = l30;
                    l[4] = l40;
                    l[5] = l50;
                }

                float volume0to3[4] = { l[0], l[1], l[2], l[3] };
                float volume4to1[4] = { l[4], l[5], l[0], l[1] };
                float volume2to5[4] = { l[2], l[3], l[4], l[5] };

                while ( ( (unsigned int)outbuffer & 15 ) && length ) {
                    outbuffer[0] += inbuffer[0] * l[0];
                    outbuffer[1] += inbuffer[1] * l[1];
                    outbuffer[2] += inbuffer[2] * l[2];
                    outbuffer[3] += inbuffer[3] * l[3];
                    outbuffer[4] += inbuffer[4] * l[4];
                    outbuffer[5] += inbuffer[5] * l[5];
                    inbuffer  += 6;
                    outbuffer += 6;
                    length--;
                }

                FMOD_DSP_Connection_Mix5_1To5_1_SIMD( inbuffer, outbuffer, length, volume0to3, volume4to1, volume2to5 );
            } else {
                for ( count = 0; count < length; count++ ) {
                    float srcval;

                    srcval        = inbuffer[0] * levelcurrent[0][0];
                    srcval       += inbuffer[1] * levelcurrent[0][1];
                    srcval       += inbuffer[2] * levelcurrent[0][2];
                    srcval       += inbuffer[3] * levelcurrent[0][3];
                    srcval       += inbuffer[4] * levelcurrent[0][4];
                    srcval       += inbuffer[5] * levelcurrent[0][5];
                    outbuffer[0] += srcval;

                    srcval        = inbuffer[0] * levelcurrent[1][0];
                    srcval       += inbuffer[1] * levelcurrent[1][1];
                    srcval       += inbuffer[2] * levelcurrent[1][2];
                    srcval       += inbuffer[3] * levelcurrent[1][3];
                    srcval       += inbuffer[4] * levelcurrent[1][4];
                    srcval       += inbuffer[5] * levelcurrent[1][5];
                    outbuffer[1] += srcval;

                    srcval        = inbuffer[0] * levelcurrent[2][0];
                    srcval       += inbuffer[1] * levelcurrent[2][1];
                    srcval       += inbuffer[2] * levelcurrent[2][2];
                    srcval       += inbuffer[3] * levelcurrent[2][3];
                    srcval       += inbuffer[4] * levelcurrent[2][4];
                    srcval       += inbuffer[5] * levelcurrent[2][5];
                    outbuffer[2] += srcval;

                    srcval        = inbuffer[0] * levelcurrent[3][0];
                    srcval       += inbuffer[1] * levelcurrent[3][1];
                    srcval       += inbuffer[2] * levelcurrent[3][2];
                    srcval       += inbuffer[3] * levelcurrent[3][3];
                    srcval       += inbuffer[4] * levelcurrent[3][4];
                    srcval       += inbuffer[5] * levelcurrent[3][5];
                    outbuffer[3] += srcval;

                    srcval        = inbuffer[0] * levelcurrent[4][0];
                    srcval       += inbuffer[1] * levelcurrent[4][1];
                    srcval       += inbuffer[2] * levelcurrent[4][2];
                    srcval       += inbuffer[3] * levelcurrent[4][3];
                    srcval       += inbuffer[4] * levelcurrent[4][4];
                    srcval       += inbuffer[5] * levelcurrent[4][5];
                    outbuffer[4] += srcval;

                    srcval        = inbuffer[0] * levelcurrent[5][0];
                    srcval       += inbuffer[1] * levelcurrent[5][1];
                    srcval       += inbuffer[2] * levelcurrent[5][2];
                    srcval       += inbuffer[3] * levelcurrent[5][3];
                    srcval       += inbuffer[4] * levelcurrent[5][4];
                    srcval       += inbuffer[5] * levelcurrent[5][5];
                    outbuffer[5] += srcval;

                    inbuffer  += 6;
                    outbuffer += 6;
                }
            }
        }
    } else {
        for ( count = 0; count < length; count++ ) {
            int count2;
            for ( count2 = 0; count2 < outchannels; count2++ ) {
                int   count3;
                float srcval  = 0.0f;
                float destval = *outbuffer;

                for ( count3 = 0; count3 < inchannels; count3++ ) {
                    srcval += inbuffer[count3] * levelcurrent[count2][count3];
                }

                destval      += srcval;
                *outbuffer++  = destval;
            }
            inbuffer += inchannels;
        }
    }

    return FMOD_OK;
} // 0x8035B2D0

/*
====================
DSPConnectionI::mixAndRamp

Mixes length samples while stepping mLevelCurrent by mLevelDelta each sample, snapping current
levels to mVolume * mLevel when the ramp finishes.
====================
*/
FMOD_RESULT DSPConnectionI::mixAndRamp( float *outbuffer, float *inbuffer, int outchannels, int inchannels, unsigned int length ) {
    unsigned int count;

    for ( count = 0; count < length; count++ ) {
        int count2;
        for ( count2 = 0; count2 < outchannels; count2++ ) {
            float srcval  = 0.0f;
            float destval = *outbuffer;
            int   count3;

            for ( count3 = 0; count3 < inchannels; count3++ ) {
                srcval                        += inbuffer[count3] * mLevelCurrent[count2][count3];
                mLevelCurrent[count2][count3] += mLevelDelta[count2][count3];
            }

            *outbuffer++ = destval += srcval;
        }
        inbuffer += inchannels;
    }

    mRampCount -= length;
    if ( !mRampCount ) {
        int   count2;
        float volume = mVolume;

        for ( count = 0; (int)count < mMaxOutputLevels; count++ ) {
            for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
                mLevelCurrent[count][count2] = volume * mLevel[count][count2];
            }
        }
    }

    return FMOD_OK;
} // 0x8035C468

/*
====================
DSPConnectionI::rampTo

Computes per-sample level deltas to ramp mLevelCurrent to mVolume * mLevel over
FMOD_DSP_CONNECTION_RAMPSAMPLES samples.
====================
*/
FMOD_RESULT DSPConnectionI::rampTo() {
    int   count;
    float oorampcount, delta = 0.0f;
    float volume = mVolume;

    oorampcount = 1.0f / FMOD_DSP_CONNECTION_RAMPSAMPLES;

    if ( mMaxInputLevels == 6 ) {
        for ( count = 0; count < mMaxOutputLevels; count++ ) {
            float value;

            value                  = ( volume * mLevel[count][0] - mLevelCurrent[count][0] ) * oorampcount;
            mLevelDelta[count][0]  = value;
            delta                 += (float)fabs( value );

            value                  = ( volume * mLevel[count][1] - mLevelCurrent[count][1] ) * oorampcount;
            mLevelDelta[count][1]  = value;
            delta                 += (float)fabs( value );

            value                  = ( volume * mLevel[count][2] - mLevelCurrent[count][2] ) * oorampcount;
            mLevelDelta[count][2]  = value;
            delta                 += (float)fabs( value );

            value                  = ( volume * mLevel[count][3] - mLevelCurrent[count][3] ) * oorampcount;
            mLevelDelta[count][3]  = value;
            delta                 += (float)fabs( value );

            value                  = ( volume * mLevel[count][4] - mLevelCurrent[count][4] ) * oorampcount;
            mLevelDelta[count][4]  = value;
            delta                 += (float)fabs( value );

            value                  = ( volume * mLevel[count][5] - mLevelCurrent[count][5] ) * oorampcount;
            mLevelDelta[count][5]  = value;
            delta                 += (float)fabs( value );
        }
    } else {
        int count2;
        for ( count = 0; count < mMaxOutputLevels; count++ ) {
            for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
                float value;

                value                       = ( volume * mLevel[count][count2] - mLevelCurrent[count][count2] ) * oorampcount;
                mLevelDelta[count][count2]  = value;
                delta                      += (float)fabs( value );
            }
        }
    }

    if ( delta >= 0.000001f ) {
        mRampCount = FMOD_DSP_CONNECTION_RAMPSAMPLES;
    }

    return FMOD_OK;
} // 0x8035C75C

/*
====================
DSPConnectionI::checkUnity

Returns FMOD_OK if the connection is an unramped unit-volume identity matrix for the given channel
counts, or FMOD_ERR_PAN otherwise.
====================
*/
FMOD_RESULT DSPConnectionI::checkUnity( int outchannels, int inchannels ) {
    int count;
    int count2;

    if ( inchannels != outchannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( mRampCount ) {
        return FMOD_ERR_PAN;
    }

    if ( 1.0f != mVolume ) {
        return FMOD_ERR_PAN;
    }

    for ( count = 0; count < outchannels; count++ ) {
        for ( count2 = 0; count2 < inchannels; count2++ ) {
            if ( count == count2 && 1.0f != mLevel[count][count2] ) {
                return FMOD_ERR_PAN;
            }
            if ( count != count2 && 0.0f != mLevel[count][count2] ) {
                return FMOD_ERR_PAN;
            }
        }
    }

    return FMOD_OK;
} // 0x8035C948

/*
====================
DSPConnectionI::setPan

Configures the connection's level matrix for the given pan position and speaker mode, then starts a
ramp.

Equivalent modulo a register swap (f2/f3) in the QUAD and PROLOGIC 2-channel pan branches.
====================
*/
FMOD_RESULT DSPConnectionI::setPan( float pan, int outchannels, int inchannels, FMOD_SPEAKERMODE speakermode ) {
    int count;
    int count2;

    if ( !mInputUnit->mSystem ) {
        return FMOD_ERR_INTERNAL;
    }

    for ( count = 0; count < outchannels; count++ ) {
        for ( count2 = 0; count2 < inchannels; count2++ ) {
            mLevel[count][count2] = 0.0f;
        }
    }

    switch ( speakermode ) {
        case FMOD_SPEAKERMODE_RAW:
            for ( count = 0; count < outchannels; count++ ) {
                for ( count2 = 0; count2 < inchannels; count2++ ) {
                    if ( count == count2 ) {
                        mLevel[count][count2] = 1.0f;
                    }
                }
            }
            break;

        case FMOD_SPEAKERMODE_MONO:
            for ( count = 0; count < inchannels; count++ ) {
                mLevel[0][count] = 1.0f;
            }
            break;

        case FMOD_SPEAKERMODE_STEREO:
        case FMOD_SPEAKERMODE_STEREO_LINEAR: {
            float l, r;
            r = ( 1.0f + pan ) / 2.0f;

            if ( inchannels == 1 ) {
                l = 1.0f - r;
                if ( speakermode == FMOD_SPEAKERMODE_STEREO ) {
                    l = (float)sqrt( l );
                    r = (float)sqrt( r );
                }
                mLevel[0][0] = l;
                mLevel[1][0] = r;
            } else if ( inchannels == 2 || inchannels == 4 || inchannels == 6 || inchannels == 8 ) {
                if ( r <= 0.5f ) {
                    l = 1.0f;
                    r = 2.0f * r;
                } else {
                    l = 2.0f * ( 1.0f - r );
                    r = 1.0f;
                }

                if ( inchannels == 2 ) {
                    mLevel[0][0] = l;
                    mLevel[1][1] = r;
                } else if ( inchannels == 4 ) {
                    mLevel[0][0] = l;
                    mLevel[0][1] = 0.0f;
                    mLevel[0][2] = 0.707f * l;
                    mLevel[0][3] = -0.707f * l;
                    mLevel[1][0] = 0.0f;
                    mLevel[1][1] = r;
                    mLevel[1][2] = 0.707f * r;
                    mLevel[1][3] = -0.707f * r;
                } else if ( inchannels == 6 ) {
                    mLevel[0][0] = l;
                    mLevel[0][1] = 0.0f;
                    mLevel[0][2] = 0.707f * l;
                    mLevel[0][3] = 0.0f;
                    mLevel[0][4] = -0.872f * l;
                    mLevel[0][5] = -0.49f * l;
                    mLevel[1][0] = 0.0f;
                    mLevel[1][1] = r;
                    mLevel[1][2] = 0.707f * r;
                    mLevel[1][3] = 0.0f;
                    mLevel[1][4] = 0.49f * r;
                    mLevel[1][5] = 0.872f * r;
                } else if ( inchannels == 8 ) {
                    mLevel[0][0] = l;
                    mLevel[0][1] = 0.0f;
                    mLevel[0][2] = 0.707f * l;
                    mLevel[0][3] = 0.0f;
                    mLevel[0][4] = -0.872f * l;
                    mLevel[0][5] = -0.49f * l;
                    mLevel[0][6] = l;
                    mLevel[0][7] = 0.0f;
                    mLevel[1][0] = 0.0f;
                    mLevel[1][1] = r;
                    mLevel[1][2] = 0.707f * r;
                    mLevel[1][3] = 0.0f;
                    mLevel[1][4] = 0.49f * r;
                    mLevel[1][5] = 0.872f * r;
                    mLevel[1][6] = 0.0f;
                    mLevel[1][7] = r;
                }
            } else {
                for ( count = 0; count < outchannels; count++ ) {
                    for ( count2 = 0; count2 < inchannels; count2++ ) {
                        if ( count == count2 ) {
                            mLevel[count][count2] = 1.0f;
                        }
                    }
                }
            }
            break;
        }

        case FMOD_SPEAKERMODE_QUAD:
        case FMOD_SPEAKERMODE_SURROUND:
        case FMOD_SPEAKERMODE_5POINT1:
        case FMOD_SPEAKERMODE_7POINT1: {
            float r = ( 1.0f + pan ) / 2.0f;
            float l;

            if ( inchannels == 1 ) {
                l            = 1.0f - r;
                l            = (float)sqrt( l );
                r            = (float)sqrt( r );
                mLevel[0][0] = l;
                mLevel[1][0] = r;
            } else if ( inchannels == 2 ) {
                if ( r <= 0.5f ) {
                    l = 1.0f;
                    r = 2.0f * r;
                } else {
                    l = 2.0f * ( 1.0f - r );
                    r = 1.0f;
                }
                mLevel[0][0] = l;
                mLevel[1][1] = r;
            } else {
                for ( count = 0; count < outchannels; count++ ) {
                    for ( count2 = 0; count2 < inchannels; count2++ ) {
                        if ( count == count2 ) {
                            mLevel[count][count2] = 1.0f;
                        }
                    }
                }
            }
            break;
        }

        case FMOD_SPEAKERMODE_PROLOGIC: {
            float r = ( 1.0f + pan ) / 2.0f;
            float l;

            if ( inchannels == 1 ) {
                l            = 1.0f - r;
                l            = (float)sqrt( l );
                r            = (float)sqrt( r );
                mLevel[0][0] = l;
                mLevel[1][0] = r;
            } else if ( inchannels == 2 ) {
                if ( r < 0.5f ) {
                    l = 1.0f;
                    r = 2.0f * r;
                } else {
                    l = 2.0f * ( 1.0f - r );
                    r = 1.0f;
                }
                mLevel[0][0] = l;
                mLevel[1][1] = r;
            } else {
                for ( count = 0; count < outchannels; count++ ) {
                    for ( count2 = 0; count2 < inchannels; count2++ ) {
                        if ( count == count2 ) {
                            mLevel[count][count2] = 1.0f;
                        }
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    mSetLevelsUsed = 1;
    return rampTo();
} // 0x8035CA08

/*
====================
DSPConnectionI::setLevels

Copies a 2D level matrix into mLevel and starts a ramp.
====================
*/
FMOD_RESULT DSPConnectionI::setLevels( float *levels, int numinputchannels ) {
    int count;
    int count2;

    if ( !levels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !numinputchannels ) {
        return FMOD_OK;
    }

    if ( mMaxOutputLevels == 6 && numinputchannels < 3 ) {
        if ( numinputchannels == 1 ) {
            mLevel[0][0] = levels[0];
            mLevel[1][0] = levels[1];
            mLevel[2][0] = levels[2];
            mLevel[3][0] = levels[3];
            mLevel[4][0] = levels[4];
            mLevel[5][0] = levels[5];
        } else {
            mLevel[0][0] = levels[0];
            mLevel[0][1] = levels[1];
            mLevel[1][0] = levels[2];
            mLevel[1][1] = levels[3];
            mLevel[2][0] = levels[4];
            mLevel[2][1] = levels[5];
            mLevel[3][0] = levels[6];
            mLevel[3][1] = levels[7];
            mLevel[4][0] = levels[8];
            mLevel[4][1] = levels[9];
            mLevel[5][0] = levels[10];
            mLevel[5][1] = levels[11];
        }
    } else {
        for ( count = 0; count < mMaxOutputLevels; count++ ) {
            for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
                if ( count2 < numinputchannels ) {
                    mLevel[count][count2] = levels[count * numinputchannels + count2];
                } else {
                    mLevel[count][count2] = 0.0f;
                }
            }
        }
    }

    mSetLevelsUsed = 1;
    return rampTo();
} // 0x8035D168

/*
====================
DSPConnectionI::getLevels

Reads the connection's level matrix into levels.
====================
*/
FMOD_RESULT DSPConnectionI::getLevels( float *levels, int numinputchannels ) {
    int count;
    int count2;

    if ( !levels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    for ( count = 0; count < mMaxOutputLevels; count++ ) {
        for ( count2 = 0; count2 < numinputchannels; count2++ ) {
            if ( count2 < mMaxInputLevels ) {
                levels[count * numinputchannels + count2] = mLevel[count][count2];
            } else {
                levels[count * numinputchannels + count2] = 0.0f;
            }
        }
    }

    return FMOD_OK;
} // 0x8035D30C

/*
====================
DSPConnectionI::setMix

Clamps volume to [-1, 1] and ramps the connection to the new volume if it changed.
====================
*/
FMOD_RESULT DSPConnectionI::setMix( float volume ) {
    if ( volume < -1.0f ) {
        volume = -1.0f;
    }
    if ( volume > 1.0f ) {
        volume = 1.0f;
    }

    if ( mVolume == volume ) {
        return FMOD_OK;
    }

    mVolume = volume;
    return rampTo();
} // 0x8035D39C

/*
====================
DSPConnectionI::copy

Copies volume, level matrices, ramp count and flags from source.
====================
*/
FMOD_RESULT DSPConnectionI::copy( DSPConnectionI *source ) {
    int count;
    int count2;

    mVolume = source->mVolume;

    for ( count = 0; count < mMaxOutputLevels; count++ ) {
        for ( count2 = 0; count2 < mMaxInputLevels; count2++ ) {
            mLevel[count][count2]        = source->mLevel[count][count2];
            mLevelCurrent[count][count2] = source->mLevelCurrent[count][count2];
            mLevelDelta[count][count2]   = source->mLevelDelta[count][count2];
        }
    }

    mRampCount     = source->mRampCount;
    mSetLevelsUsed = source->mSetLevelsUsed;

    return FMOD_OK;
} // 0x8035D3DC

} // namespace FMOD
