/*
 * fmod_channelgroupi.cpp: FMOD::ChannelGroupI, the internal channel group behind FMOD::ChannelGroup.
 * A group holds child groups, channels, group-wide volume/pitch/pause/mute state and an optional DSP
 * head inserted before its mix target.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_channelgroupi.obj)
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_channelgroupi.obj)

#include <new>

#include <fmod/fmod.h>
#include <fmod/fmod_channelgroupi.h>
#include <fmod/fmod_channeli.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_output.h>
#include <fmod/fmod_systemi.h>

namespace FMOD {

/*
====================
ChannelGroupI::validate

Checks a public ChannelGroup handle and returns its internal ChannelGroupI.
====================
*/
FMOD_RESULT ChannelGroupI::validate( ChannelGroup *channelgroup, ChannelGroupI **channelgroupi ) {
    if ( !channelgroup ) {
        return FMOD_ERR_INVALID_HANDLE;
    }
    if ( !channelgroupi ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channelgroupi = (ChannelGroupI *)channelgroup;

    return FMOD_OK;
} // 0x80335198

/*
====================
ChannelGroupI::release

Releases a user channel group; the master channel group cannot be released directly.
====================
*/
FMOD_RESULT ChannelGroupI::release() {
    if ( this == mSystem->mChannelGroup ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    return releaseInternal( false );
} // 0x803351C4

/*
====================
ChannelGroupI::releaseInternal

Releases this group, optionally recursing into child groups, moving channels back to the master
group, tearing down its DSP head and freeing its name, child list head and memory.
====================
*/
FMOD_RESULT ChannelGroupI::releaseInternal( bool releasechildren ) {
    if ( mGroupHead && releasechildren ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            ChannelGroupI *next = (ChannelGroupI *)currentgroup->getNext();
            currentgroup->releaseInternal( true );
            currentgroup = next;
        }
    }

    if ( mSystem->mChannelGroup && this != mSystem->mChannelGroup ) {
        while ( mChannelHead.getNext() != &mChannelHead ) {
            ChannelI *channel = (ChannelI *)mChannelHead.getNext()->getData();
            channel->setChannelGroup( mSystem->mChannelGroup );
        }
    }

    if ( mDSPHead && mDSPMixTarget && mDSPMixTarget != mDSPHead ) {
        mDSPMixTarget->release( false );
        mDSPMixTarget = NULL;
        if ( mDSPHead ) {
            mDSPHead->release( true );
            mDSPHead = NULL;
        }
    } else {
        if ( mDSPHead ) {
            mDSPHead->release( false );
            mDSPHead = NULL;
        }
        mDSPMixTarget = NULL;
    }

    if ( mName ) {
#line 152
        FMOD_Memory_Free( mName );
    }

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        ChannelGroupI *mastergroup;
        mSystem->getMasterChannelGroup( &mastergroup );
        if ( mastergroup && this != mastergroup ) {
            while ( currentgroup != mGroupHead ) {
                ChannelGroupI *next = (ChannelGroupI *)currentgroup->getNext();
                mastergroup->addGroup( currentgroup );
                currentgroup = next;
            }
        }
#line 177
        FMOD_Memory_Free( mGroupHead );
    }

    if ( this == mSystem->mOutput->mMusicChannelGroup ) {
        mSystem->mOutput->mMusicChannelGroup = NULL;
    }

    removeNode();
#line 187
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803351EC

/*
====================
ChannelGroupI::setVolume

Clamps the group's volume to [0, 1] and propagates the effective volume down the hierarchy.
====================
*/
FMOD_RESULT ChannelGroupI::setVolume( float volume ) {
    FMOD_RESULT result;

    if ( volume < 0.0f ) {
        volume = 0.0f;
    }
    if ( volume > 1.0f ) {
        volume = 1.0f;
    }

    mVolume = volume;

    result = setVolumeInternal();
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x8033543C

/*
====================
ChannelGroupI::setVolumeInternal

Recomputes mRealVolume from the parent group and updates all child groups and channels.
====================
*/
FMOD_RESULT ChannelGroupI::setVolumeInternal() {
    LinkedListNode *current;
    bool            forceupdatepos = false;
    float           newvolume;

    newvolume = mVolume * ( mParent ? mParent->mRealVolume : 1.0f );
    if ( newvolume != mRealVolume ) {
        forceupdatepos = true;
    }
    mRealVolume = newvolume;

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->setVolumeInternal();
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        float     v;
        ChannelI *channel = (ChannelI *)current->getData();

        channel->getVolume( &v );
        channel->setVolume( v, forceupdatepos );

        current = current->getNext();
    }

    return FMOD_OK;
} // 0x80335490

/*
====================
ChannelGroupI::getVolume
====================
*/
FMOD_RESULT ChannelGroupI::getVolume( float *volume ) {
    if ( !volume ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *volume = mVolume;

    return FMOD_OK;
} // 0x80335570

/*
====================
ChannelGroupI::setPitch

Sets the group's pitch multiplier (clamped at 0) and updates effective pitch down the hierarchy.
====================
*/
FMOD_RESULT ChannelGroupI::setPitch( float pitch ) {
    if ( pitch < 0.0f ) {
        pitch = 0.0f;
    }

    mPitch = pitch;

    return setPitchInternal();
} // 0x80335590

/*
====================
ChannelGroupI::setPitchInternal

Recomputes mRealPitch from the parent group and refreshes frequencies on child groups and channels.
====================
*/
FMOD_RESULT ChannelGroupI::setPitchInternal() {
    LinkedListNode *current;

    mRealPitch = mPitch * ( mParent ? mParent->mRealPitch : 1.0f );

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->setPitchInternal();
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        ChannelI *channel = (ChannelI *)current->getData();
        float     freq;

        channel->getFrequency( &freq );
        channel->setFrequency( freq );

        current = current->getNext();
    }

    return FMOD_OK;
} // 0x803356D8

/*
====================
ChannelGroupI::setPaused

Updates this group's paused state (when storeState is set) and refreshes all child groups and
channels.
====================
*/
FMOD_RESULT ChannelGroupI::setPaused( bool paused, bool storeState ) {
    LinkedListNode *current;

    if ( storeState ) {
        mPaused = paused;
    }

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->setPaused( paused, false );
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        ChannelI *channel = (ChannelI *)current->getData();

        channel->setPaused( ( channel->mFlags & CHANNELI_FLAG_0x1 ) ? true : false );

        current = current->getNext();
    }

    return FMOD_OK;
} // 0x8033580C

