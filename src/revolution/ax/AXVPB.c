// Reference: doldecomp/ogws src/revolution/AX/AXVPB.c
#include <math.h>
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>
#include <revolution/os.h>
#include <string.h>

// float-precision pi: 2 * M_PI compiles to the single-precision constant 6.2831855f in AXGetLpfCoefs
#define M_PI 3.1415927f

// Copies a parameter-block section one element at a time (ogws's macro and name; the compiler unrolls the loop).
#define FUNNY_COPY(dst, src, type, n)                                          \
    do {                                                                       \
                                                                               \
        type* __dst = (type*)(dst);                                              \
        type* __src = (type*)(src);                                              \
        u16 i;                                                                 \
                                                                               \
        for (i = 0; i < (n) / sizeof(type); i++) {                               \
            *__dst = *__src;                                                   \
            __dst++;                                                           \
            __src++;                                                           \
        }                                                                      \
                                                                               \
    } while (0)

#define ADDRHI(x) (((u32)(x)) >> 16 & 0xFFFF)
#define ADDRLO(x) (((u32)(x)) & 0xFFFF)

typedef struct _AXITD {
    u8 UNK_0x0[0x40];
} AXITD;

static AXPB* __AXPB = NULL;
static AXPB __s_AXPB[AX_VOICE_MAX] ATTRIBUTE_ALIGN(32);

static AXITD* __AXITD = NULL;
static AXITD __s_AXITD[AX_VOICE_MAX] ATTRIBUTE_ALIGN(32);

// Did they forget the alignment here?
static AXVPB* __AXVPB = NULL;
static AXVPB __s_AXVPB[AX_VOICE_MAX];

s32 __AXMaxVoices = 0;
static s32 __AXNumVoices = 0;

static u32 __AXRecDspCycles = 0;
static u32 __AXMaxDspCycles = 0;

static u32 __AXMixCycles[] = {2,   408,  408,  810,  1404, 1404, 1404, 1404,
                              408, 816,  816,  1218, 1812, 1812, 1812, 1812,
                              707, 1115, 1115, 1517, 2111, 2111, 2111, 2111,
                              707, 1115, 1115, 1517, 2111, 2111, 2111, 2111};

static u32 __AXRmtMixCycles[] = {4, 86, 151, 151};

static void __AXVPBInitCommon(void);
static u32 __AXGetSrcCycles(u16 select, const AXPBSRC* src);

/*
 * __AXGetNumVoices
 *
 * The number of voices the last sync handed to the DSP.
 */
s32 __AXGetNumVoices(void) {
    return __AXNumVoices;
}

/*
 * __AXServiceVPB
 *
 * Copies the fields the sync flags select from a voice to the DSP copy of its parameter block (and some back).
 */
