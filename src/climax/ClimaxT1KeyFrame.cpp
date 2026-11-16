// Unit name and boundary: Gormiti link map (ClimaxT1KeyFrame.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1KeyFrame.obj).
// ClimaxT1KeyFrameStreamReadCB and ClimaxT1KeyFrameStreamWriteCB: retail's bodies differ from Gormiti's
// (another revision: retail keeps the keyframe array's base apart from the loop's pointer).
/*
 * ClimaxT1KeyFrame.cpp: the keyframes of the ClimaxT1 effects: the RtAnim interpolation scheme whose
 * keyframes are the colours of a material. The scheme's callbacks interpolate, blend, add and
 * stream the colours.
 */

#include <climax/ClimaxT1KeyFrame.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/world/bamateri.h>

// The plugin ID of the scheme (its typeID).
#define CLIMAXT1_KEYFRAME_TYPE_ID 0xA02

// Keyframes whose times lie closer than this are treated as simultaneous.
#define CLIMAXT1_KEYFRAME_TIME_EPSILON 0.02f

static void ClimaxT1KeyFrameApply( void* material, void* IFrame );
static void ClimaxT1KeyFrameInterpolate( void* Out, void* In1, void* In2, float time, void* customData );
static void ClimaxT1KeyFrameBlend( void* Out, void* In1, void* In2, float fAlpha );
static void ClimaxT1KeyFrameAdd( void* Out, void* In1, void* In2 );
static void ClimaxT1KeyFrameMulRecipCB( void* pVoidFrame, void* pVoidStart );
static RtAnimAnimation* ClimaxT1KeyFrameStreamReadCB( RwStream* stream, RtAnimAnimation* animation );
static RwBool ClimaxT1KeyFrameStreamWriteCB( const RtAnimAnimation* animation, RwStream* stream );
static RwInt32 ClimaxT1KeyFrameStreamGetSizeCB( const RtAnimAnimation* animation );

/*
====================
ClimaxT1KeyFrameApply

Sets the colour of the material to the interpolated colour.
====================
*/
// 0x801DF200
static void ClimaxT1KeyFrameApply( void* material, void* IFrame ) {
    RwRGBA color;
    RpMaterial* result = (RpMaterial*)material;
    ClimaxT1InterpFrame* app = (ClimaxT1InterpFrame*)IFrame;

    RwRGBAFromRwRGBAReal( &color, &app->color );
    RpMaterialSetColor( result, &color );
}

/*
====================
ClimaxT1KeyFrameInterpolate

The colour between two keyframes: from the first keyframe by the share of the time that has
passed between them.
====================
*/
// 0x801DF284
static void ClimaxT1KeyFrameInterpolate( void* Out, void* In1, void* In2, float time, void* customData ) {
    ClimaxT1InterpFrame* out = (ClimaxT1InterpFrame*)Out;
    ClimaxT1KeyFrame* in1 = (ClimaxT1KeyFrame*)In1;
    ClimaxT1KeyFrame* in2 = (ClimaxT1KeyFrame*)In2;
    float dt = in2->time - in1->time;
    float fAlpha;

    if ( dt < 0.0f ) {
        dt = -dt;
    }
    if ( dt < CLIMAXT1_KEYFRAME_TIME_EPSILON ) {
        fAlpha = 0.0f;
    } else {
        fAlpha = ( time - in1->time ) / dt;
    }

    RwRGBARealSub( &out->color, &in2->color, &in1->color );
    RwRGBARealScale( &out->color, &out->color, fAlpha );
    RwRGBARealAdd( &out->color, &out->color, &in1->color );
}

/*
====================
ClimaxT1KeyFrameBlend

Blends two interpolated colours by fAlpha.
====================
*/
// 0x801DF350
static void ClimaxT1KeyFrameBlend( void* Out, void* In1, void* In2, float fAlpha ) {
    ClimaxT1InterpFrame* out = (ClimaxT1InterpFrame*)Out;
    ClimaxT1InterpFrame* in1 = (ClimaxT1InterpFrame*)In1;
    ClimaxT1InterpFrame* in2 = (ClimaxT1InterpFrame*)In2;

    RwRGBARealSub( &out->color, &in2->color, &in1->color );
    RwRGBARealScale( &out->color, &out->color, fAlpha );
    RwRGBARealAdd( &out->color, &out->color, &in1->color );
}

/*
====================
ClimaxT1KeyFrameAdd

Adds two interpolated colours.
====================
*/
// 0x801DF3E0
static void ClimaxT1KeyFrameAdd( void* Out, void* In1, void* In2 ) {
    ClimaxT1InterpFrame* out = (ClimaxT1InterpFrame*)Out;
    ClimaxT1InterpFrame* in1 = (ClimaxT1InterpFrame*)In1;
    ClimaxT1InterpFrame* in2 = (ClimaxT1InterpFrame*)In2;

    RwRGBARealAdd( &out->color, &in2->color, &in1->color );
}

