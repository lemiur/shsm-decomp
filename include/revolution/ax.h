// Reference: doldecomp/ogws include/revolution/AX/{AX,AXAlloc,AXAux,AXCL,AXPB,AXVPB}.h
//
// Audio extension (AX) public API and the types the AX units share. The AX-internal functions (__AX...) are declared
// in revolution/ax/__ax.h.
#ifndef REVOLUTION_AX_H
#define REVOLUTION_AX_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AX_SAMPLE_RATE 32000
#define AX_MAX_VOLUME 32768
#define AX_VOICE_MAX 96

#define AX_SAMPLES_PER_FRAME 96
#define AX_SAMPLE_DEPTH_BYTES sizeof(u32)
#define AX_SAMPLES_PER_FRAME_RMT 18
#define AX_FRAME_SIZE (AX_SAMPLES_PER_FRAME * AX_SAMPLE_DEPTH_BYTES)

/* Remote (Wii remote speaker) output channels. */
#define AX_RMT_MAX 4

/* Voice priorities: 0 is the free list, 1 the lowest priority a voice can be acquired at. */
#define AX_PRIORITY_FREE 0
#define AX_PRIORITY_MIN 1
#define AX_PRIORITY_MAX 31

/* AXSetMode / AXGetMode output modes. */
enum {
    AX_OUTPUT_STEREO,
    AX_OUTPUT_SURROUND,
    AX_OUTPUT_DPL2
};

/* Aux bus channel order (stereo: left, right, surround; Dolby Pro Logic II: left, right, left surround, right surround). */
typedef enum {
    AX_STEREO_L,
    AX_STEREO_R,
    AX_STEREO_S,

    AX_STEREO_MAX
} AXStereoChannel;

typedef enum {
    AX_DPL2_L,
    AX_DPL2_R,
    AX_DPL2_LS,
    AX_DPL2_RS,

    AX_DPL2_MAX
} AXDPL2Channel;

typedef enum {
    AX_VOICE_NORMAL,
    AX_VOICE_STREAM
} AXVOICETYPE;

typedef enum {
    AX_VOICE_STOP,
    AX_VOICE_RUN
} AXVOICESTATE;

typedef enum {
    AX_SAMPLE_FORMAT_DSP_ADPCM = 0,
    AX_SAMPLE_FORMAT_PCM_S16 = 10,
    AX_SAMPLE_FORMAT_PCM_S8 = 25
} AXSAMPLETYPE;

/* AXPB::srcSelect and AXPB::coefSelect values. */
enum {
    AX_PB_SRCSEL_POLYPHASE,
    AX_PB_SRCSEL_LINEAR,
    AX_PB_SRCSEL_NONE
};

enum {
    AX_PB_COEFSEL_8KHZ,
    AX_PB_COEFSEL_12KHZ,
    AX_PB_COEFSEL_16KHZ
};

typedef enum {
    AX_SRC_TYPE_NONE,
    AX_SRC_TYPE_LINEAR,
    AX_SRC_TYPE_4TAP_8K,
    AX_SRC_TYPE_4TAP_12K,
    AX_SRC_TYPE_4TAP_16K,
    AX_SRC_TYPE_4TAP_AUTO
} AXPBSRCTYPE;

/* Voice parameter block (the layout the AX DSP program reads). */

typedef struct _AXPBMIX {
    /* 0x00 */ u16 vL;
    /* 0x02 */ u16 vDeltaL;
    /* 0x04 */ u16 vR;
    /* 0x06 */ u16 vDeltaR;
    /* 0x08 */ u16 vAuxAL;
    /* 0x0A */ u16 vDeltaAuxAL;
    /* 0x0C */ u16 vAuxAR;
    /* 0x0E */ u16 vDeltaAuxAR;
    /* 0x10 */ u16 vAuxBL;
    /* 0x12 */ u16 vDeltaAuxBL;
    /* 0x14 */ u16 vAuxBR;
    /* 0x16 */ u16 vDeltaAuxBR;
    /* 0x18 */ u16 vAuxCL;
    /* 0x1A */ u16 vDeltaAuxCL;
    /* 0x1C */ u16 vAuxCR;
    /* 0x1E */ u16 vDeltaAuxCR;
    /* 0x20 */ u16 vS;
    /* 0x22 */ u16 vDeltaS;
    /* 0x24 */ u16 vAuxAS;
    /* 0x26 */ u16 vDeltaAuxAS;
    /* 0x28 */ u16 vAuxBS;
    /* 0x2A */ u16 vDeltaAuxBS;
    /* 0x2C */ u16 vAuxCS;
    /* 0x2E */ u16 vDeltaAuxCS;
} AXPBMIX;

