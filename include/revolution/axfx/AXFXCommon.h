// Reference: doldecomp/ogws include/revolution/AXFX/AXFXCommon.h
#ifndef REVOLUTION_AXFX_COMMON_H
#define REVOLUTION_AXFX_COMMON_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The three aux buses of a stereo/surround effect (one frame of AX_SAMPLES_PER_FRAME samples each). */
typedef struct AXFX_BUS {
    s32* left;     // at 0x0
    s32* right;    // at 0x4
    s32* surround; // at 0x8
} AXFX_BUS;

typedef struct AXFX_BUFFERUPDATE {
    s32* left;     // at 0x0
    s32* right;    // at 0x4
    s32* surround; // at 0x8
} AXFX_BUFFERUPDATE;

/* Dolby Pro Logic II output has a fourth channel. */
typedef struct AXFX_BUS_DPL2 {
    s32* left;           // at 0x0
    s32* right;          // at 0x4
    s32* left_surround;  // at 0x8
    s32* right_surround; // at 0xC
} AXFX_BUS_DPL2;

typedef struct AXFX_BUFFERUPDATE_DPL2 {
    s32* left;           // at 0x0
    s32* right;          // at 0x4
    s32* left_surround;  // at 0x8
    s32* right_surround; // at 0xC
} AXFX_BUFFERUPDATE_DPL2;

#ifdef __cplusplus
}
#endif

#endif
