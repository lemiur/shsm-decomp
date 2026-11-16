/*
 * fmod_reverbi.cpp: FMOD::ReverbI, the internal reverb object behind FMOD::Reverb. A reverb has four
 * instances, each with its own reverb DSP, its properties and, for every real channel of the system, the
 * channel's properties and the connection that feeds the instance.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_reverbi.obj)

#include <string.h>

#include <fmod/fmod.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_output.h>
#include <fmod/fmod_pluginfactory.h>
#include <fmod/fmod_reverbi.h>
#include <fmod/fmod_systemi.h>

// The value of x kept between lo and hi.
#define FMOD_CLAMP( x, lo, hi ) ( ( x ) < ( lo ) ? ( lo ) : ( ( x ) > ( hi ) ? ( hi ) : ( x ) ) )

// The properties a channel's reverb sends start from (the public default channel properties).
#define FMOD_REVERB_CHANNELPROPERTIES_DEFAULT \
    { 0, 0, 0, 0, 0, 0.0f, 0, 0.25f, 1.5f, 1.0f, 0, 1.0f, 0, 0.0f, 0.0f, 0.0f, 1.0f, 0, NULL }

namespace FMOD {

/*
====================
ReverbI::ReverbI

Starts with no instances, no system and no position.
====================
*/
ReverbI::ReverbI() {
    for ( int count = 0; count < FMOD_REVERB_MAXINSTANCES; count++ ) {
        mInstance[count].mDSP         = NULL;
        mInstance[count].mChannelData = NULL;
    }

    mSystem                 = NULL;
    mPosition.x             = 0.0f;
    mPosition.y             = 0.0f;
    mPosition.z             = 0.0f;
    mMinRadius              = 0.0f;
    mMaxRadius              = 0.0f;
    mMinMaxDistance         = 0.0f;
    mGain                   = 0.0f;
    m3D                     = false;
    mDisableIfNoEnvironment = false;
    mActive                 = true;
    mUserData               = NULL;
} // 0x8033FE40

