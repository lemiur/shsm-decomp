// Standard reverb, simple interface (no public header exists; modelled on AXFXReverbHi.h).
#ifndef REVOLUTION_AXFX_REVERBSTD_H
#define REVOLUTION_AXFX_REVERBSTD_H

#include <revolution/axfx/AXFXReverbStdExp.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AXFX_REVERBSTD {
    AXFX_REVERBSTD_EXP exp; // at 0x0
    f32 coloration;         // at 0xE8
    f32 mix;                // at 0xEC
    f32 time;               // at 0xF0
    f32 damping;            // at 0xF4
    f32 preDelay;           // at 0xF8
} AXFX_REVERBSTD;

BOOL AXFXReverbStdInit(AXFX_REVERBSTD* fx);
BOOL AXFXReverbStdShutdown(AXFX_REVERBSTD* fx);
void AXFXReverbStdCallback(void* chans, void* context);

#ifdef __cplusplus
}
#endif

#endif
