/*
 * fmod_sound_stream.cpp: FMOD::Stream buffering, flushing, seeking and position tracking.
 */
// Unit name and boundary: Gormiti link map (fmodex fmod_sound_stream.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_channel_stream.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_soundi.h>

namespace FMOD {

/*
====================
Stream::Stream

Initialises a streaming sound with no sub-sounds, channel, sample or loop limit.
====================
*/
Stream::Stream() {
    mLastPos          = 0;
    mSubSound         = NULL;
    mLoopCountCurrent = -1;
    mBlockSize        = 1;
    mChannel          = NULL;
    mInitialPosition  = 0;
} // 0x803441C0

/*
====================
Stream::fill

Decodes up to length samples into the stream's sample ring buffer starting at offset,
handling sentences, loops, EOF and silence-padding the remaining tail.
====================
*/
FMOD_RESULT Stream::fill( unsigned int offset, unsigned int length, unsigned int *read_out,
                          bool calledfromsentence ) {
    FMOD_RESULT  result = FMOD_OK;
    unsigned int read   = 0;
    Sample      *sample = mSample;

    if ( read_out ) {
        *read_out = 0;
    }

    if ( mSubSoundParent ) {
        sample = static_cast< Stream * >( mSubSoundParent )->mSample;
    }

    if ( !( mFlags & SOUNDI_FLAG_EOF ) ) {
        unsigned int r;
        unsigned int size;
        unsigned int len;

        if ( mSubSoundList ) {
            Stream *stream;

            stream = static_cast< Stream * >( mSubSound[mSubSoundList[mChannel->mSubSoundListCurrent].mIndex] );
            len    = length;

            do {
                unsigned int r;
                unsigned int endpoint;

                if ( ( mMode & FMOD_LOOP_NORMAL ) && mLoopCountCurrent ) {
                    endpoint = mLoopStart + mLoopLength - 1;
                } else if ( sample->mCodec->mFlags & CODEC_FLAG_KNOWNLENGTH ) {
                    endpoint = mLength - 1;
                } else {
                    endpoint = 0xFFFFFFFF; // unknown stream length
                }

                size = len;
                if ( mPosition > endpoint ) {
                    size = 0;
                } else if ( mPosition + len > endpoint ) {
                    size = ( endpoint - mPosition ) + 1;
                }

                result = stream->fill( offset, size, &r, true );
                if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
                    return result;
                }

                read      += r;
                len       -= r;
                offset    += r;
                mPosition += r;

                if ( mPosition > endpoint || result == FMOD_ERR_FILE_EOF ) {
                    bool finished = false;

                    if ( mPosition > endpoint ) {
                        if ( ( mMode & FMOD_LOOP_NORMAL ) && mLoopCountCurrent ) {
                            setPosition( mLoopStart, FMOD_TIMEUNIT_PCM );

                            if ( mLoopCountCurrent > 0 ) {
                                mLoopCountCurrent--;
                            }

                            stream = static_cast< Stream * >( mSubSound[mSubSoundIndex] );
                            continue;
                        } else {
                            mPosition = mLength;
                            mFlags   |= SOUNDI_FLAG_EOF;
                            break;
                        }
                    } else {
                        do {
                            mChannel->mSubSoundListCurrent++;
                            if ( mChannel->mSubSoundListCurrent >= mSubSoundListNum ) {
                                if ( !( mMode & FMOD_LOOP_NORMAL ) || !mLoopCountCurrent ) {
                                    mChannel->mSubSoundListCurrent = mSubSoundListNum - 1;
                                    finished = true;
                                    mPosition = mLength;
                                    mFlags   |= SOUNDI_FLAG_EOF;
                                    break;
                                } else {
                                    mChannel->mSubSoundListCurrent = 0;
                                    mPosition                      = 0;
                                }
                            }

                            mSubSoundIndex = mSubSoundList[mChannel->mSubSoundListCurrent].mIndex;
                            stream         = static_cast< Stream * >( mSubSound[mSubSoundIndex] );
                            if ( mSubSoundShared ) {
                                stream->updateSubSound( mSubSoundIndex, true );
                            }
                        } while ( !stream );
                    }

                    if ( finished ) {
                        break;
                    }

                    if ( stream ) {
                        sample->mCodec = stream->mCodec;

                        if ( mCodec != sample->mCodec ) {
                            result = sample->seek( -1, 0 );
                        } else {
                            result = sample->seek( mSubSoundIndex, 0 );
                        }

                        stream->mPosition = 0;
                        stream->mFlags   &= ~SOUNDI_FLAG_EOF;
                    }
                }
            } while ( len );
        } else {
            len = length;

            do {
                unsigned int endpoint;
                unsigned int streamlength;
                Stream      *stream      = this;
                unsigned int bytestoread = 0;

                if ( mSubSound ) {
                    stream = static_cast< Stream * >( mSubSound[mSubSoundIndex] );
                    if ( !stream ) {
                        break;
                    }
                }

                if ( mSubSoundShared ) {
                    FMOD_CODEC_WAVEFORMAT waveformat;

                    stream->mCodec->mDescription.getwaveformat( stream->mCodec, mSubSoundIndex, &waveformat );
                    streamlength = waveformat.lengthpcm;
                } else {
                    streamlength = stream->mLength;
                }

                if ( ( mMode & FMOD_LOOP_NORMAL ) && mLoopCountCurrent && !calledfromsentence ) {
                    endpoint = stream->mLoopStart + stream->mLoopLength - 1;
                } else if ( sample->mCodec->mFlags & CODEC_FLAG_KNOWNLENGTH ) {
                    endpoint = streamlength - 1;
                } else {
                    endpoint = 0xFFFFFFFF; // unknown stream length
                }

                size = len;
                if ( offset + len > sample->mLength ) {
                    size = sample->mLength - offset;
                }

                if ( stream->mPosition > endpoint ) {
                    size = 0;
                } else if ( stream->mPosition + size > endpoint ) {
                    size = ( endpoint - stream->mPosition ) + 1;
                }

                getBytesFromSamples( size, &bytestoread, false );

                if ( !bytestoread ) {
                    r      = 0;
                    result = FMOD_ERR_FILE_EOF;
                } else {
                    result = sample->read( offset, size, &r );
                    if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF && result != FMOD_ERR_FILE_DISKEJECTED ) {
                        return result;
                    }
                }

                if ( result == FMOD_OK && !r && size ) {
                    break;
                }

                read   += r;
                len    -= r;
                offset += r;
                if ( offset >= sample->mLength ) {
                    offset = 0;
                }

                stream->mLastPos   = stream->mPosition;
                stream->mPosition += r;

                if ( read_out ) {
                    *read_out = read;
                }

                if ( stream->mPosition > endpoint || result == FMOD_ERR_FILE_EOF ) {
                    if ( calledfromsentence ) {
                        return FMOD_ERR_FILE_EOF;
                    }

                    if ( ( mMode & FMOD_LOOP_NORMAL ) && mLoopCountCurrent ) {
                        stream->mPosition = mLoopStart;

                        if ( mLength != 0xFFFFFFFF ) { // known stream length
                            result = sample->seek( mSubSoundIndex, stream->mPosition );
                            if ( result != FMOD_OK ) {
                                return result;
                            }
                            stream->mPosition = sample->mPosition;
                        }

                        if ( mLoopCountCurrent > 0 ) {
                            mLoopCountCurrent--;
                        }
                    } else {
                        if ( stream != this ) {
                            mPosition = mLength;
                            mFlags   |= SOUNDI_FLAG_EOF;
                        }

                        stream->mPosition = streamlength;
                        stream->mFlags   |= SOUNDI_FLAG_EOF;
                        if ( stream->mSubSoundParent ) {
                            stream->mSubSoundParent->mFlags |= SOUNDI_FLAG_EOF;
                        }
                        break;
                    }
                } else if ( !r ) {
                    break;
                }
            } while ( len );
        }
    }

    if ( read < length ) {
        unsigned int size;
        unsigned int len = length - read;

        do {
            size = len;
            if ( offset + len > sample->mLength ) {
                size = sample->mLength - offset;
            }

            sample->clear( offset, size );

            len    -= size;
            offset += size;
            if ( offset >= sample->mLength ) {
                offset = 0;
            }
        } while ( len );
    }

    return result;
} // 0x80344230