void __AXServiceVPB(AXVPB* vpb) {
    AXPB* dst;
    AXPB* src;
    u32 sync;

    __AXNumVoices++;

    dst = &__AXPB[vpb->index];
    src = &vpb->pb;
    sync = vpb->sync;

    if (sync == 0) {
        src->state = dst->state;
        src->ve.currentVolume = dst->ve.currentVolume;
        src->addr.currentAddressHi = dst->addr.currentAddressHi;
        src->addr.currentAddressLo = dst->addr.currentAddressLo;
        return;
    }

    if (sync & AX_PBSYNC_ALL) {
        memcpy(dst, src, sizeof(AXPB));
        return;
    }

    if (sync & AX_PBSYNC_SELECT) {
        dst->srcSelect = src->srcSelect;
        dst->coefSelect = src->coefSelect;
    }

    if (sync & AX_PBSYNC_MIXER_CTRL) {
        dst->mixerCtrl = src->mixerCtrl;
    }

    if (sync & AX_PBSYNC_STATE) {
        dst->state = src->state;
    } else {
        src->state = dst->state;
    }

    if (sync & AX_PBSYNC_TYPE) {
        dst->type = src->type;
    }

    if (sync & AX_PBSYNC_MIX) {
        memcpy(&dst->mix, &src->mix, sizeof(AXPBMIX));
    }

    if (sync & AX_PBSYNC_ITD_SHIFT) {
        dst->itd.targetShiftL = src->itd.targetShiftL;
        dst->itd.targetShiftR = src->itd.targetShiftR;
    } else if (sync & AX_PBSYNC_ITD) {
        FUNNY_COPY(&dst->itd, &src->itd, u16, sizeof(AXPBITD));
        __memclr(vpb->itdBuffer, sizeof(AXITD));
    }

    if (sync & AX_PBSYNC_DPOP) {
        memcpy(&dst->dpop, &src->dpop, sizeof(AXPBDPOP));
    }

    if (sync & AX_PBSYNC_VE_DELTA) {
        src->ve.currentVolume = dst->ve.currentVolume;
        dst->ve.currentDelta = src->ve.currentDelta;
    } else if (sync & AX_PBSYNC_VE) {
        dst->ve.currentVolume = src->ve.currentVolume;
        dst->ve.currentDelta = src->ve.currentDelta;
    }

    if (sync & (AX_PBSYNC_LOOP_FLAG | AX_PBSYNC_LOOP_ADDR | AX_PBSYNC_END_ADDR |
                AX_PBSYNC_CURR_ADDR)) {
        if (sync & AX_PBSYNC_LOOP_FLAG) {
            dst->addr.loopFlag = src->addr.loopFlag;
        }

        if (sync & AX_PBSYNC_LOOP_ADDR) {
            *(u32*)&dst->addr.loopAddressHi = *(u32*)&src->addr.loopAddressHi;
        }

        if (sync & AX_PBSYNC_END_ADDR) {
            *(u32*)&dst->addr.endAddressHi = *(u32*)&src->addr.endAddressHi;
        }

        if (sync & AX_PBSYNC_CURR_ADDR) {
            *(u32*)&dst->addr.currentAddressHi =
                *(u32*)&src->addr.currentAddressHi;
        } else {
            *(u32*)&src->addr.currentAddressHi =
                *(u32*)&dst->addr.currentAddressHi;
        }
    } else if (sync & AX_PBSYNC_ADDR) {
        FUNNY_COPY(&dst->addr, &src->addr, u32, sizeof(AXPBADDR));
    } else {
        src->addr.currentAddressHi = dst->addr.currentAddressHi;
        src->addr.currentAddressLo = dst->addr.currentAddressLo;
    }

    if (sync & AX_PBSYNC_ADPCM) {
        FUNNY_COPY(&dst->adpcm, &src->adpcm, u32, sizeof(AXPBADPCM));
    }

    if (sync & AX_PBSYNC_SRC_RATIO) {
        dst->src.ratioHi = src->src.ratioHi;
        dst->src.ratioLo = src->src.ratioLo;
    } else if (sync & AX_PBSYNC_SRC) {
        FUNNY_COPY(&dst->src, &src->src, u16, sizeof(AXPBSRC));
    }

    if (sync & AX_PBSYNC_ADPCM_LOOP) {
        FUNNY_COPY(&dst->adpcmLoop, &src->adpcmLoop, u16,
                   sizeof(AXPBADPCMLOOP));
    }

    if (sync & AX_PBSYNC_LPF_COEFS) {
        dst->lpf.a0 = src->lpf.a0;
        dst->lpf.b0 = src->lpf.b0;
    } else if (sync & AX_PBSYNC_LPF) {
        FUNNY_COPY(&dst->lpf, &src->lpf, u16, sizeof(AXPBLPF));
    }

    if (sync & AX_PBSYNC_BIQUAD_COEFS) {
        dst->biquad.b0 = src->biquad.b0;
        dst->biquad.b1 = src->biquad.b1;
        dst->biquad.b2 = src->biquad.b2;
        dst->biquad.a1 = src->biquad.a1;
        dst->biquad.a2 = src->biquad.a2;
    } else if (sync & AX_PBSYNC_BIQUAD) {
        FUNNY_COPY(&dst->biquad, &src->biquad, u16, sizeof(AXPBBIQUAD));
    }

    if (sync & AX_PBSYNC_REMOTE) {
        dst->remote = src->remote;
    }

    if (sync & AX_PBSYNC_RMT_MIXER_CTRL) {
        dst->rmtMixerCtrl = src->rmtMixerCtrl;
    }

    if (sync & AX_PBSYNC_RMTMIX) {
        memcpy(&dst->rmtMix, &src->rmtMix, sizeof(AXPBRMTMIX));
    }

    if (sync & AX_PBSYNC_RMTDPOP) {
        memcpy(&dst->rmtDpop, &src->rmtDpop, sizeof(AXPBRMTDPOP));
    }

    if (sync & AX_PBSYNC_RMTSRC) {
        memcpy(&dst->rmtSrc, &src->rmtSrc, sizeof(AXPBRMTSRC));
    }

    if (sync & AX_PBSYNC_RMTIIR_LPF_COEFS) {
        dst->rmtIIR.lpf.a0 = src->rmtIIR.lpf.a0;
        dst->rmtIIR.lpf.b0 = src->rmtIIR.lpf.b0;
    } else if (sync & AX_PBSYNC_RMTIIR_BIQUAD_COEFS) {
        dst->rmtIIR.biquad.b0 = src->rmtIIR.biquad.b0;
        dst->rmtIIR.biquad.b1 = src->rmtIIR.biquad.b1;
        dst->rmtIIR.biquad.b2 = src->rmtIIR.biquad.b2;
        dst->rmtIIR.biquad.a1 = src->rmtIIR.biquad.a1;
        dst->rmtIIR.biquad.a2 = src->rmtIIR.biquad.a2;
    } else if (sync & AX_PBSYNC_RMTIIR) {
        FUNNY_COPY(&dst->rmtIIR, &src->rmtIIR, u16, sizeof(AXPBRMTIIR));
    }
}

