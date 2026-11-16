/*
 * FMOD::Listener: one 3D listener of a system (position, velocity and orientation, with the values of the
 * previous update). Members and layout from Gormiti's DWARF (sibling.exe types Listener).
 */
#ifndef _FMOD_LISTENER_H
#define _FMOD_LISTENER_H

#include <fmod/fmod.h>

namespace FMOD {

class Listener {
public:
    FMOD_VECTOR mPosition;      // 0x00
    FMOD_VECTOR mLastPosition;  // 0x0C
    FMOD_VECTOR mVelocity;      // 0x18
    FMOD_VECTOR mLastVelocity;  // 0x24
    FMOD_VECTOR mUp;            // 0x30
    FMOD_VECTOR mLastUp;        // 0x3C
    FMOD_VECTOR mFront;         // 0x48
    FMOD_VECTOR mLastFront;     // 0x54
    FMOD_VECTOR mRight;         // 0x60
    bool        mMoved;         // 0x6C
    bool        mRotated;       // 0x6D

    Listener();
};

} // namespace FMOD

#endif /* _FMOD_LISTENER_H */