typedef struct _AXPBITD {
    /* 0x00 */ u16 flag;
    /* 0x02 */ u16 bufferHi;
    /* 0x04 */ u16 bufferLo;
    /* 0x06 */ u16 shiftL;
    /* 0x08 */ u16 shiftR;
    /* 0x0A */ u16 targetShiftL;
    /* 0x0C */ u16 targetShiftR;
} AXPBITD;

typedef struct _AXPBDPOP {
    /* 0x00 */ s16 aL;
    /* 0x02 */ s16 aAuxAL;
    /* 0x04 */ s16 aAuxBL;
    /* 0x06 */ s16 aAuxCL;
    /* 0x08 */ s16 aR;
    /* 0x0A */ s16 aAuxAR;
    /* 0x0C */ s16 aAuxBR;
    /* 0x0E */ s16 aAuxCR;
    /* 0x10 */ s16 aS;
    /* 0x12 */ s16 aAuxAS;
    /* 0x14 */ s16 aAuxBS;
    /* 0x16 */ s16 aAuxCS;
} AXPBDPOP;

typedef struct _AXPBVE {
    /* 0x00 */ u16 currentVolume;
    /* 0x02 */ s16 currentDelta;
} AXPBVE;

typedef struct _AXPBADDR {
    /* 0x00 */ u16 loopFlag;
    /* 0x02 */ u16 format;
    /* 0x04 */ u16 loopAddressHi;
    /* 0x06 */ u16 loopAddressLo;
    /* 0x08 */ u16 endAddressHi;
    /* 0x0A */ u16 endAddressLo;
    /* 0x0C */ u16 currentAddressHi;
    /* 0x0E */ u16 currentAddressLo;
} AXPBADDR;

typedef struct _AXPBADPCM {
    /* 0x00 */ u16 a[8][2];
    /* 0x20 */ u16 gain;
    /* 0x22 */ u16 pred_scale;
    /* 0x24 */ u16 yn1;
    /* 0x26 */ u16 yn2;
} AXPBADPCM;

typedef struct _AXPBSRC {
    /* 0x00 */ u16 ratioHi;
    /* 0x02 */ u16 ratioLo;
    /* 0x04 */ u16 currentAddressFrac;
    /* 0x06 */ u16 last_samples[4];
} AXPBSRC;

typedef struct _AXPBADPCMLOOP {
    /* 0x00 */ u16 loop_pred_scale;
    /* 0x02 */ u16 loop_yn1;
    /* 0x04 */ u16 loop_yn2;
} AXPBADPCMLOOP;

typedef struct _AXPBLPF {
    /* 0x00 */ u16 on;
    /* 0x02 */ u16 yn1;
    /* 0x04 */ u16 a0;
    /* 0x06 */ u16 b0;
} AXPBLPF;

typedef struct _AXPBBIQUAD {
    /* 0x00 */ u16 on;
    /* 0x02 */ u16 xn1;
    /* 0x04 */ u16 xn2;
    /* 0x06 */ u16 yn1;
    /* 0x08 */ u16 yn2;
    /* 0x0A */ u16 b0;
    /* 0x0C */ u16 b1;
    /* 0x0E */ u16 b2;
    /* 0x10 */ u16 a1;
    /* 0x12 */ u16 a2;
} AXPBBIQUAD;

typedef struct _AXPBRMTMIX {
    /* 0x00 */ u16 vMain0;
    /* 0x02 */ u16 vDeltaMain0;
    /* 0x04 */ u16 vAux0;
    /* 0x06 */ u16 vDeltaAux0;
    /* 0x08 */ u16 vMain1;
    /* 0x0A */ u16 vDeltaMain1;
    /* 0x0C */ u16 vAux1;
    /* 0x0E */ u16 vDeltaAux1;
    /* 0x10 */ u16 vMain2;
    /* 0x12 */ u16 vDeltaMain2;
    /* 0x14 */ u16 vAux2;
    /* 0x16 */ u16 vDeltaAux2;
    /* 0x18 */ u16 vMain3;
    /* 0x1A */ u16 vDeltaMain3;
    /* 0x1C */ u16 vAux3;
    /* 0x1E */ u16 vDeltaAux3;
} AXPBRMTMIX;