/*
 * __AXDumpVPB
 *
 * Stops a voice that does not fit the DSP cycle budget and queues its callback.
 */
void __AXDumpVPB(AXVPB* vpb) {
    AXPB* pb = &__AXPB[vpb->index];

    if (pb->state == AX_VOICE_RUN) {
        __AXDepopVoice(pb);
    }

    vpb->pb.state = AX_VOICE_STOP;
    pb->state = AX_VOICE_STOP;

    __AXPushCallbackStack(vpb);
}

/*
 * __AXSyncPBs
 *
 * Walks the priority stacks, estimates the DSP cycles each running voice costs and syncs the voices that fit.
 */
void __AXSyncPBs(u32 baseCycles) {
    u32 cycles;
    u32 prio;
    AXVPB* head;

    __AXNumVoices = 0;

    DCInvalidateRange(__AXPB, __AXMaxVoices * sizeof(AXPB));
    DCInvalidateRange(__AXITD, __AXMaxVoices * sizeof(AXITD));

    cycles = 32 + __AXGetCommandListCycles() + __AXMaxVoices * 600 + baseCycles;

    for (prio = AX_PRIORITY_MAX; prio > AX_PRIORITY_FREE; prio--) {
        for (head = __AXGetStackHead(prio); head != NULL; head = head->next) {
            if (head->pb.itd.flag == 1) {
                cycles += 129;
            }

            if (head->depop) {
                __AXDepopVoice(&__AXPB[head->index]);
            }

            if (head->pb.state == AX_VOICE_RUN) {
                cycles += 387;

                if (head->pb.lpf.on) {
                    cycles += 309;
                }

                if (head->pb.biquad.on) {
                    cycles += 1024;
                }

                if (head->pb.itd.flag == 1) {
                    cycles += 27;
                }

                cycles += __AXGetSrcCycles(head->pb.srcSelect, &head->pb.src);
                cycles += __AXMixCycles[head->pb.mixerCtrl >> 0 & 31] +
                          __AXMixCycles[head->pb.mixerCtrl >> 16 & 31] +
                          __AXMixCycles[head->pb.mixerCtrl >> 21 & 31] +
                          __AXMixCycles[head->pb.mixerCtrl >> 26 & 31];

                if (head->pb.remote == TRUE) {
                    cycles += 613;

                    if (head->pb.rmtIIR.lpf.on == AX_PB_LPF_ON) {
                        cycles += 118;
                    } else if (head->pb.rmtIIR.biquad.on == AX_PB_BIQUAD_ON) {
                        cycles += 834;
                    }

                    cycles +=
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 0 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 2 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 4 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 6 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 8 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 10 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 12 & 3] +
                        __AXRmtMixCycles[head->pb.rmtMixerCtrl >> 14 & 3];
                }

                if (__AXMaxDspCycles > cycles) {
                    __AXServiceVPB(head);
                } else {
                    __AXDumpVPB(head);
                }

            } else {
                __AXServiceVPB(head);
            }

            head->sync = 0;
            head->depop = FALSE;
        }
    }

    __AXRecDspCycles = cycles;

    for (head = __AXGetStackHead(AX_PRIORITY_FREE); head != NULL;
         head = head->next) {
        if (head->depop) {
            __AXDepopVoice(&__AXPB[head->index]);
        }

        head->depop = FALSE;
        __AXPB[head->index].state = AX_VOICE_STOP;
    }

    DCFlushRange(__AXPB, __AXMaxVoices * sizeof(AXPB));
    DCFlushRange(__AXITD, __AXMaxVoices * sizeof(AXITD));
}

