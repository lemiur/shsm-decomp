// Reference: zeldaret/tp libs/dolphin/src/mix/mix.c (GameCube MIX). The remote-speaker half of the Wii MIX has no
// public source; the retail bytes put it out of line from mix.c (see the header comment of mix.c), so it is a unit of
// its own. Unit name and path unconfirmed (invented).
#include <revolution/ax.h>
#include <revolution/mix.h>
#include <revolution/mix/__mix.h>

/* Remote channel mode: a set bit makes that remote's aux volume absolute instead of relative to its main volume. */
#define MIX_RMT_MODE_AUX0 0x1
#define MIX_RMT_MODE_AUX1 0x2
#define MIX_RMT_MODE_AUX2 0x4
#define MIX_RMT_MODE_AUX3 0x8
#define MIX_RMT_MODE_MASK 0xF

void MIXRmtSetVolumes(AXVPB* p, u32 mode, int main0, int main1, int main2, int main3, int aux0, int aux1, int aux2,
                      int aux3) {
    MIXRmtChannel* c;

    c = &__MIXRmtChannel[p->index];
    c->mode |= mode & MIX_RMT_MODE_MASK;
    c->main0 = main0;
    c->main1 = main1;
    c->main2 = main2;
    c->main3 = main3;
    c->aux0 = aux0;
    c->aux1 = aux1;
    c->aux2 = aux2;
    c->aux3 = aux3;
    c->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80236720

void __MIXUpdateRmtChannel(int index, AXVPB* axvpb) {
    MIXRmtChannel* c;
    u16 mixerCtrl;
    u16* p;

    c = &__MIXRmtChannel[index];

    if (c->mode & (MIX_FLAG_MIX_APPLY | MIX_FLAG_MIX_DIRTY)) {
        if (c->mode & MIX_FLAG_MIX_APPLY) {
            c->vMain0 = c->vMain0Next;
            c->vAux0 = c->vAux0Next;
            c->vMain1 = c->vMain1Next;
            c->vAux1 = c->vAux1Next;
            c->vMain2 = c->vMain2Next;
            c->vAux2 = c->vAux2Next;
            c->vMain3 = c->vMain3Next;
            c->vAux3 = c->vAux3Next;
            c->mode &= ~MIX_FLAG_MIX_APPLY;
        }

        if (c->mode & MIX_FLAG_MIX_DIRTY) {
            c->vMain0Next = __MIXGetVolume(c->main0);
            c->vMain1Next = __MIXGetVolume(c->main1);
            c->vMain2Next = __MIXGetVolume(c->main2);
            c->vMain3Next = __MIXGetVolume(c->main3);

            if (c->mode & MIX_RMT_MODE_AUX0) {
                c->vAux0Next = __MIXGetVolume(c->aux0);
            } else {
                c->vAux0Next = __MIXGetVolume(c->main0 + c->aux0);
            }

            if (c->mode & MIX_RMT_MODE_AUX1) {
                c->vAux1Next = __MIXGetVolume(c->aux1);
            } else {
                c->vAux1Next = __MIXGetVolume(c->main1 + c->aux1);
            }

            if (c->mode & MIX_RMT_MODE_AUX2) {
                c->vAux2Next = __MIXGetVolume(c->aux2);
            } else {
                c->vAux2Next = __MIXGetVolume(c->main2 + c->aux2);
            }

            if (c->mode & MIX_RMT_MODE_AUX3) {
                c->vAux3Next = __MIXGetVolume(c->aux3);
            } else {
                c->vAux3Next = __MIXGetVolume(c->main3 + c->aux3);
            }

            c->mode &= ~MIX_FLAG_MIX_DIRTY;
            c->mode |= MIX_FLAG_MIX_APPLY;
        }

        mixerCtrl = 0;
        p = (u16*)&axvpb->pb.rmtMix;

        if ((*p++ = c->vMain0))
            mixerCtrl |= 0x1;
        if ((*p++ = (u16)((c->vMain0Next - c->vMain0) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x2;

        if ((*p++ = c->vAux0))
            mixerCtrl |= 0x4;
        if ((*p++ = (u16)((c->vAux0Next - c->vAux0) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x8;

        if ((*p++ = c->vMain1))
            mixerCtrl |= 0x10;
        if ((*p++ = (u16)((c->vMain1Next - c->vMain1) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x20;

        if ((*p++ = c->vAux1))
            mixerCtrl |= 0x40;
        if ((*p++ = (u16)((c->vAux1Next - c->vAux1) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x80;

        if ((*p++ = c->vMain2))
            mixerCtrl |= 0x100;
        if ((*p++ = (u16)((c->vMain2Next - c->vMain2) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x200;

        if ((*p++ = c->vAux2))
            mixerCtrl |= 0x400;
        if ((*p++ = (u16)((c->vAux2Next - c->vAux2) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x800;

        if ((*p++ = c->vMain3))
            mixerCtrl |= 0x1000;
        if ((*p++ = (u16)((c->vMain3Next - c->vMain3) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x2000;

        if ((*p++ = c->vAux3))
            mixerCtrl |= 0x4000;
        if ((*p++ = (u16)((c->vAux3Next - c->vAux3) / AX_SAMPLES_PER_FRAME_RMT)))
            mixerCtrl |= 0x8000;

        axvpb->pb.rmtMixerCtrl = mixerCtrl;
        axvpb->sync |= AX_PBSYNC_RMT_MIXER_CTRL | AX_PBSYNC_RMTMIX;
    }
} // 0x80236770

void __MIXResetRmtChannel(int index) {
    MIXRmtChannel* c;

    c = &__MIXRmtChannel[index];
    c->mode = 0;
    c->main0 = 0;
    c->main1 = 0;
    c->main2 = 0;
    c->main3 = 0;
    c->aux0 = -0x3C0;
    c->aux1 = -0x3C0;
    c->aux2 = -0x3C0;
    c->aux3 = -0x3C0;
    c->vMain0 = c->vAux0 = c->vMain1 = c->vAux1 = c->vMain2 = c->vAux2 = c->vMain3 = c->vAux3 = 0;
} // 0x80236BB0
