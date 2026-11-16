// Unit name and boundary: Gormiti link map (scalekey.obj).
// Names and layout: Gormiti link map and DWARF (scalekey.obj).
// HAnimFullKeyFrameToMatrix and HAnimFullKeyFrameBlend: retail's bodies differ from Gormiti's (another
// revision: retail's ToMatrix converts the rotation once and multiplies it by the scale matrix). Both
// are static, so retail names them by their positions.
/*
 * scalekey.cpp: the full HAnim keyframe scheme: RpHAnim's standard keyframes (time, rotation and
 * translation) with a scale, as the Climax effects animate them. The scheme's callbacks apply an
 * interpolated frame to a matrix, interpolate and blend by spherical linear interpolation of the
 * rotations, stream the keyframes, make an animation a delta from a start frame and add two
 * interpolated frames.
 */

#include <climax/scalekey.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamatrix.h>
#include <math.h>
#include <rwsdk/tool/anim/rtanim.h>

static void HAnimFullKeyFrameToMatrix( void* mtx, void* voidIFrame );
static void HAnimFullKeyFrameBlend( void* voidOut, void* voidIn1, void* voidIn2, float alpha );
static void HAnimFullKeyFrameInterpolate( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData );
static RwBool HAnimFullKeyFrameStreamWrite( const RtAnimAnimation* animation, RwStream* stream );
static RtAnimAnimation* HAnimFullKeyFrameStreamRead( RwStream* stream, RtAnimAnimation* animation );
static RwInt32 HAnimFullKeyFrameStreamGetSize( const RtAnimAnimation* animation );
static void HAnimFullKeyFrameMulRecip( void* voidAnimFrame, void* voidStartFrame );
static void HAnimFullKeyFrameAdd( void* voidOut, void* voidIn1, void* voidIn2 );

/*
====================
HAnimFullKeyFrameToMatrix

Sets a matrix to an interpolated frame: the rotation scaled, with the translation as its position.
====================
*/
// 0x801E1700
static void HAnimFullKeyFrameToMatrix( void* mtx, void* voidIFrame ) {
    RwMatrix scaleMatrix;
    RwMatrix rotMatrix;
    HAnimFullInterpFrame* iFrame = (HAnimFullInterpFrame*)voidIFrame;
    RwMatrix* matrix;

    RtQuatUnitConvertToMatrix( &iFrame->q, &rotMatrix );
    RwMatrixScale( &scaleMatrix, &iFrame->scale, rwCOMBINEREPLACE );
    matrix = (RwMatrix*)mtx;
    RwMatrixMultiply( matrix, &scaleMatrix, &rotMatrix );

    matrix->pos.x = iFrame->pos.x;
    matrix->pos.y = iFrame->pos.y;
    matrix->pos.z = iFrame->pos.z;
    matrix->flags &= ~rwMATRIXINTERNALIDENTITY;
}

