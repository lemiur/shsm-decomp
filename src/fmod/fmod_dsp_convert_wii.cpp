/*
 * fmod_dsp_convert_wii.cpp: the Wii's conversion of the mixer's float samples to 16 bit PCM.
 */
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_convert_wii.obj).
// Parameter names other than destptr, srcptr and len are invented.
// Not matched: register/schedule wall (wall.exe). Control flow is isomorphic to retail (both loops bottom-tested,
// unrolled by four), but the strided pointers and the quad count take other callee-saved registers than retail.
// The four converted samples firstFrame..fourthFrame are the loop's own temporaries; the sibling's DWARF does not list them.

// The value of a scaled sample held inside the range of a 16 bit sample.
#define FMOD_PCM16_CLAMP( _val ) ( ( _val ) < -32768 ? -32768 : ( ( _val ) > 32767 ? 32767 : (short)( _val ) ) )

/*
====================
FMOD_DSP_Convert_FloatToPCM16

Converts length samples of srcchannels interleaved floats to destchannels interleaved 16 bit samples,
scaling each by scale.
====================
*/
// 0x80357C3C
extern "C" void FMOD_DSP_Convert_FloatToPCM16( short *destptr, float *srcptr, unsigned int length, int destchannels,
                                               int srcchannels, float scale ) {
    unsigned int len = length >> 2;
    unsigned int i;

    for ( ; len != 0; len-- ) {
        int firstFrame = (int)( srcptr[0] * scale );
        int secondFrame = (int)( srcptr[srcchannels] * scale );
        int thirdFrame = (int)( srcptr[2 * srcchannels] * scale );
        int fourthFrame = (int)( srcptr[3 * srcchannels] * scale );

        destptr[0] = FMOD_PCM16_CLAMP( firstFrame );
        destptr[destchannels] = FMOD_PCM16_CLAMP( secondFrame );
        destptr[2 * destchannels] = FMOD_PCM16_CLAMP( thirdFrame );
        destptr[3 * destchannels] = FMOD_PCM16_CLAMP( fourthFrame );

        srcptr += 4 * srcchannels;
        destptr += 4 * destchannels;
    }

    for ( i = length & 3; i != 0; i-- ) {
        int val = (int)( srcptr[0] * scale );

        destptr[0] = FMOD_PCM16_CLAMP( val );
        srcptr += srcchannels;
        destptr += destchannels;
    }
}