/*
 * __AXGetPBs
 */
AXPB* __AXGetPBs(void) {
    return __AXPB;
}

/*
 * __AXSetPBDefault
 *
 * Resets a voice to the stopped default state.
 */
void __AXSetPBDefault(AXVPB* vpb) {
    vpb->pb.state = AX_VOICE_STOP;
    vpb->pb.itd.flag = 0;
    vpb->sync = AX_PBSYNC_STATE | AX_PBSYNC_ITD | AX_PBSYNC_LPF |
                AX_PBSYNC_BIQUAD | AX_PBSYNC_REMOTE | AX_PBSYNC_RMTSRC |
                AX_PBSYNC_RMTIIR;
    vpb->pb.lpf.on = 0;
    vpb->pb.biquad.on = 0;
    vpb->pb.remote = 0;
    vpb->pb.rmtIIR.lpf.on = 0;
    vpb->pb.rmtSrc.currentAddressFrac = 0;
    vpb->pb.rmtSrc.last_samples[0] = 0;
    vpb->pb.rmtSrc.last_samples[1] = 0;
    vpb->pb.rmtSrc.last_samples[2] = 0;
    vpb->pb.rmtSrc.last_samples[3] = 0;
}

/*
 * __AXVPBInit
 */
void __AXVPBInit(void) {
    __AXMaxVoices = AX_VOICE_MAX;

    __AXPB = __s_AXPB;
    __AXITD = __s_AXITD;
    __AXVPB = __s_AXVPB;

    __AXVPBInitCommon();
}

/*
 * __AXVPBInitCommon
 *
 * Clears the parameter block arrays and links the voices into the free stack.
 */
