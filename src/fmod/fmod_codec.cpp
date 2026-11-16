/*
 * fmod_codec.cpp: FMOD::Codec base methods (release, read, metadata, length and positioning).
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_codec.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_codec.obj)
//
// File::getMetadata (inline virtual, weak) is also emitted in this object, as in every object that
// calls it; retail keeps this copy.

#include <new>
#include <string.h>

#include <fmod/fmod.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_metadata.h>
#include <fmod/fmod_soundi.h>

namespace FMOD {

/*
====================
Codec::release

Closes the codec plugin, frees its file, FSB wave format memory and metadata, and releases the base plugin.
====================
*/
FMOD_RESULT Codec::release() {
    if ( mDescription.close ) {
        mDescription.close( this );
    }

    if ( mFile ) {
        mFile->close();
#line 44
        FMOD_Memory_Free( mFile );
        mFile = NULL;
    }

    if ( mWaveFormatMemory && mType == FMOD_SOUND_TYPE_FSB ) {
#line 50
        FMOD_Memory_Free( mWaveFormatMemory );
        mWaveFormatMemory = NULL;
    }

    if ( mMetadata ) {
        mMetadata->release();
        mMetadata = NULL;
    }

    return Plugin::release();
} // 0x80369770

/*
====================
Codec::read

Reads decoded PCM data (through the internal PCM ring buffer when present), refreshing metadata from the file.
====================
*/
FMOD_RESULT Codec::read( void *buffer, unsigned int sizebytes, unsigned int *bytesread ) {
    FMOD_RESULT  result        = FMOD_OK;
    bool         checkmetadata = false;
    unsigned int read          = 0;

    if ( mPCMBuffer && mPCMBufferLengthBytes ) {
        while ( sizebytes ) {
            unsigned int lenbytes     = sizebytes;
            unsigned int bytesdecoded = 0;

            if ( mPCMBufferOffsetBytes == 0 ) {
                result = mDescription.read( this, mPCMBuffer, mPCMBufferLengthBytes, &bytesdecoded );
                if ( result != FMOD_OK ) {
                    break;
                }

                mPCMBufferFilledBytes = bytesdecoded;
                lenbytes              = bytesdecoded;
                if ( lenbytes > sizebytes ) {
                    lenbytes = sizebytes;
                }

                checkmetadata = true;
            }

            if ( mPCMBufferOffsetBytes + lenbytes > mPCMBufferFilledBytes ) {
                lenbytes = mPCMBufferFilledBytes - mPCMBufferOffsetBytes;
            }

            memcpy( (char *)buffer + read, mPCMBuffer + mPCMBufferOffsetBytes, lenbytes );

            mPCMBufferOffsetBytes += lenbytes;
            if ( mPCMBufferOffsetBytes >= mPCMBufferFilledBytes ) {
                mPCMBufferOffsetBytes = 0;
            }

            if ( lenbytes == 0 ) {
                break;
            }

            sizebytes -= lenbytes;
            read      += lenbytes;
        }
    } else {
        result = mDescription.read( this, buffer, sizebytes, &read );
        if ( result == FMOD_OK ) {
            checkmetadata = true;
        }
    }

    if ( checkmetadata ) {
        getMetadataFromFile();
    }

    if ( bytesread ) {
        *bytesread = read;
    }

    return result;
} // 0x80369854

/*
====================
Codec::getMetadataFromFile

Pulls metadata tags from the backing File into the codec's Metadata list.
====================
*/
FMOD_RESULT Codec::getMetadataFromFile() {
    FMOD_RESULT result = FMOD_OK;
    Metadata   *filemetadata;

    if ( mFile ) {
        result = mFile->getMetadata( &filemetadata );
        if ( result == FMOD_OK ) {
            if ( !mMetadata ) {
#line 197
                mMetadata = new ( FMOD_Memory_Alloc( sizeof( Metadata ) ) ) Metadata;
                if ( !mMetadata ) {
                    return FMOD_ERR_MEMORY;
                }
            }

            result = mMetadata->add( filemetadata );
        }
    }

    return result;
} // 0x80369A8C

