// Reference: ThePlayerRolo/sdk_2009-12-11 include/revolution/wenc.h
// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/wenc/wenc.h (WENC_FLAG_USER_INFO)
#ifndef REVOLUTION_WENC_H
#define REVOLUTION_WENC_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wii Remote speaker ADPCM encoder. */

/* Flags of WENCGetEncodeData. */
enum {
    WENC_FLAG_USER_INFO = 1 << 0 /* continue from the state in WENCInfo (clear: start a new stream) */
};

/* Encoder state carried from one packet to the next (0x20 bytes). */
typedef struct {
    u8 data[32];
} WENCInfo;

s32 WENCGetEncodeData(WENCInfo* info, u32 flag, const s16* pcmData, s32 samples, u8* adpcmData);

#ifdef __cplusplus
}
#endif

#endif
