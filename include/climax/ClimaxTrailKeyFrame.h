// Names and layout: Gormiti link map and DWARF (ClimaxTrailKeyFrame.obj)
#ifndef CLIMAX_CLIMAXTRAILKEYFRAME_H
#define CLIMAX_CLIMAXTRAILKEYFRAME_H

#include <rwsdk/tool/anim/rtanim.h>

/*
====================
ClimaxTrailKeyFrame

A keyframe of a ClimaxTrail animation: the length and intensity of a trail at a time, following the
keyframe of the same trail that it replaces.
====================
*/
class ClimaxTrailKeyFrame {
public:
    ClimaxTrailKeyFrame* prevFrame; // 0x0
    float time;                     // 0x4
    float length;                   // 0x8
    float intensity;                // 0xC
};

/*
====================
ClimaxTrailInterpFrame

An interpolated keyframe: the pair of keyframes it lies between and the values at its time.
====================
*/
class ClimaxTrailInterpFrame {
public:
    ClimaxTrailKeyFrame* keyframe1; // 0x0
    ClimaxTrailKeyFrame* keyframe2; // 0x4
    float length;                   // 0x8
    float intensity;                // 0xC
};

/*
====================
ClimaxTrailFileKeyFrame

A keyframe as it is streamed: the previous keyframe is its index in the animation, -1 for none.
====================
*/
class ClimaxTrailFileKeyFrame {
public:
    int prevFrame;   // 0x0
    float time;      // 0x4
    float length;    // 0x8
    float intensity; // 0xC
};

extern "C" {
void ClimaxTrailKeyFrameRegisterInterpolationScheme();
}

#endif