static void __AXVPBInitCommon(void) {
    u32 i;
    u32* dst;

    __AXMaxDspCycles = OS_BUS_CLOCK / 667;
    __AXRecDspCycles = 0;

    for (dst = (u32*)__AXPB, i = __AXMaxVoices * (sizeof(AXPB) / sizeof(u32));
         i > 0; i--) {
        *dst++ = 0;
    }

    for (dst = (u32*)__AXITD, i = __AXMaxVoices * (sizeof(AXITD) / sizeof(u32));
         i > 0; i--) {
        *dst++ = 0;
    }

    for (dst = (u32*)__AXVPB, i = __AXMaxVoices * (sizeof(AXVPB) / sizeof(u32));
         i > 0; i--) {
        *dst++ = 0;
    }

    for (i = 0; i < __AXMaxVoices; i++) {
        AXPB* pb = &__AXPB[i];
        AXITD* itd = &__AXITD[i];
        AXVPB* vpb = &__AXVPB[i];

        vpb->index = i;
        vpb->itdBuffer = itd;
        __AXSetPBDefault(vpb);

        if (i == __AXMaxVoices - 1) {
            pb->nextHi = pb->nextLo = 0;
            vpb->pb.nextHi = vpb->pb.nextLo = 0;
        } else {
            vpb->pb.nextHi = ADDRHI((u32)pb + 320);
            vpb->pb.nextLo = ADDRLO((u32)pb + 320);

            pb->nextHi = ADDRHI((u32)pb + 320);
            pb->nextLo = ADDRLO((u32)pb + 320);
        }

        vpb->pb.currHi = ADDRHI(pb);
        vpb->pb.currLo = ADDRLO(pb);

        pb->currHi = ADDRHI(pb);
        pb->currLo = ADDRLO(pb);

        vpb->pb.itd.bufferHi = ADDRHI(itd);
        vpb->pb.itd.bufferLo = ADDRLO(itd);

        pb->itd.bufferHi = ADDRHI(itd);
        pb->itd.bufferLo = ADDRLO(itd);

        vpb->priority = AX_PRIORITY_MIN;
        __AXPushFreeStack(vpb);
    }

    DCFlushRange(__AXPB, __AXMaxVoices * sizeof(AXPB));
}


/*
 * __AXVPBQuit (invented name)
 */
void __AXVPBQuit(void) {
    __AXPB = NULL;
    __AXITD = NULL;
    __AXVPB = NULL;
    __AXMaxVoices = 0;
} // 0x80230190

/*
 * AXSetVoiceSrcType
 */
void AXSetVoiceSrcType(AXVPB* vpb, u32 type) {
    BOOL enabled = OSDisableInterrupts();

    switch (type) {
    case AX_SRC_TYPE_NONE:
        vpb->pb.srcSelect = AX_PB_SRCSEL_NONE;
        break;
    case AX_SRC_TYPE_LINEAR:
        vpb->pb.srcSelect = AX_PB_SRCSEL_LINEAR;
        break;
    case AX_SRC_TYPE_4TAP_8K:
        vpb->pb.srcSelect = AX_PB_SRCSEL_POLYPHASE;
        vpb->pb.coefSelect = AX_PB_COEFSEL_8KHZ;
        break;
    case AX_SRC_TYPE_4TAP_12K:
        vpb->pb.srcSelect = AX_PB_SRCSEL_POLYPHASE;
        vpb->pb.coefSelect = AX_PB_COEFSEL_12KHZ;
        break;
    case AX_SRC_TYPE_4TAP_16K:
        vpb->pb.srcSelect = AX_PB_SRCSEL_POLYPHASE;
        vpb->pb.coefSelect = AX_PB_COEFSEL_16KHZ;
        break;
    }

    vpb->sync |= AX_PBSYNC_SELECT;

    OSRestoreInterrupts(enabled);
} // 0x802301B0

/*
 * AXSetVoiceState
 */
void AXSetVoiceState(AXVPB* vpb, u16 state) {
    BOOL enabled = OSDisableInterrupts();

    if (vpb->pb.state == state) {
        OSRestoreInterrupts(enabled);
        return;
    }

    vpb->pb.state = state;
    vpb->sync |= AX_PBSYNC_STATE;

    if (state == AX_VOICE_STOP) {
        vpb->depop = TRUE;
    }

    OSRestoreInterrupts(enabled);
} // 0x80230270

/*
 * AXSetVoiceType
 */
void AXSetVoiceType(AXVPB* vpb, u16 type) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.type = type;
    vpb->sync |= AX_PBSYNC_TYPE;

    OSRestoreInterrupts(enabled);
} // 0x802302E0

/*
 * AXSetVoiceAddr
 */
