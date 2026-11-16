/*
 * FMOD::CodecDSP, the Nintendo GameCube/Wii DSP-ADPCM (GCADPCM) codec (fmod_codec_dsp.cpp).
 * Layout: Gormiti DWARF (sibling.exe types FMOD::CodecDSP, FMOD_GCADPCMHDR).
 */
#ifndef _FMOD_CODEC_DSP_H
#define _FMOD_CODEC_DSP_H

#include <fmod/fmod_codeci.h>

#pragma pack(push, 1)
// The 0x60-byte header of a DSP ADPCM file (big-endian, read straight from the file; it sits at an odd
// offset of CodecDSP, hence the packing).
struct FMOD_GCADPCMHDR {
    unsigned int   num_samples;                     // 0x00
    unsigned int   num_adpcm_nibbles;               // 0x04
    unsigned int   sample_rate;                     // 0x08
    unsigned short loop_flag;                       // 0x0C
    unsigned short format;                          // 0x0E
    unsigned int   sa;                              // 0x10
    unsigned int   ea;                              // 0x14
    unsigned int   ca;                              // 0x18
    unsigned short coef[16];                        // 0x1C
    unsigned short gain;                            // 0x3C
    unsigned short pred_scale;                      // 0x3E
    unsigned short yn1;                             // 0x40
    unsigned short yn2;                             // 0x42
    unsigned short loop_pred_scale;                 // 0x44
    unsigned short loop_yn1;                        // 0x46
    unsigned short loop_yn2;                        // 0x48
    unsigned short pad[11];                         // 0x4A
};
#pragma pack(pop)

namespace FMOD {

class CodecDSP : public Codec {
public:
    FMOD_GCADPCMHDR       mHeader;                  // 0xFD
    FMOD_CODEC_WAVEFORMAT mWaveFormat;              // 0x160
    void                 *mPluginDataPtr;           // 0x288

    static FMOD_CODEC_DESCRIPTION_EX *getDescriptionEx();

    FMOD_RESULT openInternal( unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo );
    FMOD_RESULT closeInternal();                    // no copy in retail (inlined)
    FMOD_RESULT readInternal( void *buffer, unsigned int sizebytes, unsigned int *bytesread );       // no copy in retail (inlined)
    FMOD_RESULT setPositionInternal( int subsound, unsigned int position, unsigned int postype );     // no copy in retail (inlined)
    FMOD_RESULT canPointInternal();                 // no copy in retail (inlined)

    static FMOD_RESULT F_CALLBACK openCallback( FMOD_CODEC_STATE *codec_state, unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo );
    static FMOD_RESULT F_CALLBACK closeCallback( FMOD_CODEC_STATE *codec_state );
    static FMOD_RESULT F_CALLBACK readCallback( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread );
    static FMOD_RESULT F_CALLBACK setPositionCallback( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, unsigned int postype );
    static FMOD_RESULT F_CALLBACK canPointCallback( FMOD_CODEC_STATE *codec_state );
};

} // namespace FMOD

#endif /* _FMOD_CODEC_DSP_H */