/*
====================
HAnimFullKeyFrameBlend

Blends two interpolated frames by alpha: the rotation spherically (linearly where the rotations are nearly
the same), the translation and scale linearly. The second rotation is negated in place when the two are
more than a right angle apart, so the path is the short one.
====================
*/
// 0x801E18A8
static void HAnimFullKeyFrameBlend( void* voidOut, void* voidIn1, void* voidIn2, float alpha ) {
    HAnimFullInterpFrame* out = (HAnimFullInterpFrame*)voidOut;
    HAnimFullInterpFrame* in1 = (HAnimFullInterpFrame*)voidIn1;
    HAnimFullInterpFrame* in2 = (HAnimFullInterpFrame*)voidIn2;
    float beta;
    float cosTheta;

    cosTheta = RwV3dDotProductMacro( &in1->q.imag, &in2->q.imag ) + in1->q.real * in2->q.real;

    if ( cosTheta < 0.0f ) {
        cosTheta = -cosTheta;
        RwV3dNegateMacro( &in2->q.imag, &in2->q.imag );
        in2->q.real = -in2->q.real;
    }

    beta = 1.0f - alpha;
    if ( cosTheta < 0.999f ) {
        float theta;
        float cosecTheta;

        theta = (float)acos( cosTheta );
        cosecTheta = 1.0f / (float)sin( theta );
        beta = cosecTheta * (float)sin( beta * theta );
        alpha = cosecTheta * (float)sin( alpha * theta );
    }

    out->q.imag.x = beta * in1->q.imag.x + alpha * in2->q.imag.x;
    out->q.imag.y = beta * in1->q.imag.y + alpha * in2->q.imag.y;
    out->q.imag.z = beta * in1->q.imag.z + alpha * in2->q.imag.z;
    out->q.real = beta * in1->q.real + alpha * in2->q.real;

    out->pos.x = in1->pos.x + alpha * ( in2->pos.x - in1->pos.x );
    out->pos.y = in1->pos.y + alpha * ( in2->pos.y - in1->pos.y );
    out->pos.z = in1->pos.z + alpha * ( in2->pos.z - in1->pos.z );
    out->scale.x = in1->scale.x + alpha * ( in2->scale.x - in1->scale.x );
    out->scale.y = in1->scale.y + alpha * ( in2->scale.y - in1->scale.y );
    out->scale.z = in1->scale.z + alpha * ( in2->scale.z - in1->scale.z );
}

/*
====================
HAnimFullKeyFrameInterpolate

Interpolates between two keyframes at a time: the blend at the fraction of the interval the time is at.
====================
*/
// 0x801E1ADC
static void HAnimFullKeyFrameInterpolate( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData ) {
    HAnimFullKeyFrame* in1 = (HAnimFullKeyFrame*)voidIn1;
    HAnimFullKeyFrame* in2 = (HAnimFullKeyFrame*)voidIn2;
    float fraction;

    fraction = ( time - in1->time ) / ( in2->time - in1->time );
    HAnimFullKeyFrameBlend( voidOut, voidIn1, voidIn2, fraction );
}

/*
====================
HAnimFullKeyFrameStreamWrite

Writes an animation's keyframes: for each, its time, rotation, translation and scale, then the offset
in bytes of its previous keyframe from the first.
====================
*/
// 0x801E1AF4
static RwBool HAnimFullKeyFrameStreamWrite( const RtAnimAnimation* animation, RwStream* stream ) {
    int i;
    HAnimFullKeyFrame* frames;

    frames = (HAnimFullKeyFrame*)animation->pFrames;
    for ( i = 0; i < animation->numFrames; i++ ) {
        int temp;

        temp = (char*)frames[i].prevFrame - (char*)frames;
        if ( !RwStreamWriteReal( stream, &frames[i].time, sizeof( HAnimFullKeyFrame ) - sizeof( HAnimFullKeyFrame* ) )
             || !RwStreamWriteInt32( stream, &temp, sizeof( int ) ) ) {
            return FALSE;
        }
    }
    return TRUE;
}

/*
====================
HAnimFullKeyFrameStreamRead

Reads the keyframes HAnimFullKeyFrameStreamWrite writes.
====================
*/
// 0x801E1B8C
static RtAnimAnimation* HAnimFullKeyFrameStreamRead( RwStream* stream, RtAnimAnimation* animation ) {
    int i;
    HAnimFullKeyFrame* frames;

    frames = (HAnimFullKeyFrame*)animation->pFrames;
    for ( i = 0; i < animation->numFrames; i++ ) {
        int temp;

        if ( !RwStreamReadReal( stream, &frames[i].time, sizeof( HAnimFullKeyFrame ) - sizeof( HAnimFullKeyFrame* ) )
             || !RwStreamReadInt32( stream, &temp, sizeof( int ) ) ) {
            return NULL;
        }
        frames[i].prevFrame = (HAnimFullKeyFrame*)( (char*)frames + temp );
    }
    return animation;
}

