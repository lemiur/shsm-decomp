/*
 * fmod_globals.cpp: FMOD's global state, the head of the list of systems and the global memory pool.
 * The static initialiser constructs the three objects; SystemI's implicit destructor is emitted here,
 * the first unit that registers one for destruction.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_globals.obj)

#include <fmod/fmod_globals.h>

namespace FMOD {

Global  gGlobalMem;
SystemI gSystemHeadMem;
MemPool gSystemPoolMem;

} // namespace FMOD

FMOD::Global *lbl_80529020 = &FMOD::gGlobalMem;

namespace FMOD {

/*
====================
Global::init

Points the state at the system list head and the pool and clears the rest. Inlined into the
constructor of gGlobalMem; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
void Global::init() {
    mSystemHead        = &gSystemHeadMem;
    mMemPool           = &gSystemPoolMem;
    mDSPClock.mHi      = 0;
    mDSPClock.mLo      = 0;
    mDSPClockTimeStamp = 0;
    mSystemInitCount   = 0;
    mFileBusy          = 0;
    mSystemCallback    = NULL;
    mMemoryTypeFlags   = 0;
    mFileCrit          = NULL;
}

} // namespace FMOD
