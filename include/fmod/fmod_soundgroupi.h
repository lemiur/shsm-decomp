/*
 * FMOD::SoundGroupI, the internal object behind a sound group: a list of sounds that share a volume
 * and a limit on how many play at once (fmod_soundgroupi.cpp). Declared in the order of Gormiti's link
 * map; only the functions retail kept are declared.
 * Layout: Gormiti DWARF (sibling.exe types SoundGroupI), size 0x38. No vtable.
 */
#ifndef _FMOD_SOUNDGROUPI_H
#define _FMOD_SOUNDGROUPI_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class SystemI;

class SoundGroupI : public LinkedListNode {
public:
    SystemI        *mSystem;            // 0x0C
    void           *mUserData;          // 0x10
    LinkedListNode  mSoundHead;         // 0x14, list head of the sounds in the group
    char           *mName;              // 0x20
    int             mMaxAudible;        // 0x24, -1 for no limit
    int             mMaxAudibleBehavior; // 0x28, FMOD_SOUNDGROUP_BEHAVIOR (type not declared yet)
    int             mPlayCount;         // 0x2C
    float           mFadeSpeed;         // 0x30
    float           mVolume;            // 0x34

    SoundGroupI();
    FMOD_RESULT releaseInternal();
    FMOD_RESULT getNumPlaying( int *numplaying );
};

} // namespace FMOD

#endif /* _FMOD_SOUNDGROUPI_H */