void AXSetVoiceAddr(AXVPB* vpb, AXPBADDR* addr) {
    BOOL enabled = OSDisableInterrupts();

    u32* dst = (u32*)&vpb->pb.addr;
    const u32* src = (u32*)addr;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;

    switch (addr->format) {
    case AX_SAMPLE_FORMAT_PCM_S16:
        dst++;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0x08000000; // adpcm.gain 0x0800, pred_scale 0: the DSP's fixed gain for 16-bit PCM
        *dst = 0;
        break;
    case AX_SAMPLE_FORMAT_PCM_S8:
        dst++;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0x01000000; // adpcm.gain 0x0100, pred_scale 0: the DSP's fixed gain for 8-bit PCM
        *dst = 0;
        break;
    case AX_SAMPLE_FORMAT_DSP_ADPCM:
    default:
        break;
    }

    vpb->sync &= ~(AX_PBSYNC_LOOP_FLAG | AX_PBSYNC_LOOP_ADDR |
                   AX_PBSYNC_END_ADDR | AX_PBSYNC_CURR_ADDR);
    vpb->sync |= AX_PBSYNC_ADDR | AX_PBSYNC_ADPCM;

    OSRestoreInterrupts(enabled);
} // 0x80230330

/*
 * AXSetVoiceCurrentAddr
 */
void AXSetVoiceCurrentAddr(AXVPB* vpb, u32 addr) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.addr.currentAddressHi = addr >> 16;
    vpb->pb.addr.currentAddressLo = addr;
    vpb->sync |= AX_PBSYNC_CURR_ADDR;

    OSRestoreInterrupts(enabled);
} // 0x80230420

/*
 * AXSetVoiceAdpcm
 */
void AXSetVoiceAdpcm(AXVPB* vpb, AXPBADPCM* adpcm) {
    BOOL enabled = OSDisableInterrupts();

    u32* dst = (u32*)&vpb->pb.adpcm;
    const u32* src = (u32*)adpcm;

    // The parameter block is copied word by word, unrolled by hand.
    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;
    dst++;
    src++;

    *dst = *src;

    vpb->sync |= AX_PBSYNC_ADPCM;

    OSRestoreInterrupts(enabled);
} // 0x80230480

/*
 * AXSetVoiceSrc
 */
void AXSetVoiceSrc(AXVPB* vpb, AXPBSRC* src) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.src.ratioHi = src->ratioHi;
    vpb->pb.src.ratioLo = src->ratioLo;
    vpb->pb.src.currentAddressFrac = src->currentAddressFrac;
    vpb->pb.src.last_samples[0] = src->last_samples[0];
    vpb->pb.src.last_samples[1] = src->last_samples[1];
    vpb->pb.src.last_samples[2] = src->last_samples[2];
    vpb->pb.src.last_samples[3] = src->last_samples[3];

    vpb->sync &= ~AX_PBSYNC_SRC_RATIO;
    vpb->sync |= AX_PBSYNC_SRC;

    OSRestoreInterrupts(enabled);
} // 0x80230520

/*
 * AXSetVoiceSrcRatio
 */
void AXSetVoiceSrcRatio(AXVPB* vpb, f32 ratio) {
    BOOL enabled = OSDisableInterrupts();
    u32 ratioFixed = 65536.0f * ratio;

    vpb->pb.src.ratioHi = ratioFixed >> 16;
    vpb->pb.src.ratioLo = ratioFixed;
    vpb->sync |= AX_PBSYNC_SRC_RATIO;

    OSRestoreInterrupts(enabled);
} // 0x802305B0

/*
 * AXSetVoiceAdpcmLoop
 */
void AXSetVoiceAdpcmLoop(AXVPB* vpb, AXPBADPCMLOOP* loop) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.adpcmLoop.loop_pred_scale = loop->loop_pred_scale;
    vpb->pb.adpcmLoop.loop_yn1 = loop->loop_yn1;
    vpb->pb.adpcmLoop.loop_yn2 = loop->loop_yn2;
    vpb->sync |= AX_PBSYNC_ADPCM_LOOP;

    OSRestoreInterrupts(enabled);
} // 0x80230630

