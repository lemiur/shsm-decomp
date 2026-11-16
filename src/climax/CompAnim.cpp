// Unit name and boundary: Gormiti link map (CompAnim.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1Effects/CompAnim.cpp).
// KeyFrameInterpolate_F_Std and KeyFrameInterpolate_V2_Std: retail's bodies differ from Gormiti's (another
// revision); they take Gormiti's names from the registrations in CompressedAnimRegister. The DeCompressKey_*
// functions have no copy in retail: they are inlined into the interpolation callbacks. Retail has none of
// Gormiti's compressors (Compress_*, PackStreamAnim, AddLog, CompressedAnimConvert).
// The variable-length scheme keeps the animation's real keyframe count in RtAnimAnimation::customData, which
// RenderWare declares a pointer and Gormiti's DWARF types the same (void*): the count is stored and read as an
// integer through a cast, and numFrames counts only the whole keyframes until KeyFrameStreamGetSize moves the
// count back for the header.
// All 13 functions match. The unit stays NonMatching: its .sdata2 pool does not (gatecheck layout main/climax/CompAnim).
// Retail's pool opens with 0.0, 65535.0, 1.0, 8191.0, 2047.0, 2048.0 before the first int-to-float conversion
// constants; the retained functions give 2048.0 before 2047.0 and none of the first four ahead of them. Those
// constants come from code compiled earlier in this object and dropped from the link: Gormiti's CompAnim.obj has
// eight constants ahead of the unpackers', from its Compress_Std_V2 and Compress_Std_F (compressors, with no copy in
// retail). Their bodies, which nothing in retail can check, are the open work that would let the pool match.
/*
 * CompAnim.cpp: the compressed animation schemes of the Climax effects: RtAnim interpolation schemes whose
 * keyframes are the standard and the full HAnim keyframes in a packed form. A scheme's callbacks stream the
 * packed keyframes and unpack the two keyframes an interpolation lies between into RpHAnim's own, which then
 * interpolates them.
 */

#include <climax/CompAnim.h>
#include <climax/scalekey.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plugin/hanim/rphanim.h>
#include <math.h>

// The type IDs of the compressed schemes (their typeID); names invented.
#define COMPANIM_STD_F_TYPE_ID  0x1103
#define COMPANIM_FULL_F_TYPE_ID 0x1104
#define COMPANIM_STD_V2_TYPE_ID 0x1105
#define COMPANIM_STD_P_TYPE_ID  0x1106

// The variable-length scheme's packed keys (see KeyFrameInterpolate_V2_Std); names invented. A key is a 16-bit
// word of a time and flags, followed by the parts the flags announce.
#define COMPANIM_V2_KEY_TIME_MASK       0x1FFF  // the time, in COMPANIM_V2_TIME_RANGE ths of the interval
#define COMPANIM_V2_TIME_RANGE          8191.0f
#define COMPANIM_V2_KEY_REAL_NEGATIVE   0x2000  // the rotation's real part is negative
#define COMPANIM_V2_KEY_HAS_QUART       0x4000  // a rotation's x, y and z follow
#define COMPANIM_V2_KEY_HAS_TRANS       0x8000  // the translation parts the node's key data names follow
// The first whole keyframe's key data: where the packed keys start, and which translation parts they carry.
#define COMPANIM_V2_OFFSET_MASK         0x1FFFFFFF
#define COMPANIM_V2_DATA_TRANS_X        0x20000000
#define COMPANIM_V2_DATA_TRANS_Y        0x40000000
#define COMPANIM_V2_DATA_TRANS_Z        0x80000000
// A rotation component of 0x7FFF is about zero: a node's rotation until a key names another.
#define COMPANIM_V2_QUART_ZERO          0x7FFF

// The interpolation callback of the full HAnim scheme, which the full compressed scheme calls with the
// keyframes it has unpacked.
static RtAnimKeyFrameInterpolateCallBack spfAnim_Full_KeyFrameInterpolate;

static RwBool KeyFrameStreamWrite_F( const RtAnimAnimation* animation, RwStream* stream );
static RtAnimAnimation* KeyFrameStreamRead_F( RwStream* stream, RtAnimAnimation* animation );
static RwBool KeyFrameStreamWrite_P( const RtAnimAnimation* animation, RwStream* stream );
static RtAnimAnimation* KeyFrameStreamRead_P( RwStream* stream, RtAnimAnimation* animation );
static RwBool KeyFrameStreamWrite_V2( const RtAnimAnimation* animation, RwStream* stream );
static RtAnimAnimation* KeyFrameStreamRead_V2( RwStream* stream, RtAnimAnimation* animation );
static RwInt32 KeyFrameStreamGetSize( const RtAnimAnimation* animation );
static RwInt32 KeyFrameStreamGetSize_P( const RtAnimAnimation* animation );
void KeyFrameInterpolate_F_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData );
void KeyFrameInterpolate_P_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData );
void KeyFrameInterpolate_F_Full( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData );
void KeyFrameInterpolate_V2_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData );

