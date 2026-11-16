/*
 * fmod_soundgroupi.cpp: FMOD::SoundGroupI, the internal sound group. Allocation tags carry the
 * original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_soundgroupi.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_soundgroupi.h>
#include <fmod/fmod_soundi.h>

namespace FMOD {

/*
====================
SoundGroupI::SoundGroupI

An empty group: no name, no limit on the sounds playing, no fade and full volume.
====================
*/
SoundGroupI::SoundGroupI() {
    mMaxAudible = -1;
    mName       = NULL;
    mFadeSpeed  = 0.0f;
    mVolume     = 1.0f;
} // 0x80344E40

/*
====================
SoundGroupI::releaseInternal

Frees the name, unlinks the group from the system's list and frees it.
====================
*/
FMOD_RESULT SoundGroupI::releaseInternal() {
    if ( mName ) {
#line 129
        FMOD_Memory_Free( mName );
    }

    removeNode();

#line 134
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x80344E80

/*
====================
SoundGroupI::getNumPlaying

Totals the channels playing from the sounds of the group.
====================
*/
FMOD_RESULT SoundGroupI::getNumPlaying( int *numplaying ) {
    LinkedListNode *current;

    if ( !numplaying ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    current = mSoundHead.getNext();

    *numplaying = 0;

    while ( current != &mSoundHead ) {
        SoundI *sound = static_cast< SoundI * >( current->mNodeData );

        *numplaying += sound->mNumAudible;

        current = current->getNext();
    }

    return FMOD_OK;
} // 0x80344F1C

} // namespace FMOD