/*
 * AXSetVoiceLpf
 */
void AXSetVoiceLpf(AXVPB* vpb, AXPBLPF* lpf) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.lpf.on = lpf->on;
    vpb->pb.lpf.yn1 = lpf->yn1;
    vpb->pb.lpf.a0 = lpf->a0;
    vpb->pb.lpf.b0 = lpf->b0;
    vpb->sync |= AX_PBSYNC_LPF;

    OSRestoreInterrupts(enabled);
} // 0x80230690

/*
 * AXSetVoiceLpfCoefs
 */
void AXSetVoiceLpfCoefs(AXVPB* vpb, u16 a0, u16 b0) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.lpf.a0 = a0;
    vpb->pb.lpf.b0 = b0;
    vpb->sync |= AX_PBSYNC_LPF_COEFS;

    OSRestoreInterrupts(enabled);
} // 0x80230700

/*
 * AXGetLpfCoefs
 */
void AXGetLpfCoefs(u16 freq, u16* a, u16* b) {
    // The double cos, cast back: the cosf inline (ogws) emits the .sdata2 constants in another order.
    f32 rf31 = 2.0f - (f32)cos(2 * M_PI * freq / AX_SAMPLE_RATE);
    f32 rf30 = (f32)sqrt(rf31 * rf31 - 1.0f) - rf31;

    *b = 32768 * -rf30;
    *a = 32767 - *b;
} // 0x80230760

/*
 * AXSetVoiceBiquad
 */
void AXSetVoiceBiquad(AXVPB* vpb, AXPBBIQUAD* biquad) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.biquad.on = biquad->on;
    vpb->pb.biquad.xn1 = biquad->xn1;
    vpb->pb.biquad.xn2 = biquad->xn2;
    vpb->pb.biquad.yn1 = biquad->yn1;
    vpb->pb.biquad.yn2 = biquad->yn2;
    vpb->pb.biquad.b0 = biquad->b0;
    vpb->pb.biquad.b1 = biquad->b1;
    vpb->pb.biquad.b2 = biquad->b2;
    vpb->pb.biquad.a1 = biquad->a1;
    vpb->pb.biquad.a2 = biquad->a2;
    vpb->sync |= AX_PBSYNC_BIQUAD;

    OSRestoreInterrupts(enabled);
} // 0x80230820

/*
 * AXSetVoiceBiquadCoefs
 */
void AXSetVoiceBiquadCoefs(AXVPB* vpb, u16 b0, u16 b1, u16 b2, u16 a1, u16 a2) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.biquad.b0 = b0;
    vpb->pb.biquad.b1 = b1;
    vpb->pb.biquad.b2 = b2;
    vpb->pb.biquad.a1 = a1;
    vpb->pb.biquad.a2 = a2;
    vpb->sync |= AX_PBSYNC_BIQUAD_COEFS;

    OSRestoreInterrupts(enabled);
} // 0x802308C0

/*
 * AXSetVoiceRmtOn
 */
void AXSetVoiceRmtOn(AXVPB* vpb, u16 on) {
    BOOL enabled = OSDisableInterrupts();

    vpb->pb.remote = on;
    vpb->sync |= AX_PBSYNC_REMOTE;

    OSRestoreInterrupts(enabled);
} // 0x80230930

/*
 * AXGetMaxVoices
 */
s32 AXGetMaxVoices(void) {
    return __AXMaxVoices;
} // 0x80230980

/*
 * __AXGetSrcCycles
 */
static u32 __AXGetSrcCycles(u16 select, const AXPBSRC* src) {
    u32 ratio = src->ratioHi << 16 | src->ratioLo;

    if (select == 0) {
        return (ratio * 512 + 32768) / 65536 + 1561;
    }

    if (select == 1) {
        return (ratio * 512 + 32768) / 65536 + 1466;
    }

    return 605;
}