/*
====================
KeyFrameStreamWrite_F

Writes the animation's keyframes of the standard or the full float scheme: the scheme's data, then for each
keyframe its offset in bytes from the keyframe it replaces and its time, then the packed rotation and
translation (and scale). Fails for an animation of another scheme.
====================
*/
// 0x801DFD6C
static RwBool KeyFrameStreamWrite_F( const RtAnimAnimation* animation, RwStream* stream ) {
    if ( animation == NULL ) {
        return FALSE;
    }

    int HeaderSize = -1;
    const RwReal* HeaderPtr = NULL;

    if ( animation->interpInfo->typeID == COMPANIM_STD_F_TYPE_ID ) {
        HeaderPtr = (const RwReal*)animation->customData;
        HeaderSize = sizeof( StCompAnimData_std_f );
    }
    if ( animation->interpInfo->typeID == COMPANIM_STD_P_TYPE_ID ) {
        HeaderPtr = NULL;
        HeaderSize = 0;
    }
    if ( animation->interpInfo->typeID == COMPANIM_FULL_F_TYPE_ID ) {
        HeaderPtr = (const RwReal*)animation->customData;
        HeaderSize = sizeof( StCompAnimData_full_f );
    }
    if ( HeaderSize < 0 ) {
        return FALSE;
    }
    if ( HeaderSize != 0 ) {
        RwStreamWriteReal( stream, HeaderPtr, HeaderSize );
    }

    char* KeyPtr = (char*)animation->pFrames;
    int KeySize = animation->interpInfo->animKeyFrameSize;
    int KeyCount = animation->numFrames;
    for ( int Key = 0; Key < KeyCount; Key++ ) {
        RtAnimKeyFrameHeader* KeyHeader = (RtAnimKeyFrameHeader*)KeyPtr;
        int PrevKeyOffset = (char*)KeyHeader - (char*)KeyHeader->prevFrame;

        RwStreamWriteInt32( stream, &PrevKeyOffset, sizeof( RwInt32 ) );
        RwStreamWriteReal( stream, &KeyHeader->time, sizeof( RwReal ) );
        KeyPtr += KeySize;
    }

    switch ( animation->interpInfo->typeID ) {
    case COMPANIM_STD_F_TYPE_ID: {
        StCompAnim_Std_F_KeyFrame* KeyPtr = (StCompAnim_Std_F_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamWriteInt32( stream, (const RwInt32*)&KeyPtr->Quart_P_32, sizeof( RwInt32 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Quart_P_16, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_X, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_Y, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_Z, sizeof( RwInt16 ) );
        }
        break;
    }
    case COMPANIM_STD_P_TYPE_ID: {
        StCompAnim_Std_P_KeyFrame* KeyPtr = (StCompAnim_Std_P_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamWriteInt16( stream, &KeyPtr->Quart_X, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_Y, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_Z, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_W, sizeof( RwInt16 ) );
            RwStreamWriteReal( stream, &KeyPtr->Trans_X, sizeof( RwReal ) );
            RwStreamWriteReal( stream, &KeyPtr->Trans_Y, sizeof( RwReal ) );
            RwStreamWriteReal( stream, &KeyPtr->Trans_Z, sizeof( RwReal ) );
        }
        break;
    }
    case COMPANIM_FULL_F_TYPE_ID: {
        StCompAnim_Full_F_KeyFrame* KeyPtr = (StCompAnim_Full_F_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamWriteInt16( stream, &KeyPtr->Quart_I_X, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_I_Y, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_I_Z, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, &KeyPtr->Quart_R, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_X, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_Y, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Trans_Z, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Scale_X, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Scale_Y, sizeof( RwInt16 ) );
            RwStreamWriteInt16( stream, (const RwInt16*)&KeyPtr->Scale_Z, sizeof( RwInt16 ) );
        }
        break;
    }
    }
    return TRUE;
}

/*
====================
KeyFrameStreamRead_F

Reads the keyframes KeyFrameStreamWrite_F wrote into the animation, whose keyframes are already allocated.
Returns NULL for an animation of another scheme.
====================
*/
// 0x801E003C
static RtAnimAnimation* KeyFrameStreamRead_F( RwStream* stream, RtAnimAnimation* animation ) {
    if ( animation == NULL ) {
        return NULL;
    }

    int HeaderSize = -1;
    RwReal* HeaderPtr = NULL;

    if ( animation->interpInfo->typeID == COMPANIM_STD_F_TYPE_ID ) {
        HeaderPtr = (RwReal*)animation->customData;
        HeaderSize = sizeof( StCompAnimData_std_f );
    }
    if ( animation->interpInfo->typeID == COMPANIM_STD_P_TYPE_ID ) {
        HeaderPtr = NULL;
        HeaderSize = 0;
    }
    if ( animation->interpInfo->typeID == COMPANIM_FULL_F_TYPE_ID ) {
        HeaderPtr = (RwReal*)animation->customData;
        HeaderSize = sizeof( StCompAnimData_full_f );
    }
    if ( HeaderSize < 0 ) {
        return NULL;
    }
    if ( HeaderSize != 0 ) {
        RwStreamReadReal( stream, HeaderPtr, HeaderSize );
    }

    char* KeyPtr = (char*)animation->pFrames;
    int KeySize = animation->interpInfo->animKeyFrameSize;
    int KeyCount = animation->numFrames;
    for ( int Key = 0; Key < KeyCount; Key++ ) {
        RtAnimKeyFrameHeader* KeyHeader = (RtAnimKeyFrameHeader*)KeyPtr;
        int PrevKeyOffset = 0;

        RwStreamReadInt32( stream, &PrevKeyOffset, sizeof( RwInt32 ) );
        RwStreamReadReal( stream, &KeyHeader->time, sizeof( RwReal ) );
        KeyHeader->prevFrame = KeyPtr - PrevKeyOffset;
        KeyPtr += KeySize;
    }

    switch ( animation->interpInfo->typeID ) {
    case COMPANIM_STD_F_TYPE_ID: {
        StCompAnim_Std_F_KeyFrame* KeyPtr = (StCompAnim_Std_F_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamReadInt32( stream, (RwInt32*)&KeyPtr->Quart_P_32, sizeof( RwInt32 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Quart_P_16, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_X, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_Y, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_Z, sizeof( RwInt16 ) );
        }
        break;
    }
    case COMPANIM_STD_P_TYPE_ID: {
        StCompAnim_Std_P_KeyFrame* KeyPtr = (StCompAnim_Std_P_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamReadInt16( stream, &KeyPtr->Quart_X, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_Y, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_Z, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_W, sizeof( RwInt16 ) );
            RwStreamReadReal( stream, &KeyPtr->Trans_X, sizeof( RwReal ) );
            RwStreamReadReal( stream, &KeyPtr->Trans_Y, sizeof( RwReal ) );
            RwStreamReadReal( stream, &KeyPtr->Trans_Z, sizeof( RwReal ) );
        }
        break;
    }
    case COMPANIM_FULL_F_TYPE_ID: {
        StCompAnim_Full_F_KeyFrame* KeyPtr = (StCompAnim_Full_F_KeyFrame*)animation->pFrames;

        for ( int Key = 0; Key < KeyCount; Key++, KeyPtr++ ) {
            RwStreamReadInt16( stream, &KeyPtr->Quart_I_X, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_I_Y, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_I_Z, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, &KeyPtr->Quart_R, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_X, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_Y, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Trans_Z, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Scale_X, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Scale_Y, sizeof( RwInt16 ) );
            RwStreamReadInt16( stream, (RwInt16*)&KeyPtr->Scale_Z, sizeof( RwInt16 ) );
        }
        break;
    }
    }
    return animation;
}

/*
====================
KeyFrameStreamWrite_P

Writes the animation's keyframes of the standard scheme with 16-bit rotations: for each keyframe its offset in
bytes from the keyframe it replaces, its time, the rotation and the translation.
====================
*/
// 0x801E0318
static RwBool KeyFrameStreamWrite_P( const RtAnimAnimation* animation, RwStream* stream ) {
    if ( animation == NULL ) {
        return FALSE;
    }

    char* KeyPtr = (char*)animation->pFrames;
    int KeySize = animation->interpInfo->animKeyFrameSize;
    int KeyCount = animation->numFrames;
    for ( int Key = 0; Key < KeyCount; Key++ ) {
        RtAnimKeyFrameHeader* KeyHeader = (RtAnimKeyFrameHeader*)KeyPtr;
        int PrevKeyOffset = (char*)KeyHeader - (char*)KeyHeader->prevFrame;
        RwStreamWriteInt32( stream, &PrevKeyOffset, sizeof( RwInt32 ) );
        RwStreamWriteReal( stream, &KeyHeader->time, sizeof( RwReal ) );

        StCompAnim_Std_P_KeyFrame* PKey = (StCompAnim_Std_P_KeyFrame*)KeyPtr;
        RwStreamWriteInt16( stream, &PKey->Quart_X, sizeof( RwInt16 ) );
        RwStreamWriteInt16( stream, &PKey->Quart_Y, sizeof( RwInt16 ) );
        RwStreamWriteInt16( stream, &PKey->Quart_Z, sizeof( RwInt16 ) );
        RwStreamWriteInt16( stream, &PKey->Quart_W, sizeof( RwInt16 ) );
        RwStreamWriteReal( stream, &PKey->Trans_X, sizeof( RwReal ) );
        RwStreamWriteReal( stream, &PKey->Trans_Y, sizeof( RwReal ) );
        RwStreamWriteReal( stream, &PKey->Trans_Z, sizeof( RwReal ) );
        KeyPtr += KeySize;
    }
    return TRUE;
}

/*
====================
KeyFrameStreamRead_P

Reads the keyframes KeyFrameStreamWrite_P wrote into the animation, whose keyframes are already allocated.
====================
*/
// 0x801E0418
static RtAnimAnimation* KeyFrameStreamRead_P( RwStream* stream, RtAnimAnimation* animation ) {
    if ( animation == NULL ) {
        return NULL;
    }

    char* KeyPtr = (char*)animation->pFrames;
    int KeySize = animation->interpInfo->animKeyFrameSize;
    int KeyCount = animation->numFrames;
    for ( int Key = 0; Key < KeyCount; Key++ ) {
        RtAnimKeyFrameHeader* KeyHeader = (RtAnimKeyFrameHeader*)KeyPtr;
        int PrevKeyOffset = 0;
        RwStreamReadInt32( stream, &PrevKeyOffset, sizeof( RwInt32 ) );
        RwStreamReadReal( stream, &KeyHeader->time, sizeof( RwReal ) );
        KeyHeader->prevFrame = KeyPtr - PrevKeyOffset;

        StCompAnim_Std_P_KeyFrame* PKey = (StCompAnim_Std_P_KeyFrame*)KeyPtr;
        RwStreamReadInt16( stream, &PKey->Quart_X, sizeof( RwInt16 ) );
        RwStreamReadInt16( stream, &PKey->Quart_Y, sizeof( RwInt16 ) );
        RwStreamReadInt16( stream, &PKey->Quart_Z, sizeof( RwInt16 ) );
        RwStreamReadInt16( stream, &PKey->Quart_W, sizeof( RwInt16 ) );
        RwStreamReadReal( stream, &PKey->Trans_X, sizeof( RwReal ) );
        RwStreamReadReal( stream, &PKey->Trans_Y, sizeof( RwReal ) );
        RwStreamReadReal( stream, &PKey->Trans_Z, sizeof( RwReal ) );
        KeyPtr += KeySize;
    }
    return animation;
}

/*
====================
KeyFrameStreamWrite_V2

Writes the animation's keyframes of the variable-length standard scheme: the count of the whole keyframes (the
first two of each node), those keyframes, then the packed remainder. The animation's real keyframe count lives
in its custom data, where KeyFrameStreamRead_V2 left it; numFrames stands for the whole keyframes only.
====================
*/
// 0x801E0524
static RwBool KeyFrameStreamWrite_V2( const RtAnimAnimation* animation, RwStream* stream ) {
    char* KeyPtr;
    int KeySize;
    int KeyCount;

    if ( animation == NULL ) {
        return FALSE;
    }

    KeyPtr = (char*)animation->pFrames;
    KeySize = animation->interpInfo->animKeyFrameSize;
    KeyCount = RtAnimAnimationGetNumNodes( animation ) * 2;

    RwStreamWriteInt32( stream, &KeyCount, sizeof( RwInt32 ) );
    RwStreamWriteInt32( stream, (RwInt32*)KeyPtr, KeySize * KeyCount );

    KeyPtr += KeySize * KeyCount;
    int FakeKeyCount = (int)animation->customData;
    int VLengthKeyDataSize = KeySize * ( FakeKeyCount - KeyCount );
    RwStreamWriteInt16( stream, (RwInt16*)KeyPtr, VLengthKeyDataSize );

    RtAnimAnimation* Anim = (RtAnimAnimation*)animation;
    Anim->numFrames = KeyCount;
    return TRUE;
}

/*
====================
KeyFrameStreamRead_V2

Reads the keyframes KeyFrameStreamWrite_V2 wrote: the whole keyframes, then the packed remainder. The animation's
numFrames (the real keyframe count) moves to its custom data, and numFrames is left at the whole keyframes' count.
The second whole keyframe of each node replaces the first. Returns NULL whether it succeeded or not, which
RtAnimAnimationStreamRead ignores.
====================
*/
// 0x801E05E0
static RtAnimAnimation* KeyFrameStreamRead_V2( RwStream* stream, RtAnimAnimation* animation ) {
    int KeySize;
    int KeyCount;

    if ( animation == NULL ) {
        return NULL;
    }

    KeySize = animation->interpInfo->animKeyFrameSize;
    KeyCount = 0;

    RwStreamReadInt32( stream, &KeyCount, sizeof( RwInt32 ) );
    RwStreamReadInt32( stream, (RwInt32*)animation->pFrames, KeySize * KeyCount );

    unsigned char* VarKeyData = (unsigned char*)animation->pFrames;
    char* KeyPtr = (char*)VarKeyData + KeySize * KeyCount;
    int FakeKeyCount = animation->numFrames;
    animation->customData = (void*)FakeKeyCount;

    int VLengthKeyDataSize = KeySize * ( FakeKeyCount - KeyCount );
    RwStreamReadInt16( stream, (RwInt16*)KeyPtr, VLengthKeyDataSize );

    int NumNodes = KeyCount / 2;
    StCompAnim_Std_V2_KeyFrame* ActualKeys = (StCompAnim_Std_V2_KeyFrame*)animation->pFrames;
    for ( int ind = 0; ind < NumNodes; ind++ ) {
        ActualKeys[ind + NumNodes].PrevData.prevFrame = &ActualKeys[ind];
    }

    animation->customData = (void*)FakeKeyCount;
    animation->numFrames = KeyCount;
    return NULL;
}

/*
====================
KeyFrameStreamGetSize

The stream size of an animation of the standard or the full float scheme, the 16-bit-rotation scheme or the
variable-length scheme; 0 for no animation or another scheme. For the variable-length scheme it first moves the
real keyframe count from the custom data to numFrames, where the animation's header is written from.
====================
*/
// 0x801E0784
static RwInt32 KeyFrameStreamGetSize( const RtAnimAnimation* animation ) {
    if ( animation == NULL ) {
        return 0;
    }

    if ( animation->interpInfo->typeID == COMPANIM_STD_F_TYPE_ID ) {
        return animation->numFrames * sizeof( StCompAnim_Std_F_KeyFrame ) + sizeof( StCompAnimData_std_f );
    }
    if ( animation->interpInfo->typeID == COMPANIM_FULL_F_TYPE_ID ) {
        return animation->numFrames * sizeof( StCompAnim_Full_F_KeyFrame ) + sizeof( StCompAnimData_full_f );
    }
    if ( animation->interpInfo->typeID == COMPANIM_STD_P_TYPE_ID ) {
        return animation->numFrames * sizeof( StCompAnim_Std_P_KeyFrame );
    }
    if ( animation->interpInfo->typeID == COMPANIM_STD_V2_TYPE_ID ) {
        RtAnimAnimation* Anim = (RtAnimAnimation*)animation;
        int FakeKeyCount = (int)Anim->customData;
        Anim->numFrames = FakeKeyCount;

        int KeySize = animation->interpInfo->animKeyFrameSize;
        int KeyCount = RtAnimAnimationGetNumNodes( animation ) * 2;
        int VLengthKeyDataSize = KeySize * ( FakeKeyCount - KeyCount );
        int StreamSize = KeySize * KeyCount;
        StreamSize += sizeof( RwInt32 );
        StreamSize += VLengthKeyDataSize;
        return StreamSize;
    }
    return 0;
}

/*
====================
KeyFrameStreamGetSize_P

The stream size of an animation of StCompAnim_Std_P_KeyFrame keyframes; 0 for no animation.
====================
*/
// 0x801E0844
static RwInt32 KeyFrameStreamGetSize_P( const RtAnimAnimation* animation ) {
    if ( animation != NULL ) {
        return animation->numFrames * sizeof( StCompAnim_Std_P_KeyFrame );
    }
    return 0;
}

/*
====================
DeCompressKey_Std_F

Unpacks a keyframe of the standard float scheme: the rotation's four 12-bit components out of its 48 bits, and
the translation out of its 16-bit fractions of the animation's range.
====================
*/
// no copy in retail (inlined)
static void DeCompressKey_Std_F( StCompAnim_Std_F_KeyFrame* SrcKey, RpHAnimKeyFrame* DstKey, StCompAnimData_std_f* cmpanm ) {
    int Quart_Img_X = SrcKey->Quart_P_32 >> 20;
    int Quart_Img_Y = ( SrcKey->Quart_P_32 >> 8 ) & 0xFFF;
    int Quart_Img_Z = ( ( SrcKey->Quart_P_32 & 0xFF ) << 4 ) | ( ( SrcKey->Quart_P_16 >> 12 ) & 0xF );
    int Quart_Real = SrcKey->Quart_P_16 & 0xFFF;

    DstKey->q.imag.x = ( (float)Quart_Img_X - 2048.0f ) / 2047.0f;
    DstKey->q.imag.y = ( (float)Quart_Img_Y - 2048.0f ) / 2047.0f;
    DstKey->q.imag.z = ( (float)Quart_Img_Z - 2048.0f ) / 2047.0f;
    DstKey->q.real = ( (float)Quart_Real - 2048.0f ) / 2047.0f;

    DstKey->t.x = cmpanm->Tbase.x + (float)SrcKey->Trans_X / 65535.0f * cmpanm->TScale.x;
    DstKey->t.y = cmpanm->Tbase.y + (float)SrcKey->Trans_Y / 65535.0f * cmpanm->TScale.y;
    DstKey->t.z = cmpanm->Tbase.z + (float)SrcKey->Trans_Z / 65535.0f * cmpanm->TScale.z;

    DstKey->time = SrcKey->time;
    DstKey->prevFrame = NULL;
}

/*
====================
DeCompressKey_Std_P

Unpacks a keyframe of the 16-bit-rotation standard scheme: the rotation's components are fractions of 32767.
====================
*/
// no copy in retail (inlined)
static void DeCompressKey_Std_P( StCompAnim_Std_P_KeyFrame* SrcKey, RpHAnimKeyFrame* DstKey ) {
    DstKey->q.imag.x = ( 1.0f / 32767.0f ) * (float)SrcKey->Quart_X;
    DstKey->q.imag.y = ( 1.0f / 32767.0f ) * (float)SrcKey->Quart_Y;
    DstKey->q.imag.z = ( 1.0f / 32767.0f ) * (float)SrcKey->Quart_Z;
    DstKey->q.real = ( 1.0f / 32767.0f ) * (float)SrcKey->Quart_W;

    DstKey->t.x = SrcKey->Trans_X;
    DstKey->t.y = SrcKey->Trans_Y;
    DstKey->t.z = SrcKey->Trans_Z;

    DstKey->time = SrcKey->time;
    DstKey->prevFrame = NULL;
}

/*
====================
DeCompressKey_Full_F

Unpacks a keyframe of the full float scheme: the rotation's components are fractions of 16383, the translation
and the scale are 16-bit fractions of the animation's ranges.
====================
*/
// no copy in retail (inlined)
static void DeCompressKey_Full_F( StCompAnim_Full_F_KeyFrame* SrcKey, HAnimFullKeyFrame* DstKey, StCompAnimData_full_f* cmpanm ) {
    DstKey->q.imag.x = (float)SrcKey->Quart_I_X / 16383.0f;
    DstKey->q.imag.y = (float)SrcKey->Quart_I_Y / 16383.0f;
    DstKey->q.imag.z = (float)SrcKey->Quart_I_Z / 16383.0f;
    DstKey->q.real = (float)SrcKey->Quart_R / 16383.0f;

    DstKey->pos.x = cmpanm->Tbase.x + (float)SrcKey->Trans_X / 65535.0f * cmpanm->TScale.x;
    DstKey->pos.y = cmpanm->Tbase.y + (float)SrcKey->Trans_Y / 65535.0f * cmpanm->TScale.y;
    DstKey->pos.z = cmpanm->Tbase.z + (float)SrcKey->Trans_Z / 65535.0f * cmpanm->TScale.z;

    DstKey->scale.x = cmpanm->Sbase.x + (float)SrcKey->Scale_X / 65535.0f * cmpanm->SScale.x;
    DstKey->scale.y = cmpanm->Sbase.y + (float)SrcKey->Scale_Y / 65535.0f * cmpanm->SScale.y;
    DstKey->scale.z = cmpanm->Sbase.z + (float)SrcKey->Scale_Z / 65535.0f * cmpanm->SScale.z;

    DstKey->time = SrcKey->time;
    DstKey->prevFrame = NULL;
}

/*
====================
KeyFrameInterpolate_F_Std

Interpolates between two keyframes of the standard float scheme: unpacks them and lets RpHAnim interpolate. The
custom data is the animation's StCompAnimData_std_f.
====================
*/
// 0x801E0860
void KeyFrameInterpolate_F_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData ) {
    RpHAnimKeyFrame KeyIn1;
    RpHAnimKeyFrame KeyIn2;

    DeCompressKey_Std_F( (StCompAnim_Std_F_KeyFrame*)voidIn1, &KeyIn1, (StCompAnimData_std_f*)customData );
    DeCompressKey_Std_F( (StCompAnim_Std_F_KeyFrame*)voidIn2, &KeyIn2, (StCompAnimData_std_f*)customData );
    RpHAnimKeyFrameInterpolate( voidOut, &KeyIn1, &KeyIn2, time, NULL );
}

/*
====================
KeyFrameInterpolate_P_Std

Interpolates between two keyframes of the 16-bit-rotation standard scheme: unpacks them and lets RpHAnim
interpolate.
====================
*/
// 0x801E0AD8
void KeyFrameInterpolate_P_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData ) {
    RpHAnimKeyFrame KeyIn1;
    RpHAnimKeyFrame KeyIn2;

    DeCompressKey_Std_P( (StCompAnim_Std_P_KeyFrame*)voidIn1, &KeyIn1 );
    DeCompressKey_Std_P( (StCompAnim_Std_P_KeyFrame*)voidIn2, &KeyIn2 );
    RpHAnimKeyFrameInterpolate( voidOut, &KeyIn1, &KeyIn2, time, NULL );
}