/*
====================
ChannelGroupI::getPaused
====================
*/
FMOD_RESULT ChannelGroupI::getPaused( bool *paused ) {
    if ( !paused ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *paused = mPaused;

    return FMOD_OK;
} // 0x803358F0

/*
====================
ChannelGroupI::setMute

Updates this group's mute state (when storeState is set) and refreshes all child groups and
channels.
====================
*/
FMOD_RESULT ChannelGroupI::setMute( bool mute, bool storeState ) {
    LinkedListNode *current;

    if ( storeState ) {
        mMute = mute;
    }

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->setMute( mute, false );
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        ChannelI *channel = (ChannelI *)current->getData();

        channel->setMute( ( channel->mFlags & CHANNELI_FLAG_0x2 ) ? true : false );

        current = current->getNext();
    }

    return FMOD_OK;
} // 0x80335910

/*
====================
ChannelGroupI::getMute
====================
*/
FMOD_RESULT ChannelGroupI::getMute( bool *mute ) {
    if ( !mute ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *mute = mMute;

    return FMOD_OK;
} // 0x803359F4

/*
====================
ChannelGroupI::stop

Stops all child groups and channels in this group.
====================
*/
FMOD_RESULT ChannelGroupI::stop() {
    LinkedListNode *current;

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->stop();
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        LinkedListNode *next = current->getNext();
        ((ChannelI *)current->getData())->stop();
        current = next;
    }

    return FMOD_OK;
} // 0x80335A14

/*
====================
ChannelGroupI::overrideReverbProperties

Applies reverb channel properties to all child groups and channels.
====================
*/
FMOD_RESULT ChannelGroupI::overrideReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    LinkedListNode *current;

    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->overrideReverbProperties( prop );
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        ((ChannelI *)current->getData())->setReverbProperties( prop );
        current = current->getNext();
    }

    return FMOD_OK;
} // 0x80335B20

/*
====================
ChannelGroupI::overrideSpeakerMix

Applies speaker mix levels to all child groups and channels.
====================
*/
FMOD_RESULT ChannelGroupI::overrideSpeakerMix( float frontleft, float frontright, float center, float lfe,
                                               float backleft, float backright, float sideleft, float sideright ) {
    LinkedListNode *current;

    if ( mGroupHead ) {
        ChannelGroupI *currentgroup = (ChannelGroupI *)mGroupHead->getNext();
        while ( currentgroup != mGroupHead ) {
            currentgroup->overrideSpeakerMix( frontleft, frontright, center, lfe, backleft, backright,
                                              sideleft, sideright );
            currentgroup = (ChannelGroupI *)currentgroup->getNext();
        }
    }

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        ((ChannelI *)current->getData())->setSpeakerMix( frontleft, frontright, center, lfe, backleft,
                                                          backright, sideleft, sideright, true );
        current = current->getNext();
    }

    return FMOD_OK;
} // 0x80335C54

