// Unit name and boundary: Gormiti link map (fmodex.a fmod_codec_raw.obj)
//
// File::getSize (inline virtual, weak) is also emitted in this object, as in every object that calls it
// out of line; retail keeps the one copy of fmod_systemi.obj (0x8034F048), so this one is discarded at the link.

#include <fmod/fmod_codec_raw.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_soundi.h>
#include <string.h>  // memset

namespace FMOD {

// The codec description. Its constructor is the static initialiser; getDescriptionEx fills it in.
static FMOD_CODEC_DESCRIPTION_EX lbl_80525588;

// 0x8036D3B4
FMOD_CODEC_DESCRIPTION_EX *CodecRaw::getDescriptionEx()
{
    memset( &lbl_80525588, 0, sizeof( lbl_80525588 ) );
    lbl_80525588.name = "FMOD Raw Codec";
    lbl_80525588.version = 0x00010100;
    lbl_80525588.timeunits = FMOD_TIMEUNIT_PCM | FMOD_TIMEUNIT_RAWBYTES;
    lbl_80525588.open = openCallback;
    lbl_80525588.close = closeCallback;
    lbl_80525588.read = readCallback;
    lbl_80525588.setposition = setPositionCallback;
    lbl_80525588.canpoint = (void *)canPointCallback;
    lbl_80525588.mType = FMOD_SOUND_TYPE_RAW;
    lbl_80525588.mSize = sizeof( CodecRaw );
    return &lbl_80525588;
}

// FMOD_CODEC_DESCRIPTION_EX hook: opens the raw PCM file; the sound's format, channels and rate come from the
// FMOD_CREATESOUNDEXINFO the user passed, the length from the file size.
// 0x8036D45C
FMOD_RESULT CodecRaw::openInternal( unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    FMOD_RESULT result;

    lbl_80529020 = mGlobal;
    mType = FMOD_SOUND_TYPE_RAW;
    mMetadata = 0;
    numsubsounds = 0;
    waveformat = 0;
    result = mFile->seek( 0, 0 );
    if ( result != FMOD_OK )
    {
        return result;
    }
    waveformat = &mWaveFormat;
    result = mFile->getSize( &waveformat->lengthbytes );
    if ( result != FMOD_OK )
    {
        return result;
    }
    mSrcDataOffset = 0;
    if ( usermode & FMOD_SOFTWARE )
    {
        if ( usermode & FMOD_CREATECOMPRESSEDSAMPLE )
        {
            return FMOD_ERR_FORMAT;
        }
        if ( userexinfo->format < FMOD_SOUND_FORMAT_PCM8 || userexinfo->format > FMOD_SOUND_FORMAT_PCMFLOAT )
        {
            return FMOD_ERR_FORMAT;
        }
    }
    waveformat->format = userexinfo->format;
    waveformat->channels = userexinfo->numchannels;
    waveformat->frequency = userexinfo->defaultfrequency;
    SoundI::getSamplesFromBytes( waveformat->lengthbytes, &waveformat->lengthpcm, userexinfo->numchannels, userexinfo->format );
    waveformat->blockalign = ( waveformat->channels * 16 ) / 8;
    numsubsounds = 0;
    return result;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecRaw::closeInternal()
{
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecRaw::readInternal( void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    FMOD_RESULT result;

    if ( waveformat->format == FMOD_SOUND_FORMAT_PCM16 )
    {
        result = mFile->read( buffer, 2, sizebytes / 2, bytesread );
        *bytesread *= 2;
    }
    else
    {
        result = mFile->read( buffer, 1, sizebytes, bytesread );
    }
    return result;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecRaw::setPositionInternal( int subsound, unsigned int position, unsigned int postype )
{
    FMOD_RESULT result;
    unsigned int offset;

    if ( postype == FMOD_TIMEUNIT_RAWBYTES )
    {
        offset = position;
    }
    else
    {
        offset = (unsigned int)( ( (unsigned long long)position * waveformat->lengthbytes ) / waveformat->lengthpcm );
        offset /= waveformat->blockalign;
        offset *= waveformat->blockalign;
    }
    result = mFile->seek( mSrcDataOffset + offset, 0 );
    if ( result != FMOD_OK )
    {
        return result;
    }
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecRaw::canPointInternal()
{
    return FMOD_OK;
}

// 0x8036D6C0
FMOD_RESULT F_CALLBACK CodecRaw::openCallback( FMOD_CODEC_STATE *codec_state, unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    return static_cast<CodecRaw *>( codec_state )->openInternal( usermode, userexinfo );
}

// 0x8036D6D0
FMOD_RESULT F_CALLBACK CodecRaw::closeCallback( FMOD_CODEC_STATE *codec_state )
{
    return static_cast<CodecRaw *>( codec_state )->closeInternal();
}

// 0x8036D6D8
FMOD_RESULT F_CALLBACK CodecRaw::readCallback( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    return static_cast<CodecRaw *>( codec_state )->readInternal( buffer, sizebytes, bytesread );
}

// Seeks to a sample position or a byte offset of the raw data, rounded down to a whole block.
// 0x8036D754
FMOD_RESULT F_CALLBACK CodecRaw::setPositionCallback( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, unsigned int postype )
{
    return static_cast<CodecRaw *>( codec_state )->setPositionInternal( subsound, position, postype );
}

// 0x8036D7F0
FMOD_RESULT F_CALLBACK CodecRaw::canPointCallback( FMOD_CODEC_STATE *codec_state )
{
    return static_cast<CodecRaw *>( codec_state )->canPointInternal();
}

} // namespace FMOD
