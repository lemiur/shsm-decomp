// Names and layout: Gormiti link map and DWARF (Listener.obj). Gormiti's source path is
// game/System/Audio/FMOD/Listener.cpp; the unit keeps the path it was created under.

#include <climax/Audio/Listener.h>
#include <climax/Audio/System.h>

namespace Audio {

/*
====================
Listener::Listener

A listener belongs to no FMOD listener slot until Create.
====================
*/
// 0x801FDB50
Listener::Listener() :
    m_fmodID( -1 ) {
}

/*
====================
Listener::Create

Binds the listener to FMOD listener slot id.
====================
*/
// 0x801FDB5C
int Listener::Create( int id ) {
    m_fmodID = id;
    return 0;
}

/*
====================
Listener::Update

Hands the listener's position, velocity and orientation to the event system. A position or
velocity that is not supplied is read back from FMOD (the velocity as the move since the
position FMOD held before).
====================
*/
// 0x801FDB68
int Listener::Update( float frameInterval, float* pos, float* vel, float* forward, float* up ) {
    FMOD_VECTOR position;
    FMOD_VECTOR velocity;

    // A negative interval means "the last frame's". Nothing reads frameInterval afterwards: retail keeps
    // the compare and drops the assignment (Gormiti Debug line 31, the same statement).
    if ( frameInterval < 0.0f ) {
        frameInterval = System::GetInstance()->GetLastFrameInterval();
    }

    if ( pos != NULL ) {
        position.x = pos[0]; position.y = pos[1]; position.z = pos[2];
    } else {
        System::GetInstance()->m_fmodSystem->get3DListenerAttributes( m_fmodID, &position, NULL, NULL, NULL );
    }

    if ( vel != NULL ) {
        velocity.x = vel[0]; velocity.y = vel[1]; velocity.z = vel[2];
    } else {
        FMOD_VECTOR lastPosition;
        System::GetInstance()->m_fmodSystem->get3DListenerAttributes( m_fmodID, &lastPosition, NULL, NULL, NULL );
        velocity.x = position.x - lastPosition.x;
        velocity.y = position.y - lastPosition.y;
        velocity.z = position.z - lastPosition.z;
    }

    FMOD_VECTOR* fmodForward = (FMOD_VECTOR*)forward;
    FMOD_VECTOR* fmodUp = (FMOD_VECTOR*)up;
    System::GetInstance()->m_fmodEventSystem->set3DListenerAttributes( m_fmodID, &position, &velocity, fmodForward, fmodUp );
    return 0;
}

}
