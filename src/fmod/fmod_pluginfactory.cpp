/*
 * fmod_pluginfactory.cpp: FMOD::PluginFactory, the lists of DSP units, codecs and outputs a system can
 * create, and the default callbacks of a codec's state.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_pluginfactory.obj)

#include <new>
#include <string.h>

#include <fmod/fmod.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_dsp_codec.h>
#include <fmod/fmod_dsp_resampler.h>
#include <fmod/fmod_dsp_soundcard.h>
#include <fmod/fmod_dsp_wavetable.h>
#include <fmod/fmod_dspi.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_output.h>
#include <fmod/fmod_output_polled.h>
#include <fmod/fmod_pluginfactory.h>
#include <fmod/fmod_string.h>
#include <fmod/fmod_systemi.h>

namespace FMOD {

/*
====================
Codec::defaultMetaData

The metadata callback of a codec's state: the tag goes to the codec itself.
====================
*/
FMOD_RESULT F_CALLBACK Codec::defaultMetaData( FMOD_CODEC_STATE *codec_state, FMOD_TAGTYPE tagtype, char *name,
                                               void *data, unsigned int datalen, FMOD_TAGDATATYPE datatype,
                                               int unique ) {
    return static_cast<Codec *>( codec_state )->metaData( tagtype, name, data, datalen, datatype, unique == 1 ? true : false );
} // 0x8033E828

/*
====================
Codec::defaultFileSeek

The seek callback of a codec's state: the handle is the codec's File.
====================
*/
FMOD_RESULT F_CALLBACK Codec::defaultFileSeek( void *handle, unsigned int pos, void *userdata ) {
    return static_cast<File *>( handle )->seek( pos, 0 );
} // 0x8033E844

/*
====================
Codec::defaultFileRead

The read callback of a codec's state: reads sizebytes items of one byte from the codec's File.
====================
*/
FMOD_RESULT F_CALLBACK Codec::defaultFileRead( void *handle, void *buffer, unsigned int sizebytes,
                                               unsigned int *bytesread, void *userdata ) {
    return static_cast<File *>( handle )->read( buffer, 1, sizebytes, bytesread );
} // 0x8033E84C

/*
====================
PluginFactory::PluginFactory

The three lists start empty (the descriptions' nodes link themselves) and the first handle is 1.
====================
*/
PluginFactory::PluginFactory() {
    mSystem              = NULL;
    mCurrentPluginHandle = 1;

    memset( mPluginPath, 0, sizeof( mPluginPath ) );

    mDSPHead.initNode();
    mCodecHead.initNode();
    mOutputHead.initNode();
} // 0x8033E85C