typedef struct _AXPBRMTDPOP {
    /* 0x00 */ s16 aMain0;
    /* 0x02 */ s16 aMain1;
    /* 0x04 */ s16 aMain2;
    /* 0x06 */ s16 aMain3;
    /* 0x08 */ s16 aAux0;
    /* 0x0A */ s16 aAux1;
    /* 0x0C */ s16 aAux2;
    /* 0x0E */ s16 aAux3;
} AXPBRMTDPOP;

typedef struct _AXPBRMTSRC {
    /* 0x00 */ u16 currentAddressFrac;
    /* 0x02 */ u16 last_samples[4];
} AXPBRMTSRC;

typedef union __AXPBRMTIIR {
    AXPBLPF lpf;
    AXPBBIQUAD biquad;
} AXPBRMTIIR;

typedef struct _AXPB {
    /* 0x000 */ u16 nextHi;
    /* 0x002 */ u16 nextLo;
    /* 0x004 */ u16 currHi;
    /* 0x006 */ u16 currLo;
    /* 0x008 */ u16 srcSelect;
    /* 0x00A */ u16 coefSelect;
    /* 0x00C */ u32 mixerCtrl;
    /* 0x010 */ u16 state;
    /* 0x012 */ u16 type;
    /* 0x014 */ AXPBMIX mix;
    /* 0x044 */ AXPBITD itd;
    /* 0x052 */ AXPBDPOP dpop;
    /* 0x06A */ AXPBVE ve;
    /* 0x06E */ AXPBADDR addr;
    /* 0x07E */ AXPBADPCM adpcm;
    /* 0x0A6 */ AXPBSRC src;
    /* 0x0B4 */ AXPBADPCMLOOP adpcmLoop;
    /* 0x0BA */ AXPBLPF lpf;
    /* 0x0C2 */ AXPBBIQUAD biquad;
    /* 0x0D6 */ u16 remote;
    /* 0x0D8 */ u16 rmtMixerCtrl;
    /* 0x0DA */ AXPBRMTMIX rmtMix;
    /* 0x0FA */ AXPBRMTDPOP rmtDpop;
    /* 0x10A */ AXPBRMTSRC rmtSrc;
    /* 0x114 */ AXPBRMTIIR rmtIIR;
    /* 0x128 */ u8 padding[0x140 - 0x128]; // pad: SDK name (ogws AXPB.h), pads the block to the DSP's 0x140 bytes
} AXPB;

/* AXVPB::sync flags: which parameter block fields __AXSyncPBs copies to the DSP's copy. */
enum {
    AX_PBSYNC_SELECT = (1 << 0),
    AX_PBSYNC_MIXER_CTRL = (1 << 1),
    AX_PBSYNC_STATE = (1 << 2),
    AX_PBSYNC_TYPE = (1 << 3),
    AX_PBSYNC_MIX = (1 << 4),
    AX_PBSYNC_ITD = (1 << 5),
    AX_PBSYNC_ITD_SHIFT = (1 << 6),
    AX_PBSYNC_DPOP = (1 << 7),
    AX_PBSYNC_VE = (1 << 8),
    AX_PBSYNC_VE_DELTA = (1 << 9),
    AX_PBSYNC_ADDR = (1 << 10),
    AX_PBSYNC_LOOP_FLAG = (1 << 11),
    AX_PBSYNC_LOOP_ADDR = (1 << 12),
    AX_PBSYNC_END_ADDR = (1 << 13),
    AX_PBSYNC_CURR_ADDR = (1 << 14),
    AX_PBSYNC_ADPCM = (1 << 15),
    AX_PBSYNC_SRC = (1 << 16),
    AX_PBSYNC_SRC_RATIO = (1 << 17),
    AX_PBSYNC_ADPCM_LOOP = (1 << 18),
    AX_PBSYNC_LPF = (1 << 19),
    AX_PBSYNC_LPF_COEFS = (1 << 20),
    AX_PBSYNC_BIQUAD = (1 << 21),
    AX_PBSYNC_BIQUAD_COEFS = (1 << 22),
    AX_PBSYNC_REMOTE = (1 << 23),
    AX_PBSYNC_RMT_MIXER_CTRL = (1 << 24),
    AX_PBSYNC_RMTMIX = (1 << 25),
    AX_PBSYNC_RMTDPOP = (1 << 26),
    AX_PBSYNC_RMTSRC = (1 << 27),
    AX_PBSYNC_RMTIIR = (1 << 28),
    AX_PBSYNC_RMTIIR_LPF_COEFS = (1 << 29),
    AX_PBSYNC_RMTIIR_BIQUAD_COEFS = (1 << 30),
    AX_PBSYNC_ALL = (1 << 31)
};

