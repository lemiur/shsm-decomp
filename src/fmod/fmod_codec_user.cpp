// Unit name and boundary: Gormiti link map (fmodex.a fmod_codec_user.obj)
//
// File::getSize (inline virtual, weak) is also emitted in this object, as in every object that calls it
// out of line; retail keeps the one copy of fmod_systemi.obj (0x8034F048), so this one is discarded at the link.

#include <fmod/fmod_codec_user.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_soundi.h>
#include <string.h>  // memset

namespace FMOD {

// The codec description. Its constructor is the static initialiser; getDescriptionEx fills it in.
static FMOD_CODEC_DESCRIPTION_EX lbl_805255F8;

// 0x8036D820
FMOD_CODEC_DESCRIPTION_EX *CodecUser::getDescriptionEx()
{
    memset( &lbl_805255F8, 0, sizeof( lbl_805255F8 ) );
    lbl_805255F8.name = "FMOD User Reader Codec";
    lbl_805255F8.version = 0x00010100;
    lbl_805255F8.timeunits = FMOD_TIMEUNIT_PCM;
    lbl_805255F8.open = openCallback;
    lbl_805255F8.close = closeCallback;
    lbl_805255F8.read = readCallback;
    lbl_805255F8.setposition = setPositionCallback;
    lbl_805255F8.mType = FMOD_SOUND_TYPE_USER;
    lbl_805255F8.mSize = sizeof( CodecUser );
    return &lbl_805255F8;
}

// The size in bytes of one block of the format for the given number of interleaved channels, the format's
// block alignment (helper name invented; 64-bit shift as in SoundI::getBytesFromSamples, which it resembles).
static inline FMOD_RESULT getBlockAlign( int *blockalign, int channels, FMOD_SOUND_FORMAT format )
{
    int bits = 0;

    SoundI::getBitsFromFormat( format, &bits );
    if ( bits )
    {
        *blockalign = (int)( (long long)bits >> 3 );
    }
    else
    {
        switch ( format )
        {
            case FMOD_SOUND_FORMAT_GCADPCM:  *blockalign = 8;  break;
            case FMOD_SOUND_FORMAT_IMAADPCM: *blockalign = 36; break;
            case FMOD_SOUND_FORMAT_VAG:      *blockalign = 16; break;
            case FMOD_SOUND_FORMAT_XMA:      *blockalign = 1;  return FMOD_OK;
            case FMOD_SOUND_FORMAT_MPEG:     *blockalign = 1;  return FMOD_OK;
            case FMOD_SOUND_FORMAT_NONE:     *blockalign = 0;  break;
            default:                         return FMOD_OK;
        }
    }
    *blockalign *= channels;
    return FMOD_OK;
}

// FMOD_CODEC_DESCRIPTION_EX hook: opens a user-created sound; the sound's format, channels and rate come from
// the FMOD_CREATESOUNDEXINFO the user passed, the length from its length member.
// 0x8036D8BC
FMOD_RESULT CodecUser::openInternal( unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    FMOD_RESULT result;

    lbl_80529020 = mGlobal;
    mType = FMOD_SOUND_TYPE_USER;
    mMetadata = 0;
    numsubsounds = 0;
    waveformat = 0;
    result = mFile->seek( 0, 0 );
    if ( result != FMOD_OK )
    {
        return result;
    }
    waveformat = &mWaveFormat;
    if ( userexinfo->format < FMOD_SOUND_FORMAT_PCM8 || userexinfo->format > FMOD_SOUND_FORMAT_PCMFLOAT )
    {
        return FMOD_ERR_FORMAT;
    }
    result = mFile->getSize( &waveformat->lengthbytes );
    if ( result != FMOD_OK )
    {
        return result;
    }
    mSrcDataOffset = 0;
    if ( userexinfo->length )
    {
        mFlags |= CODEC_FLAG_USERLENGTH;
    }
    waveformat->format = userexinfo->format;
    waveformat->channels = userexinfo->numchannels;
    waveformat->frequency = userexinfo->defaultfrequency;
    SoundI::getSamplesFromBytes( userexinfo->length, &waveformat->lengthpcm, userexinfo->numchannels, userexinfo->format );
    getBlockAlign( &waveformat->blockalign, userexinfo->numchannels, userexinfo->format );
    numsubsounds = 0;
    return result;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecUser::closeInternal()
{
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecUser::readInternal( void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    *bytesread = sizebytes;
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecUser::setPositionInternal( int subsound, unsigned int position, unsigned int postype )
{
    return FMOD_OK;
}

// 0x8036DC28
FMOD_RESULT F_CALLBACK CodecUser::openCallback( FMOD_CODEC_STATE *codec_state, unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    return static_cast<CodecUser *>( codec_state )->openInternal( usermode, userexinfo );
}

// 0x8036DC38
FMOD_RESULT F_CALLBACK CodecUser::closeCallback( FMOD_CODEC_STATE *codec_state )
{
    return static_cast<CodecUser *>( codec_state )->closeInternal();
}

// 0x8036DC40
FMOD_RESULT F_CALLBACK CodecUser::readCallback( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    return static_cast<CodecUser *>( codec_state )->readInternal( buffer, sizebytes, bytesread );
}

// 0x8036DC4C
FMOD_RESULT F_CALLBACK CodecUser::setPositionCallback( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, unsigned int postype )
{
    return static_cast<CodecUser *>( codec_state )->setPositionInternal( subsound, position, postype );
}

} // namespace FMOD