/*
====================
Stream::flush

Fills the sample ring buffer from offset 0, clears any remaining tail, and clears the flush flag.
====================
*/
FMOD_RESULT Stream::flush() {
    if ( mSample ) {
        unsigned int size = mSample->mLength;
        FMOD_RESULT  result;

        if ( mLength < mSample->mLength ) {
            size = mLength;
        }

        result = fill( 0, size, NULL, false );
        if ( result != FMOD_OK && result != FMOD_ERR_FILE_EOF ) {
            return result;
        }

        if ( size < mSample->mLength ) {
            mSample->clear( size, mSample->mLength - size );
        }
    }

    mFlags &= ~SOUNDI_FLAG_0x10;

    return FMOD_OK;
} // 0x80344950

/*
====================
Stream::setPosition

Seeks the stream (or active sub-sound/sentence entry) to position in postype units.
====================
*/
FMOD_RESULT Stream::setPosition( unsigned int position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT result   = FMOD_OK;
    bool        seekable = true;

    if ( postype == FMOD_TIMEUNIT_PCM ) {
        if ( position > ( ( mMode & FMOD_LOOP_OFF ) ? ( mLength - 1 ) : ( mLoopStart + mLoopLength - 1 ) ) ) {
            return FMOD_ERR_INVALID_POSITION;
        }
    }

    if ( mCodec->mFile ) {
        seekable = ( mCodec->mFile->mFlags & FILE_FLAG_SEEKABLE ) ? true : false;
    }

    mFlags &= ~( SOUNDI_FLAG_EOF | SOUNDI_FLAG_0x40 );
    if ( mSubSoundParent ) {
        mSubSoundParent->mFlags &= ~( SOUNDI_FLAG_EOF | SOUNDI_FLAG_0x40 );
    }

    if ( seekable ) {
        if ( mSubSound && mSubSoundList && postype == FMOD_TIMEUNIT_PCM ) {
            int          count;
            unsigned int offset = 0;

            for ( count = 0; count < mSubSoundListNum; count++ ) {
                SoundI *subsound = mSubSound[mSubSoundList[count].mIndex];

                if ( subsound ) {
                    if ( !mSubSoundList ) {
                        return FMOD_ERR_INVALID_PARAM;
                    }

                    if ( position >= offset && position < offset + mSubSoundList[count].mLength ) {
                        Stream *substream = static_cast< Stream * >( subsound );

                        mChannel->mSubSoundListCurrent = count;
                        mSubSoundIndex                 = mSubSoundList[count].mIndex;

                        if ( mSubSoundShared ) {
                            substream->updateSubSound( mSubSoundIndex, true );
                        } else {
                            substream->mSubSoundIndex = mSubSoundIndex;
                            mSample->mCodec           = substream->mCodec;
                        }

                        substream->setPosition( position - offset, postype );
                        break;
                    }

                    offset += mSubSoundList[count].mLength;
                }
            }
        } else if ( mSubSound && postype == FMOD_TIMEUNIT_PCM ) {
            SoundI *subsound = mSubSound[mSubSoundIndex];

            if ( subsound ) {
                result = static_cast< Stream * >( subsound )->setPosition( position, postype );
            }
        } else {
            mCodec->reset();

            result = mCodec->setPosition(
                ( ( mSubSoundParent && mSubSoundParent->mNumSubSounds ) || mNumSubSounds ) ? mSubSoundIndex : 0,
                position, postype );
            if ( result != FMOD_OK ) {
                return result;
            }

            if ( mSubSoundParent ) {
                mSubSoundParent->mSubSoundIndex = mSubSoundIndex;
            }
        }

        if ( mSample && mSample->mPostSetPositionCallback ) {
            mSample->mPostSetPositionCallback( reinterpret_cast< FMOD_SOUND * >( this ), mSubSoundIndex,
                                               position, postype );
        }

        if ( postype != FMOD_TIMEUNIT_MS && postype != FMOD_TIMEUNIT_PCM && postype != FMOD_TIMEUNIT_PCMBYTES ) {
            position = 0;
        }

        mPosition = position;
        mLastPos  = position;
    } else {
        if ( mLastPos != 0 || position != 0 ) {
            return FMOD_ERR_FILE_COULDNOTSEEK;
        }
    }

    return result;
} // 0x803449F8