/*
====================
PluginFactory::release

Unloads every codec, DSP unit and output, then frees the factory itself.
====================
*/
FMOD_RESULT PluginFactory::release() {
    FMOD_RESULT result;
    int         count;
    int         numcodecs;
    int         numdsps;
    int         numoutputs;

    result = getNumCodecs( &numcodecs );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < numcodecs; count++ ) {
        unsigned int handle;

        result = getCodecHandle( 0, &handle );
        result = unloadPlugin( handle );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    result = getNumDSPs( &numdsps );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < numdsps; count++ ) {
        unsigned int handle;

        result = getDSPHandle( 0, &handle );
        result = unloadPlugin( handle );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    result = getNumOutputs( &numoutputs );
    if ( result != FMOD_OK ) {
        return result;
    }

    for ( count = 0; count < numoutputs; count++ ) {
        unsigned int handle;

        result = getOutputHandle( 0, &handle );
        result = unloadPlugin( handle );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

#line 153
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x8033E920

/*
====================
PluginFactory::setSystem
====================
*/
FMOD_RESULT PluginFactory::setSystem( SystemI *system ) {
    mSystem = system;

    return FMOD_OK;
} // 0x8033EB70

/*
====================
PluginFactory::setPluginPath

The directory plugins are loaded from; it must fit mPluginPath.
====================
*/
FMOD_RESULT PluginFactory::setPluginPath( const char *path ) {
    if ( FMOD_strlen( path ) >= (int)sizeof( mPluginPath ) ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    FMOD_strncpy( mPluginPath, path, sizeof( mPluginPath ) );

    return FMOD_OK;
} // 0x8033EB7C
/*
====================
PluginFactory::unloadPlugin

Takes the plugin registered under handle out of whichever list holds it and frees its description.
====================
*/
FMOD_RESULT PluginFactory::unloadPlugin( unsigned int handle ) {
    FMOD_RESULT                 result;
    FMOD_OUTPUT_DESCRIPTION_EX *output;
    FMOD_CODEC_DESCRIPTION_EX  *codec;
    FMOD_DSP_DESCRIPTION_EX    *dsp;

    result = getOutput( handle, &output );
    if ( result == FMOD_OK ) {
        output->removeNode();
#line 594
        FMOD_Memory_Free( output );
        return FMOD_OK;
    }

    if ( result == FMOD_ERR_PLUGIN_MISSING ) {
        result = getCodec( handle, &codec );
        if ( result == FMOD_OK ) {
            codec->removeNode();
#line 609
            FMOD_Memory_Free( codec );
        } else if ( result == FMOD_ERR_PLUGIN_MISSING ) {
            result = getDSP( handle, &dsp );
            if ( result == FMOD_OK ) {
                dsp->removeNode();
#line 633
                FMOD_Memory_Free( dsp );
            }
        }
    }

    return result;
} // 0x8033EBDC

/*
====================
PluginFactory::registerCodec

Adds a copy of the codec description to the list, ahead of the first codec of higher priority, and
hands back its handle.
====================
*/
FMOD_RESULT PluginFactory::registerCodec( FMOD_CODEC_DESCRIPTION_EX *description, unsigned int *handle,
                                          unsigned int priority ) {
    FMOD_CODEC_DESCRIPTION_EX *newcodec;

    if ( !description ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 666
    newcodec = new ( FMOD_Memory_Calloc( sizeof( FMOD_CODEC_DESCRIPTION_EX ) ) ) FMOD_CODEC_DESCRIPTION_EX;
    if ( !newcodec ) {
        return FMOD_ERR_MEMORY;
    }

    newcodec->name                    = description->name;
    newcodec->version                 = description->version;
    newcodec->timeunits               = description->timeunits;
    newcodec->defaultasstream         = description->defaultasstream;
    newcodec->open                    = description->open;
    newcodec->close                   = description->close;
    newcodec->read                    = description->read;
    newcodec->getlength               = description->getlength;
    newcodec->setposition             = description->setposition;
    newcodec->getposition             = description->getposition;
    newcodec->soundcreate             = description->soundcreate;
    newcodec->getwaveformat           = description->getwaveformat;
    newcodec->mType                   = description->mType;
    newcodec->mSize                   = description->mSize;
    newcodec->mModule                 = description->mModule;
    newcodec->reset                   = description->reset;
    newcodec->canpoint                = description->canpoint;
    newcodec->getmusicnumchannels     = description->getmusicnumchannels;
    newcodec->setmusicchannelvolume   = description->setmusicchannelvolume;
    newcodec->getmusicchannelvolume   = description->getmusicchannelvolume;
    newcodec->gethardwaremusicchannel = description->gethardwaremusicchannel;
    newcodec->update                  = description->update;
    newcodec->getmemoryused           = description->getmemoryused;

    newcodec->mHandle = mCurrentPluginHandle++;

    newcodec->addSorted( &mCodecHead, priority );

    if ( handle ) {
        *handle = newcodec->mHandle;
    }

    return FMOD_OK;
} // 0x8033EE10

/*
====================
PluginFactory::registerOutput

Adds a copy of the output description to the end of the list and hands back its handle. The start and
stop callbacks are copied twice.
====================
*/
FMOD_RESULT PluginFactory::registerOutput( FMOD_OUTPUT_DESCRIPTION_EX *description, unsigned int *handle ) {
    FMOD_OUTPUT_DESCRIPTION_EX *newoutput;

    if ( !description ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 938
    newoutput = new ( FMOD_Memory_Calloc( sizeof( FMOD_OUTPUT_DESCRIPTION_EX ) ) ) FMOD_OUTPUT_DESCRIPTION_EX;
    if ( !newoutput ) {
        return FMOD_ERR_MEMORY;
    }

    newoutput->name                 = description->name;
    newoutput->version              = description->version;
    newoutput->polling              = description->polling;
    newoutput->getnumdrivers        = description->getnumdrivers;
    newoutput->getdrivername        = description->getdrivername;
    newoutput->getdrivercaps        = description->getdrivercaps;
    newoutput->init                 = description->init;
    newoutput->close                = description->close;
    newoutput->start                = description->start;
    newoutput->stop                 = description->stop;
    newoutput->update               = description->update;
    newoutput->gethandle            = description->gethandle;
    newoutput->getposition          = description->getposition;
    newoutput->lock                 = description->lock;
    newoutput->unlock               = description->unlock;
    newoutput->getsamplemaxchannels = description->getsamplemaxchannels;
    newoutput->getdriverinfo        = description->getdriverinfo;
    newoutput->getdrivercapsex      = description->getdrivercapsex;
    newoutput->getdrivercapsex2     = description->getdrivercapsex2;
    newoutput->initex               = description->initex;
    newoutput->start                = description->start;
    newoutput->stop                 = description->stop;
    newoutput->updatefinished       = description->updatefinished;
    newoutput->createsample         = description->createsample;
    newoutput->getsoundram          = description->getsoundram;
    newoutput->record_getnumdrivers = description->record_getnumdrivers;
    newoutput->record_getdriverinfo = description->record_getdriverinfo;
    newoutput->record_getdrivercaps = description->record_getdrivercaps;
    newoutput->record_start         = description->record_start;
    newoutput->record_stop          = description->record_stop;
    newoutput->record_getposition   = description->record_getposition;
    newoutput->record_lock          = description->record_lock;
    newoutput->record_unlock        = description->record_unlock;
    newoutput->reverb_setproperties = description->reverb_setproperties;
    newoutput->postmixcallback      = description->postmixcallback;
    newoutput->mType                = description->mType;
    newoutput->mSize                = description->mSize;
    newoutput->mModule              = description->mModule;
    newoutput->getmemoryused        = description->getmemoryused;

    newoutput->mHandle = mCurrentPluginHandle++;

    newoutput->addBefore( &mOutputHead );

    if ( handle ) {
        *handle = newoutput->mHandle;
    }

    return FMOD_OK;
} // 0x8033EFE4

/*
====================
PluginFactory::getNumCodecs

The number of codecs registered.
====================
*/
FMOD_RESULT PluginFactory::getNumCodecs( int *numcodecs ) {
    if ( !numcodecs ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numcodecs = mCodecHead.count();

    return FMOD_OK;
} // 0x8033F1FC

/*
====================
PluginFactory::getNumDSPs

The number of DSP units registered.
====================
*/
FMOD_RESULT PluginFactory::getNumDSPs( int *numdsps ) {
    if ( !numdsps ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numdsps = mDSPHead.count();

    return FMOD_OK;
} // 0x8033F238

/*
====================
PluginFactory::getNumOutputs

The number of outputs registered.
====================
*/
FMOD_RESULT PluginFactory::getNumOutputs( int *numoutputs ) {
    if ( !numoutputs ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *numoutputs = mOutputHead.count();

    return FMOD_OK;
} // 0x8033F274

/*
====================
PluginFactory::getCodecHandle

The handle of the index-th codec.
====================
*/
FMOD_RESULT PluginFactory::getCodecHandle( int index, unsigned int *handle ) {
    LinkedListNode *node;
    FMOD_CODEC_DESCRIPTION_EX *codec;

    if ( !handle ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    node = mCodecHead.getNodeByIndex( index );
    if ( !node ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    codec = static_cast<FMOD_CODEC_DESCRIPTION_EX *>( node );
    *handle = codec->mHandle;

    return FMOD_OK;
} // 0x8033F2B0

/*
====================
PluginFactory::getDSPHandle

The handle of the index-th DSP unit.
====================
*/
FMOD_RESULT PluginFactory::getDSPHandle( int index, unsigned int *handle ) {
    LinkedListNode *node;
    FMOD_DSP_DESCRIPTION_EX *dsp;

    if ( !handle ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    node = mDSPHead.getNodeByIndex( index );
    if ( !node ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    dsp = static_cast<FMOD_DSP_DESCRIPTION_EX *>( node );
    *handle = dsp->mHandle;

    return FMOD_OK;
} // 0x8033F324

/*
====================
PluginFactory::getOutputHandle

The handle of the index-th output.
====================
*/
FMOD_RESULT PluginFactory::getOutputHandle( int index, unsigned int *handle ) {
    LinkedListNode *node;
    FMOD_OUTPUT_DESCRIPTION_EX *output;

    if ( !handle ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    node = mOutputHead.getNodeByIndex( index );
    if ( !node ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    output = static_cast<FMOD_OUTPUT_DESCRIPTION_EX *>( node );
    *handle = output->mHandle;

    return FMOD_OK;
} // 0x8033F398

/*
====================
PluginFactory::getCodec

The codec registered under handle.
====================
*/
FMOD_RESULT PluginFactory::getCodec( unsigned int handle, FMOD_CODEC_DESCRIPTION_EX **description ) {
    FMOD_CODEC_DESCRIPTION_EX *head;
    FMOD_CODEC_DESCRIPTION_EX *current;

    if ( !description ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *description = NULL;

    head = &mCodecHead;
    for ( current = static_cast<FMOD_CODEC_DESCRIPTION_EX *>( head->getNext() ); current != head;
          current = static_cast<FMOD_CODEC_DESCRIPTION_EX *>( current->getNext() ) ) {
        if ( current->mHandle == handle ) {
            *description = current;
            return FMOD_OK;
        }
    }

    return FMOD_ERR_PLUGIN_MISSING;
} // 0x8033F40C

/*
====================
PluginFactory::getDSP

The DSP unit registered under handle.
====================
*/
FMOD_RESULT PluginFactory::getDSP( unsigned int handle, FMOD_DSP_DESCRIPTION_EX **description ) {
    FMOD_DSP_DESCRIPTION_EX *head;
    FMOD_DSP_DESCRIPTION_EX *current;

    if ( !description ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *description = NULL;

    head = &mDSPHead;
    for ( current = static_cast<FMOD_DSP_DESCRIPTION_EX *>( head->getNext() ); current != head;
          current = static_cast<FMOD_DSP_DESCRIPTION_EX *>( current->getNext() ) ) {
        if ( current->mHandle == handle ) {
            *description = current;
            return FMOD_OK;
        }
    }

    return FMOD_ERR_PLUGIN_MISSING;
} // 0x8033F474

/*
====================
PluginFactory::getOutput

The output registered under handle.
====================
*/
FMOD_RESULT PluginFactory::getOutput( unsigned int handle, FMOD_OUTPUT_DESCRIPTION_EX **description ) {
    FMOD_OUTPUT_DESCRIPTION_EX *head;
    FMOD_OUTPUT_DESCRIPTION_EX *current;

    if ( !description ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *description = NULL;

    head = &mOutputHead;
    for ( current = static_cast<FMOD_OUTPUT_DESCRIPTION_EX *>( head->getNext() ); current != head;
          current = static_cast<FMOD_OUTPUT_DESCRIPTION_EX *>( current->getNext() ) ) {
        if ( current->mHandle == handle ) {
            *description = current;
            return FMOD_OK;
        }
    }

    return FMOD_ERR_PLUGIN_MISSING;
} // 0x8033F4DC

/*
====================
PluginFactory::createCodec

Makes a codec from its description: at least sizeof( Codec ) bytes, or the size the description asks
for (a codec class of its own derives from Codec), with a copy of the description.
====================
*/
FMOD_RESULT PluginFactory::createCodec( FMOD_CODEC_DESCRIPTION_EX *codecdesc, Codec **codec ) {
    Codec *newcodec;

    if ( !codecdesc || !codec ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 1409
    newcodec = new ( FMOD_Memory_Calloc( codecdesc->mSize >= sizeof( Codec ) ? codecdesc->mSize : sizeof( Codec ) ) )
        Codec;
    if ( !newcodec ) {
        return FMOD_ERR_MEMORY;
    }

    memcpy( &newcodec->mDescription, codecdesc, sizeof( FMOD_CODEC_DESCRIPTION_EX ) );

    *codec = newcodec;

    if ( !newcodec->mDescription.getwaveformat ) {
        newcodec->mDescription.getwaveformat = Codec::defaultGetWaveFormat;
    }

    return FMOD_OK;
} // 0x8033F544

/*
====================
Codec::defaultGetWaveFormat

The waveformat callback of a codec's state: copies the format of one subsound.
====================
*/
FMOD_RESULT F_CALLBACK Codec::defaultGetWaveFormat( FMOD_CODEC_STATE *codec_state, int index,
                                                    FMOD_CODEC_WAVEFORMAT *waveformat ) {
    if ( !codec_state->waveformat ) {
        return FMOD_ERR_INTERNAL;
    }

    if ( index < 0 || ( codec_state->numsubsounds == 0 && index > 0 ) ||
         ( codec_state->numsubsounds > 0 && index >= codec_state->numsubsounds ) ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    memcpy( waveformat, &codec_state->waveformat[index], sizeof( FMOD_CODEC_WAVEFORMAT ) );

    return FMOD_OK;
} // 0x8033F68C

/*
====================
PluginFactory::createDSP

Makes a DSP unit from its description when the caller's pointer is empty: the unit's class follows the
description's category, and the unit is initialised by its alloc callback and then handed its create callback.
====================
*/
FMOD_RESULT PluginFactory::createDSP( FMOD_DSP_DESCRIPTION_EX *dspdesc, DSPI **dsp ) {
    FMOD_RESULT result;
    DSPI       *newdsp;
    void       *memory;

    if ( !dspdesc || !dsp ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    newdsp = *dsp;
    if ( !newdsp ) {
        switch ( dspdesc->mCategory ) {
        case FMOD_DSP_CATEGORY_FILTER:
            dspdesc->mSize = dspdesc->mSize >= sizeof( DSPFilter ) ? dspdesc->mSize : sizeof( DSPFilter );
#line 1542
            memory = FMOD_Memory_Calloc( dspdesc->mSize );
            if ( !memory ) {
                return FMOD_ERR_MEMORY;
            }
            newdsp = new ( memory ) DSPFilter;
            break;

        case FMOD_DSP_CATEGORY_DSPCODECMPEG:
        case FMOD_DSP_CATEGORY_DSPCODECADPCM:
        case FMOD_DSP_CATEGORY_DSPCODECXMA:
        case FMOD_DSP_CATEGORY_DSPCODECRAW:
#line 1680
            memory = FMOD_Memory_CallocType( dspdesc->mSize >= sizeof( DSPCodec ) ? dspdesc->mSize : sizeof( DSPCodec ), 0x200000 );
            if ( !memory ) {
                return FMOD_ERR_MEMORY;
            }
            if ( dspdesc->mCategory != FMOD_DSP_CATEGORY_DSPCODECRAW ) {
                return FMOD_ERR_FORMAT;
            }
            newdsp = new ( memory ) DSPCodec;
            // identity TODO: retail then stores a Codec-derived object at +0x48A8 (its vtable lbl_8044CCEC, file callbacks
            // Codec::defaultFileRead/Seek/MetaData, a self-linked list at +0x491C). The layout is not in the headers yet.
            break;

        case FMOD_DSP_CATEGORY_SOUNDCARD:
            dspdesc->mSize = dspdesc->mSize >= sizeof( DSPSoundCard ) ? dspdesc->mSize : sizeof( DSPSoundCard );
#line 1772
            newdsp = new ( FMOD_Memory_Calloc( dspdesc->mSize >= sizeof( DSPSoundCard ) ? dspdesc->mSize : sizeof( DSPSoundCard ) ) ) DSPSoundCard;
            break;

        case FMOD_DSP_CATEGORY_WAVETABLE:
#line 1802
            memory = FMOD_Memory_Calloc( dspdesc->mSize >= sizeof( DSPWaveTable ) ? dspdesc->mSize : sizeof( DSPWaveTable ) );
            if ( !memory ) {
                return FMOD_ERR_MEMORY;
            }
            newdsp = new ( memory ) DSPWaveTable;
            break;

        case FMOD_DSP_CATEGORY_RESAMPLER:
#line 1752
            memory = FMOD_Memory_Calloc( dspdesc->mSize >= sizeof( DSPResampler ) ? dspdesc->mSize : sizeof( DSPResampler ) );
            if ( !memory ) {
                return FMOD_ERR_MEMORY;
            }
            newdsp = new ( memory ) DSPResampler;
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
        }
    }

    if ( !newdsp ) {
        *dsp = NULL;
        return FMOD_ERR_MEMORY;
    }

    newdsp->mSystem = mSystem;
    result          = newdsp->alloc( dspdesc );
    if ( result ) {
#line 1835
        FMOD_Memory_Free( newdsp );
        return result;
    }

    if ( dspdesc->create ) {
        newdsp->instance = (FMOD_DSP *)newdsp;
        result           = ( (FMOD_RESULT( F_CALLBACK * )( FMOD_DSP_STATE * ))dspdesc->create )( newdsp );
        if ( result ) {
#line 1886
            FMOD_Memory_Free( newdsp );
            return result;
        }
    }

    *dsp = newdsp;
    return FMOD_OK;
} // 0x8033F704

/*
====================
PluginFactory::createOutput

Makes an output from its description: a polled output (with its own thread) when the description asks
for polling, a plain one otherwise, each at least as big as its class or the size the description asks
for.
====================
*/
FMOD_RESULT PluginFactory::createOutput( FMOD_OUTPUT_DESCRIPTION_EX *outputdesc, Output **output ) {
    Output *newoutput;

    if ( !outputdesc || !output ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( outputdesc->polling ) {
#line 1937
        newoutput = new ( FMOD_Memory_Calloc( outputdesc->mSize >= sizeof( OutputPolled ) ? outputdesc->mSize : sizeof( OutputPolled ) ) ) OutputPolled;
    } else {
#line 1941
        newoutput = new ( FMOD_Memory_Calloc( outputdesc->mSize >= sizeof( Output ) ? outputdesc->mSize : sizeof( Output ) ) ) Output;
    }

    if ( !newoutput ) {
        *output = NULL;
        return FMOD_ERR_MEMORY;
    }

    memcpy( &newoutput->mDescription, outputdesc, sizeof( FMOD_OUTPUT_DESCRIPTION_EX ) );

    newoutput->mSystem       = mSystem;
    newoutput->readfrommixer = Output::mixCallback;

    *output = newoutput;

    return FMOD_OK;
} // 0x8033FAE0

} // namespace FMOD
