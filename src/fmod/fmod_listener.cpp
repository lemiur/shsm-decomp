/*
 * fmod_listener.cpp: FMOD::Listener, a 3D listener.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_listener.obj)

#include <fmod/fmod_listener.h>

namespace FMOD {

/*
====================
Listener::Listener

Places the listener at the origin, at rest, looking down +Z with +Y up. The values of the previous
update are not set.
====================
*/
Listener::Listener() {
    mPosition.x = 0.0f;
    mPosition.y = 0.0f;
    mPosition.z = 0.0f;

    mLastPosition.x = 0.0f;
    mLastPosition.y = 0.0f;
    mLastPosition.z = 0.0f;

    mVelocity.x = 0.0f;
    mVelocity.y = 0.0f;
    mVelocity.z = 0.0f;

    mUp.x = 0.0f;
    mUp.y = 1.0f;
    mUp.z = 0.0f;

    mFront.x = 0.0f;
    mFront.y = 0.0f;
    mFront.z = 1.0f;

    mRight.x = 1.0f;
    mRight.y = 0.0f;
    mRight.z = 0.0f;
} // 0x8033D21C

} // namespace FMOD
