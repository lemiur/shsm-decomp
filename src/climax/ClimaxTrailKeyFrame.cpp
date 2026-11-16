// Unit name and boundary: Gormiti link map (ClimaxTrailKeyFrame.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxTrailKeyFrame.obj).
// ClimaxTrailKeyFrameStreamReadCB and ClimaxTrailKeyFrameStreamWriteCB: retail's bodies differ from
// Gormiti's (another revision: retail keeps the keyframe array's base apart from the loop's pointer).
/*
 * ClimaxTrailKeyFrame.cpp: the keyframes of the ClimaxTrail effects: the RtAnim interpolation scheme
 * whose keyframes are the length and intensity of a trail. The scheme's callbacks interpolate,
 * blend, add and stream the two values.
 */

#include <climax/ClimaxTrailKeyFrame.h>
#include <rwsdk/plcore/babinary.h>

// The plugin ID of the scheme (its typeID).
#define CLIMAXTRAIL_KEYFRAME_TYPE_ID 0xA0B

static void ClimaxTrailKeyFrameApply( void* pReal2, void* IFrame );
static void ClimaxTrailKeyFrameInterpolate( void* Out, void* In1, void* In2, float time, void* customData );
static void ClimaxTrailKeyFrameBlend( void* Out, void* In1, void* In2, float fAlpha );
static void ClimaxTrailKeyFrameAdd( void* Out, void* In1, void* In2 );
static void ClimaxTrailKeyFrameMulRecipCB( void* pVoidFrame, void* pVoidStart );
static RtAnimAnimation* ClimaxTrailKeyFrameStreamReadCB( RwStream* stream, RtAnimAnimation* animation );
static RwBool ClimaxTrailKeyFrameStreamWriteCB( const RtAnimAnimation* animation, RwStream* stream );
static RwInt32 ClimaxTrailKeyFrameStreamGetSizeCB( const RtAnimAnimation* animation );

/*
====================
ClimaxTrailKeyFrameApply

Stores the interpolated intensity and length: the scheme's result is a pair of reals.
====================
*/
// 0x801E295C
static void ClimaxTrailKeyFrameApply( void* pReal2, void* IFrame ) {
    float* result = (float*)pReal2;
    ClimaxTrailInterpFrame* app = (ClimaxTrailInterpFrame*)IFrame;

    result[0] = app->intensity;
    result[1] = app->length;
}

/*
====================
ClimaxTrailKeyFrameInterpolate

The values between two keyframes, linearly by time.
====================
*/
// 0x801E2970
static void ClimaxTrailKeyFrameInterpolate( void* Out, void* In1, void* In2, float time, void* customData ) {
    ClimaxTrailInterpFrame* out = (ClimaxTrailInterpFrame*)Out;
    ClimaxTrailKeyFrame* in1 = (ClimaxTrailKeyFrame*)In1;
    ClimaxTrailKeyFrame* in2 = (ClimaxTrailKeyFrame*)In2;

    out->intensity = in1->intensity + time * ( in2->intensity - in1->intensity );
    out->length = in1->length + time * ( in2->length - in1->length );
}

/*
====================
ClimaxTrailKeyFrameBlend

Blends two interpolated keyframes by fAlpha.
====================
*/
// 0x801E29A4
static void ClimaxTrailKeyFrameBlend( void* Out, void* In1, void* In2, float fAlpha ) {
    ClimaxTrailInterpFrame* out = (ClimaxTrailInterpFrame*)Out;
    ClimaxTrailInterpFrame* in1 = (ClimaxTrailInterpFrame*)In1;
    ClimaxTrailInterpFrame* in2 = (ClimaxTrailInterpFrame*)In2;

    out->intensity = in1->intensity + fAlpha * ( in2->intensity - in1->intensity );
    out->length = in1->length + fAlpha * ( in2->length - in1->length );
}

/*
====================
ClimaxTrailKeyFrameAdd

Adds two interpolated keyframes.
====================
*/
// 0x801E29D8
static void ClimaxTrailKeyFrameAdd( void* Out, void* In1, void* In2 ) {
    ClimaxTrailInterpFrame* out = (ClimaxTrailInterpFrame*)Out;
    ClimaxTrailInterpFrame* in1 = (ClimaxTrailInterpFrame*)In1;
    ClimaxTrailInterpFrame* in2 = (ClimaxTrailInterpFrame*)In2;

    out->intensity = in1->intensity + in2->intensity;
    out->length = in1->length + in2->length;
}

/*
====================
ClimaxTrailKeyFrameMulRecipCB

Divides the values of a keyframe by those of the start keyframe.
====================
*/
// 0x801E29FC
static void ClimaxTrailKeyFrameMulRecipCB( void* pVoidFrame, void* pVoidStart ) {
    ClimaxTrailKeyFrame* start = (ClimaxTrailKeyFrame*)pVoidStart;
    ClimaxTrailKeyFrame* frame = (ClimaxTrailKeyFrame*)pVoidFrame;

    frame->intensity /= start->intensity;
    frame->length /= start->length;
}

