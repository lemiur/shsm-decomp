// Reference: doldecomp/mkdd libs/dolphin/thp/THPAudio.c (the retail decoder is a later revision with
// rounding, 16-bit saturation and an optional mono mix-down; the sample unpacking is unchanged)
#include <revolution/thp.h>

/* THPAudioDecode flag bits (invented names). */
#define THP_AUDIO_PLANAR  0x1 /* write the left channel after the right one instead of interleaving */
#define THP_AUDIO_MIXDOWN 0x2 /* mix the channels down to mono at -3 dB */

#define THP_PCM_MAX       32767
#define THP_PCM_MIN       (-32768)
#define THP_MIXDOWN_GAIN  0x5A82 /* sqrt(1/2) in 0.15 fixed point */
#define THP_MIXDOWN_ROUND 0x4000 /* half of 1 << 15 */

static s32 __THPAudioGetNewSample(THPAudioDecodeInfo* info);
static void __THPAudioInitialize(THPAudioDecodeInfo* info, u8* ptr);

/*
 * THPAudioDecode
 *
 * Decodes one THP audio frame (4-bit ADPCM, one or two channels) into 16-bit PCM and returns the sample count.
 */
u32 THPAudioDecode(s16* audioBuffer, u8* audioFrame, s32 flag) {
    THPAudioRecordHeader* header;
    THPAudioDecodeInfo decInfo;
    THPAudioDecodeInfo decInfoR;
    u8* left;
    u8* right;
    s16* decLeftPtr;
    s16* decRightPtr;
    s16 yn1;
    s16 yn2;
    s16 yn1R;
    s16 yn2R;
    s64 ynR;
    s32 sampleR;
    BOOL mixdown;
    s32 i;
    s32 step;
    s32 sample;
    s64 yn;

    if (audioBuffer == NULL || audioFrame == NULL) {
        return 0;
    }

    header = (THPAudioRecordHeader*)audioFrame;
    left = audioFrame + sizeof(THPAudioRecordHeader);
    right = left + header->offsetNextChannel;

    if (flag & THP_AUDIO_PLANAR) {
        decRightPtr = audioBuffer;
        decLeftPtr = audioBuffer + header->sampleSize;
        step = 1;
    } else {
        decRightPtr = audioBuffer;
        decLeftPtr = audioBuffer + 1;
        step = 2;
    }

    mixdown = FALSE;
    if (flag & THP_AUDIO_MIXDOWN) {
        mixdown = TRUE;
    }

    if (header->offsetNextChannel == 0) {
        __THPAudioInitialize(&decInfo, left);

        yn1 = header->lYn1;
        yn2 = header->lYn2;

        for (i = 0; i < header->sampleSize; i++) {
            sample = __THPAudioGetNewSample(&decInfo);
            yn = header->lCoef[decInfo.predictor][1] * yn2;
            yn += header->lCoef[decInfo.predictor][0] * yn1;
            yn += (sample << decInfo.scale) << 11;
            yn += 0x400;
            yn >>= 11;

            if (yn > THP_PCM_MAX) {
                yn = THP_PCM_MAX;
            } else if (yn < THP_PCM_MIN) {
                yn = THP_PCM_MIN;
            }

            yn2 = yn1;
            yn1 = (s16)yn;

            if (mixdown) {
                // A mono stream mixes the one sample with itself, so it is doubled and scaled by sqrt(1/2).
                yn = (yn << 1) * THP_MIXDOWN_GAIN + THP_MIXDOWN_ROUND;
                yn >>= 15;

                if (yn > THP_PCM_MAX) {
                    yn = THP_PCM_MAX;
                } else if (yn < THP_PCM_MIN) {
                    yn = THP_PCM_MIN;
                }
            }

            *decLeftPtr = (s16)yn;
            decLeftPtr += step;
            *decRightPtr = (s16)yn;
            decRightPtr += step;
        }
    } else {
        __THPAudioInitialize(&decInfo, left);
        __THPAudioInitialize(&decInfoR, right);

        yn1 = header->lYn1;
        yn2 = header->lYn2;
        yn1R = header->rYn1;
        yn2R = header->rYn2;

        for (i = 0; i < header->sampleSize; i++) {
            sample = __THPAudioGetNewSample(&decInfo);
            yn = header->lCoef[decInfo.predictor][1] * yn2;
            yn += header->lCoef[decInfo.predictor][0] * yn1;
            yn += (sample << decInfo.scale) << 11;
            yn += 0x400;
            yn >>= 11;

            if (yn > THP_PCM_MAX) {
                yn = THP_PCM_MAX;
            } else if (yn < THP_PCM_MIN) {
                yn = THP_PCM_MIN;
            }

            yn2 = yn1;
            yn1 = (s16)yn;

            sampleR = __THPAudioGetNewSample(&decInfoR);
            ynR = header->rCoef[decInfoR.predictor][1] * yn2R;
            ynR += header->rCoef[decInfoR.predictor][0] * yn1R;
            ynR += (sampleR << decInfoR.scale) << 11;
            ynR += 0x400;
            ynR >>= 11;

            if (ynR > THP_PCM_MAX) {
                ynR = THP_PCM_MAX;
            } else if (ynR < THP_PCM_MIN) {
                ynR = THP_PCM_MIN;
            }

            yn2R = yn1R;
            yn1R = (s16)ynR;

            if (mixdown) {
                yn = (yn + ynR) * THP_MIXDOWN_GAIN + THP_MIXDOWN_ROUND;
                yn >>= 15;

                if (yn > THP_PCM_MAX) {
                    yn = THP_PCM_MAX;
                } else if (yn < THP_PCM_MIN) {
                    yn = THP_PCM_MIN;
                }
                ynR = yn;
            }

            *decLeftPtr = (s16)yn;
            decLeftPtr += step;
            *decRightPtr = (s16)ynR;
            decRightPtr += step;
        }
    }

    return header->sampleSize;
} // 0x8020D210

/*
 * __THPAudioGetNewSample
 *
 * Returns the next signed 4-bit ADPCM nibble, starting a new 8-nibble block header when needed.
 */
static s32 __THPAudioGetNewSample(THPAudioDecodeInfo* info) {
    s32 sample;

    if (!(info->offsetNibbles & 0x0f)) {
        info->predictor = (u8)((*(info->encodeData) & 0x70) >> 4);
        info->scale = (u8)((*(info->encodeData) & 0xF));
        info->encodeData++;
        info->offsetNibbles += 2;
    }

    if (info->offsetNibbles & 0x1) {
        sample = (s32)((*(info->encodeData) & 0xF) << 28) >> 28;
        info->encodeData++;
    } else {
        sample = (s32)((*(info->encodeData) & 0xF0) << 24) >> 28;
    }

    info->offsetNibbles++;
    return sample;
}

/*
 * __THPAudioInitialize
 *
 * Points the decoder at the start of a channel and reads its first block header.
 */
static void __THPAudioInitialize(THPAudioDecodeInfo* info, u8* ptr) {
    info->encodeData = ptr;
    info->offsetNibbles = 2;
    info->predictor = (u8)((*(info->encodeData) & 0x70) >> 4);
    info->scale = (u8)((*(info->encodeData) & 0xF));
    info->encodeData++;
}