/* LPF / biquad `on` values. */
enum {
    AX_PB_LPF_ON = 1,
    AX_PB_BIQUAD_ON
};

/* AXPB::mixerCtrl bits: which of the mixer's levels the DSP interpolates. */
typedef enum {
    AX_MIXER_CTRL_L = (1 << 0),
    AX_MIXER_CTRL_R = (1 << 1),
    AX_MIXER_CTRL_DELTA = (1 << 2),
    AX_MIXER_CTRL_S = (1 << 3),
    AX_MIXER_CTRL_DELTA_S = (1 << 4),

    AX_MIXER_CTRL_AL = (1 << 16),
    AX_MIXER_CTRL_AR = (1 << 17),
    AX_MIXER_CTRL_DELTA_A = (1 << 18),
    AX_MIXER_CTRL_AS = (1 << 19),
    AX_MIXER_CTRL_DELTA_AS = (1 << 20),

    AX_MIXER_CTRL_BL = (1 << 21),
    AX_MIXER_CTRL_BR = (1 << 22),
    AX_MIXER_CTRL_DELTA_B = (1 << 23),
    AX_MIXER_CTRL_BS = (1 << 24),
    AX_MIXER_CTRL_DELTA_BS = (1 << 25),

    AX_MIXER_CTRL_CL = (1 << 26),
    AX_MIXER_CTRL_CR = (1 << 27),
    AX_MIXER_CTRL_DELTA_C = (1 << 28),
    AX_MIXER_CTRL_CS = (1 << 29),
    AX_MIXER_CTRL_DELTA_CS = (1 << 30)
} AXMIXERCTRL;

/* AXPB::rmtMixerCtrl bits (the remote's four output channels). */
typedef enum {
    AX_MIXER_CTRL_RMT_M0 = (1 << 0),
    AX_MIXER_CTRL_RMT_DELTA_M0 = (1 << 1),
    AX_MIXER_CTRL_RMT_A0 = (1 << 2),
    AX_MIXER_CTRL_RMT_DELTA_A0 = (1 << 3),

    AX_MIXER_CTRL_RMT_M1 = (1 << 4),
    AX_MIXER_CTRL_RMT_DELTA_M1 = (1 << 5),
    AX_MIXER_CTRL_RMT_A1 = (1 << 6),
    AX_MIXER_CTRL_RMT_DELTA_A1 = (1 << 7),

    AX_MIXER_CTRL_RMT_M2 = (1 << 8),
    AX_MIXER_CTRL_RMT_DELTA_M2 = (1 << 9),
    AX_MIXER_CTRL_RMT_A2 = (1 << 10),
    AX_MIXER_CTRL_RMT_DELTA_A2 = (1 << 11),

    AX_MIXER_CTRL_RMT_M3 = (1 << 12),
    AX_MIXER_CTRL_RMT_DELTA_M3 = (1 << 13),
    AX_MIXER_CTRL_RMT_A3 = (1 << 14),
    AX_MIXER_CTRL_RMT_DELTA_A3 = (1 << 15)
} AXMIXERCTRLRMT;

typedef void (*AXVoiceCallback)(void* vpb);

/* Voice: the AX library's handle on a voice parameter block. */
typedef struct _AXVPB {
    /* 0x00 */ struct _AXVPB* next;  /* priority stack link (toward the bottom) */
    /* 0x04 */ struct _AXVPB* prev;  /* priority stack link (toward the head) */
    /* 0x08 */ struct _AXVPB* next1; /* callback stack link */
    /* 0x0C */ u32 priority;
    /* 0x10 */ AXVoiceCallback callback;
    /* 0x14 */ u32 userContext;
    /* 0x18 */ u32 index;
    /* 0x1C */ u32 sync;
    /* 0x20 */ u32 depop;
    /* 0x24 */ void* itdBuffer;
    /* 0x28 */ AXPB pb;
} AXVPB;

