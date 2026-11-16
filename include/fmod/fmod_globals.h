/*
 * FMOD's global objects (fmod_globals.cpp): the global state, the head of the list of systems and
 * the pool the systems allocate from. Names from Gormiti's link map.
 */
#ifndef _FMOD_GLOBALS_H
#define _FMOD_GLOBALS_H

#include "fmod_memory.h"
#include "fmod_systemi.h"

namespace FMOD {

extern Global  gGlobalMem;          // the state lbl_80529020 points at
extern SystemI gSystemHeadMem;      // the head of the list of systems (Global::mSystemHead)
extern MemPool gSystemPoolMem;      // the global memory pool (Global::mMemPool)

} // namespace FMOD

#endif /* _FMOD_GLOBALS_H */