/*
====================
Codec::getLength

Returns the length of the current subsound in the requested time unit.
====================
*/
FMOD_RESULT Codec::getLength( unsigned int *length, FMOD_TIMEUNIT lengthtype ) {
    FMOD_RESULT result;

    if ( lengthtype == FMOD_TIMEUNIT_RAWBYTES ) {
        FMOD_CODEC_WAVEFORMAT waveformat;

        result = mDescription.getwaveformat( this, mSubSoundIndex, &waveformat );
        if ( result != FMOD_OK ) {
            return result;
        }

        *length = waveformat.lengthbytes;

        return FMOD_OK;
    } else {
        if ( !mDescription.getlength ) {
            *length = 0;
            return FMOD_ERR_UNSUPPORTED;
        }

        result = mDescription.getlength( this, length, lengthtype );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x80369B78

/*
====================
Codec::setPosition

Converts position from the caller's time unit into one the codec supports and seeks to it.
====================
*/
FMOD_RESULT Codec::setPosition( int subsound, unsigned int position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT           result;
    FMOD_CODEC_WAVEFORMAT waveformat;

    if ( numsubsounds && subsound >= numsubsounds ) {
        return FMOD_ERR_INVALID_POSITION;
    }

    if ( !mDescription.setposition ) {
        return FMOD_ERR_UNSUPPORTED;
    }

    if ( subsound < 0 ) {
        subsound = mSubSoundIndex;
    }

    if ( !numsubsounds ) {
        subsound = 0;
    }

    result = mDescription.getwaveformat( this, subsound, &waveformat );
    if ( result != FMOD_OK ) {
        return result;
    }

    if ( mDescription.timeunits & FMOD_TIMEUNIT_PCM ) {
        if ( postype & FMOD_TIMEUNIT_PCMBYTES ) {
            SoundI::getSamplesFromBytes( position, &position, waveformat.channels, waveformat.format );
            postype = FMOD_TIMEUNIT_PCM;
        } else if ( postype & FMOD_TIMEUNIT_MS ) {
            position = (unsigned int)( ( (float)position / 1000.0f ) * (float)waveformat.frequency );
            postype  = FMOD_TIMEUNIT_PCM;
        }
    } else if ( mDescription.timeunits & FMOD_TIMEUNIT_PCMBYTES ) {
        if ( postype & FMOD_TIMEUNIT_PCM ) {
            SoundI::getBytesFromSamples( position, &position, waveformat.channels, waveformat.format );
            postype = FMOD_TIMEUNIT_PCMBYTES;
        } else if ( postype & FMOD_TIMEUNIT_MS ) {
            position = (unsigned int)( ( (float)position / 1000.0f ) * (float)waveformat.frequency );
            SoundI::getBytesFromSamples( position, &position, waveformat.channels, waveformat.format );
            postype = FMOD_TIMEUNIT_PCMBYTES;
        }
    } else if ( mDescription.timeunits & FMOD_TIMEUNIT_MS ) {
        if ( postype & FMOD_TIMEUNIT_PCM ) {
            position = (unsigned int)( 1000.0f * ( (float)position / (float)waveformat.frequency ) );
            postype  = FMOD_TIMEUNIT_MS;
        } else if ( postype & FMOD_TIMEUNIT_PCMBYTES ) {
            SoundI::getSamplesFromBytes( position, &position, waveformat.channels, waveformat.format );
            position = (unsigned int)( 1000.0f * ( (float)position / (float)waveformat.frequency ) );
            postype  = FMOD_TIMEUNIT_MS;
        }
    }

    if ( !( postype & mDescription.timeunits ) ) {
        return FMOD_ERR_FORMAT;
    }

    mPCMBufferOffsetBytes = 0;

    result = mDescription.setposition( this, subsound, position, postype );
    if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
        return result;
    }

    mSubSoundIndex = subsound;

    return FMOD_OK;
} // 0x80369C38

/*
====================
Codec::getPosition

Queries the codec's current playback position in the requested time unit.
====================
*/
FMOD_RESULT Codec::getPosition( unsigned int *position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT result;

    if ( postype == FMOD_TIMEUNIT_RAWBYTES ) {
        if ( !mFile ) {
            *position = 0;
        }

        result = mFile->tell( position );
        if ( result != FMOD_OK ) {
            *position = 0;
            return result;
        }

        *position -= mSrcDataOffset;
    }

    if ( !mDescription.getposition ) {
        return FMOD_ERR_UNSUPPORTED;
    }

    if ( !( postype & mDescription.timeunits ) ) {
        return FMOD_ERR_FORMAT;
    }

    result = mDescription.getposition( this, position, postype );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x8036A368

/*
====================
Codec::metaData

Appends a metadata tag to the codec's Metadata list, allocating the list on first use.
====================
*/
FMOD_RESULT Codec::metaData( FMOD_TAGTYPE type, const char *name, void *data, unsigned int datalen,
                             FMOD_TAGDATATYPE datatype, bool unique ) {
    if ( !mMetadata ) {
#line 452
        mMetadata = new ( FMOD_Memory_Alloc( sizeof( Metadata ) ) ) Metadata;
        if ( !mMetadata ) {
            return FMOD_ERR_MEMORY;
        }
    }

    return mMetadata->addTag( type, name, data, datalen, datatype, unique );
} // 0x8036A44C

} // namespace FMOD