/*
====================
KeyFrameInterpolate_F_Full

Interpolates between two keyframes of the full float scheme: unpacks them and lets the full HAnim scheme
(spfAnim_Full_KeyFrameInterpolate) interpolate. The custom data is the animation's StCompAnimData_full_f.
====================
*/
// 0x801E0C44
void KeyFrameInterpolate_F_Full( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData ) {
    HAnimFullKeyFrame KeyIn1;
    HAnimFullKeyFrame KeyIn2;

    DeCompressKey_Full_F( (StCompAnim_Full_F_KeyFrame*)voidIn1, &KeyIn1, (StCompAnimData_full_f*)customData );
    DeCompressKey_Full_F( (StCompAnim_Full_F_KeyFrame*)voidIn2, &KeyIn2, (StCompAnimData_full_f*)customData );
    spfAnim_Full_KeyFrameInterpolate( voidOut, &KeyIn1, &KeyIn2, time, NULL );
}

/*
====================
DeCompressKey_Std_V2

Unpacks a keyframe of the variable-length standard scheme: the time as a 13-bit fraction of the interval's end
time, the rotation's x, y and z as 16-bit fractions of the range -1 to 1 and its real part rebuilt from them
(negated by the key's sign bit), the translation as 16-bit fractions of the range from Key1 to Key1 plus Key2.
====================
*/
// no copy in retail (inlined)
static void DeCompressKey_Std_V2( RpHAnimKeyFrame* DstKey, int Tx, int Ty, int Tz, int Qx, int Qy, int Qz, unsigned int Key,
                                  StCompAnim_Std_V2_KeyFrame* Key1, StCompAnim_Std_V2_KeyFrame* Key2 ) {
    DstKey->time = (float)( Key & COMPANIM_V2_KEY_TIME_MASK ) / COMPANIM_V2_TIME_RANGE * Key2->time;

    DstKey->t.x = Key1->x + (float)Tx / 65535.0f * Key2->x;
    DstKey->t.y = Key1->y + (float)Ty / 65535.0f * Key2->y;
    DstKey->t.z = Key1->z + (float)Tz / 65535.0f * Key2->z;

    DstKey->q.imag.x = (float)Qx / 32767.5f - 1.0f;
    DstKey->q.imag.y = (float)Qy / 32767.5f - 1.0f;
    DstKey->q.imag.z = (float)Qz / 32767.5f - 1.0f;

    float l = 1.0f - ( DstKey->q.imag.x * DstKey->q.imag.x + DstKey->q.imag.y * DstKey->q.imag.y +
                       DstKey->q.imag.z * DstKey->q.imag.z );
    if ( l < 0.0f ) {
        l = 0.0f;
    }
    DstKey->q.real = (float)sqrt( l );
    if ( Key & COMPANIM_V2_KEY_REAL_NEGATIVE ) {
        DstKey->q.real *= -1.0f;
    }

    DstKey->prevFrame = NULL;
}

