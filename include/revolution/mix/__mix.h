// MIX-internal declarations shared by mix.c and mix_rmt.c. Each function and global is declared once, here or in mix.h.
#ifndef REVOLUTION_MIX_INTERNAL_H
#define REVOLUTION_MIX_INTERNAL_H

#include <revolution/ax.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIX_MAX_VOICES 96

/* Update state kept in the top bits of a channel's mode: the mix volumes changed (DIRTY), or the volumes computed by
 * the previous update are due to become current (APPLY). */
#define MIX_FLAG_MIX_DIRTY 0x40000000
#define MIX_FLAG_MIX_APPLY 0x80000000

/* The remote speaker (Wii Remote) side of a voice: dB settings for the four remotes and the volumes they ramp to. */
typedef struct MIXRmtChannel {
    /* 0x00 */ u32 mode;
    /* 0x04 */ int main0;
    /* 0x08 */ int main1;
    /* 0x0C */ int main2;
    /* 0x10 */ int main3;
    /* 0x14 */ int aux0;
    /* 0x18 */ int aux1;
    /* 0x1C */ int aux2;
    /* 0x20 */ int aux3;
    /* 0x24 */ u16 vMain0;
    /* 0x26 */ u16 vMain0Next;
    /* 0x28 */ u16 vAux0;
    /* 0x2A */ u16 vAux0Next;
    /* 0x2C */ u16 vMain1;
    /* 0x2E */ u16 vMain1Next;
    /* 0x30 */ u16 vAux1;
    /* 0x32 */ u16 vAux1Next;
    /* 0x34 */ u16 vMain2;
    /* 0x36 */ u16 vMain2Next;
    /* 0x38 */ u16 vAux2;
    /* 0x3A */ u16 vAux2Next;
    /* 0x3C */ u16 vMain3;
    /* 0x3E */ u16 vMain3Next;
    /* 0x40 */ u16 vAux3;
    /* 0x42 */ u16 vAux3Next;
} MIXRmtChannel;

extern MIXRmtChannel* __MIXRmtChannel;

u16 __MIXGetVolume(int db);
void __MIXResetRmtChannel(int index);
void __MIXUpdateRmtChannel(int index, AXVPB* axvpb);

#ifdef __cplusplus
}
#endif

#endif // REVOLUTION_MIX_INTERNAL_H
