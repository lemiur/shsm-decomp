/*
 * FMOD::CodecUser, the codec behind user-created sounds (fmod_codec_user.cpp).
 * Layout: Gormiti DWARF (sibling.exe types FMOD::CodecUser).
 */
#ifndef _FMOD_CODEC_USER_H
#define _FMOD_CODEC_USER_H

#include <fmod/fmod_codeci.h>

namespace FMOD {

class CodecUser : public Codec {
public:
    FMOD_CODEC_WAVEFORMAT mWaveFormat;              // 0x100

    static FMOD_CODEC_DESCRIPTION_EX *getDescriptionEx();

    FMOD_RESULT openInternal( unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo );
    FMOD_RESULT closeInternal();                    // no copy in retail (inlined)
    FMOD_RESULT readInternal( void *buffer, unsigned int sizebytes, unsigned int *bytesread );       // no copy in retail (inlined)
    FMOD_RESULT setPositionInternal( int subsound, unsigned int position, unsigned int postype );     // no copy in retail (inlined)

    static FMOD_RESULT F_CALLBACK openCallback( FMOD_CODEC_STATE *codec_state, unsigned int usermode, FMOD_CREATESOUNDEXINFO *userexinfo );
    static FMOD_RESULT F_CALLBACK closeCallback( FMOD_CODEC_STATE *codec_state );
    static FMOD_RESULT F_CALLBACK readCallback( FMOD_CODEC_STATE *codec_state, void *buffer, unsigned int sizebytes, unsigned int *bytesread );
    static FMOD_RESULT F_CALLBACK setPositionCallback( FMOD_CODEC_STATE *codec_state, int subsound, unsigned int position, unsigned int postype );
};

} // namespace FMOD

#endif /* _FMOD_CODEC_USER_H */