/*
====================
HAnimFullKeyFrameStreamGetSize

The bytes the keyframes of an animation take in a stream.
====================
*/
// 0x801E1C24
static RwInt32 HAnimFullKeyFrameStreamGetSize( const RtAnimAnimation* animation ) {
    return sizeof( HAnimFullKeyFrame ) * animation->numFrames;
}

/*
====================
HAnimFullKeyFrameMulRecip

Makes a keyframe relative to a start frame: its rotation is multiplied by the start frame's reciprocal and
the start frame's translation and scale are taken from its own.
====================
*/
// 0x801E1C30
static void HAnimFullKeyFrameMulRecip( void* voidAnimFrame, void* voidStartFrame ) {
    HAnimFullKeyFrame* animFrame = (HAnimFullKeyFrame*)voidAnimFrame;
    HAnimFullKeyFrame* startFrame = (HAnimFullKeyFrame*)voidStartFrame;
    RtQuat recip, temp;

    temp = animFrame->q;
    RtQuatReciprocal( &recip, &startFrame->q );
    RtQuatMultiply( &animFrame->q, &recip, &temp );

    animFrame->pos.x -= startFrame->pos.x;
    animFrame->pos.y -= startFrame->pos.y;
    animFrame->pos.z -= startFrame->pos.z;
    animFrame->scale.x -= startFrame->scale.x;
    animFrame->scale.y -= startFrame->scale.y;
    animFrame->scale.z -= startFrame->scale.z;
}

/*
====================
HAnimFullKeyFrameAdd

Adds two interpolated frames: multiplies their rotations and adds their translations and scales.
====================
*/
// 0x801E1E30
static void HAnimFullKeyFrameAdd( void* voidOut, void* voidIn1, void* voidIn2 ) {
    HAnimFullInterpFrame* out = (HAnimFullInterpFrame*)voidOut;
    HAnimFullInterpFrame* in1 = (HAnimFullInterpFrame*)voidIn1;
    HAnimFullInterpFrame* in2 = (HAnimFullInterpFrame*)voidIn2;

    RtQuatMultiply( &out->q, &in1->q, &in2->q );

    out->pos.x = in1->pos.x + in2->pos.x;
    out->pos.y = in1->pos.y + in2->pos.y;
    out->pos.z = in1->pos.z + in2->pos.z;
    out->scale.x = in1->scale.x + in2->scale.x;
    out->scale.y = in1->scale.y + in2->scale.y;
    out->scale.z = in1->scale.z + in2->scale.z;
}

/*
====================
HAnimFullKeyFrameRegister

Registers the keyframe type with RtAnim.
====================
*/
// 0x801E1FAC
int HAnimFullKeyFrameRegister() {
    RtAnimInterpolatorInfo info;

    info.typeID = HANIMFULLKEYFRAME_TYPE_ID;
    info.animKeyFrameSize = sizeof( HAnimFullKeyFrame );
    info.interpKeyFrameSize = sizeof( HAnimFullInterpFrame );
    info.keyFrameApplyCB = HAnimFullKeyFrameToMatrix;
    info.keyFrameBlendCB = HAnimFullKeyFrameBlend;
    info.keyFrameInterpolateCB = HAnimFullKeyFrameInterpolate;
    info.keyFrameAddCB = HAnimFullKeyFrameAdd;
    info.keyFrameMulRecipCB = HAnimFullKeyFrameMulRecip;
    info.keyFrameStreamReadCB = HAnimFullKeyFrameStreamRead;
    info.keyFrameStreamWriteCB = HAnimFullKeyFrameStreamWrite;
    info.keyFrameStreamGetSizeCB = HAnimFullKeyFrameStreamGetSize;
    info.customDataSize = 0;
    info.field_0x30 = 0;
    return RtAnimRegisterInterpolationScheme( &info ) != 0;
}