/*
====================
KeyFrameInterpolate_V2_Std

Interpolates between the keyframes of the variable-length standard scheme that surround a time. In1 is the
first whole keyframe of the node, In2 the second: In1's key data says which of the translation parts the packed
words of the following keyframes carry (bits 29 to 31), as an offset from In1 (bits 0 to 28). Each packed key
is a word of the time (bits 0 to 12), the rotation's sign (13), whether a rotation follows (14) and whether the
translation parts follow (15), then those 16-bit parts. The time scales by In2's time, which holds the end of
the interval. The first key at or after the time and the one before it are unpacked and interpolated by RpHAnim;
a key exactly at the time is used as it is.
====================
*/
// 0x801E0F80
void KeyFrameInterpolate_V2_Std( void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData ) {
    StCompAnim_Std_V2_KeyFrame* KeyIn1 = (StCompAnim_Std_V2_KeyFrame*)voidIn1;
    StCompAnim_Std_V2_KeyFrame* KeyIn2 = (StCompAnim_Std_V2_KeyFrame*)voidIn2;

    int IntTime = (int)( COMPANIM_V2_TIME_RANGE * ( time / KeyIn2->time ) );
    int KeyData = KeyIn1->PrevData.KeyData;

    unsigned short* SrcData = (unsigned short*)( (char*)KeyIn1 + ( KeyData & COMPANIM_V2_OFFSET_MASK ) );

    int CurrentTransX = 0;
    int CurrentTransY = 0;
    int CurrentTransZ = 0;
    int CurrentQuartX = COMPANIM_V2_QUART_ZERO;
    int CurrentQuartY = COMPANIM_V2_QUART_ZERO;
    int CurrentQuartZ = COMPANIM_V2_QUART_ZERO;
    int CurrentKey;

    do {
        CurrentKey = *SrcData++;
        if ( CurrentKey & COMPANIM_V2_KEY_HAS_TRANS ) {
            if ( KeyData & COMPANIM_V2_DATA_TRANS_X ) {
                CurrentTransX = *SrcData++;
            }
            if ( KeyData & COMPANIM_V2_DATA_TRANS_Y ) {
                CurrentTransY = *SrcData++;
            }
            if ( KeyData & COMPANIM_V2_DATA_TRANS_Z ) {
                CurrentTransZ = *SrcData++;
            }
        }
        if ( CurrentKey & COMPANIM_V2_KEY_HAS_QUART ) {
            CurrentQuartX = SrcData[0];
            CurrentQuartY = SrcData[1];
            CurrentQuartZ = SrcData[2];
            SrcData += 3;
        }
    } while ( ( *SrcData & COMPANIM_V2_KEY_TIME_MASK ) < IntTime );

    RpHAnimKeyFrame StdKey1;
    DeCompressKey_Std_V2( &StdKey1, CurrentTransX, CurrentTransY, CurrentTransZ, CurrentQuartX, CurrentQuartY,
                          CurrentQuartZ, CurrentKey, KeyIn1, KeyIn2 );

    if ( ( CurrentKey & COMPANIM_V2_KEY_TIME_MASK ) == IntTime ) {
        RpHAnimInterpFrame* pOut = (RpHAnimInterpFrame*)voidOut;

        pOut->q = StdKey1.q;
        pOut->t = StdKey1.t;
        return;
    }

    CurrentKey = *SrcData++;
    if ( CurrentKey & COMPANIM_V2_KEY_HAS_TRANS ) {
        if ( KeyData & COMPANIM_V2_DATA_TRANS_X ) {
            CurrentTransX = *SrcData++;
        }
        if ( KeyData & COMPANIM_V2_DATA_TRANS_Y ) {
            CurrentTransY = *SrcData++;
        }
        if ( KeyData & COMPANIM_V2_DATA_TRANS_Z ) {
            CurrentTransZ = *SrcData++;
        }
    }
    if ( CurrentKey & COMPANIM_V2_KEY_HAS_QUART ) {
        CurrentQuartX = SrcData[0];
        CurrentQuartY = SrcData[1];
        CurrentQuartZ = SrcData[2];
        SrcData += 3;
    }

    RpHAnimKeyFrame StdKey2;
    DeCompressKey_Std_V2( &StdKey2, CurrentTransX, CurrentTransY, CurrentTransZ, CurrentQuartX, CurrentQuartY,
                          CurrentQuartZ, CurrentKey, KeyIn1, KeyIn2 );
    if ( StdKey2.time < time ) {
        StdKey2.time = time;
    }
    RpHAnimKeyFrameInterpolate( voidOut, &StdKey1, &StdKey2, time, NULL );
}
/*
====================
CompressedAnimRegister

Registers the compressed schemes with RtAnim: the standard float scheme, the variable-length standard scheme
and the 16-bit-rotation standard scheme (all of RpHAnim's matrix callbacks), and the full float scheme (a copy
of the full HAnim scheme's callbacks). Returns false when RtAnim refuses one.
====================
*/
// 0x801E13F8
int CompressedAnimRegister() {
    RtAnimInterpolatorInfo info;
    RtAnimInterpolatorInfo* CompInfo;

    info.typeID = COMPANIM_STD_F_TYPE_ID;
    info.interpKeyFrameSize = sizeof( RpHAnimInterpFrame );
    info.animKeyFrameSize = sizeof( StCompAnim_Std_F_KeyFrame );
    info.keyFrameApplyCB = RpHAnimKeyFrameApply;
    info.keyFrameBlendCB = RpHAnimKeyFrameBlend;
    info.keyFrameInterpolateCB = KeyFrameInterpolate_F_Std;
    info.keyFrameAddCB = RpHAnimKeyFrameAdd;
    info.keyFrameMulRecipCB = NULL;
    info.keyFrameStreamReadCB = KeyFrameStreamRead_F;
    info.keyFrameStreamWriteCB = KeyFrameStreamWrite_F;
    info.keyFrameStreamGetSizeCB = KeyFrameStreamGetSize;
    info.customDataSize = sizeof( StCompAnimData_std_f );
    info.field_0x30 = 0;
    if ( !RtAnimRegisterInterpolationScheme( &info ) ) {
        return 0;
    }

    info.typeID = COMPANIM_STD_V2_TYPE_ID;
    info.interpKeyFrameSize = sizeof( RpHAnimInterpFrame );
    info.animKeyFrameSize = sizeof( StCompAnim_Std_V2_KeyFrame );
    info.keyFrameApplyCB = RpHAnimKeyFrameApply;
    info.keyFrameBlendCB = RpHAnimKeyFrameBlend;
    info.keyFrameInterpolateCB = KeyFrameInterpolate_V2_Std;
    info.keyFrameAddCB = RpHAnimKeyFrameAdd;
    info.keyFrameMulRecipCB = NULL;
    info.keyFrameStreamReadCB = KeyFrameStreamRead_V2;
    info.keyFrameStreamWriteCB = KeyFrameStreamWrite_V2;
    info.keyFrameStreamGetSizeCB = KeyFrameStreamGetSize;
    info.customDataSize = 0;
    info.field_0x30 = 0;
    if ( !RtAnimRegisterInterpolationScheme( &info ) ) {
        return 0;
    }

    info.typeID = COMPANIM_STD_P_TYPE_ID;
    info.interpKeyFrameSize = sizeof( RpHAnimInterpFrame );
    info.animKeyFrameSize = sizeof( StCompAnim_Std_P_KeyFrame );
    info.keyFrameApplyCB = RpHAnimKeyFrameApply;
    info.keyFrameBlendCB = RpHAnimKeyFrameBlend;
    info.keyFrameInterpolateCB = KeyFrameInterpolate_P_Std;
    info.keyFrameAddCB = RpHAnimKeyFrameAdd;
    info.keyFrameMulRecipCB = NULL;
    info.keyFrameStreamReadCB = KeyFrameStreamRead_P;
    info.keyFrameStreamWriteCB = KeyFrameStreamWrite_P;
    info.keyFrameStreamGetSizeCB = KeyFrameStreamGetSize_P;
    info.customDataSize = 0;
    info.field_0x30 = 0;
    if ( !RtAnimRegisterInterpolationScheme( &info ) ) {
        return 0;
    }

    CompInfo = RtAnimGetInterpolatorInfo( HANIMFULLKEYFRAME_TYPE_ID );
    if ( CompInfo != NULL ) {
        info = *CompInfo;
        spfAnim_Full_KeyFrameInterpolate = info.keyFrameInterpolateCB;
        info.typeID = COMPANIM_FULL_F_TYPE_ID;
        info.animKeyFrameSize = sizeof( StCompAnim_Full_F_KeyFrame );
        info.keyFrameInterpolateCB = KeyFrameInterpolate_F_Full;
        info.keyFrameMulRecipCB = NULL;
        info.keyFrameStreamReadCB = KeyFrameStreamRead_F;
        info.keyFrameStreamWriteCB = KeyFrameStreamWrite_F;
        info.keyFrameStreamGetSizeCB = KeyFrameStreamGetSize;
        info.customDataSize = sizeof( StCompAnimData_full_f );
        info.field_0x30 = 0;
        if ( !RtAnimRegisterInterpolationScheme( &info ) ) {
            return 0;
        }
    }
    return 1;
}