typedef void (*AXAuxCallback)(void* chans, void* context);

/* Output callbacks: once per audio frame, and when the DSP exceeds its frame budget (with the cycles used). */
typedef void (*AXOutCallback)(void);
typedef void (*AXExceedCallback)(u32 cycles);

/* Timing of one audio frame, in time base ticks. */
typedef struct _AXPROFILE {
    /* 0x00 */ s64 timeBegin;
    /* 0x08 */ s64 timeProcessAuxBegin;
    /* 0x10 */ s64 timeProcessAuxEnd;
    /* 0x18 */ s64 timeUserFrameBegin;
    /* 0x20 */ s64 timeUserFrameEnd;
    /* 0x28 */ s64 timeEnd;
    /* 0x30 */ s32 numVoices;
    /* 0x34 */ u32 unk_0x34; /* never written by the output unit */
} AXPROFILE;

// AX
void AXInit(void);
void AXInitEx(u32 mode);
void AXQuit(void);
BOOL AXIsInit(void);

// AXAlloc
void AXFreeVoice(AXVPB* vpb);
AXVPB* AXAcquireVoice(u32 prio, AXVoiceCallback callback, u32 userContext);
void AXSetVoicePriority(AXVPB* vpb, u32 prio);

// AXVPB
void AXSetVoiceSrcType(AXVPB* vpb, u32 type);
void AXSetVoiceState(AXVPB* vpb, u16 state);
void AXSetVoiceType(AXVPB* vpb, u16 type);
void AXSetVoiceAddr(AXVPB* vpb, AXPBADDR* addr);
void AXSetVoiceCurrentAddr(AXVPB* vpb, u32 addr);
void AXSetVoiceAdpcm(AXVPB* vpb, AXPBADPCM* adpcm);
void AXSetVoiceSrc(AXVPB* vpb, AXPBSRC* src);
void AXSetVoiceSrcRatio(AXVPB* vpb, f32 ratio);
void AXSetVoiceAdpcmLoop(AXVPB* vpb, AXPBADPCMLOOP* loop);
void AXSetVoiceLpf(AXVPB* vpb, AXPBLPF* lpf);
void AXSetVoiceLpfCoefs(AXVPB* vpb, u16 a0, u16 b0);
void AXGetLpfCoefs(u16 freq, u16* a, u16* b);
void AXSetVoiceBiquad(AXVPB* vpb, AXPBBIQUAD* biquad);
void AXSetVoiceBiquadCoefs(AXVPB* vpb, u16 b0, u16 b1, u16 b2, u16 a1, u16 a2);
void AXSetVoiceRmtOn(AXVPB* vpb, u16 on);
s32 AXGetMaxVoices(void);

// AXAux
void AXRegisterAuxACallback(AXAuxCallback callback, void* context);
void AXRegisterAuxBCallback(AXAuxCallback callback, void* context);
void AXRegisterAuxCCallback(AXAuxCallback callback, void* context);
void AXGetAuxACallback(AXAuxCallback* callback, void** context);
void AXGetAuxBCallback(AXAuxCallback* callback, void** context);
void AXGetAuxCCallback(AXAuxCallback* callback, void** context);

// AXCL
void AXSetMode(u32 mode);
u32 AXGetMode(void);
u16 AXGetAuxAReturnVolume(void);
u16 AXGetAuxBReturnVolume(void);
u16 AXGetAuxCReturnVolume(void);
void AXSetMasterVolume(u16 volume);
void AXSetAuxAReturnVolume(u16 volume);
void AXSetAuxBReturnVolume(u16 volume);
void AXSetAuxCReturnVolume(u16 volume);

// AXOut
AXOutCallback AXRegisterCallback(AXOutCallback callback);
s32 AXRmtGetSamplesLeft(void);
s32 AXRmtGetSamples(s32 chan, s16* out, s32 num);
s32 AXRmtAdvancePtr(s32 num);

#ifdef __cplusplus
}
#endif

#endif
