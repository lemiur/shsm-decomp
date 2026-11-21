// Names and layout: Gormiti link map and DWARF (DynamicTrails.obj).
#ifndef GAME_DYNAMICTRAILS_H
#define GAME_DYNAMICTRAILS_H

#include <rwsdk/tool/anim/rtanim.h>
#include <rwsdk/world/baclump.h>

namespace Effects {

/*
====================
Effects::TrailAnimInterpolator

Plays the animation of a dynamic trail: the animation, the time it has reached, the RtAnim
interpolator that samples it, and the trail's interpolated length and intensity.
====================
*/
class TrailAnimInterpolator {
public:
    void AddTime( float deltaTime );

    RtAnimAnimation* m_Anim;            // 0x0
    float m_CurrentTime;                // 0x4
    RtAnimInterpolator* m_Interpolator; // 0x8
    float m_Data[ 2 ];                  // 0xC: length and intensity
};

RpAtomic* FindFirstAtomic( RpAtomic* atomic, void* data );

}   // namespace Effects

/*
====================
unk_TrailNode

A node of the trail list that fn_8007F20C walks: the next node is the first word. The members
after it are read by fn_8007F044 and are not yet recovered. Name invented.
====================
*/
class unk_TrailNode {
public:
    unk_TrailNode* next; // 0x0
};

// The head of the trail list. Defined in DynamicTrails.cpp.
extern unk_TrailNode* lbl_80529608;

extern "C" {
void fn_8007F044( unk_TrailNode* node, void* param1, int param2 );
void fn_8007F20C( void* param1, int param2 );
}

#endif
