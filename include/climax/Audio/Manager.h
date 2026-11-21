#ifndef CLIMAX_AUDIO_MANAGER_H
#define CLIMAX_AUDIO_MANAGER_H

// Names: the dtk signature names of the Audio::Manager members (config/R5WEA4/symbols.txt). Only the member
// used so far is declared; the rest of the class is not reconstructed yet.

#include <revolution/types.h>

namespace Audio {

class Manager {
public:
    static void InitWiiController(long channel);
};

}  // namespace Audio

#endif
