// Unit name and boundary: Gormiti link map (fmodex.a fmod_codec_dsp.obj)

#include <fmod/fmod_codec_dsp.h>
#include <fmod/fmod_file.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_soundi.h>
#include <string.h>  // memset

namespace FMOD {

// The codec description. Its constructor is the static initialiser; getDescriptionEx fills it in.
static FMOD_CODEC_DESCRIPTION_EX lbl_80524540;

// 0x80356C38
FMOD_CODEC_DESCRIPTION_EX *CodecDSP::getDescriptionEx()
{
    memset( &lbl_80524540, 0, sizeof( lbl_80524540 ) );
    lbl_80524540.name = "GCADPCM DSP Codec";
    lbl_80524540.version = 0x00010100;
    lbl_80524540.timeunits = FMOD_TIMEUNIT_PCM;
    lbl_80524540.open = openCallback;
    lbl_80524540.close = closeCallback;
    lbl_80524540.read = readCallback;
    lbl_80524540.setposition = setPositionCallback;
    lbl_80524540.canpoint = (void *)canPointCallback;
    lbl_80524540.mType = FMOD_SOUND_TYPE_GCADPCM;
    lbl_80524540.mSize = sizeof( CodecDSP );
    return &lbl_80524540;
}

// FMOD_CODEC_DESCRIPTION_EX hook: opens a DSP ADPCM file; the header at the start of the file gives the
// sample count, rate and loop flag of the (always mono) GCADPCM sound.
// 0x80356CE0
FMOD_RESULT CodecDSP::openInternal( unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    FMOD_RESULT result;
    unsigned int rd;

    result = mFile->seek( 0, 0 );
    if ( result != FMOD_OK )
    {
        return result;
    }
    result = mFile->read( &mHeader, 1, sizeof( FMOD_GCADPCMHDR ), &rd );
    if ( result != FMOD_OK )
    {
        return result;
    }
    if ( (int)mHeader.sample_rate < 0 || mHeader.loop_flag > 1 || mHeader.format != 0 ||
         mHeader.num_samples > mHeader.num_adpcm_nibbles || mHeader.gain != 0 )
    {
        return FMOD_ERR_FORMAT;
    }
    result = mFile->tell( &mSrcDataOffset );
    if ( result != FMOD_OK )
    {
        return result;
    }
    waveformat = &mWaveFormat;
    waveformat->format = FMOD_SOUND_FORMAT_GCADPCM;
    waveformat->channels = 1;
    waveformat->frequency = mHeader.sample_rate;
    waveformat->lengthpcm = mHeader.num_samples;
    waveformat->lengthpcm /= 14;
    waveformat->lengthpcm *= 14;
    waveformat->blockalign = 32;
    SoundI::getBytesFromSamples( mHeader.num_samples, &waveformat->lengthbytes, waveformat->channels, waveformat->format );
    if ( mHeader.loop_flag )
    {
        usermode |= FMOD_LOOP_NORMAL;
    }
    waveformat->mode = usermode;
    mPluginDataPtr = mHeader.coef;
    plugindata = &mPluginDataPtr;
    numsubsounds = 0;
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecDSP::closeInternal()
{
    return FMOD_OK;
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecDSP::readInternal( void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    return mFile->read( buffer, 1, sizebytes, bytesread );
}

// Seeks to a sample position of the file, in whole bytes of the format.
// no copy in retail (inlined)
inline FMOD_RESULT CodecDSP::setPositionInternal( int subsound, unsigned int position, unsigned int postype )
{
    FMOD_RESULT result;
    unsigned int bytes;

    if ( waveformat->format >= FMOD_SOUND_FORMAT_MAX || waveformat->format < 0 )
    {
        result = FMOD_ERR_FORMAT;
    }
    else
    {
        result = FMOD_OK;
    }
    if ( result != FMOD_OK )
    {
        return result;
    }
    SoundI::getBytesFromSamples( position, &bytes, waveformat->channels, waveformat->format );
    return mFile->seek( mSrcDataOffset + bytes, 0 );
}

// no copy in retail (inlined)
inline FMOD_RESULT CodecDSP::canPointInternal()
{
    return FMOD_OK;
}

// 0x80356FCC
FMOD_RESULT F_CALLBACK CodecDSP::openCallback( FMOD_CODEC_STATE *codec_state, unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo )
{
    return static_cast<CodecDSP *>( codec_state )->openInternal( usermode, userexinfo );
}

// 0x80356FDC
FMOD_RESULT F_CALLBACK CodecDSP::closeCallback( FMOD_CODEC_STATE *codec_state )
{
    return static_cast<CodecDSP *>( codec_state )->closeInternal();
}

// 0x80356FE4
FMOD_RESULT F_CALLBACK CodecDSP::readCallback( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread )
{
    return static_cast<CodecDSP *>( codec_state )->readInternal( buffer, sizebytes, bytesread );
}

// Not byte-exact (96.15%, wall class: register/schedule). The format test of setPositionInternal written
// with || on waveformat->format gives retail's blocks (two signed compares sharing one FMOD_ERR_FORMAT
// block), but this source reloads the format for the inlined SoundI::getBytesFromSamples where retail keeps
// the first load in r9. Any spelling that holds the format in a variable (a local, an int local, an inline
// helper's parameter, by value or through the waveformat pointer; ||, &&, !, if/else-if, statement-split
// returns, ?:) is merged by the compiler into one unsigned compare (cmplwi 10). Gormiti's DWARF names the
// callback's one local pRsAdpcm (the cast codec state). Closer state (97.32%): a format local declared
// first (`FMOD_SOUND_FORMAT format = waveformat->format;`) with the test still on waveformat->format and
// format passed on keeps both signed compares and retail's r9; what remains is retail's reload of
// waveformat before the channels read. Exact only when channels is read through a const this
// (`static_cast<const CodecDSP *>( this )->waveformat->channels`, a probe): a const-qualified read of
// waveformat, e.g. inside an inlined const member, is the missing piece. A const pointee read, a read
// through the FMOD_CODEC_STATE base and a const read of the format initialiser do not do it, nor does
// an inline layer that reads channels through a plain CodecDSP pointer parameter (no reload). In this
// source's form the pointer load survives the result diamond and the format load does not; retail is the
// reverse (format kept in r9, pointer reloaded), so the format is held in a variable from before the test
// while the test itself still reads memory (two signed compares).
// 0x80357004
FMOD_RESULT F_CALLBACK CodecDSP::setPositionCallback( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, unsigned int postype )
{
    return static_cast<CodecDSP *>( codec_state )->setPositionInternal( subsound, position, postype );
}

// 0x803571C4
FMOD_RESULT F_CALLBACK CodecDSP::canPointCallback( FMOD_CODEC_STATE *codec_state )
{
    return static_cast<CodecDSP *>( codec_state )->canPointInternal();
}

} // namespace FMOD
