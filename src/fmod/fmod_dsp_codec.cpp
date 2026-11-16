/*
 * fmod_dsp_codec.cpp: FMOD::DSPCodec, the DSP unit for playing through a codec.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_codec.obj)
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_dsp_codec.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_codeci.h>
#include <fmod/fmod_dsp_codec.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

/*
====================
DSPCodec::release

Closes the codec, releases the resampler, and frees this object.
====================
*/
// 0x8035AFDC
FMOD_RESULT DSPCodec::release( bool freethis ) {
    FMOD_RESULT result;

    result = mCodec->mDescription.close( mCodec );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = DSPResampler::release( false );
    if ( freethis ) {
#line 152
        lbl_80529020->mMemPool->free( this, __FILE__, __LINE__, ( mWaveFormat.format == FMOD_SOUND_FORMAT_XMA ) ? FMOD_MEMORY_XBOX360_PHYSICAL : FMOD_MEMORY_NORMAL );
    }

    return result;
}

} // namespace FMOD