/*
====================
Stream::getPosition

Queries the current stream position in postype units (sentence sub-sound, PCM, MS or codec).
====================
*/
FMOD_RESULT Stream::getPosition( unsigned int *position, FMOD_TIMEUNIT postype ) {
    FMOD_RESULT result;

    if ( mOpenState != FMOD_OPENSTATE_READY && mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    if ( !position ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    switch ( postype ) {
        case ( FMOD_TIMEUNIT_SENTENCE_SUBSOUND | FMOD_TIMEUNIT_BUFFERED ): {
            *position = mChannel->mSubSoundListCurrent;
            break;
        }

        case FMOD_TIMEUNIT_PCM: {
            *position = mLastPos;
            break;
        }

        case FMOD_TIMEUNIT_MS: {
            *position = (unsigned int)( (float)mLastPos / 1000.0f * mDefaultFrequency );
            break;
        }

        case FMOD_TIMEUNIT_PCMBYTES: {
            break;
        }

        default: {
            result = mCodec->getPosition( position, postype );
            if ( result != FMOD_OK ) {
                return result;
            }
            break;
        }
    }

    return FMOD_OK;
} // 0x80344D20

/*
====================
Stream::setLoopCount

Updates both the current remaining loop count and the stored loop count.
====================
*/
FMOD_RESULT Stream::setLoopCount( int loopcount ) {
    if ( mOpenState != FMOD_OPENSTATE_READY && mOpenState != FMOD_OPENSTATE_SETPOSITION ) {
        return FMOD_ERR_NOTREADY;
    }

    mLoopCountCurrent = loopcount;
    mLoopCount        = loopcount;

    return FMOD_OK;
} // 0x80344E0C

bool Stream::isStream() {
    return true;
} // 0x80344E38

} // namespace FMOD
