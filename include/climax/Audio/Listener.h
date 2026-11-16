#ifndef CLIMAX_AUDIO_LISTENER_H
#define CLIMAX_AUDIO_LISTENER_H

// Names and layout: Gormiti link map and DWARF (Listener.obj,
// game/System/Audio/FMOD/Listener.h)

#include <revolution/types.h>

namespace Audio {

/*
 * A 3D listener of the audio system: the id of the FMOD listener slot it drives.
 */
class Listener {
public:
                    Listener();

    int             Create( int id );
    int             Update( float frameInterval, float* pos, float* vel, float* forward, float* up );

private:
    int             m_fmodID;   // 0x00, -1 until Create
};

}

#endif
