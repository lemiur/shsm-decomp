// Names and layout: Gormiti link map and DWARF (ClimaxT1KeyFrame.obj)
#ifndef CLIMAX_CLIMAXT1KEYFRAME_H
#define CLIMAX_CLIMAXT1KEYFRAME_H

#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/tool/anim/rtanim.h>

/*
====================
ClimaxT1KeyFrame

A keyframe of a ClimaxT1 animation: the colour of a material at a time, following the keyframe of
the same material that it replaces.
====================
*/
class ClimaxT1KeyFrame {
public:
    ClimaxT1KeyFrame* prevFrame; // 0x0
    float time;                  // 0x4
    RwRGBAReal color;            // 0x8
};

/*
====================
ClimaxT1InterpFrame

An interpolated keyframe: the pair of keyframes it lies between and the colour at its time.
====================
*/
class ClimaxT1InterpFrame {
public:
    ClimaxT1KeyFrame* keyframe1; // 0x0
    ClimaxT1KeyFrame* keyframe2; // 0x4
    RwRGBAReal color;            // 0x8
};

/*
====================
ClimaxT1FileKeyFrame

A keyframe as it is streamed: the previous keyframe is its index in the animation, -1 for none.
====================
*/
class ClimaxT1FileKeyFrame {
public:
    int prevFrame;    // 0x0
    float time;       // 0x4
    RwRGBAReal color; // 0x8
};

extern "C" {
int ClimaxT1KeyFrameRegisterInterpolationScheme();
}

#endif