/*
====================
ClimaxT1KeyFrameMulRecipCB

Divides the colour of a keyframe by the colour of the start keyframe.
====================
*/
// 0x801DF424
static void ClimaxT1KeyFrameMulRecipCB( void* pVoidFrame, void* pVoidStart ) {
    ClimaxT1KeyFrame* start = (ClimaxT1KeyFrame*)pVoidStart;
    ClimaxT1KeyFrame* frame = (ClimaxT1KeyFrame*)pVoidFrame;

    frame->color.alpha /= start->color.alpha;
    frame->color.red /= start->color.red;
    frame->color.green /= start->color.green;
    frame->color.blue /= start->color.blue;
}

/*
====================
ClimaxT1KeyFrameStreamReadCB

Reads the keyframes of the animation. A keyframe is streamed with the index of its previous
keyframe, which is stored as a pointer. Returns NULL when the stream ends early.
====================
*/
// 0x801DF468
static RtAnimAnimation* ClimaxT1KeyFrameStreamReadCB( RwStream* stream, RtAnimAnimation* animation ) {
    int i;
    int nFrames = animation->numFrames;
    ClimaxT1KeyFrame* frame = (ClimaxT1KeyFrame*)animation->pFrames;
    ClimaxT1FileKeyFrame fileFrame;
    int isRead = TRUE;

    for ( i = 0; i < nFrames; i++ ) {
        if ( !RwStreamReadInt32( stream, &fileFrame.prevFrame, sizeof( RwInt32 ) ) ||
             !RwStreamReadReal( stream, &fileFrame.time, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.color.alpha, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.color.red, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.color.green, sizeof( RwReal ) ) ||
             !RwStreamReadReal( stream, &fileFrame.color.blue, sizeof( RwReal ) ) ) {
            isRead = FALSE;
            break;
        }
        if ( fileFrame.prevFrame != -1 ) {
            frame[i].prevFrame = &frame[fileFrame.prevFrame];
        } else {
            frame[i].prevFrame = NULL;
        }
        frame[i].time = fileFrame.time;
        frame[i].color = fileFrame.color;
    }
    return isRead ? animation : NULL;
}

/*
====================
ClimaxT1KeyFrameStreamWriteCB

Writes the keyframes of the animation, each with the index of its previous keyframe.
====================
*/
// 0x801DF5BC
static RwBool ClimaxT1KeyFrameStreamWriteCB( const RtAnimAnimation* animation, RwStream* stream ) {
    int i;
    int nFrames = animation->numFrames;
    ClimaxT1KeyFrame* frame = (ClimaxT1KeyFrame*)animation->pFrames;
    ClimaxT1FileKeyFrame fileFrame;
    int isWritten = TRUE;

    for ( i = 0; i < nFrames; i++ ) {
        if ( frame[i].prevFrame != NULL ) {
            fileFrame.prevFrame = frame[i].prevFrame - frame;
        } else {
            fileFrame.prevFrame = -1;
        }
        fileFrame.time = frame[i].time;
        fileFrame.color = frame[i].color;
        if ( !RwStreamWriteInt32( stream, &fileFrame.prevFrame, sizeof( RwInt32 ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.time, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.color.alpha, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.color.red, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.color.green, sizeof( RwReal ) ) ||
             !RwStreamWriteReal( stream, &fileFrame.color.blue, sizeof( RwReal ) ) ) {
            isWritten = FALSE;
            break;
        }
    }
    return isWritten;
}

/*
====================
ClimaxT1KeyFrameStreamGetSizeCB

The size of the streamed keyframes.
====================
*/
// 0x801DF70C
static RwInt32 ClimaxT1KeyFrameStreamGetSizeCB( const RtAnimAnimation* animation ) {
    return animation->numFrames * sizeof( ClimaxT1FileKeyFrame );
}

/*
====================
ClimaxT1KeyFrameRegisterInterpolationScheme

Registers the keyframe type with RtAnim. Returns false when RtAnim refuses it.
====================
*/
// 0x801DF718
extern "C" int ClimaxT1KeyFrameRegisterInterpolationScheme() {
    RtAnimInterpolatorInfo interpInfo;

    interpInfo.typeID = CLIMAXT1_KEYFRAME_TYPE_ID;
    interpInfo.animKeyFrameSize = sizeof( ClimaxT1KeyFrame );
    interpInfo.interpKeyFrameSize = sizeof( ClimaxT1InterpFrame );
    interpInfo.keyFrameApplyCB = ClimaxT1KeyFrameApply;
    interpInfo.keyFrameBlendCB = ClimaxT1KeyFrameBlend;
    interpInfo.keyFrameInterpolateCB = ClimaxT1KeyFrameInterpolate;
    interpInfo.keyFrameAddCB = ClimaxT1KeyFrameAdd;
    interpInfo.keyFrameMulRecipCB = ClimaxT1KeyFrameMulRecipCB;
    interpInfo.keyFrameStreamReadCB = ClimaxT1KeyFrameStreamReadCB;
    interpInfo.keyFrameStreamWriteCB = ClimaxT1KeyFrameStreamWriteCB;
    interpInfo.keyFrameStreamGetSizeCB = ClimaxT1KeyFrameStreamGetSizeCB;
    interpInfo.customDataSize = 0;
    interpInfo.field_0x30 = 0;
    return RtAnimRegisterInterpolationScheme( &interpInfo );
}