/*
====================
ChannelGroupI::updateChildMixTarget

Propagates a new mix target DSP down child groups that lack their own DSP head and reconnects
their channels.
====================
*/
FMOD_RESULT ChannelGroupI::updateChildMixTarget( DSPI *dsp ) {
    FMOD_RESULT result;
    DSPI       *olddsp = mDSPMixTarget;

    if ( mDSPHead ) {
        return FMOD_OK;
    }

    mDSPMixTarget = dsp;

    if ( mGroupHead ) {
        LinkedListNode *node = mGroupHead->getNext();
        while ( node != mGroupHead ) {
            result = ((ChannelGroupI *)node)->updateChildMixTarget( dsp );
            if ( result != FMOD_OK ) {
                return result;
            }
            node = node->getNext();
        }
    }

    LinkedListNode *current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        DSPI     *dsphead;
        ChannelI *channel = (ChannelI *)current->getData();
        current = current->getNext();

        result = channel->getDSPHead( &dsphead );
        if ( result == FMOD_OK ) {
            result = olddsp->disconnectFrom( dsphead, NULL );
            if ( result != FMOD_OK ) {
                return result;
            }
            result = channel->setChannelGroupInternal( this, true, true );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    return FMOD_OK;
} // 0x80335DC0

/*
====================
ChannelGroupI::addGroup

Attaches group as a child of this group, wiring its DSP head or mix target and inheriting pause,
mute, volume and pitch state.
====================
*/
FMOD_RESULT ChannelGroupI::addGroup( ChannelGroupI *group ) {
    FMOD_RESULT result;

    if ( !group ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    group->removeNode();

    if ( group->mDSPHead ) {
        group->mDSPHead->disconnectAll( false, true );
    }

    if ( !mGroupHead ) {
#line 1185
        mGroupHead = new ( FMOD_Memory_Calloc( sizeof( ChannelGroupI ) ) ) ChannelGroupI;
        if ( !mGroupHead ) {
            return FMOD_ERR_MEMORY;
        }
    }

    group->addBefore( mGroupHead );

    if ( mDSPMixTarget ) {
        if ( group->mDSPHead ) {
            result = mDSPMixTarget->addInputQueued( group->mDSPHead, false, NULL, NULL );
            if ( result != FMOD_OK ) {
                return result;
            }
        } else {
            result = group->updateChildMixTarget( mDSPMixTarget );
            if ( result != FMOD_OK ) {
                return result;
            }
        }
    }

    group->mParent = this;

    result = group->setPaused( mPaused, false );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = group->setMute( mMute, false );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = group->setVolumeInternal();
    if ( result != FMOD_OK ) {
        return result;
    }

    result = group->setPitchInternal();
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x80335EB4

/*
====================
ChannelGroupI::getDSPHead
====================
*/
FMOD_RESULT ChannelGroupI::getDSPHead( DSPI **dsp ) {
    if ( !dsp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !mDSPHead ) {
        *dsp = NULL;
        return FMOD_ERR_DSP_NOTFOUND;
    }

    *dsp = mDSPHead;

    return FMOD_OK;
} // 0x803362B4

/*
====================
ChannelGroupI::addDSP

Inserts a DSP unit before this group's DSP head, splitting a shared mix-target head into a
dedicated head if needed.
====================
*/
FMOD_RESULT ChannelGroupI::addDSP( DSPI *dsp, DSPConnectionI **dspconnection ) {
    FMOD_RESULT result;

    if ( !dsp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !mDSPHead ) {
        return FMOD_ERR_DSP_NOTFOUND;
    }

    if ( mDSPHead == mDSPMixTarget ) {
        DSPI                *output;
        FMOD_DSP_DESCRIPTION description = mDSPHead->mDescription;

        result = mSystem->createDSP( &description, &mDSPHead );
        if ( result != FMOD_OK ) {
            return result;
        }

        mDSPHead->setDefaults( (float)mSystem->mOutputRate, -1.0f, -1.0f, -1 );
        mDSPHead->setActive( true );

        result = mDSPMixTarget->getOutput( 0, &output, NULL, true );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = output->disconnectFrom( mDSPMixTarget, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = output->addInput( mDSPHead, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }

        result = mDSPHead->addInput( mDSPMixTarget, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    result = mDSPHead->insertInputBetween( dsp, 0, dspconnection );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x803362EC

/*
====================
ChannelGroupI::getNumChannels
====================
*/
FMOD_RESULT ChannelGroupI::getNumChannels( int *numchannels ) {
    if ( !numchannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numchannels = mNumChannels;

    return FMOD_OK;
} // 0x803364AC

/*
====================
ChannelGroupI::getChannel
====================
*/
FMOD_RESULT ChannelGroupI::getChannel( int index, Channel **channel ) {
    int             count = 0;
    LinkedListNode *current;

    if ( !channel ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *channel = NULL;

    current = mChannelHead.getNext();
    while ( current != &mChannelHead ) {
        if ( count == index ) {
            *channel = (Channel *)((ChannelI *)current->getData())->mHandleCurrent;
            return FMOD_OK;
        }
        current = current->getNext();
        count++;
    }

    return FMOD_ERR_INVALID_PARAM;
} // 0x803364CC

} // namespace FMOD
