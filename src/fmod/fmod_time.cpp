/*
 * fmod_time.cpp: FMOD::TimeStamp, the timer behind FMOD's CPU usage figures.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_time.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_time.h>

namespace FMOD {

/*
====================
TimeStamp::TimeStamp

Clears the times and the pause state; the averaged figures are not set.
====================
*/
TimeStamp::TimeStamp() {
    mIn       = 0;
    mOut      = 0;
    mTotalIn  = 0;
    mTotalOut = 0;
    mPaused   = false;
    mPausedIn  = 0;
    mPausedOut = 0;
    mPausedTotal    = 0;
    mPausedRefCount = 0;
    mTiming = false;
} // 0x8037741C

/*
====================
TimeStamp::stampIn

Starts timing a section.
====================
*/
FMOD_RESULT TimeStamp::stampIn() {
    FMOD_OS_Time_GetNs( &mIn );
    mTiming = true;

    return FMOD_OK;
} // 0x8037744C

/*
====================
TimeStamp::stampOut

Ends the section and folds it into the smoothed figures. damp is the weight (a percentage) kept of
the earlier figures, so 0 keeps only the latest period.
====================
*/
FMOD_RESULT TimeStamp::stampOut( int damp ) {
    unsigned int val;
    float        total;
    float        smoothedtotal;
    float        dampratio = (float)damp / 100.0f;

    FMOD_OS_Time_GetNs( &val );

    mOut      = val;
    mTotalOut = val;

    if ( mTotalOut < mTotalIn ) {
        total = 0.0f;
    } else {
        total = (float)( mTotalOut - mTotalIn );
    }

    mAvTotal *= dampratio;
    mAvTotal += total;
    smoothedtotal = mAvTotal * ( 1.0f - dampratio );

    mPercent *= dampratio;
    if ( mOut > mIn ) {
        mPercent += 100.0f * (float)( ( mOut - mIn ) - mPausedTotal ) / smoothedtotal;
    }

    mCPUUsage = mPercent * ( 1.0f - dampratio );

    mTotalIn        = val;
    mPausedTotal    = 0;
    mPausedRefCount = 0;
    mTiming         = false;

    return FMOD_OK;
} // 0x80377484

/*
====================
TimeStamp::setPaused

Pauses or resumes the timing; pauses nest. Time spent paused is taken off the section.
====================
*/
FMOD_RESULT TimeStamp::setPaused( bool paused ) {
    if ( !mTiming ) {
        return FMOD_OK;
    }

    if ( paused ) {
        if ( mPausedRefCount == 0 ) {
            FMOD_OS_Time_GetNs( &mPausedIn );
        }
        mPausedRefCount++;
    } else {
        mPausedRefCount--;
        if ( mPausedRefCount == 0 ) {
            FMOD_OS_Time_GetNs( &mPausedOut );
            if ( mPausedOut > mPausedIn ) {
                mPausedTotal += mPausedOut - mPausedIn;
            }
        }
    }

    mPaused = paused;

    return FMOD_OK;
} // 0x803775CC

} // namespace FMOD
