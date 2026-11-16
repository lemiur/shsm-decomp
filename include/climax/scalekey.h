#ifndef CLIMAX_SCALEKEY_H
#define CLIMAX_SCALEKEY_H

// Names and layout: Gormiti link map and DWARF (scalekey.obj; HAnimFullKeyFrame from CompAnim.obj).

#include <rwsdk/plcore/bavector.h>
#include <rwsdk/tool/quat/rtquat.h>

// The plugin ID of the scheme (its typeID); invented name. CompAnim.cpp derives its full float scheme from it.
#define HANIMFULLKEYFRAME_TYPE_ID 0x44A07195

/*
====================
HAnimFullKeyFrame

A keyframe of the full HAnim scheme: the standard HAnim keyframe (time, rotation, translation) with a
scale. prevFrame is the keyframe it replaces.
====================
*/
struct HAnimFullKeyFrame {
    HAnimFullKeyFrame*  prevFrame;  // 0x00
    float               time;       // 0x04
    RtQuat              q;          // 0x08
    RwV3d               pos;        // 0x18
    RwV3d               scale;      // 0x24
};

/*
====================
HAnimFullInterpFrame

An interpolated frame of the full HAnim scheme: the two keyframes it lies between, and the rotation,
translation and scale at that time.
====================
*/
struct HAnimFullInterpFrame {
    HAnimFullKeyFrame*  keyFrame1;  // 0x00
    HAnimFullKeyFrame*  keyFrame2;  // 0x04
    RtQuat              q;          // 0x08
    RwV3d               pos;        // 0x18
    RwV3d               scale;      // 0x24
};

extern "C" {
int HAnimFullKeyFrameRegister();
}

#endif