/*
====================
ClimaxTrailKeyFrameStreamReadCB

Reads the keyframes of the animation. A keyframe is streamed with the index of its previous
keyframe, which is stored as a pointer. Returns NULL when the stream ends early.
====================
*/
// 0x801E2A20
static RtAnimAnimation* ClimaxTrailKeyFrameStreamReadCB( RwStream* stream, RtAnimAnimation* animation ) {
    int i;
    int nFrames = animation->numFrames;
    ClimaxTrailKeyFrame* frame = (ClimaxTrailKeyFrame*)animation->pFrames;
    ClimaxTrailFileKeyFrame fileFrame;
    int isRead = TRUE;

    for ( i = 0; i < nFrames; i++ ) {
        if ( !RwStreamReadInt32( stream, &fileFrame.prevFrame, sizeof( RwInt32 ) ) ||
             !RwStreamReadReal( stream, &fileFrame.time, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.intensity, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.length, sizeof( RwReal ) ) ) {
            isRead = FALSE;
            break;
        }
        if ( fileFrame.prevFrame != -1 ) {
            frame[i].prevFrame = &frame[fileFrame.prevFrame];
        } else {
            frame[i].prevFrame = NULL;
        }
        frame[i].time = fileFrame.time;
        frame[i].intensity = fileFrame.intensity;
        frame[i].length = fileFrame.length;
    }
    return isRead ? animation : NULL;
}

/*
====================
ClimaxTrailKeyFrameStreamWriteCB

Writes the keyframes of the animation, each with the index of its previous keyframe.
====================
*/
// 0x801E2B34
static RwBool ClimaxTrailKeyFrameStreamWriteCB( const RtAnimAnimation* animation, RwStream* stream ) {
    int i;
    int nFrames = animation->numFrames;
    ClimaxTrailKeyFrame* frame = (ClimaxTrailKeyFrame*)animation->pFrames;
    ClimaxTrailFileKeyFrame fileFrame;
    int isWritten = TRUE;

    for ( i = 0; i < nFrames; i++ ) {
        if ( frame[i].prevFrame != NULL ) {
            fileFrame.prevFrame = frame[i].prevFrame - frame;
        } else {
            fileFrame.prevFrame = -1;
        }
        fileFrame.time = frame[i].time;
        fileFrame.intensity = frame[i].intensity;
        fileFrame.length = frame[i].length;
        if ( !RwStreamWriteInt32( stream, &fileFrame.prevFrame, sizeof( RwInt32 ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.time, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.intensity, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.length, sizeof( RwReal ) ) ) {
            isWritten = FALSE;
            break;
        }
    }
    return isWritten;
}

/*
====================
ClimaxTrailKeyFrameStreamGetSizeCB

The size of the streamed keyframes.
====================
*/
// 0x801E2C34
static RwInt32 ClimaxTrailKeyFrameStreamGetSizeCB( const RtAnimAnimation* animation ) {
    return animation->numFrames * sizeof( ClimaxTrailFileKeyFrame );
}

/*
====================
ClimaxTrailKeyFrameRegisterInterpolationScheme

Registers the keyframe type with RtAnim.
====================
*/
// 0x801E2C40
extern "C" void ClimaxTrailKeyFrameRegisterInterpolationScheme() {
    RtAnimInterpolatorInfo interpInfo;

    interpInfo.typeID = CLIMAXTRAIL_KEYFRAME_TYPE_ID;
    interpInfo.animKeyFrameSize = sizeof( ClimaxTrailKeyFrame );
    interpInfo.interpKeyFrameSize = sizeof( ClimaxTrailInterpFrame );
    interpInfo.keyFrameApplyCB = ClimaxTrailKeyFrameApply;
    interpInfo.keyFrameBlendCB = ClimaxTrailKeyFrameBlend;
    interpInfo.keyFrameInterpolateCB = ClimaxTrailKeyFrameInterpolate;
    interpInfo.keyFrameAddCB = ClimaxTrailKeyFrameAdd;
    interpInfo.keyFrameMulRecipCB = ClimaxTrailKeyFrameMulRecipCB;
    interpInfo.keyFrameStreamReadCB = ClimaxTrailKeyFrameStreamReadCB;
    interpInfo.keyFrameStreamWriteCB = ClimaxTrailKeyFrameStreamWriteCB;
    interpInfo.keyFrameStreamGetSizeCB = ClimaxTrailKeyFrameStreamGetSizeCB;
    interpInfo.customDataSize = 0;
    interpInfo.field_0x30 = 0;
    RtAnimRegisterInterpolationScheme( &interpInfo );
}