/*
====================
ReverbI::validate

The internal reverb behind a public handle.
====================
*/
FMOD_RESULT ReverbI::validate( Reverb *reverb, ReverbI **reverbi ) {
    if ( !reverbi ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    if ( !reverb ) {
        return FMOD_ERR_INVALID_HANDLE;
    }

    *reverbi = (ReverbI *)reverb;

    return FMOD_OK;
} // 0x8033FEAC

/*
====================
ReverbI::init

Releases whatever the reverb held, then binds it to system and gives its four instances the "off"
properties. Retail's body differs from Gormiti's (another revision).
====================
*/
FMOD_RESULT ReverbI::init( SystemI *system, bool is_3d, FMOD_REVERB_MODE mode ) {
    FMOD_REVERB_PROPERTIES prop = FMOD_PRESET_OFF;
    int                    instance;

    release( false );

    mSystem = system;
    mMode   = mode;

    for ( instance = 0; instance < FMOD_REVERB_MAXINSTANCES; instance++ ) {
        memset( &mInstance[instance].mProps, 0xCC, sizeof( FMOD_REVERB_PROPERTIES ) );
        mInstance[instance].mProps.Instance = instance;
        setProperties( &prop );
    }

    if ( !mInstance[0].mChannelData ) {
        FMOD_REVERB_CHANNELPROPERTIES defaultprop = {
            0, 0, 0, 0, 0, 0.0f, 0, 0.25f, 1.5f, 1.0f, 0, 1.0f, 0, 0.0f, 0.0f, 0.0f, 1.0f,
            FMOD_REVERB_CHANNELFLAGS_DIRECTHFAUTO | FMOD_REVERB_CHANNELFLAGS_ROOMAUTO |
                FMOD_REVERB_CHANNELFLAGS_ROOMHFAUTO | FMOD_REVERB_CHANNELFLAGS_INSTANCE0,
            NULL
        };
        int channel;

        defaultprop.Flags = ( defaultprop.Flags & ~FMOD_REVERB_CHANNELFLAGS_INSTANCE0 ) |
                            FMOD_REVERB_CHANNELFLAGS_INSTANCE0;

#line 138
        mInstance[0].mChannelData = (FMOD_REVERB_CHANNELDATA *)FMOD_Memory_Calloc( mSystem->mNumChannels * sizeof( FMOD_REVERB_CHANNELDATA ) );
        if ( !mInstance[0].mChannelData ) {
            return FMOD_ERR_MEMORY;
        }

        for ( channel = 0; channel < mSystem->mNumChannels; channel++ ) {
            memcpy( &mInstance[0].mChannelData[channel], &defaultprop, sizeof( FMOD_REVERB_CHANNELPROPERTIES ) );
            mInstance[0].mChannelData[channel].mDSPConnection = NULL;
            mInstance[0].mChannelData[channel].mPresenceGain  = 0.0f;
        }
    }

    mPosition.z     = 0.0f;
    mPosition.y     = 0.0f;
    mPosition.x     = 0.0f;
    mMinRadius      = 0.0f;
    mMaxRadius      = 0.0f;
    mMinMaxDistance = 0.0f;
    mGain           = 0.0f;
    m3D             = is_3d;

    if ( this == &system->mReverbGlobal ) {
        mDisableIfNoEnvironment = true;
    }

    return FMOD_OK;
} // 0x8033FED8

/*
====================
ReverbI::release

Frees the channel data and the reverb unit of every instance, unlinks the reverb and, when freethis,
frees it.
====================
*/
FMOD_RESULT ReverbI::release( bool freethis ) {
    int count;

    for ( count = 0; count < FMOD_REVERB_MAXINSTANCES; count++ ) {
        if ( mInstance[count].mChannelData ) {
#line 197
            FMOD_Memory_Free( mInstance[count].mChannelData );
            mInstance[count].mChannelData = NULL;
        }

        releaseDSP( count );
    }

    removeNode();

    if ( freethis ) {
#line 230
        FMOD_Memory_Free( this );
    }

    return FMOD_OK;
} // 0x80340184

/*
====================
ReverbI::createDSP

Makes the reverb unit of one instance from the system's SFX reverb plugin, and gives the instance its
channel data when it has none. Retail's body differs from Gormiti's (another revision).
====================
*/
FMOD_RESULT ReverbI::createDSP( int instance ) {
    FMOD_RESULT result;
    int         numdsps;
    int         count;

    if ( instance < 0 || instance >= FMOD_REVERB_MAXINSTANCES ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }
    if ( !mSystem ) {
        return FMOD_ERR_INITIALIZATION;
    }

    if ( !mInstance[instance].mChannelData ) {
#line 269
        mInstance[instance].mChannelData = (FMOD_REVERB_CHANNELDATA *)FMOD_Memory_Calloc( mSystem->mNumChannels * sizeof( FMOD_REVERB_CHANNELDATA ) );
        if ( !mInstance[instance].mChannelData ) {
            return FMOD_ERR_MEMORY;
        }

        for ( int channel = 0; channel < mSystem->mNumChannels; channel++ ) {
            resetChanProperties( instance, channel );
            resetConnectionPointer( instance, channel );
        }
    }

    if ( !mInstance[instance].mDSP ) {
        result = mSystem->mPluginFactory->getNumDSPs( &numdsps );
        if ( result != FMOD_OK ) {
            return result;
        }

        for ( count = 0; count < numdsps; count++ ) {
            FMOD_DSP_DESCRIPTION_EX *descriptionex = NULL;
            unsigned int             handle;

            result = mSystem->mPluginFactory->getDSPHandle( count, &handle );
            if ( result != FMOD_OK ) {
                continue;
            }
            result = mSystem->mPluginFactory->getDSP( handle, &descriptionex );
            if ( result != FMOD_OK ) {
                continue;
            }

            if ( descriptionex->mType == FMOD_DSP_TYPE_SFXREVERB ) {
                FMOD_DSP_DESCRIPTION_EX dsp_desc = *descriptionex;

                dsp_desc.channels = mSystem->mMaxOutputChannels;

                result = mSystem->mPluginFactory->createDSP( &dsp_desc, &mInstance[instance].mDSP );
                if ( result != FMOD_OK ) {
                    return result;
                }

                result = mInstance[instance].mDSP->setParameter( 0, -10000.0f );
                if ( result != FMOD_OK ) {
                    return result;
                }
                return FMOD_OK;
            }
        }

        return FMOD_ERR_PLUGIN_MISSING;
    }

    return FMOD_OK;
} // 0x8034029C

/*
====================
ReverbI::releaseDSP

Disconnects and releases the reverb unit of one instance. Retail has no out-of-line copy: release inlines
it (Gormiti link map: UNUSED).
====================
*/
inline FMOD_RESULT ReverbI::releaseDSP( int instance ) {
    if ( instance < 0 || instance >= FMOD_REVERB_MAXINSTANCES ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }

    if ( mInstance[instance].mDSP ) {
        FMOD_RESULT result;

        result = mInstance[instance].mDSP->disconnectFrom( NULL, NULL );
        if ( result != FMOD_OK ) {
            return result;
        }
        result = mInstance[instance].mDSP->release( true );
        if ( result != FMOD_OK ) {
            return result;
        }
        mInstance[instance].mDSP = NULL;
    }

    return FMOD_OK;
} // no copy in retail (inlined)

/*
====================
ReverbI::getChanProperties

The properties of one channel's send into one instance, and the connection that carries it.
====================
*/
FMOD_RESULT ReverbI::getChanProperties( int instance, int index, FMOD_REVERB_CHANNELPROPERTIES *props,
                                        DSPConnectionI **connection ) {
    if ( instance < 0 || instance >= FMOD_REVERB_MAXINSTANCES ) {
        if ( connection ) {
            *connection = NULL;
        }
        return FMOD_ERR_REVERB_INSTANCE;
    }
    if ( !mInstance[instance].mChannelData ) {
        if ( connection ) {
            *connection = NULL;
        }
        return FMOD_ERR_REVERB_INSTANCE;
    }
    if ( index >= 0 && index < mSystem->mNumChannels ) {
        if ( props ) {
            memcpy( props, &mInstance[instance].mChannelData[index], sizeof( FMOD_REVERB_CHANNELPROPERTIES ) );
        }
        if ( connection ) {
            *connection = mInstance[instance].mChannelData[index].mDSPConnection;
        }
    } else {
        if ( connection ) {
            *connection = NULL;
        }
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
} // 0x803405B4

/*
====================
ReverbI::setChanProperties

Sets the properties of one channel's send into one instance, and the connection that carries it.
====================
*/
FMOD_RESULT ReverbI::setChanProperties( int instance, int index, const FMOD_REVERB_CHANNELPROPERTIES *props,
                                        DSPConnectionI *connection ) {
    if ( instance < 0 || instance >= FMOD_REVERB_MAXINSTANCES || !mInstance[instance].mChannelData ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }
    if ( index >= 0 && index < mSystem->mNumChannels ) {
        if ( props ) {
            memcpy( &mInstance[instance].mChannelData[index], props, sizeof( FMOD_REVERB_CHANNELPROPERTIES ) );
            mInstance[instance].mChannelData[index].mChanProps.Flags &=
                ~( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 | FMOD_REVERB_CHANNELFLAGS_INSTANCE1 |
                   FMOD_REVERB_CHANNELFLAGS_INSTANCE2 | FMOD_REVERB_CHANNELFLAGS_INSTANCE3 );
            mInstance[instance].mChannelData[index].mChanProps.Flags |= FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance;
        }
        if ( connection ) {
            mInstance[instance].mChannelData[index].mDSPConnection = connection;
        }
    } else {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
} // 0x803406AC

/*
====================
ReverbI::resetChanProperties

Gives one channel's send into one instance the default properties and drops its connection.
====================
*/
FMOD_RESULT ReverbI::resetChanProperties( int instance, int index ) {
    FMOD_REVERB_CHANNELPROPERTIES defaultprop = FMOD_REVERB_CHANNELPROPERTIES_DEFAULT;

    if ( !mInstance[instance].mChannelData ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    defaultprop.Flags = ( defaultprop.Flags & ~FMOD_REVERB_CHANNELFLAGS_INSTANCE0 ) |
                        ( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance );
    memcpy( &mInstance[instance].mChannelData[index], &defaultprop, sizeof( FMOD_REVERB_CHANNELPROPERTIES ) );
    mInstance[instance].mChannelData[index].mPresenceGain = 0.0f;

    return resetConnectionPointer( instance, index );
} // 0x8034078C

/*
====================
ReverbI::resetConnectionPointer

Forgets the connection of one channel's send into one instance.
====================
*/
FMOD_RESULT ReverbI::resetConnectionPointer( int instance, int index ) {
    if ( !mInstance[instance].mChannelData ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mInstance[instance].mChannelData[index].mDSPConnection = NULL;

    return FMOD_OK;
} // 0x80340868

/*
====================
ReverbI::set3DAttributes

Places the reverb and gives it the radii inside which it is at full strength and outside which it is off.
====================
*/
FMOD_RESULT ReverbI::set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance ) {
    if ( position ) {
        mPosition.x = position->x;
        mPosition.y = position->y;
        mPosition.z = position->z;
    }

    m3D        = true;
    mMinRadius = mindistance;
    if ( mMinRadius > mMaxRadius ) {
        mMaxRadius = mMinRadius;
    }

    mMaxRadius = maxdistance;
    if ( mMaxRadius < mMinRadius ) {
        mMinRadius = mMaxRadius;
    }

    mMinMaxDistance = mMaxRadius - mMinRadius;

    return FMOD_OK;
} // 0x8034089C

/*
====================
ReverbI::get3DAttributes

The position and radii set by set3DAttributes; an argument left NULL is not written.
====================
*/
FMOD_RESULT ReverbI::get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance ) {
    if ( position ) {
        position->x = mPosition.x;
        position->y = mPosition.y;
        position->z = mPosition.z;
    }
    if ( mindistance ) {
        *mindistance = mMinRadius;
    }
    if ( maxdistance ) {
        *maxdistance = mMaxRadius;
    }

    return FMOD_OK;
} // 0x8034090C

/*
====================
ReverbI::setProperties

Sets the properties of the instance prop_source names, every value held inside its range. The global
reverb tells the output about them. The old properties are saved and never read (as in the original
source, which Gormiti's DWARF shows with a local oldprops).
====================
*/
FMOD_RESULT ReverbI::setProperties( const FMOD_REVERB_PROPERTIES *prop_source ) {
    FMOD_REVERB_PROPERTIES  oldprops;
    FMOD_REVERB_PROPERTIES *newprops;
    int                     instance = prop_source->Instance;

    if ( instance > 3 || instance < 0 ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }

    newprops = &mInstance[instance].mProps;
    memcpy( &oldprops, newprops, sizeof( FMOD_REVERB_PROPERTIES ) );

    newprops->Instance          = FMOD_CLAMP( prop_source->Instance, 0, 3 );
    newprops->Environment       = FMOD_CLAMP( prop_source->Environment, -1, 25 );
    newprops->EnvSize           = FMOD_CLAMP( prop_source->EnvSize, 1.0f, 100.0f );
    newprops->EnvDiffusion      = FMOD_CLAMP( prop_source->EnvDiffusion, 0.0f, 1.0f );
    newprops->Room              = FMOD_CLAMP( prop_source->Room, -10000, 0 );
    newprops->RoomHF            = FMOD_CLAMP( prop_source->RoomHF, -10000, 0 );
    newprops->RoomLF            = FMOD_CLAMP( prop_source->RoomLF, -10000, 0 );
    newprops->DecayTime         = FMOD_CLAMP( prop_source->DecayTime, 0.1f, 20.0f );
    newprops->DecayHFRatio      = FMOD_CLAMP( prop_source->DecayHFRatio, 0.1f, 2.0f );
    newprops->DecayLFRatio      = FMOD_CLAMP( prop_source->DecayLFRatio, 0.1f, 2.0f );
    newprops->Reflections       = FMOD_CLAMP( prop_source->Reflections, -10000, 1000 );
    newprops->ReflectionsDelay  = FMOD_CLAMP( prop_source->ReflectionsDelay, 0.0f, 0.3f );
    newprops->ReflectionsPan[0] = prop_source->ReflectionsPan[0];
    newprops->ReflectionsPan[1] = prop_source->ReflectionsPan[1];
    newprops->ReflectionsPan[2] = prop_source->ReflectionsPan[2];
    newprops->Reverb            = FMOD_CLAMP( prop_source->Reverb, -10000, 2000 );
    newprops->ReverbDelay       = FMOD_CLAMP( prop_source->ReverbDelay, 0.0f, 0.1f );
    newprops->ReverbPan[0]      = prop_source->ReverbPan[0];
    newprops->ReverbPan[1]      = prop_source->ReverbPan[1];
    newprops->ReverbPan[2]      = prop_source->ReverbPan[2];
    newprops->EchoTime          = FMOD_CLAMP( prop_source->EchoTime, 0.075f, 0.25f );
    newprops->EchoDepth         = FMOD_CLAMP( prop_source->EchoDepth, 0.0f, 1.0f );
    newprops->ModulationTime    = FMOD_CLAMP( prop_source->ModulationTime, 0.04f, 4.0f );
    newprops->ModulationDepth   = FMOD_CLAMP( prop_source->ModulationDepth, 0.0f, 1.0f );
    newprops->AirAbsorptionHF   = FMOD_CLAMP( prop_source->AirAbsorptionHF, -100.0f, 0.0f );
    newprops->HFReference       = FMOD_CLAMP( prop_source->HFReference, 1000.0f, 20000.0f );
    newprops->LFReference       = FMOD_CLAMP( prop_source->LFReference, 20.0f, 1000.0f );
    newprops->RoomRolloffFactor = FMOD_CLAMP( prop_source->RoomRolloffFactor, 0.0f, 10.0f );
    newprops->Diffusion         = FMOD_CLAMP( prop_source->Diffusion, 0.0f, 100.0f );
    newprops->Density           = FMOD_CLAMP( prop_source->Density, 0.0f, 100.0f );
    newprops->Flags             = prop_source->Flags;

    if ( this == &mSystem->mReverbGlobal && mSystem->mOutput->mDescription.reverb_setproperties ) {
        mSystem->mOutput->readfrommixer = Output::mixCallback;
        return mSystem->mOutput->mDescription.reverb_setproperties( mSystem->mOutput, &mInstance[instance].mProps );
    }

    return FMOD_OK;
} // 0x80340954

/*
====================
ReverbI::getProperties

The properties of the instance prop->Instance names.
====================
*/
FMOD_RESULT ReverbI::getProperties( FMOD_REVERB_PROPERTIES *prop ) {
    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    if ( prop->Instance < 0 || prop->Instance >= FMOD_REVERB_MAXINSTANCES ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }

    *prop = mInstance[prop->Instance].mProps;

    return FMOD_OK;
} // 0x80340E68

/*
====================
ReverbI::setActive

Turns the reverb on or off.
====================
*/
FMOD_RESULT ReverbI::setActive( bool active ) {
    mActive = active;

    return FMOD_OK;
} // 0x80340F9C

/*
====================
ReverbI::getActive

Whether the reverb is on.
====================
*/
FMOD_RESULT ReverbI::getActive( bool *active ) {
    if ( !active ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *active = mActive;

    return FMOD_OK;
} // 0x80340FA8

/*
====================
ReverbI::setPresenceGain

Sets how strongly one channel's send is heard in one instance.
====================
*/
FMOD_RESULT ReverbI::setPresenceGain( int instance, int index, float gain ) {
    if ( instance < 0 || instance >= FMOD_REVERB_MAXINSTANCES ) {
        return FMOD_ERR_REVERB_INSTANCE;
    }
    if ( index < 0 || index >= mSystem->mNumChannels ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    mInstance[instance].mChannelData[index].mPresenceGain = gain;

    return FMOD_OK;
} // 0x80340FC8

} // namespace FMOD
