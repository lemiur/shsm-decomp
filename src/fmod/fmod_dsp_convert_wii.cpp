/*
 * fmod_dsp_convert_wii.cpp: the Wii's conversion of the mixer's float samples to 16 bit PCM.
 */
// Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_convert_wii.obj)
// Not matched (92.00%): register allocation of the strided pointers in the four-way unrolled loop (wall: register/schedule);
// the structure (unrolled by four, remainder loop, clamp shape, spill slots) is retail's. Parameter and local
// names other than destptr, srcptr and len (Gormiti DWARF) are invented.

// The value of a scaled sample held inside the range of a 16 bit sample.
#define FMOD_PCM16_CLAMP( _val ) ( ( _val ) < -32768 ? -32768 : ( ( _val ) > 32767 ? 32767 : (short)( _val ) ) )

/*
====================
FMOD_DSP_Convert_FloatToPCM16

Converts length samples of srcchannels interleaved floats to destchannels interleaved 16 bit samples,
scaling each by scale.
====================
*/
extern "C" void FMOD_DSP_Convert_FloatToPCM16( short *destptr, float *srcptr, unsigned int length, int destchannels,
                                               int srcchannels, float scale ) {
    unsigned int len = length >> 2;

    while ( len ) {
        int val0 = (int)( srcptr[0] * scale );
        int val1 = (int)( scale * srcptr[srcchannels] );
        int val2 = (int)( scale * srcptr[srcchannels * 2] );
        int val3 = (int)( scale * srcptr[srcchannels * 3] );

        destptr[0]                = FMOD_PCM16_CLAMP( val0 );
        destptr[destchannels]     = FMOD_PCM16_CLAMP( val1 );
        destptr[destchannels * 2] = FMOD_PCM16_CLAMP( val2 );
        destptr[destchannels * 3] = FMOD_PCM16_CLAMP( val3 );

        srcptr += srcchannels * 4;
        destptr += destchannels * 4;
        len--;
    }

    len = length & 3;
    while ( len ) {
        int val = (int)( *srcptr * scale );

        *destptr = FMOD_PCM16_CLAMP( val );

        srcptr += srcchannels;
        destptr += destchannels;
        len--;
    }
} // 0x80357C3C
